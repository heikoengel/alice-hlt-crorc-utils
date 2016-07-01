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
  "    -c [Id]          Channel ID, default:0\n"                               \
  "    -p [tpcPatch]    TPC Patch ID, default:0\n"                             \
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
int configureFcf(librorc::fastclusterfinder *fcf, char *mapping);
std::vector<librorc::ScatterGatherEntry> eventToBuffer(const char *filename,
                                                       librorc::buffer *buffer);
void cleanup(crorc_hwcf_coproc_handler **stream, librorc::bar **bar,
             librorc::device **dev);
void checkHwcfFlags(librorc::EventDescriptor *report,
                    const char *outputFileName);
void printStatusLine(uint32_t deviceId, struct streamStatus_t sts);

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
  int channelId = 0;
  char *mappingfile = NULL;
  uint32_t tpcPatch = 0;
  uint32_t rcuVersion = 1;
  int arg;
  while ((arg = getopt(argc, argv, "hn:c:m:p:r:")) != -1) {
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
    case 'p':
      tpcPatch = strtoul(optarg, NULL, 0);
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

  crorc_hwcf_coproc_handler *stream = NULL;
  librorc::device *dev = NULL;
  librorc::bar *bar = NULL;

  try {
    dev = new librorc::device(deviceId);
    bar = new librorc::bar(dev, 1);
    stream = new crorc_hwcf_coproc_handler(dev, bar, channelId, EB_SIZE);
  } catch (int e) {
    cerr << "ERROR: failed to initialize C-RORC: " << librorc::errMsg(e)
         << endl;
    cleanup(&stream, &bar, &dev);
    return -1;
  }

  if (stream->initializeClusterFinder(mappingfile, tpcPatch, rcuVersion)) {
    cerr << "ERROR: Failed to intialize Clusterfinder with mappingfile "
         << mappingfile << endl;
    cleanup(&stream, &bar, &dev);
    return -1;
  }

  if (stream->initializeZmq(ZMQ_BASE_PORT + channelId)) {
    cerr << "ERROR: Failed to initialize ZMQ." << endl;
    cleanup(&stream, &bar, &dev);
    return -1;
  }

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = abort_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  struct timeval now, last;
  gettimeofday(&now, NULL);
  last = now;

  while (!done) {
    stream->pollZmq();

    if (stream->inputFilesPending()) {
      int result = stream->enqueueNextEventToDevice();
      if (result && result != EAGAIN) {
        cerr << "ERROR: Failed to enqueue event " << stream->nextInputFile()
             << ", failed with: " << strerror(result) << "(" << result << ")"
             << endl;
        cleanup(&stream, &bar, &dev);
        return -1;
      }
    }

    stream->pollForEventToDeviceCompletion();

    if (stream->outputFilesPending() || stream->refFilesPending()) {
      librorc::EventDescriptor *report = NULL;
      uint64_t librorcEventReference = 0;
      const uint32_t *event = NULL;
      if (stream->pollForEventToHost(&report, &event, &librorcEventReference)) {
        if (stream->outputFilesPending()) {
          checkHwcfFlags(report, stream->nextOutputFile());
          if (stream->writeEventToNextOutputFile(report, event)) {
            cerr << "ERROR: Failed to write event to file "
                 << stream->nextOutputFile() << ": " << strerror(errno) << endl;
          }
        }

        if (stream->refFilesPending()) {
          if (stream->compareEventWithNextRefFile(report, event)) {
            cerr << "comparing output with " << stream->nextRefFile()
                 << " failed:" << strerror(errno) << endl;
          }
        }
        stream->releaseEventToHost(librorcEventReference);
      }
    }

    if (stream->isDone()) {
      done = true;
    }

    gettimeofday(&now, NULL);
    if (done || timediff_us(last, now) > 1000000) {
      streamStatus_t sts = stream->getStatus();
      printStatusLine(channelId, sts);
      last = now;
    }
  }

  cleanup(&stream, &bar, &dev);
  return 0;
}

void printStatusLine(uint32_t deviceId, struct streamStatus_t sts) {
      cout << "Input Queued: " << sts.nInputsQueued
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

void cleanup(crorc_hwcf_coproc_handler **stream, librorc::bar **bar,
             librorc::device **dev) {
  if (*stream) {
    delete *stream;
    *stream = NULL;
  }
  if (*bar) {
    delete *bar;
    *bar = NULL;
  }
  if (*dev) {
    delete *dev;
    *dev = NULL;
  }
}
