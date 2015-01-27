/**
 * Copyright (c) 2014, Heiko Engel <hengel@cern.ch>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of University Frankfurt, FIAS, CERN nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL A COPYRIGHT HOLDER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **/

#include <cstdio>
#include <iomanip>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <librorc.h>

using namespace std;

#define HELP_TEXT                                                              \
  "crorc_ddr3ctrl parameters:\n"                                               \
  "General:\n"                                                                 \
  "-h|--help               show this help\n"                                   \
  "-n|--device [id]        select target device (required)\n"                  \
  "Module Status related:\n"                                                   \
  "-m|--module [0,1]       select target DDR3 module. If no module is \n"      \
  "                        selected all module options are applied to both \n" \
  "                        modules\n"                                          \
  "-r|--modulereset [0,1]  set/unset module reset\n"                           \
  "-t|--spd-timing         print SPD timing register values\n"                 \
  "-s|--controllerstatus   print controller initialization status\n"           \
  "Data Replay related:\n"                                                     \
  "-c|--channel [id]       select target replay channel. If no channel is \n"  \
  "                        selected all replay options are applied to all \n"  \
  "                        available channels\n"                               \
  "-R|--channelreset [0,1] set/unset replay channel reset\n"                   \
  "-f|--file [path]        file to be loaded into DDR3\n"                      \
  "-O|--oneshot [0,1]      set/unset oneshot replay mode\n"                    \
  "-C|--continuous [0,1]   set/unset oneshot replay mode\n"                    \
  "-e|--enable [0,1]       set/unset replay channel enable\n"                  \
  "-D|--disablereplay      disable replay gracefully\n"                        \
  "-P|--replaystatus       show replay channel status\n"                       \
  "\n"

/********** Prototypes **********/
uint64_t getDdr3ModuleCapacity(librorc::sysmon *sm, uint8_t module_number);
uint32_t getNumberOfReplayChannels(librorc::bar *bar, librorc::sysmon *sm);
uint32_t fileToRam(librorc::sysmon *sm, uint32_t channelId,
                   const char *filename, uint32_t addr, bool is_last_event);
int waitForReplayDone(librorc::datareplaychannel *dr);
void printChannelStatus(uint32_t ChannelId, librorc::datareplaychannel *dr,
                        uint32_t start_addr);

