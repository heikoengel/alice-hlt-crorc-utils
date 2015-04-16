/**
 *  crorc_push_file.cpp
 *  Copyright (C) 2015 Heiko Engel <hengel@cern.ch>
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

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <librorc.h>
#include <iostream>

using namespace std;

#define HELP_TEXT                                                              \
  "crorc_push_file parameters:\n"                                              \
  "  -h          show this help\n"                                             \
  "  -n [devid]  Select device, optional, default: 0\n"                        \
  "  -n [chid]   Select channel, optional, default: 0\n"                       \
  "  -f [file]   Select file to be sent, required\n"

int main(int argc, char *argv[]) {
  int arg;
  uint32_t deviceId = 0;
  uint32_t channelId = 0;
  char *filename = NULL;
  while ((arg = getopt(argc, argv, "hn:c:f:")) != -1) {
    switch (arg) {
    case 'h':
      cout << HELP_TEXT;
      return 0;
    case 'n':
      deviceId = strtol(optarg, NULL, 0);
      if (deviceId > 255) {
        cerr << "Invalid DeviceId " << deviceId << endl;
        return -1;
      }
      break;
    case 'c':
      channelId = strtol(optarg, NULL, 0);
      if (channelId > 11) {
        cerr << "Invalid ChannelId" << channelId << endl;
        return -1;
      }
      break;
    case 'f':
      filename = optarg;
      break;
    default:
      cerr << HELP_TEXT;
      return -1;
    }
  }

  if (!filename) {
    cerr << HELP_TEXT;
    return -1;
  }

  struct stat filestat;
  int ret = stat(filename, &filestat);
  if (ret != 0) {
    perror(filename);
  }
  if (!S_ISREG(filestat.st_mode)) {
    cerr << "ERROR: " << filename << " is not a regular file." << endl;
    return -1;
  }

  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    perror(filename);
    return -1;
  }

  librorc::event_stream *es = NULL;
  try {
    es = new librorc::event_stream(deviceId, channelId,
                                   librorc::kEventStreamToDevice);
  } catch (int e) {
    cerr << "Failed to initialize event stream: " << librorc::errMsg(e) << endl;
    close(fd);
    return -1;
  }

  // find two non-existant librorc buffers with consecutive IDs
  uint32_t eBufId = 0;
  bool found = false;
  while (!found) {
    bool ebuf_exists = false;
    bool rbuf_exists = false;
    try {
      librorc::buffer *buf = new librorc::buffer(es->m_dev, eBufId, false);
      ebuf_exists = true;
      delete buf;
    } catch (...) {
      ebuf_exists = false;
    }
    try {
      librorc::buffer *buf = new librorc::buffer(es->m_dev, eBufId + 1, false);
      rbuf_exists = true;
      delete buf;
    } catch (...) {
      rbuf_exists = false;
    }
    if (!ebuf_exists && !rbuf_exists) {
      found = true;
    } else {
      eBufId += 2;
    }
  }

  cout << "INFO: Using librorc buffer IDs " << eBufId << " and " << (eBufId + 1)
       << endl;

  ret = es->initializeDma(eBufId, filestat.st_size);
  if (ret != 0) {
      cerr << "Failed to initialize DMA: " << librorc::errMsg(ret) << endl;
  }

  librorc::siu *siu = es->getSiu();
  if (!siu->linkOpen()) {
    cerr << "SIU link is not open, cannot send data. Send RDYRX from DIU to "
            "open the link" << endl;
    delete es;
    close(fd);
    return -1;
  }

  es->m_link->setDefaultDataSource();
  es->m_link->setFlowControlEnable(1);
  es->m_link->setChannelActive(1);
  siu->setEnable(1);

  uint32_t *ebuf = es->m_eventBuffer->getMem();
  ssize_t bytes = read(fd, ebuf, filestat.st_size);
  if (bytes != filestat.st_size) {
    cerr << "Failed to copy event to EventBuffer, expected " << filestat.st_size
         << ", but wrote " << bytes << endl;
    close(fd);
    delete siu;
    delete es;
    return -1;
  }

  std::vector<librorc::ScatterGatherEntry> event_sglist;
  if (!es->m_eventBuffer->composeSglistFromBufferSegment(0, filestat.st_size,
                                                         &event_sglist)) {
    cerr << "Failed to compose sglist for event" << endl;
    delete siu;
    delete es;
    return -1;
  }

  es->m_channel->announceEvent(event_sglist);
  
  librorc::EventDescriptor *report = NULL;
  const uint32_t *event = 0;
  uint64_t librorcEventReference = 0;
  while(!es->getNextEvent(&report, &event, &librorcEventReference)) {
      usleep(100);
  }
  bool cmpl_failed = (((report->calc_event_size) >> 30)!=0);
  if ( cmpl_failed ) {
      cerr << "DMA Completion failed" << endl;
  }
  es->releaseEvent(librorcEventReference);

  es->m_link->setFlowControlEnable(0);
  es->m_link->setChannelActive(0);
  siu->setEnable(0);

  delete siu;
  delete es;
  close(fd);
  return 0;
}
