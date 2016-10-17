/**
 * @file crorc_dma_in_pgsweep.cpp
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2016-07-18
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * */

#include "crorc_dma_out.h"
#include <librorc.h>
#include <sys/shm.h>
#include <signal.h>

using namespace std;

#define EVENTSIZE_DW_MIN 64
#define EVENTSIZE_DW_MAX 0x800000
#define TIME_ES_TO_STAT 1
#define TIME_STAT_TO_STAT 1
#define ITERATIONS_PER_ES 5
#define LIBRORC_MAX_DMA_CHANNELS 12
#define HELP_TEXT                                                              \
  "crorc_dma_out_pgsweep options:\n"                                            \
  " -n [deviceId]  select C-RORC device, optional, default:0\n"                \
  " -h             show this help\n"

bool done = false;

void abort_handler(int s) {
  cerr << "Caught signal " << s << endl;
  if (done == true) {
    exit(-1);
  } else {
    done = true;
  }
}

typedef struct {
  struct t_sts chstats[LIBRORC_MAX_DMA_CHANNELS];
  timeval time;
} tChannelSnapshot;

typedef struct {
  uint64_t bytes;
  uint64_t events;
  uint64_t time;
} tSnapshotDiff;

int setEventSizeDW(librorc::bar *bar, uint32_t ChannelId,
                   uint32_t EventSizeDW) {
  /** get current link */
  librorc::link *current_link = new librorc::link(bar, ChannelId);

  /** set new EventSizeDW
   *  '-1' because EOE is attached to each event. Without '-1' but
   *  EventSizeDWs aligned to the max payload size boundaries a new full
   *  packet is sent containing only the EOE word. -> would take
   *  bandwidth but would not appear in number of bytes transferred
   *  */
  current_link->setDdlReg(RORC_REG_DDL_PG_EVENT_LENGTH, EventSizeDW - 1);

  delete current_link;
  return 0;
}

tChannelSnapshot
getSnapshot(struct t_sts *chstats[LIBRORC_MAX_DMA_CHANNELS]) {
  tChannelSnapshot chss;

  // capture current time
  gettimeofday(&(chss.time), 0);

  for (int i = 0; i < LIBRORC_MAX_DMA_CHANNELS; i++) {
    memcpy(&(chss.chstats[i]), chstats[i], sizeof(struct t_sts));
  }
  return chss;
}

int64_t timediff_us(struct timeval from, struct timeval to) {
  return ((int64_t)(to.tv_sec - from.tv_sec) * 1000000LL +
          (int64_t)(to.tv_usec - from.tv_usec));
}

tSnapshotDiff getSnapshotDiff(tChannelSnapshot last, tChannelSnapshot current) {
  tSnapshotDiff sd;
  sd.bytes = 0;
  sd.events = 0;
  for (uint32_t chID = 0; chID < LIBRORC_MAX_DMA_CHANNELS; chID++) {
    sd.bytes += (current.chstats[chID].bytes_received -
                 last.chstats[chID].bytes_received);
    sd.events += (current.chstats[chID].n_events - last.chstats[chID].n_events);
  }
  sd.time = timediff_us(last.time, current.time);
  return sd;
}

uint32_t nextMpsEventSizeDW(uint32_t EventSizeDW, uint32_t maxPayloadSize) {
  uint32_t numPkts = (EventSizeDW << 2) / maxPayloadSize;
  uint32_t newSize = ((2 * numPkts * maxPayloadSize) >> 2);
  if (newSize > EVENTSIZE_DW_MAX) {
    return EVENTSIZE_DW_MIN;
  } else {
    return newSize;
  }
}

