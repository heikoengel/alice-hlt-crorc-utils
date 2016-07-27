/**
 * @file crorc_dma_benchmark.cpp
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2015-11-26
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

#include <iostream>
#include <thread>
#include <librorc.h>
#include <signal.h>

using namespace std;

#define DEVICE_ID 0
#define EVENTBUFFER_SIZE (1<<30)

#define PG_EVENTSIZE_DW_START 64
#define PG_EVENTSIZE_DW_END   0x800000
#define SAMPLE_TIME           1.0
#define SWEEP_SAMPLES         5
#define THREAD_USLEEP         1000

#define HELP_TEXT                                                              \
  "crorc_dma_benchmark options: "                                              \
  "   -s [size]      PCIe packet size in bytes, default: max. supported value" \
  "   -p [size]      PatternGenerator event size in DWs or 0 for PRBS size"    \
  "   -S             Sweep PatternGenerator event size"                        \
  "   -v             Verbose mode"

bool done = false;
void abort_handler(int s) {
  cerr << "Caught signal " << s << endl;
  if (done == true) {
    exit(-1);
  } else {
    done = true;
  }
}

struct rdoInfo_t {
  uint64_t ebBytes;
  uint64_t rbBytes;
  uint32_t lastEventSizeDw;
};


uint32_t getNextEventSize(uint32_t eventSizeDw, uint32_t maxPayloadBytes) {
  uint32_t numPkts = (eventSizeDw << 2) / maxPayloadBytes;
  uint32_t nextSize = ((numPkts + 1) * maxPayloadBytes) >> 2;
  cout << "Setting ES=" << nextSize << endl;
  if (nextSize > PG_EVENTSIZE_DW_END) {
    return PG_EVENTSIZE_DW_START;
  } else {
    return nextSize;
  }
}

void ChannelReadout(int chId) { //, struct rdoInfo_t &rdoInfo) {

  librorc::event_stream *es = NULL;
  librorc::patterngenerator *pg = NULL;

  try {
    es =
        new librorc::event_stream(DEVICE_ID, chId, librorc::kEventStreamToHost);
    pg = es->getPatternGenerator();
    if (pg == NULL) {
      throw(-1);
    }
    pg->disable();
    pg->configureMode(PG_PATTERN_INC, 0, 0);
    //if (pgSizeDw) {
    pg->setStaticEventSize(PG_EVENTSIZE_DW_START);
    //} else {
    //  pg->setPrbsSize(0x100, 0x3f0000);
    //}
    /*if (pcieSize) {
      es->overridePciePacketSize(pcieSize);
    }*/
    es->initializeDma(2 * chId, EVENTBUFFER_SIZE);
    es->m_link->setChannelActive(0);
    es->m_link->setFlowControlEnable(1);
    es->m_channel->clearEventCount();
    es->m_channel->clearStallCount();
    es->m_channel->readAndClearPtrStallFlags();
    es->m_link->setDataSourcePatternGenerator();
    es->m_link->setChannelActive(1);
    pg->enable();
    cout << "Ch" << chId << " init done." << endl;
  }
  catch (int e) {
    cout << "Channel " << chId
         << " failed event stream initialization: " << librorc::errMsg(e)
         << endl;
    if (es) {
      delete es;
      es = NULL;
    }
  }

  while (!done) {
    librorc::EventDescriptor *report = NULL;
    const uint32_t *event = NULL;
    uint64_t librorcReference;
    if (es->getNextEvent(&report, &event, &librorcReference)) {
      //es->updateChannelStatus(report);
#if 0
      uint32_t bytesize = (report->calc_event_size & 0x3fffffff) << 2;
      rdoInfo.ebBytes += (bytesize + 4); //EOE-Word
      rdoInfo.rbBytes += sizeof(librorc::EventDescriptor);
      rdoInfo.lastEventSizeDw = bytesize >> 2;
#endif
      es->releaseEvent(librorcReference);
    }/* else {
      usleep(THREAD_USLEEP);
    }*/
  }
  if (pg) {
    pg->disable();
    delete pg;
  }
  if (es) {
    es->m_link->setFlowControlEnable(0);
    es->m_link->setChannelActive(0);
    delete es;
  }
    cout << "Ch" << chId << " de-init done." << endl;
}