int main(int argc, char *argv[]) {
  int sSpdTimings = 0;
  int sStatus = 0;
  int sReset = 0;
  uint32_t sResetVal = 0;

  uint32_t deviceId = 0xffffffff;
  uint32_t moduleId = 0xffffffff;
  uint32_t channelId = 0xffffffff;

  int sFileToDdr3 = 0;
  vector<string> list_of_filenames;
  int sSetOneshot = 0;
  uint32_t sOneshotVal = 0;
  int sSetContinuous = 0;
  uint32_t sContinuousVal = 0;
  int sSetChannelEnable = 0;
  uint32_t sChannelEnableVal = 0;
  int sSetDisableReplay = 0;
  int sSetReplayStatus = 0;
  int sSetChannelReset = 0;
  uint32_t sChannelResetVal = 0;

  static struct option long_options[] = {
      // General
      {"help", no_argument, 0, 'h'},
      {"device", required_argument, 0, 'n'},
      // Module Status related
      {"module", required_argument, 0, 'm'},
      {"modulereset", required_argument, 0, 'r'},
      {"spd-timing", no_argument, 0, 't'},
      {"controllerstatus", no_argument, 0, 's'},
      // Data Replay related
      {"channel", required_argument, 0, 'c'},
      {"file", required_argument, 0, 'f'},
      {"oneshow", required_argument, 0, 'O'},
      {"continuous", required_argument, 0, 'C'},
      {"enable", required_argument, 0, 'e'},
      {"disablereplay", no_argument, 0, 'D'},
      {"replaystatus", no_argument, 0, 'P'},
      {"channelreset", no_argument, 0, 'R'},
      {0, 0, 0, 0}};

  if (argc > 1) {
    while (1) {
      int opt =
          getopt_long(argc, argv, "hn:m:r:tsc:f:O:C:e:DPR:", long_options, NULL);
      if (opt == -1) {
        break;
      }

      switch (opt) {
      case 'h':
        printf(HELP_TEXT);
        return -1;
      case 'n':
        deviceId = strtol(optarg, NULL, 0);
        break;
      case 'c':
        channelId = strtol(optarg, NULL, 0);
        break;
      case 'm':
        moduleId = strtol(optarg, NULL, 0);
        break;
      case 'r':
        sReset = 1;
        sResetVal = strtol(optarg, NULL, 0);
        break;
      case 't':
        sSpdTimings = 1;
        break;
      case 's':
        sStatus = 1;
        break;
      case 'f':
        list_of_filenames.push_back(optarg);
        sFileToDdr3 = 1;
        break;
      case 'O':
        sSetOneshot = 1;
        sOneshotVal = strtol(optarg, NULL, 0);
        break;
      case 'C':
        sSetContinuous = 1;
        sContinuousVal = strtol(optarg, NULL, 0);
        break;
      case 'e':
        sSetChannelEnable = 1;
        sChannelEnableVal = strtol(optarg, NULL, 0);
        break;
      case 'D':
        sSetDisableReplay = 1;
        break;
      case 'P':
        sSetReplayStatus = 1;
        break;
      case 'R':
        sSetChannelReset = 1;
        sChannelResetVal = strtol(optarg, NULL, 0);
        break;
      case '?':
        return -1;
      default:
        continue;
      }
    }
  } else {
    printf(HELP_TEXT);
    return -1;
  }

  if (deviceId == 0xffffffff) {
    cerr << "No device selected - using device 0" << endl;
    deviceId = 0;
  }

  uint32_t moduleStartId, moduleEndId;
  if (moduleId == 0xffffffff) {
    moduleStartId = 0;
    moduleEndId = 1;
  } else if (moduleId > 1) {
    cerr << "Invalid ModuleId selected: " << moduleId << endl;
    abort();
  } else {
    moduleStartId = moduleId;
    moduleEndId = moduleId;
  }

  /** Instantiate device **/
  librorc::device *dev = NULL;
  try {
    dev = new librorc::device(deviceId);
  } catch (...) {
    cerr << "ERROR: Failed to intialize device " << deviceId << endl;
    return -1;
  }

  /** Instantiate a new bar */
  librorc::bar *bar = NULL;
  try {
    bar = new librorc::bar(dev, 1);
  } catch (...) {
    cerr << "ERROR: failed to initialize BAR." << endl;
    delete dev;
    abort();
  }

  /** Instantiate a new sysmon */
  librorc::sysmon *sm;
  try {
    sm = new librorc::sysmon(bar);
  } catch (...) {
    cerr << "ERROR: failed to initialize System Monitor." << endl;
    delete bar;
    delete dev;
    abort();
  }

  // any SO-DIMM module related options set?
  if (sReset || sStatus || sSpdTimings) {
    uint32_t ddrctrl = bar->get32(RORC_REG_DDR3_CTRL);

    for (moduleId = moduleStartId; moduleId <= moduleEndId; moduleId++) {
      uint32_t bitrate = sm->ddr3Bitrate(moduleId);
      if (bitrate == 0) {
        cout << "No DDR3 Controller " << moduleId
             << " available in Firmware. Skipping..." << endl;
        continue;
      }

      if (sReset) {
        sm->ddr3SetReset(moduleId, sResetVal);
      }

      if (sStatus) {
        uint32_t off = (16 * moduleId);
        bool OK = (((ddrctrl >> off) & 0x1ff) == 0x9e);
        bool RESET = (((ddrctrl >> off) & 1) == 1);
        const char* status = (RESET) ? "RESET" : ((OK) ? "OK" : "ERROR");
        cout << endl << "Controller " << moduleId << " Status: "
             << status << endl;
        cout << "\tReset: " << ((ddrctrl >> off) & 1) << endl;
        cout << "\tPhyInitDone: " << ((ddrctrl >> (off + 1)) & 1) << endl;
        cout << "\tPLL Lock: " << ((ddrctrl >> (off + 2)) & 1) << endl;
        cout << "\tRead Levelling Started: " << ((ddrctrl >> (off + 3)) & 1)
             << endl;
        cout << "\tRead Levelling Done: " << ((ddrctrl >> (off + 4)) & 1)
             << endl;
        cout << "\tRead Levelling Error: " << ((ddrctrl >> (off + 5)) & 1)
             << endl;
        cout << "\tWrite Levelling Started: " << ((ddrctrl >> (off + 6)) & 1)
             << endl;
        cout << "\tWrite Levelling Done: " << ((ddrctrl >> (off + 7)) & 1)
             << endl;
        cout << "\tWrite Levelling Error: " << ((ddrctrl >> (off + 8)) & 1)
             << endl;
        cout << "\tMax. Controller Capacity: "
             << (sm->ddr3ControllerMaxModuleSize(moduleId)>>20) << " MB" << endl;

        if (sm->firmwareIsHltHardwareTest()) {
          uint32_t rdcnt, wrcnt;
          if (moduleId == 0) {
            rdcnt = bar->get32(RORC_REG_DDR3_C0_TESTER_RDCNT);
            wrcnt = bar->get32(RORC_REG_DDR3_C0_TESTER_WRCNT);
          } else {
            rdcnt = bar->get32(RORC_REG_DDR3_C1_TESTER_RDCNT);
            wrcnt = bar->get32(RORC_REG_DDR3_C1_TESTER_WRCNT);
          }

          cout << "\tRead Count: " << rdcnt << endl;
          cout << "\tWrite Count: " << wrcnt << endl;
          cout << "\tTG Error: " << ((ddrctrl >> (off + 15)) & 1) << endl;
        }
        cout << endl;
      }

      if (sSpdTimings) {
        cout << endl << "Module " << moduleId << " SPD Readings:" << endl;
        cout << "Part Number      : "
             << sm->ddr3SpdReadString(moduleId, 128, 145) << endl;

        uint32_t spdrev = sm->ddr3SpdRead(moduleId, 0x01);
        cout << "SPD Revision     : " << hex << setw(1) << setfill('0')
             << (spdrev >> 4) << "." << (spdrev & 0x0f) << endl;
        cout << "DRAM Device Type : 0x" << hex << setw(2) << setfill('0')
             << (int)sm->ddr3SpdRead(moduleId, 0x02) << endl;
        cout << "Module Type      : 0x" << hex << setw(2) << setfill('0')
             << (int)sm->ddr3SpdRead(moduleId, 0x03) << endl;
        uint32_t density = sm->ddr3SpdRead(moduleId, 0x04);
        uint32_t ba_bits = (((density >> 4) & 0x7) + 3);
        uint32_t sd_cap = 256 * (1 << (density & 0xf));

        cout << "Bank Address     : " << dec << ba_bits << " bit" << endl;
        cout << "SDRAM Capacity   : " << dec << sd_cap << " Mbit" << endl;

        uint32_t mod_org = sm->ddr3SpdRead(moduleId, 0x07);
        uint32_t n_ranks = ((mod_org >> 3) & 0x7) + 1;
        uint32_t dev_width = 4 * (1 << (mod_org & 0x07));

        cout << "Number of Ranks  : " << dec << n_ranks << endl;
        cout << "Device Width     : " << dec << dev_width << " bit" << endl;

        uint32_t mod_width = sm->ddr3SpdRead(moduleId, 0x08);
        uint32_t pb_width = 8 * (1 << (mod_width & 0x7));

        cout << "Bus Width        : " << dec << pb_width << " bit" << endl;

        uint32_t total_cap = sd_cap / 8 * pb_width / dev_width * n_ranks;
        cout << "Total Capacity   : " << dec << total_cap << " MB" << endl;

        uint32_t mtb_dividend = sm->ddr3SpdRead(moduleId, 10);
        uint32_t mtb_divisor = sm->ddr3SpdRead(moduleId, 11);
        float timebase = (float)mtb_dividend / (float)mtb_divisor;
        cout << "Medium Timebase  : " << timebase << " ns" << endl;

        uint32_t tckmin = sm->ddr3SpdRead(moduleId, 12);
        cout << "tCKmin           : " << tckmin *timebase << " ns" << endl;

        uint32_t taamin = sm->ddr3SpdRead(moduleId, 16);
        cout << "tAAmin           : " << taamin *timebase << " ns" << endl;

        uint32_t twrmin = sm->ddr3SpdRead(moduleId, 17);
        cout << "tWRmin           : " << twrmin *timebase << " ns" << endl;

        uint32_t trcdmin = sm->ddr3SpdRead(moduleId, 18);
        cout << "tRCDmin          : " << trcdmin *timebase << " ns" << endl;

        uint32_t trrdmin = sm->ddr3SpdRead(moduleId, 19);
        cout << "tRRDmin          : " << trrdmin *timebase << " ns" << endl;

        uint32_t trpmin = sm->ddr3SpdRead(moduleId, 20);
        cout << "tRPmin           : " << trpmin *timebase << " ns" << endl;

        uint32_t trasrcupper = sm->ddr3SpdRead(moduleId, 21);

        uint32_t trasmin =
            ((trasrcupper & 0x0f) << 8) | sm->ddr3SpdRead(moduleId, 22);
        cout << "tRASmin          : " << trasmin *timebase << " ns" << endl;

        uint32_t trcmin =
            ((trasrcupper & 0xf0) << 4) | sm->ddr3SpdRead(moduleId, 23);
        cout << "tRCmin           : " << trcmin *timebase << " ns" << endl;

        uint32_t trfcmin = (sm->ddr3SpdRead(moduleId, 25) << 8) |
                           sm->ddr3SpdRead(moduleId, 24);
        cout << "tRFCmin          : " << trfcmin *timebase << " ns" << endl;

        uint32_t twtrmin = sm->ddr3SpdRead(moduleId, 26);
        cout << "tWTRmin          : " << twtrmin *timebase << " ns" << endl;

        uint32_t trtpmin = sm->ddr3SpdRead(moduleId, 27);
        cout << "tRTPmin          : " << trtpmin *timebase << " ns" << endl;

        uint32_t tfawmin = ((sm->ddr3SpdRead(moduleId, 28) << 8) & 0x0f) |
                           sm->ddr3SpdRead(moduleId, 29);
        cout << "tFAWmin          : " << tfawmin *timebase << " ns" << endl;

        uint32_t tropts = sm->ddr3SpdRead(moduleId, 32);
        cout << "Thermal Sensor   : " << (int)((tropts >> 7) & 1) << endl;

        uint32_t cassupport = (sm->ddr3SpdRead(moduleId, 15) << 8) |
                              sm->ddr3SpdRead(moduleId, 14);
        cout << "CAS Latencies    : ";
        for (int i = 0; i < 14; i++) {
          if ((cassupport >> i) & 1) {
            cout << "CL" << (i + 4) << " ";
          }
        }
        cout << endl;
      }
    }
  }

  // any DataReplay related options set?
  if (sSetOneshot || sSetContinuous || sSetChannelEnable || sSetDisableReplay ||
      sFileToDdr3 || sSetReplayStatus || sSetChannelReset ) {

    uint32_t nchannels = getNumberOfReplayChannels(bar, sm);
    uint32_t startChannel, endChannel;

    if (channelId == 0xffffffff) {
      startChannel = 0;
      endChannel = nchannels - 1;
    } else if (channelId >= nchannels) {
      cerr << "ERROR: invalid channel selected: " << channelId << endl;
      abort();
    } else {
      startChannel = channelId;
      endChannel = channelId;
    }

    uint64_t module_size[2] = {0, 0};
    uint64_t max_ctrl_size[2] = {0, 0};

    // get size of installed RAM modules and max supported size from controller
    if (startChannel < 6) {
      module_size[0] = getDdr3ModuleCapacity(sm, 0);
      max_ctrl_size[0] = sm->ddr3ControllerMaxModuleSize(0);
    }
    if (endChannel > 5) {
      module_size[1] = getDdr3ModuleCapacity(sm, 1);
      max_ctrl_size[1] = sm->ddr3ControllerMaxModuleSize(1);
    }

    /**
     * now iterate over all selected channels
     **/
    for (uint32_t chId = startChannel; chId <= endChannel; chId++) {

      /**
       * get controllerId from selected channel:
       * DDLs 0 to 5 are fed from SO-DIMM 0
       * DDLs 6 to 11 are fed from DO-DIMM 1
       **/
      uint32_t controllerId = (chId < 6) ? 0 : 1;

      /** skip channel if no DDR3 module was detected */
      if (!module_size[controllerId]) {
        continue;
      }

      /**
       * set default channel start address:
       * divide module size into 8 partitions. We can have up to 6
       * channels per module, so dividing by 6 is also fine here,
       * but /8 gives the nicer boundaries.
       * The additional factor 8 comes from the data width of the
       * DDR3 interface: 64bit = 8 byte for each address.
       * 1/(8*8) = 2^(-6) => shift right by 6 bit
       **/
      uint32_t ddr3_ch_start_addr;
      uint32_t module_ch = (controllerId==0) ? chId : (chId-6);
      if (max_ctrl_size[controllerId] >= module_size[controllerId]) {
        ddr3_ch_start_addr = module_ch * (module_size[controllerId] >> 6);
      } else {
        ddr3_ch_start_addr = module_ch * (max_ctrl_size[controllerId] >> 6);
      }

      /** create link instance */
      librorc::link *link = new librorc::link(bar, chId);

      if (!link->isDdlDomainReady()) {
        cout << "WARNING: Channel " << chId
             << " clock not ready - skipping..." << endl;
        delete link;
        continue;
      }

      /** create data replay channel instance */
      librorc::datareplaychannel *dr = new librorc::datareplaychannel(link);

#ifdef MODELSIM
      /** wait for phy_init_done in simulation */
      while (!(bar->get32(RORC_REG_DDR3_CTRL) & (1 << 1))) {
        usleep(100);
      }
#endif

      if (!sm->ddr3ModuleInitReady(controllerId)) {
        cout << "ERROR: DDR3 Controller " << controllerId
             << " is not ready - doing nothing." << endl;
        abort();
      }

      if (sFileToDdr3) {
        uint32_t next_addr = ddr3_ch_start_addr;
        vector<string>::iterator iter, end;
        iter = list_of_filenames.begin();
        end = list_of_filenames.end();

        /** iterate over list of files */
        while( iter != end )
        {
            bool is_last_event = (iter == (end - 1));
            const char *filename = (*iter).c_str();
            next_addr =
                fileToRam(sm, chId, filename, next_addr, is_last_event);
            ++iter;
        }
        dr->setStartAddress(ddr3_ch_start_addr);
        cout << "Ch " << chId << ": wrote " << list_of_filenames.size()
             << " file(s) to RAM." << endl;
      }

      if (sSetOneshot) {
        dr->setModeOneshot(sOneshotVal);
      }

      if (sSetChannelReset) {
        dr->setReset(sChannelResetVal);
      }

      if (sSetContinuous) {
        dr->setModeContinuous(sContinuousVal);
      }

      if (sSetChannelEnable) {
        dr->setEnable(sChannelEnableVal);
      }

      if (sSetDisableReplay) {
        if (dr->isInReset()) {
          // already in reset: disable and release reset
          dr->setEnable(0);
          dr->setReset(0);
        } else if (!dr->isEnabled()) {
          // not in reset, not enabled, this is where we want to end
          cout << "Channel " << chId << " is not enabled, skipping..."
               << endl;
        } else {
          // enable OneShot if not already enabled
          if (!dr->isOneshotEnabled()) {
            dr->setModeOneshot(1);
          }

          // wait for OneShot replay to complete
          if (waitForReplayDone(dr) < 0) {
            cout << "Timeout waiting for Replay-Done, skipping..." << endl;
          } else {
            // we are now at the end of an event, so it's safe to disable the
            // channel
            dr->setEnable(0);
            // disable OneShot again
            dr->setModeOneshot(0);
            // toggle reset
            dr->setReset(1);
            dr->setReset(0);
          }
        }
      } // sSetDisableReplay

      if (sSetReplayStatus) {
        printChannelStatus(chId, dr, ddr3_ch_start_addr);
      }
      delete dr;
      delete link;

    } // for-loop over selected channels
  }   // any DataReplay related options set

  delete sm;
  delete bar;
  delete dev;
  return 0;
}

