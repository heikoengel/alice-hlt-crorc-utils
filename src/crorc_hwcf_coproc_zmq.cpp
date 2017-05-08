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
#include <getopt.h>
#include "crorc_hwcf_coproc_handler.hpp"

#define HELP_TEXT                                                              \
  "usage: crorc_hwcf_coproc [parameters]\n"                                    \
  "    -n [Id]          Device ID, default:0\n"                                \
  "    -c [Id]          optional channel ID, default:all\n"                    \
  "    -r [rcuVersion]  TPC RCU version, default:1\n"                          \
  "    -m [mappingfile] Path to AliRoot TPC Row Mapping File\n"                \
  "    -b               batch mode, queue multiple events at onece and "       \
  "                     don't print stats after each event.\n"

#define ES2HOST_EB_ID 0
#define ES2DEV_EB_ID 2
#define EB_SIZE 0x40000000 // 1GB
//#define EB_SIZE 0x800000 // 8MB

#define ZMQ_BASE_PORT 5555

using namespace std;

/**
 * Prototypes
 **/
void checkHwcfFlags(librorc::EventDescriptor *report, string outputFileName);
void printEventStatsHeader();
void printEventStats(crorc_hwcf_coproc_handler *stream,
                     librorc::EventDescriptor *report, const uint32_t *event);