int main(int argc, char *argv[]) {
  int32_t DeviceId = 0;
  struct t_sts *chstats[LIBRORC_MAX_DMA_CHANNELS];
  int32_t shID[LIBRORC_MAX_DMA_CHANNELS];
  char *shm[LIBRORC_MAX_DMA_CHANNELS];
  int arg;
  int nChannels = 0;

  /** parse command line arguments **/
  while ((arg = getopt(argc, argv, "hn:c:")) != -1) {
    switch (arg) {
    case 'n':
      DeviceId = strtol(optarg, NULL, 0);
      break;
    case 'h':
      cout << HELP_TEXT;
      return 0;
    case 'c':
      nChannels = strtol(optarg, NULL, 0);
      break;
    default:
      cout << "Unknown parameter (" << arg << ")!" << endl;
      cout << HELP_TEXT;
      break;
    }
  }

  /** Initialize shm channels */
  for (int chID = 0; chID < LIBRORC_MAX_DMA_CHANNELS; chID++) {
    shID[chID] =
        shmget(SHM_BASE + chID, sizeof(struct t_sts), IPC_CREAT | 0666);
    if (shID[chID] == -1) {
      perror("shmget");
      return -1;
    }
    shm[chID] = NULL;
    shm[chID] = (char *)shmat(shID[chID], NULL, 0);
    if (shm[chID] == (char *)-1) {
      perror("shmat");
      return -1;
    }

    chstats[chID] = NULL;
    chstats[chID] = (struct t_sts *)shm[chID];
  }

  librorc::device *dev = NULL;
  librorc::bar *bar = NULL;
  librorc::sysmon *sm = NULL;

  try {
    dev = new librorc::device(DeviceId);
    bar = new librorc::bar(dev, 1);
    sm = new librorc::sysmon(bar);
  }
  catch (int e) {
    cout << "ERROR: failed to initialize device: " << librorc::errMsg(e)
         << endl;
    if (sm) {
      delete sm;
    }
    if (bar) {
      delete bar;
    }
    if (dev) {
      delete dev;
    }
    return -1;
  }

  uint32_t maxPayloadSize = 128;//dev->maxPayloadSize();
  uint32_t startChannel = 0;
  uint32_t endChannel =
      (nChannels == 0) ? (sm->numberOfChannels() - 1) : (nChannels - 1);

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = abort_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  printf("# numPciePkts, EventSizeBytes, ebrate, rbrate, eventrate\n");
  uint32_t EventSizeDW = EVENTSIZE_DW_MIN;
  while (!done) {
    EventSizeDW = nextMpsEventSizeDW(EventSizeDW, maxPayloadSize);
    uint64_t EventSizeBytes = (EventSizeDW << 2);
    uint64_t numPciePkts = EventSizeBytes / maxPayloadSize;
    for (uint32_t chID = startChannel; chID <= endChannel; chID++) {
      chstats[chID]->eventSize = EventSizeBytes;
    }

    // wait until all channels are receiving the requested eventSize
    bool esMatch = false;
    while(!esMatch) {
      esMatch = true;
      tChannelSnapshot css = getSnapshot(chstats);
      for (uint32_t chID = startChannel; chID <= endChannel; chID++) {
        uint32_t actEventSize = css.chstats[chID].rcvEventSize;
        if (actEventSize != EventSizeBytes) {
          // cout << "Channel " << chID << " expected " << EventSizeBytes << ", received "
          //      << chstats[chID]->rcvEventSize << " bytes" << endl;
          esMatch = false;
          break;
        }
      }
    }

    sleep(TIME_ES_TO_STAT);
    tChannelSnapshot last_status = getSnapshot(chstats);

    for (int itCount = 0; itCount < ITERATIONS_PER_ES; itCount++) {
      sleep(TIME_STAT_TO_STAT);
      tChannelSnapshot cur_status = getSnapshot(chstats);
      tSnapshotDiff diff = getSnapshotDiff(last_status, cur_status);
      uint64_t ebrate = diff.bytes * 1000000 / diff.time;
      uint64_t rbrate = diff.events * 16 * 1000000 / diff.time;
      printf("%ld, %ld, %ld, %ld, %ld\n", numPciePkts, EventSizeBytes, ebrate,
             rbrate, diff.events);
      last_status = cur_status;
    }
  }

  for (int32_t i = 0; i < LIBRORC_MAX_DMA_CHANNELS; i++) {
    if (shm[i] != NULL) {
      shmdt(shm[i]);
      shm[i] = NULL;
    }
  }

  delete sm;
  delete bar;
  delete dev;

  return 0;
}
