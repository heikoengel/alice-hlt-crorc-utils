/**
 *  crorc_qsfp_ctrl.cpp
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

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <unistd.h>
#include <librorc.h>

using namespace std;

#define HELP_TEXT \
  "crorc_qsfp_ctrl parameters: \n\
        -h              Print this help \n\
        -n [deviceId]   (optional) Target device, default: 0 \n\
        -q [0..2]       (optional) select single QSFP ID. \n\
                        default: iterate over all installed modules\n\
        -r [0/1]        Set QSFP Reset \n\
"

float mWatt2dBm(float mwatt) { return 10 * log10(mwatt); }

string stripWhitespaces(string input) {
  string output;
  for (int i = 0; i < input.length(); i++) {
    int c = input[i];
    if (c != 32) {
      output += (input[i]);
    }
  }
  return output;
}

int main(int argc, char *argv[]) {
  uint32_t deviceId = 0;
  uint32_t qsfpId = 0;
  bool qsfpIdSet = false;
  uint32_t resetVal = 0;
  bool doReset = 0;
  int arg;

  /** parse command line arguments */
  while ((arg = getopt(argc, argv, "hn:q:r:")) != -1) {
    switch (arg) {
    case 'h':
      cout << HELP_TEXT;
      return 0;
      break;
    case 'n':
      deviceId = strtoul(optarg, NULL, 0);
      break;
    case 'q':
      qsfpId = strtoul(optarg, NULL, 0);
      qsfpIdSet = true;
      if (qsfpId > 2) {
        cerr << "Invalid QSFP ID " << qsfpId << endl;
        return -1;
      }
      break;
    case 'r':
      resetVal = (strtoul(optarg, NULL, 0) & 1);
      doReset = true;
      break;
    default:
      cout << "Unknown parameter (" << arg << ")!" << endl;
      cout << HELP_TEXT;
      return -1;
      break;
    }
  }

  librorc::device *dev = NULL;
  try {
    dev = new librorc::device(deviceId);
  } catch (int e) {
    cerr << "Failed to intialize device " << deviceId << ": " << librorc::errMsg(e)
         << endl;
    return -1;
  }

  librorc::bar *bar = NULL;
  try {
    bar = new librorc::bar(dev, 1);
  } catch (...) {
    cerr << "ERROR: failed to initialize BAR." << endl;
    delete dev;
    return -1;
  }

  librorc::sysmon *sm;
  try {
    sm = new librorc::sysmon(bar);
  } catch (...) {
    cerr << "Sysmon init failed!" << endl;
    delete bar;
    delete dev;
    return -1;
  }

  uint32_t qsfp_start, qsfp_end;
  if (qsfpIdSet) {
    qsfp_start = qsfp_end = qsfpId;
  } else {
    qsfp_start = 0;
    qsfp_end = 2;
  }

  for (uint32_t id = qsfp_start; id <= qsfp_end; id++) {

    if (doReset) {
      cout << "Setting QSFP" << id << " Reset to " << resetVal << endl;
      sm->qsfpSetReset(id, resetVal);
    }

    cout << "======= QSFP " << id << " =======" << endl;
    cout << "\tModule Reset  : " << sm->qsfpGetReset(id) << endl;

    if (!doReset && sm->qsfpIsPresent(id) && !sm->qsfpGetReset(id)) {
      try {
        cout << fixed << setprecision(2);
        cout << "\tModule Present: "
             << stripWhitespaces(sm->qsfpVendorName(id)) << " "
             << stripWhitespaces(sm->qsfpPartNumber(id)) << " Rev. "
             << stripWhitespaces(sm->qsfpRevisionNumber(id)) << endl;
        cout << "\tSerial        : " << sm->qsfpSerialNumber(id) << endl;
        cout << "\tTemperature   : " << (int)sm->qsfpTemperature(id) << " degC"
             << endl;
        cout << "\tVoltage       : " << sm->qsfpVoltage(id) << " V" << endl;
        for (int qsfp_channel = 0; qsfp_channel < 4; qsfp_channel++) {
          float mwatt = sm->qsfpRxPower(id, qsfp_channel);
          cout << "\tRX Power CH" << qsfp_channel << "  : " << mwatt << " mW ("
               << mWatt2dBm(mwatt) << " dBm)" << endl;
        }
        for (int qsfp_channel = 0; qsfp_channel < 4; qsfp_channel++) {
          float txbias = sm->qsfpTxBias(id, qsfp_channel);
          cout << "\tTX Bias CH" << qsfp_channel << "   : " << txbias << " mA"
               << endl;
        }
      } catch (...) {
        cout << "QSFP readout failed!" << endl;
      }
    } else {
      cout << "\tModule Present: " << sm->qsfpIsPresent(id) << endl;
    }

  }

  delete sm;
  delete bar;
  delete dev;
  return 0;
}