/**
 * get DDR3 module capacity in bytes
 **/
uint64_t getDdr3ModuleCapacity(librorc::sysmon *sm, uint8_t module_number) {
  uint64_t total_cap = 0;
  try {
    uint8_t density = sm->ddr3SpdRead(module_number, 0x04);
    /** lower 4 bit: 0000->256 Mbit, ..., 0110->16 Gbit */
    uint64_t sd_cap = ((uint64_t)256 << (20 + (density & 0xf)));
    uint8_t mod_org = sm->ddr3SpdRead(module_number, 0x07);
    uint8_t n_ranks = ((mod_org >> 3) & 0x7) + 1;
    uint8_t dev_width = 4 * (1 << (mod_org & 0x07));
    uint8_t mod_width = sm->ddr3SpdRead(module_number, 0x08);
    uint8_t pb_width = 8 * (1 << (mod_width & 0x7));
    total_cap = sd_cap / 8 * pb_width / dev_width * n_ranks;
  } catch (...) {
    cout << "WARNING: Failed to read from DDR3 SPD on SO-DIMM "
         << (uint32_t)module_number << endl << "Is a module installed?" << endl;
    total_cap = 0;
  }
  return total_cap;
}

/**
 * get number of channels with DataReplay support
 **/
uint32_t getNumberOfReplayChannels(librorc::bar *bar, librorc::sysmon *sm) {
  uint32_t nDmaChannels = sm->numberOfChannels();
  uint32_t i = 0;
  for (i = 0; i < nDmaChannels; i++) {
    librorc::link *link = new librorc::link(bar, i);
    if (!link->ddr3DataReplayAvailable()) {
      delete link;
      break;
    }
    delete link;
  }
  return i;
}