void printStatusLine(uint32_t channelId, crorc_hwcf_coproc_handler *stream);
void printHwcfConfig(struct fcfConfig_t cfg);

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
  bool batchMode = false;
  int arg;
  fcfConfig_t fcfcfg = fcfDefaultConfig;

  static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"channel", required_argument, 0, 'c'},
      {"device", required_argument, 0, 'n'},
      {"mapping", required_argument, 0, 'm'},
      {"batch", no_argument, 0, 'b'},
      {"rcu2-data", required_argument, 0, 'r'},
      {"deconvolute-pad", required_argument, 0, 'd'},
      {"single-pad-suppression", required_argument, 0, 's'},
      {"bypass-merger", required_argument, 0, 'B'},
      {"cluster-lower-limit", required_argument, 0, 'l'},
      {"cluster-qmax-lower-limit", required_argument, 0, 'q'},
      {"single-sequence-limit", required_argument, 0, 'S'},
      {"merger-distance", required_argument, 0, 'M'},
      {"use-time-follow", required_argument, 0, 't'},
      {"noise-suppression", required_argument, 0, 'N'},
      {"noise-suppression-for-minima", required_argument, 0, 'i'},
      {"noise-suppression-neighbor", required_argument, 0, 'u'},
      {"tag-deconvoluted-clusters", required_argument, 0, 'D'},
      {"tag-border-clusters", required_argument, 0, 'e'},
      {"correct-edge-clusters", required_argument, 0, 'E'},
      {0, 0, 0, 0}};

  while ((arg = getopt_long(argc, argv, "hn:c:m:r:bd:s:B:l:q:S:M:t:N:i:u:D:e:E:",
                            long_options, NULL)) != -1) {
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
      if (strtoul(optarg, NULL, 0) > 0) {
        rcuVersion = 2;
      } else {
        rcuVersion = 1;
      }
      break;
    case 'b':
      batchMode = true;
      break;
    case 'm':
      mappingfile = optarg;
      break;
    case 'd':
      fcfcfg.deconvolute_pad = (strtoul(optarg, NULL, 0)) & 1;
      break;
    case 's':
      fcfcfg.single_pad_suppression = (strtoul(optarg, NULL, 0)) & 1;
      break;
    case 'B':
      fcfcfg.bypass_merger = (strtoul(optarg, NULL, 0)) & 1;
      break;
    case 'l':
      fcfcfg.cluster_lower_limit = (strtoul(optarg, NULL, 0)) & 0xffff;
      break;
    case 'q':
      fcfcfg.cluster_qmax_lower_limit = (strtoul(optarg, NULL, 0)) & 0x7ff;
      break;
    case 'S':
      fcfcfg.single_seq_limit = (strtoul(optarg, NULL, 0)) & 0xffff;
      break;
    case 'M':
      fcfcfg.merger_distance = (strtoul(optarg, NULL, 0)) & 0xffff;
      break;
    case 't':
      fcfcfg.use_time_follow = (strtoul(optarg, NULL, 0)) & 0xffff;
      break;
    case 'N':
      fcfcfg.noise_suppression = (strtoul(optarg, NULL, 0)) & 0xffff;
      break;
    case 'i':
      fcfcfg.noise_suppression_minimum = (strtoul(optarg, NULL, 0)) & 0xffff;
      break;
    case 'u':
      fcfcfg.noise_suppression_neighbor = (strtoul(optarg, NULL, 0)) & 0xffff;
      break;
    case 'D':
      fcfcfg.tag_deconvoluted_clusters = (strtoul(optarg, NULL, 0)) & 0x3;
      break;
    case 'e':
      fcfcfg.tag_border_clusters = (strtoul(optarg, NULL, 0)) & 1;
      break;
    case 'E':
      fcfcfg.correct_edge_clusters = (strtoul(optarg, NULL, 0)) & 1;
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

  time_t rawtime;
  time(&rawtime);
  printf("# Date: %s# Firmware Rev.: %07x, Firmware Date: %08x, RCU%d\n",
         ctime(&rawtime), sm->FwRevision(), sm->FwBuildDate(), rcuVersion);
  printHwcfConfig(fcfcfg);

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
    // cout << "INFO: initializing channel " << (chStart + i) << "..." << endl;
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

    if (stream[i]->initializeClusterFinder(mappingfile, chStart + i, rcuVersion,
                                           fcfcfg)) {
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

  // cout << "INFO: initialization done, waiting for data..." << endl;

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = abort_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  struct timeval now, last;
  gettimeofday(&now, NULL);
  last = now;

  if (!batchMode) {
    printEventStatsHeader();
  }

  while (!done) {
    for (int i = 0; i < nCh; i++) {

      // check for new commands via ZMQ
      stream[i]->pollZmq();

      // push events to device
      if (stream[i]->inputFilesPending() &&
          (batchMode || stream[i]->eventsInChain() == 0)) {
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
      librorc::EventDescriptor *report = NULL;
      uint64_t librorcEventReference = 0;
      const uint32_t *event = NULL;
      if (stream[i]
              ->pollForEventToHost(&report, &event, &librorcEventReference)) {
        if (stream[i]->outputFilesPending()) {
          string nextOutputFile = stream[i]->nextOutputFile();
          checkHwcfFlags(report, nextOutputFile);
          if (stream[i]->writeEventToNextOutputFile(report, event)) {
            cerr << "ERROR: Failed to write event to file " << nextOutputFile
                 << ": " << strerror(errno) << endl;
          } else {
          }
        }

        if (stream[i]->refFilesPending()) {
          string nextRefFile = stream[i]->nextRefFile();
          if (stream[i]->compareEventWithNextRefFile(report, event)) {
            cerr << nextRefFile << " : ";
            switch (errno) {
            case EFBIG:
              cerr << " Size mismatch";
              break;
            case EILSEQ:
              cerr << " Pattern mismatch";
              break;
            default:
              cerr << strerror(errno);
              break;
            }
            cerr << endl;
            stream[i]->markRefFileDone();
          }
        }
        if (!batchMode) {
          printEventStats(stream[i], report, event);
          stream[i]->fcfClearStats();
        }
        stream[i]->releaseEventToHost(librorcEventReference);
      }

    } // for

    bool all_done = true;
    for (int i = 0; i < nCh; i++) {
      if (!stream[i]->isDone() || stream[i]->eventsInChain() > 0) {
        all_done = false;
      }
    }
    done |= all_done;

    if (batchMode) {
      gettimeofday(&now, NULL);
      if (done || timediff_us(last, now) > 1000000) {
        if (done) {
          cout << "=========== stopping ==========" << endl;
        }
        for (int i = 0; i < nCh; i++) {
          printStatusLine(chStart + i, stream[i]);
        }
        last = now;
      }
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

void printStatusLine(uint32_t channelId, crorc_hwcf_coproc_handler *stream) {
  struct streamStatus_t sts = stream->getStatus();
  cout << "Ch: " << channelId << " InQueued: " << sts.nInputsQueued
       << ", InDone: " << sts.nInputsDone << ", OutDone: " << sts.nOutputsDone
       << ", InChain: " << stream->eventsInChain() << endl;
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

void checkHwcfFlags(librorc::EventDescriptor *report, string outputFileName) {
  uint32_t hwcfflags = (report->calc_event_size >> 30) & 0x3;
  if (hwcfflags & 0x1) {
    cerr << "WARNING: Found RCU protocol error(s) in " << outputFileName
         << endl;
  }
  if (hwcfflags & 0x2) {
    cerr << "WARNING: Found ALTRO channel error flag(s) set in "
         << outputFileName << endl;
  }
}

void printEventStatsHeader() {
  printf("# inputSize, outputSize, nClusters, "
         "procTimeCC, inputIdleTimeCC, xoffTimeCC, "
         "numCandidates, mergerIdlePercent, fifoMergerMax, fifoDividerMax, "
         "file\n");
}

void printEventStats(crorc_hwcf_coproc_handler *stream,
                     librorc::EventDescriptor *report, const uint32_t *event) {
  uint32_t dmaWords = (report->calc_event_size & 0x3fffffff);
  uint32_t outputSize = dmaWords << 2;
  uint32_t procTimeCC = stream->fcfProcTimeCC();
  uint32_t inputIdleTimeCC = stream->fcfInputIdleTimeCC();
  uint32_t xoffTimeCC = stream->fcfXoffTimeCC();
  uint32_t hdrTrlSize = (10 + 9) * sizeof(uint32_t);
  uint32_t nClusters = (outputSize - hdrTrlSize) / (6 * sizeof(uint32_t));
  uint32_t inputSize =
      (dmaWords > 9) ? (event[dmaWords - 9] * 4 + hdrTrlSize) : 0;
  float mergerIdlePercent = stream->fcfMergerIdlePercent();
  uint32_t numCandidates = stream->fcfNumCandidates();
  uint32_t fifoMergerMax = stream->fcfMergerInputFifoMax();
  uint32_t fifoDividerMax = stream->fcfDividerInputFifoMax();
  printf("%u, %u, %u, %u, %u, %u, %u, %f, %d, %d, %s\n", inputSize, outputSize,
         nClusters, procTimeCC, inputIdleTimeCC, xoffTimeCC, numCandidates,
         mergerIdlePercent, fifoMergerMax, fifoDividerMax,
         stream->lastInputFile());
}

void printHwcfConfig(struct fcfConfig_t cfg) {
  printf("# BypassMerger: %d, ChargeFluctiation: %d, ClusterLowerLimit: %d, "
         "ClusterQmaxLowerLimit: %d, DeconvPad: %d, MergerDistance: %d, "
         "NoiseSuppr: %d, NoiseSupprMin: %d, NoiseSupprNeighbor: %d, "
         "SinglePadSuppr: %d, SingleSeqLimit: %d, TagDeconvClusters: %d, "
         "TagBorderClusters: %d, UseTimeFollow: %d\n",
         cfg.bypass_merger, cfg.charge_fluctuation, cfg.cluster_lower_limit,
         cfg.cluster_qmax_lower_limit, cfg.deconvolute_pad, cfg.merger_distance,
         cfg.noise_suppression, cfg.noise_suppression_minimum,
         cfg.noise_suppression_neighbor, cfg.single_pad_suppression,
         cfg.single_seq_limit, cfg.tag_deconvoluted_clusters,
         cfg.tag_border_clusters, cfg.use_time_follow);
}