int main(int argc, char *argv[]) {


  int arg;
  uint32_t pgSizeDw = 0;
  int pcieSize = 0;
  int pgSweep = 0;
  uint32_t maxPayloadBytes = 0;
  int verbose = 0;

  while ((arg = getopt(argc, argv, "s:p:Svh")) != -1) {
    switch (arg) {
    case 'h':
      cout << HELP_TEXT;
      return 0;
    case 's':
      pcieSize = strtol(optarg, NULL, 0);
      if (pcieSize < 0 || pcieSize > 256) {
        cerr << "ERROR: invalid PCIe payload size: " << pcieSize << endl;
        return -1;
      }
      break;
    case 'p':
      pgSizeDw = strtoul(optarg, NULL, 0);
      if (pgSizeDw < 0) {
        cerr << "ERROR: invalid PG event size: " << pgSizeDw << endl;
        return -1;
      }
      break;
    case 'S':
      pgSweep = 1;
      pgSizeDw = PG_EVENTSIZE_DW_START;
      break;
    case 'v':
      verbose = 1;
      break;
    default:
      cerr << "ERROR: Unknown parameter " << arg << endl;
      cout << HELP_TEXT;
      return -1;
    }
  }

  int max_channels = 12;

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = abort_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  struct timeval tNow, tLast;
  gettimeofday(&tNow, NULL);
  tLast = tNow;
  uint64_t ebLastBytes = 0;
  uint64_t rbLastBytes = 0;
  int nSamples = 0;

  std::thread rdoThread[max_channels];
  //struct rdoInfo_t rdoInfo[max_channels];

  cout << "# pgSizeDw, ebRate, rbRate" << endl;
    
  for (int i = 0; i < max_channels; i++) {
    //rdoInfo[i].ebBytes = 0;
    //rdoInfo[i].rbBytes = 0;
    rdoThread[i] = std::thread(ChannelReadout, i);//, std::ref(rdoInfo[i]));
  }


#if 0
  while (!done) {
    gettimeofday(&tNow, NULL);
    double tdiff = librorc::gettimeofdayDiff(tLast, tNow);
    if (tdiff >= SAMPLE_TIME) {
      uint64_t ebBytes = 0;
      uint64_t rbBytes = 0;
      for (int i = 0; i < max_channels; i++) {
        ebBytes += rdoInfo[i].ebBytes;
        rbBytes += rdoInfo[i].rbBytes;
      }
      uint64_t ebBytesDiff = ebBytes - ebLastBytes;
      uint64_t rbBytesDiff = rbBytes - rbLastBytes;
      double ebRate = ((ebBytesDiff/tdiff)/1000.0/1000.0);
      double rbRate = ((rbBytesDiff/tdiff)/1000.0/1000.0);
        cout << pgSizeDw << ", " << ebRate << ", " << rbRate << endl;

      ebLastBytes = ebBytes;
      rbLastBytes = rbBytes;
      tLast = tNow;

      /*if (pgSweep) {
        if (nSamples < SWEEP_SAMPLES) {
          nSamples++;
        } else {
          nSamples = 0;
          pgSizeDw = getNextEventSize(pgSizeDw, maxPayloadBytes);
          for (i = 0; i < max_channels; i++) {
            pg[i]->setStaticEventSize(pgSizeDw-1); //EOE-Word
          }
          //pgSizeDwChanged = 1; // skip one sample after change
        }
      } // pgSweep*/
    } // tdiff
  } // while(!done)
#endif

  for (int i = 0; i < max_channels; i++) {
    rdoThread[i].join();
  }

  return 0;
}
