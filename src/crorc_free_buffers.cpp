/**
 *  crorc_free_buffers.cpp
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
#include <iostream>
#include <getopt.h>
#include <librorc.h>

int main(int argc, char *argv[]) {
    int arg;
    int deviceId = 0;
    bool noConfirm = false;
    bool deleteAll = false;
    int64_t bufferId = -1;
    bool haveConfirmation = false;

    while ((arg = getopt(argc, argv, "hn:yab:")) != -1) {
      switch (arg) {
      case 'h':
        std::cout << "crorc_free_buffers parameters:" << std::endl
                  << " -h     : show this help" << std::endl
                  << " -n [ID]: select C-RORC ID (optional, default:0)"
                  << std::endl << " -b [ID]: deallocate specific librorc buffer"
                  << std::endl << " -a     : deallocate all librorc buffers"
                  << std::endl << " -y     : don't ask for confirmation"
                  << std::endl;
        return 0;
      case 'n':
        deviceId = strtol(optarg, NULL, 0);
        break;
      case 'b':
        bufferId = strtol(optarg, NULL, 0);
        break;
      case 'y':
        noConfirm = true;
        break;
      case 'a':
        deleteAll = true;
        break;
      default:
        std::cout << "Unknown parameter (" << arg << ")!" << std::endl;
        return -1;
      }
    }

    if (!deleteAll && (bufferId < 0)) {
      std::cerr << "Please specify either a buffer ID via '-b [ID]' or select "
                   "all buffers via '-a'" << std::endl;
      return -1;
    }

    librorc::device *dev = NULL;
    librorc::buffer *buf = NULL;

    try {
      dev = new librorc::device(deviceId);
    } catch (...) {
      std::cerr << "Failed to open device " << deviceId << std::endl;
      return -1;
    }

    if (!deleteAll) {
      try {
        buf = new librorc::buffer(dev, bufferId, 0);
      } catch (...) {
        std::cerr << "Failed to connect to buffer " << bufferId << std::endl;
        delete dev;
        return -1;
      }
    }

    if (!noConfirm) {
      char answer;
      std::cout << "Enter 'y' to confirm deallocation of ";
      if (deleteAll) {
        std::cout << "all DMA Buffers";
      } else {
        std::cout << "DMA Buffer " << bufferId;
      }
      std::cout << " of C-RORC " << deviceId << ": ";
      std::cin >> answer;
      if (answer == 'y' || answer == 'Y') {
        haveConfirmation = true;
      } else {
        haveConfirmation = false;
      }
    } else {
      haveConfirmation = true;
    }

    if (haveConfirmation) {
      if (deleteAll) {
        dev->deleteAllBuffers();
      } else {
        buf->deallocate();
      }
    } else {
      std::cerr << "Confirmation failed, aborting" << std::endl;
      return -1;
    }

    if (buf) {
      delete buf;
    }
    delete dev;
    return 0;
}
