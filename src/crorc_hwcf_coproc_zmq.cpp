/**
 *  crorc_hwcf_coproc_zmq.cpp
 *  Copyright (C) 2016 Heiko Engel <hengel@cern.ch>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/signal.h>
#include <errno.h>
#include "crorc_hwcf_coproc_handler.hpp"

#define HELP_TEXT                                                              \
  "usage: crorc_hwcf_coproc [parameters]\n"                                    \
  "    -n [Id]          Device ID, default:0\n"                                \
  "    -c [Id]          optional channel ID, default:all\n"                    \
  "    -r [rcuVersion]  TPC RCU version, default:1\n"                          \
  "    -m [mappingfile] Path to AliRoot TPC Row Mapping File\n"

#define ES2HOST_EB_ID 0
#define ES2DEV_EB_ID 2
#define EB_SIZE 0x40000000 // 1GB
//#define EB_SIZE 0x800000 // 8MB

#define ZMQ_BASE_PORT 5555

using namespace std;

/**
 * Prototypes
 **/
void checkHwcfFlags(librorc::EventDescriptor *report,
                    const char *outputFileName);
void printStatusLine(uint32_t channelId, struct streamStatus_t sts);

inline long long timediff_us(struct timeval from, struct timeval to) {
  return ((long long)(to.tv_sec - from.tv_sec) * 1000000LL +
          (long long)(to.tv_usec - from.tv_usec));
}

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

/**
 * main
 **/
