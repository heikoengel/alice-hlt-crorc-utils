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

bool done = false;
void abort_handler(int s) {
  cerr << "Caught signal " << s << endl;
  if (done == true) {
    exit(-1);
  } else {
    done = true;
  }
}

int main(int argc, char *argv[]) {

  librorc::event_stream *es[12];
  librorc::patterngenerator *pg[12];

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
      pg[i]->setPrbsSize(0x100, 0x3f0000);
      es[i]->overridePciePacketSize(128);
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
  uint64_t totalBytes = 0;
  uint64_t lastBytes = 0;

  while (!done) {
    gettimeofday(&tNow, NULL);
    for (i = 0; i < max_channels; i++) {
      if (es[i]->getNextEvent(&report, &event, &librorcReference)) {
        es[i]->updateChannelStatus(report);
        totalBytes += (report->calc_event_size << 2);
        es[i]->releaseEvent(librorcReference);
      }
    }
    double tdiff = librorc::gettimeofdayDiff(tLast, tNow);
    if (tdiff > 1.0) {
      uint64_t bytesDiff = totalBytes - lastBytes;
      double totalRate = ((bytesDiff/tdiff)/1024.0/1024.0);
      cout << totalRate << " MB/s, " << totalRate / max_channels
           << " MB/s per channel" << endl;
      lastBytes = totalBytes;
      tLast = tNow;
    }
  }

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
