/**
 * @file crorc_dma_out.cpp
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2016-07-26
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
#include <getopt.h>
#include <signal.h>
#include <sys/shm.h>
#include <errno.h>
#include <iostream>

#define BUFFERSIZE (1ull << 30) // 1GB

using namespace std;

bool done = false;

// Signal handler
void abort_handler(int s) {
  cerr << "Caught signal " << s << endl;
  if (done == true) {
    exit(-1);
  } else {
    done = true;
  }
}

inline int64_t timediff_us(struct timeval from, struct timeval to) {
  return ((int64_t)(to.tv_sec - from.tv_sec) * 1000000LL +
          (int64_t)(to.tv_usec - from.tv_usec));
}

void printStatusLine(struct t_sts *cs_cur, struct t_sts *cs_last,
                     int64_t tdiff_us);

int main(int argc, char *argv[]) {
  librorc::event_stream *es = NULL;
  int deviceId = 0;
  int channelId = 0;
  int pciPacketSize = 128;
  static struct option long_options[] = {
    { "device", required_argument, 0, 'n' },
    { "channel", required_argument, 0, 'c' },
    { "packetsize", required_argument, 0, 'P' },
    { "help", no_argument, 0, 'h' },
    { 0, 0, 0, 0 }
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "n:c:P:", long_options, NULL)) != -1) {
    switch (opt) {
    case 'n':
      deviceId = strtoul(optarg, NULL, 0);
      break;
    case 'c':
      channelId = strtoul(optarg, NULL, 0);
      break;
    case 'P':
      pciPacketSize = strtoul(optarg, NULL, 0);
      break;
    default:
      return -1;
      break;
    }
  }

  int shmId =
      shmget(SHM_BASE + channelId, sizeof(struct t_sts), IPC_CREAT | 0666);
  if (shmId < 0) {
    cerr << "ERROR: Failed to get shared memory: " << strerror(errno) << endl;
    return -1;
  }
  void *shm = shmat(shmId, NULL, 0);
  if (shm == (void *)-1) {
    cerr << "ERROR: failed to attach to shared memory: " << strerror(errno)
         << endl;
    return -1;
  }
  struct t_sts *sts = (struct t_sts *)shm;
  memset(sts, 0, sizeof(struct t_sts));
  sts->deviceId = deviceId;
  sts->channelId = channelId;
  sts->eventSize = (1<<10)*sizeof(uint32_t);

  try {
    es = new librorc::event_stream(deviceId, channelId,
                                   librorc::kEventStreamToDevice);
  }
  catch (int e) {
    cerr << "ERROR: Failed to initialize channel " << channelId << ": "
         << librorc::errMsg(e) << endl;
    return -1;
  }
  int result = es->initializeDma(channelId*2, BUFFERSIZE);
  if (result) {
    cerr << "ERROR: Failed to initialize DMA for channel " << channelId << ": "
         << librorc::errMsg(result) << endl;
    delete es;
    return -1;
  }

  // wait for DDL clock domain to be ready
  while (!es->m_link->isDdlDomainReady()) {
    usleep(100);
  }
  es->m_link->setFlowControlEnable(0);
  es->m_link->setChannelActive(0);
  es->m_channel->clearEventCount();
  es->m_channel->clearStallCount();
  es->m_channel->readAndClearPtrStallFlags();
  if (pciPacketSize) {
    es->m_channel->setPciePacketSize(pciPacketSize);
  }
  es->m_link->setFlowControlEnable(1);
  es->m_link->setChannelActive(1);

  // register signal handler for event loop
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = abort_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  uint32_t outFifoDepth = es->m_channel->outFifoDepth();
  uint32_t sgentries_avail = outFifoDepth - es->m_channel->outFifoFillState();
  uint32_t maxPendingEvents = 0;
  uint32_t pendingEvents = 0;
  uint32_t rcvEventSizeBytes = 0;
  uint64_t rcvOffset = 0;
  uint64_t sendOffset = 0;
  uint64_t ebSize = es->m_eventBuffer->size();
  struct timeval tlast, tcur;
  gettimeofday(&tcur, NULL);
  tlast = tcur;
  struct t_sts cs_last;
  memset(&cs_last, 0, sizeof(struct t_sts));

  while(!done) {
    gettimeofday(&tcur, NULL);
    if (!maxPendingEvents || pendingEvents < maxPendingEvents) {
      // push new event
      vector<librorc::ScatterGatherEntry> list;
      if (es->m_eventBuffer->composeSglistFromBufferSegment(
               sendOffset, sts->eventSize, &list)) {
        if (sgentries_avail > list.size()) {
          es->m_channel->announceEvent(list);
          pendingEvents++;
          sgentries_avail -= list.size();
          sendOffset += sts->eventSize;
          if (sendOffset >= ebSize) {
            sendOffset -= ebSize;
          }
        } else {
          sgentries_avail = outFifoDepth - es->m_channel->outFifoFillState();
        }
      } else {
        cerr << "ERROR: failed to compose SG list: sendOffset=" << sendOffset
             << ", eventSize=" << sts->eventSize << endl;
      }
    }
    
    librorc::EventDescriptor *report = NULL;
    const uint32_t *event = NULL;
    uint64_t librorc_reference = 0;
    if (es->getNextEvent(&report, &event, &librorc_reference)) {
      pendingEvents--;
      rcvEventSizeBytes = (report->calc_event_size & 0x3fffffff) << 2;
      rcvOffset = report->offset;
      sts->rcvEventSize = rcvEventSizeBytes;
      sts->n_events++;
      sts->bytes_received += rcvEventSizeBytes;
      es->releaseEvent(librorc_reference);
    }

    int64_t tdiff_us = timediff_us(tlast, tcur);
    if (tdiff_us > 1000000) {
      printStatusLine(sts, &cs_last, tdiff_us);
      // cout << "PendingEvents: " << pendingEvents << endl;
      memcpy(&cs_last, sts, sizeof(struct t_sts));
      tlast = tcur;
    }
  }

  es->m_link->setFlowControlEnable(0);
  es->m_link->setChannelActive(0);

  if (sts) {
    shmdt(sts);
  }
 
  delete es;
  return 0;
}

void printStatusLine(struct t_sts *cs_cur, struct t_sts *cs_last,
                     int64_t tdiff_us) {
  uint64_t events_diff = cs_cur->n_events - cs_last->n_events;
  float event_rate_khz = (events_diff * 1000000.0) / tdiff_us / 1000.0;
  uint64_t bytes_diff = cs_cur->bytes_received - cs_last->bytes_received;
  float mbytes_rate = (bytes_diff * 1000000.0) / tdiff_us / (float)(1 << 20);
  float total_receiced_GB = (cs_cur->bytes_received / (float)(1 << 30));
  cout.precision(2);
  cout.setf(ios::fixed, ios::floatfield);
  cout << "Ch" << cs_cur->channelId << " -  #Events: " << cs_cur->n_events
       << ", Size: " << total_receiced_GB << " GB, ";
  if (events_diff) {
    cout << "Data Rate: " << mbytes_rate
         << " MB/s, Event Rate: " << event_rate_khz << " kHz";
  } else {
    cout << "Data Rate: - , Event Rate: - ";
  }
  cout << endl;
}