/**
 * write a file to DDR3
 **/
uint32_t fileToRam(librorc::sysmon *sm, uint32_t channelId,
                   const char *filename, uint32_t addr,
                   bool is_last_event) {
  uint32_t next_addr;
  int fd_in = open(filename, O_RDONLY);
  if (fd_in == -1) {
    cout << "ERROR: Failed to open input file" << filename << endl;
    abort();
  }
  struct stat fd_in_stat;
  fstat(fd_in, &fd_in_stat);

  uint32_t *event = (uint32_t *)mmap(NULL, fd_in_stat.st_size, PROT_READ,
                                     MAP_SHARED, fd_in, 0);
  if (event == MAP_FAILED) {
    cout << "ERROR: failed to mmap input file" << filename << endl;
    abort();
  }

  try {
    next_addr =
        sm->ddr3DataReplayEventToRam(event,
                                     (fd_in_stat.st_size >> 2), // num_dws
                                     addr,     // ddr3 start address
                                     channelId,      // channel
                                     is_last_event); // last event
  } catch (int e) {
      const char* reason;
    switch(e) {
        case LIBRORC_SYSMON_ERROR_DATA_REPLAY_TIMEOUT:
            reason = "Timeout";
            break;
        case LIBRORC_SYSMON_ERROR_DATA_REPLAY_INVALID:
            reason = "Invalid Channel";
            break;
        default:
            reason = "unknown";
            break;
    }

    cout << reason << " Exception (" << e << ") while writing event to RAM:" << endl
         << "File " << filename << " Channel " << channelId << " Addr " << hex
         << addr << dec << " LastEvent " << is_last_event << endl;
    abort();
  }

  munmap(event, fd_in_stat.st_size);
  close(fd_in);
  return next_addr;
}

/**
 * Wait for DataReplay operation to finish
 **/
int waitForReplayDone(librorc::datareplaychannel *dr) {
  uint32_t timeout = 10000;
  while (!dr->isDone() && (timeout > 0)) {
    timeout--;
    usleep(100);
  }
  return (timeout == 0) ? (-1) : 0;
}

/**
 * print the status of a replay channel
 **/
void printChannelStatus(uint32_t ChannelId, librorc::datareplaychannel *dr,
                        uint32_t start_addr) {
  cout << "Channel " << ChannelId << " Config:" << endl << "\tStart Address: 0x"
       << hex << dr->startAddress() << dec << endl
       << "\tReset: " << dr->isInReset() << endl
       << "\tContinuous: " << dr->isContinuousEnabled() << endl
       << "\tOneshot: " << dr->isOneshotEnabled() << endl
       << "\tEnabled: " << dr->isEnabled() << endl;

  cout << "Channel " << ChannelId << " Status:" << endl
       << "\tNext Address: " << hex << dr->nextAddress() << dec << endl
       << "\tWaiting: " << dr->isWaiting() << endl << "\tDone: " << dr->isDone()
       << endl;
}
