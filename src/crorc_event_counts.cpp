/**
 *  crorc_event_counts.cpp
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

#include <cstdio>
#include <iomanip>
#include <unistd.h>
#include <librorc.h>

using namespace std;

#define HELP_TEXT                                                              \
  "Dump the C-RORC status to stdout. \n\
usage: crorc_status_dump -n [DeviceID] \n\
"

#define HEXSTR(x, width)                                                       \
  "0x" << setw(width) << setfill('0') << hex << x << setfill(' ') << dec

int main(int argc, char *argv[]) {
  int32_t device_number = 0;
  int arg;

  /** parse command line arguments **/
  while ((arg = getopt(argc, argv, "n:h")) != -1) {
    switch (arg) {
    case 'n': {
      device_number = strtol(optarg, NULL, 0);
    } break;

    case 'h': {
      cout << HELP_TEXT;
      return 0;
    } break;

    default: {
      cout << "Unknown parameter" << endl;
      return -1;
    } break;
    }
  }

  /** Instantiate device **/
  librorc::device *dev = NULL;
  try {
    dev = new librorc::device(device_number);
  }
  catch (int e) {
    cout << "Failed to intialize device " << device_number << ": "
         << librorc::errMsg(e) << endl;
    return -1;
  }

  /** Instantiate a new bar */
  librorc::bar *bar = NULL;
  try {
    bar = new librorc::bar(dev, 1);
  }
  catch (int e) {
    cout << "ERROR: failed to initialize BAR:" << librorc::errMsg(e) << endl;
    delete dev;
    return -1;
  }

  /** Instantiate a new sysmon */
  librorc::sysmon *sm;
  try {
    sm = new librorc::sysmon(bar);
  }
  catch (...) {
    cout << "Sysmon init failed!" << endl;
    delete bar;
    delete dev;
    return -1;
  }

  uint32_t nChannels = sm->numberOfChannels();
  for (uint32_t chId = 0; chId < nChannels; chId++) {
    librorc::link *link = new librorc::link(bar, chId);

    switch (link->linkType()) {
    case RORC_CFG_LINK_TYPE_VIRTUAL:
    case RORC_CFG_LINK_TYPE_LINKTEST:
      break;

    case RORC_CFG_LINK_TYPE_DIU: {
      librorc::diu *diu = new librorc::diu(link);
      uint32_t enable = diu->getEnable();
      uint32_t eventCount = diu->getEventcount();
      delete diu;
      cout << "Ch" << chId << ": type=src, enable=" << enable
           << ", eventcount=" << eventCount << endl;
    } break;

    case RORC_CFG_LINK_TYPE_SIU: {
      librorc::siu *siu = new librorc::siu(link);
      uint32_t enable = siu->getEnable();
      uint32_t eventCount = siu->getEventcount();
      delete siu;
      cout << "Ch" << chId << ": type=snk, enable=" << enable
           << ", eventcount=" << eventCount << endl;
    } break;

    default:
      break;
    }
    delete link;
  }

  delete sm;
  delete bar;
  delete dev;
  return 0;
}
