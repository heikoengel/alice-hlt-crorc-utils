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
#include <librorc.h>
#include <signal.h>

using namespace std;

#define DEVICE_ID 0
#define EVENTBUFFER_SIZE (1<<30)

#define PG_EVENTSIZE_DW_START 64
#define PG_EVENTSIZE_DW_END   0x800000
#define SAMPLE_TIME        1.0
#define SWEEP_SAMPLES      5

#define HELP_TEXT                                                              \
  "crorc_dma_benchmark options: "                                              \
  "   -s [size]      PCIe packet size in bytes, default: max. supported value" \
  "   -p [size]      PatternGenerator event size in DWs or 0 for PRBS size"    \
  "   -S             Sweep PatternGenerator event size"

bool done = false;
void abort_handler(int s) {
  cerr << "Caught signal " << s << endl;
  if (done == true) {
    exit(-1);
  } else {
    done = true;
  }
}

uint32_t getNextEventSize(uint32_t eventSizeDw, uint32_t maxPayloadBytes) {
  uint32_t numPkts = (eventSizeDw << 2) / maxPayloadBytes;
  uint32_t nextSize = ((numPkts + 1) * maxPayloadBytes) >> 2;
  if (nextSize > PG_EVENTSIZE_DW_END) {
    return PG_EVENTSIZE_DW_START;
  } else {
    return nextSize;
  }
}

int main(int argc, char *argv[]) {

  librorc::event_stream *es[12];
  librorc::patterngenerator *pg[12];

  int arg;
  uint32_t pgSizeDw = 0;
  int pcieSize = 0;
  int pgSweep = 0;
  uint32_t maxPayloadBytes = 0;

  while ((arg = getopt(argc, argv, "s:p:Sh")) != -1) {
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
    default:
      cerr << "ERROR: Unknown parameter " << arg << endl;
      cout << HELP_TEXT;
      return -1;
    }
  }

  int max_channels = 12;
  int i;

  for (i = 0; i < max_channels; i++) {
    es[i] = NULL;
    pg[i] = NULL;
  }
  try {
    for (i = 0; i < max_channels; i++) {
      cout << "Initializing Channel " << i << endl;
      es[i] = new librorc::event_stream(DEVICE_ID, i, librorc::kEventStreamToHost);
      pg[i] = es[i]->getPatternGenerator();
      if(pg[i] == NULL) {
        throw(-1);
      }
      pg[i]->disable();
      pg[i]->configureMode(PG_PATTERN_INC, 0, 0);
      if (pgSizeDw) {
        pg[i]->setStaticEventSize(pgSizeDw);
      } else {
        pg[i]->setPrbsSize(0x100, 0x3f0000);
      }
      if (pcieSize) {
        es[i]->overridePciePacketSize(pcieSize);
      }
      es[i]->initializeDma(2*i, EVENTBUFFER_SIZE);
      es[i]->m_link->setChannelActive(0);
      es[i]->m_link->setFlowControlEnable(1);
      es[i]->m_channel->clearEventCount();
      es[i]->m_channel->clearStallCount();
      es[i]->m_channel->readAndClearPtrStallFlags();
      es[i]->m_link->setDataSourcePatternGenerator();
      es[i]->m_link->setChannelActive(1);
    }
  }
  catch (int e) {
    cout << "Channel " << i
         << " failed event stream initialization: " << librorc::errMsg(e)
         << endl << "Skipping this channel and all following." << endl;
    max_channels = i + 1;
    if (es[i]) {
      delete es[i];
      es[i] = NULL;
    }
  }

  if (max_channels == 1) {
    cerr << "ERROR: No channel could be intialized - exiting!" << endl;
    return -1;
  }

  if (pcieSize) {
    maxPayloadBytes = pcieSize;
  } else {
    maxPayloadBytes = es[0]->m_dev->maxPayloadSize();
  }

  es[0]->m_sm->clearMaxPcieDeadtime();
  cout << "Starting PatternGenerators..." << endl;
  for (i = 0; i < max_channels; i++) {
    pg[i]->enable();
  }

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = abort_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  librorc::EventDescriptor *report = NULL;
  const uint32_t *event = NULL;
  uint64_t librorcReference;

  struct timeval tNow, tLast;
  gettimeofday(&tNow, NULL);
  tLast = tNow;
  uint64_t ebBytes = 0;
  uint64_t ebLastBytes = 0;
  uint64_t rbBytes = 0;
  uint64_t rbLastBytes = 0;
  int nSamples = 0;
  int pgSizeDwChanged = 0;
        
  cout << "# pgSizeDw, ebRate, rbRate, deadtime" << endl;

  while (!done) {
    gettimeofday(&tNow, NULL);
    for (i = 0; i < max_channels; i++) {
      if (es[i]->getNextEvent(&report, &event, &librorcReference)) {
        es[i]->updateChannelStatus(report);
        uint32_t bytesize = (report->calc_event_size & 0x3fffffff) << 2;
        ebBytes += bytesize + 4; //EOE-Word
        rbBytes += sizeof(librorc::EventDescriptor);
        es[i]->releaseEvent(librorcReference);
      }
    }
    double tdiff = librorc::gettimeofdayDiff(tLast, tNow);
    if (tdiff >= SAMPLE_TIME) {
      float deadtime = es[0]->m_sm->maxPcieDeadtime();
      es[0]->m_sm->clearMaxPcieDeadtime();
      uint64_t ebBytesDiff = ebBytes - ebLastBytes;
      uint64_t rbBytesDiff = rbBytes - rbLastBytes;
      double ebRate = ((ebBytesDiff/tdiff)/1000.0/1000.0);
      double rbRate = ((rbBytesDiff/tdiff)/1000.0/1000.0);
      /*cout << ebRate << " MB/s, " << ebRate / max_channels
           << " MB/s per channel, max. Deadtime: " << deadtime
           << " us" << endl;*/
      if(!pgSizeDwChanged) {
        cout << pgSizeDw << ", " << ebRate << ", " << rbRate << ", " << deadtime
             << endl;
      } else {
        pgSizeDwChanged = 0;
      }

      ebLastBytes = ebBytes;
      rbLastBytes = rbBytes;
      tLast = tNow;

      if (pgSweep) {
        if (nSamples < SWEEP_SAMPLES) {
          nSamples++;
        } else {
          nSamples = 0;
          pgSizeDw = getNextEventSize(pgSizeDw, maxPayloadBytes);
          for (i = 0; i < max_channels; i++) {
            pg[i]->setStaticEventSize(pgSizeDw-1); //EOE-Word
          }
          pgSizeDwChanged = 1; // skip one sample after change
        }
      } // pgSweep
    } // tdiff
  } // while(!done)

  for (i = 0; i < max_channels; i++) {
    if (pg[i]) {
      pg[i]->disable();
      delete pg[i];
    }
    if (es[i]) {
      es[i]->m_link->setFlowControlEnable(0);
      es[i]->m_link->setChannelActive(0);
      delete es[i];
    }
  }

  return 0;
}