int main(int argc, char *argv[]) {
  int deviceId = 0;
  int channelId = -1;
  char *mappingfile = NULL;
  uint32_t rcuVersion = 1;
  int arg;
  while ((arg = getopt(argc, argv, "hn:c:m:r:")) != -1) {
    switch (arg) {
    case 'h':
      cout << HELP_TEXT;
      return 0;
    case 'n':
      deviceId = strtoul(optarg, NULL, 0);
      break;
    case 'c':
      channelId = strtoul(optarg, NULL, 0);
      break;
    case 'r':
      rcuVersion = strtoul(optarg, NULL, 0);
      break;
    case 'm':
      mappingfile = optarg;
      break;
    }
  }

  if (!mappingfile) {
    cerr << "ERROR: no FCF mapping file provided!" << endl;
    cout << HELP_TEXT;
    return -1;
  }

  librorc::device *dev = NULL;
  librorc::bar *bar = NULL;
  librorc::sysmon *sm = NULL;

  try {
    dev = new librorc::device(deviceId);
    bar = new librorc::bar(dev, 1);
    sm = new librorc::sysmon(bar);
  }
  catch (int e) {
    cerr << "ERROR: failed to initialize C-RORC: " << librorc::errMsg(e)
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

  int chStart, chEnd;
  int nCh;
  if (channelId < 0) {
    chStart = 0;
    chEnd = (sm->numberOfChannels() / 2) - 1;
    nCh = chEnd + 1;
  } else {
    chStart = channelId;
    chEnd = channelId;
    nCh = 1;
  }
  delete sm;

  crorc_hwcf_coproc_handler *stream[nCh];
  for (int i = 0; i < nCh; i++) {
    stream[i] = NULL;
  }
  for (int i = 0; i < nCh; i++) {
    cout << "INFO: initializing channel " << (chStart + i) << "..." << endl;
    try {
      stream[i] = new crorc_hwcf_coproc_handler(dev, bar, chStart + i, EB_SIZE);
    }
    catch (int e) {
      cerr << "ERROR: failed to initialize channel " << (chStart + i) << ": "
           << librorc::errMsg(e) << endl;
      done = true;
    }

    if (done) {
      break;
    }

    if (stream[i]
            ->initializeClusterFinder(mappingfile, chStart + i, rcuVersion)) {
      cerr << "ERROR: Failed to intialize Clusterfinder on channel "
           << (chStart + i) << " with mappingfile " << mappingfile << endl;
      done = true;
    }

    if (done) {
      break;
    }

    if (stream[i]->initializeZmq(ZMQ_BASE_PORT + chStart + i)) {
      cerr << "ERROR: Failed to initialize ZMQ for channel " << (chStart + i)
           << "." << endl;
      done = true;
    }
  }

  cout << "INFO: initialization done, waiting for data..." << endl;

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = abort_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  struct timeval now, last;
  gettimeofday(&now, NULL);
  last = now;

  while (!done) {
    for (int i = 0; i < nCh; i++) {

      // check for new commands via ZMQ
      stream[i]->pollZmq();

      // push events to device
      if (stream[i]->inputFilesPending()) {
        int result = stream[i]->enqueueNextEventToDevice();
        if (result && result != EAGAIN) {
          cerr << "ERROR: Failed to enqueue event "
               << stream[i]->nextInputFile()
               << ", failed with: " << strerror(result) << "(" << result << ")"
               << endl;
          done = true;
          break;
        }
      }
      if (done) {
        break;
      }

      stream[i]->pollForEventToDeviceCompletion();

      // check for events from device
      if (stream[i]->outputFilesPending() || stream[i]->refFilesPending()) {
        librorc::EventDescriptor *report = NULL;
        uint64_t librorcEventReference = 0;
        const uint32_t *event = NULL;
        if (stream[i]
                ->pollForEventToHost(&report, &event, &librorcEventReference)) {
          if (stream[i]->outputFilesPending()) {
            checkHwcfFlags(report, stream[i]->nextOutputFile());
            if (stream[i]->writeEventToNextOutputFile(report, event)) {
              cerr << "ERROR: Failed to write event to file "
                   << stream[i]->nextOutputFile() << ": " << strerror(errno)
                   << endl;
            }
          }

          if (stream[i]->refFilesPending()) {
            if (stream[i]->compareEventWithNextRefFile(report, event)) {
              cerr << "comparing output with " << stream[i]->nextRefFile()
                   << " failed:" << strerror(errno) << endl;
            }
          }
          stream[i]->releaseEventToHost(librorcEventReference);
        }
      } // *FilesPending

    } // for

    bool all_done = true;
    for (int i = 0; i < nCh; i++) {
      if (!stream[i]->isDone()) {
        all_done = false;
      }
    }
    done |= all_done;

    gettimeofday(&now, NULL);
    if (done || timediff_us(last, now) > 1000000) {
      if (done) {
        cout << "=========== stopping ==========" << endl;
      }
      for (int i = 0; i < nCh; i++) {
        streamStatus_t sts = stream[i]->getStatus();
        printStatusLine(chStart + i, sts);
      }
      last = now;
    }
  } // while(!done)

  for (int i = 0; i < nCh; i++) {
    if (stream[i]) {
      delete stream[i];
    }
  }
  if (bar) {
    delete bar;
  }
  if (dev) {
    delete dev;
  }
  return 0;
}

void printStatusLine(uint32_t channelId, struct streamStatus_t sts) {
  cout << "Ch: " << channelId << " Input Queued: " << sts.nInputsQueued
       << ", Input done: " << sts.nInputsDone
       << ", Output done: " << sts.nOutputsDone << endl;
}

#if 0
int checkEvent(librorc::EventDescriptor *report, const uint32_t *event) {
  uint32_t dmaWords = (report->calc_event_size & 0x3fffffff);
  uint32_t diuWords = (report->reported_event_size & 0x3fffffff);
  uint32_t diuErrorFlag = (report->reported_event_size >> 30) & 1;
  uint32_t rcuErrorFlag = (report->calc_event_size >> 31) & 1;
  return 0;
};
#endif

void checkHwcfFlags(librorc::EventDescriptor *report,
                    const char *outputFileName) {
  uint32_t hwcfflags = (report->calc_event_size >> 30) & 0x3;
  if (hwcfflags & 0x1) {
    printf("WARNING: Found RCU protocol error(s) in %s\n", outputFileName);
  }
  if (hwcfflags & 0x2) {
    printf("WARNING: Found ALTRO channel error flag(s) set in %s\n",
           outputFileName);
  }
}
