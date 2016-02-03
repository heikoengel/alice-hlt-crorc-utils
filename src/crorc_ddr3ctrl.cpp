/**
 *  crorc_ddr3ctrl.cpp
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

#include <cstdio>
#include <iomanip>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <librorc.h>

using namespace std;

#define HELP_TEXT                                                              \
  "crorc_ddr3ctrl parameters:\n"                                               \
  "General:\n"                                                                 \
  "-h|--help               show this help\n"                                   \
  "-v|--verbose            be verbose\n"                                       \
  "-n|--device [id]        select target device (required)\n"                  \
  "Module Status related:\n"                                                   \
  "-m|--module [0,1]       select target DDR3 module. If no module is \n"      \
  "                        selected all module options are applied to both \n" \
  "                        modules\n"                                          \
  "-r|--modulereset [0,1]  set/unset module reset\n"                           \
  "-I|--initmodule         (re)initialize module if reset or error state \n"   \
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
  "-l|--limit [count]      set event limit for continuous replay\n"            \
  "-D|--disablereplay      disable replay gracefully\n"                        \
  "-P|--replaystatus       show replay channel status\n"                       \
  "-W|--wait               wait for non-continous replay to finish\n"          \
  "-T|--timeout [s]        timeout for -W|--wait option\n"                     \
  "\n"

#define DDR3_MAX_RESET_RETRIES 5
#define DDR3_INIT_TIMEOUT_RETRIES 100
#define DDR3_INIT_TIMEOUT_PERIOD 1000

/********** Prototypes **********/
uint64_t getDdr3ModuleCapacity(librorc::sysmon *sm, uint8_t module_number);
uint32_t getNumberOfReplayChannels(librorc::bar *bar, librorc::sysmon *sm);
int fileToRam(librorc::sysmon *sm, uint32_t channelId, const char *filename,
              uint32_t addr, bool is_last_event, uint32_t &next_addr);
int waitForReplayDone(librorc::datareplaychannel *dr);
void printChannelStatus(uint32_t ChannelId, librorc::datareplaychannel *dr,
                        int verbose);
void printControllerStatus(librorc::ddr3 *ddr, librorc::sysmon *sm);
void printSpdTiming(librorc::sysmon *sm, int moduleId);
uint32_t getDataReplayStartAddress(uint32_t chId, uint32_t max_ctrl_size,
                                   uint32_t module_size);
uint32_t getDataReplayMaxAddress(uint32_t chId, uint32_t max_ctrl_size,
                                   uint32_t module_size);


int main(int argc, char *argv[]) {
  int ret = 0;
  int verbose = 0;
  int sSpdTimings = 0;
  int sStatus = 0;
  int sReset = 0;
  uint32_t sResetVal = 0;
  int sInitModule = 0;

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
  int sSetEventLimit = 0;
  uint32_t sEventLimitVal = 0;
  int sSetDisableReplay = 0;
  int sSetReplayStatus = 0;
  int sSetChannelReset = 0;
  uint32_t sChannelResetVal = 0;
  int sSetWait = 0;
  int sSetTimeout = 0;
  uint32_t sTimeoutVal = 0;
  bool isModuleOp = false;
  bool isChannelOp = false;

  static struct option long_options[] = {
      // General
      {"help", no_argument, 0, 'h'},
      {"verbose", no_argument, 0, 'v'},
      {"device", required_argument, 0, 'n'},
      // Module Status related
      {"module", required_argument, 0, 'm'},
      {"modulereset", required_argument, 0, 'r'},
      {"spd-timing", no_argument, 0, 't'},
      {"controllerstatus", no_argument, 0, 's'},
      {"initmodule", no_argument, 0, 'I'},
      // Data Replay related
      {"channel", required_argument, 0, 'c'},
      {"file", required_argument, 0, 'f'},
      {"oneshow", required_argument, 0, 'O'},
      {"continuous", required_argument, 0, 'C'},
      {"enable", required_argument, 0, 'e'},
      {"limit", required_argument, 0, 'L'},
      {"disablereplay", no_argument, 0, 'D'},
      {"replaystatus", no_argument, 0, 'P'},
      {"channelreset", no_argument, 0, 'R'},
      {"wait", no_argument, 0, 'W'},
      {"timeout", required_argument, 0, 'T'},
      {0, 0, 0, 0}};

  if (argc > 1) {
    while (1) {
      int opt = getopt_long(argc, argv, "hvn:m:r:tsIc:f:O:C:e:L:DPR:WT:",
                            long_options, NULL);
      if (opt == -1) {
        break;
      }

      switch (opt) {
      case 'h':
        printf(HELP_TEXT);
        return -1;
      case 'v':
        verbose = 1;
        break;
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
        isModuleOp = true;
        sReset = 1;
        sResetVal = strtol(optarg, NULL, 0);
        break;
      case 't':
        isModuleOp = true;
        sSpdTimings = 1;
        break;
      case 's':
        isModuleOp = true;
        sStatus = 1;
        break;
      case 'I':
        isModuleOp = true;
        sInitModule = 1;
        break;
      case 'f':
        isChannelOp = true;
        list_of_filenames.push_back(optarg);
        sFileToDdr3 = 1;
        break;
      case 'O':
        isChannelOp = true;
        sSetOneshot = 1;
        sOneshotVal = strtol(optarg, NULL, 0);
        break;
      case 'C':
        isChannelOp = true;
        sSetContinuous = 1;
        sContinuousVal = strtol(optarg, NULL, 0);
        break;
      case 'e':
        isChannelOp = true;
        sSetChannelEnable = 1;
        sChannelEnableVal = strtol(optarg, NULL, 0);
        break;
      case 'L':
        isChannelOp = true;
        sSetEventLimit = 1;
        sEventLimitVal = strtoul(optarg, NULL, 0);
        break;
      case 'D':
        isChannelOp = true;
        sSetDisableReplay = 1;
        break;
      case 'P':
        isChannelOp = true;
        sSetReplayStatus = 1;
        break;
      case 'R':
        isChannelOp = true;
        sSetChannelReset = 1;
        sChannelResetVal = strtol(optarg, NULL, 0);
        break;
      case 'W':
        isChannelOp = true;
        sSetWait = 1;
        break;
      case 'T':
        sSetTimeout = 1;
        sTimeoutVal = strtol(optarg, NULL, 0);
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

  if (sReset && sInitModule) {
    cerr << "WARNING: --modulereset and --initmodule cannot be used at the "
            "same time, --modulereset is ignored" << endl;
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
    return -1;
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
    return -1;
  }

  /** Instantiate a new sysmon */
  librorc::sysmon *sm;
  try {
    sm = new librorc::sysmon(bar);
  } catch (...) {
    cerr << "ERROR: failed to initialize System Monitor." << endl;
    delete bar;
    delete dev;
    return -1;
  }

  // any SO-DIMM module related options set?
  if (isModuleOp) {

    for (moduleId = moduleStartId; moduleId <= moduleEndId; moduleId++) {
      librorc::ddr3 *ddr = new librorc::ddr3(bar, moduleId);

      if (!ddr->isImplemented()) {
        if (verbose) {
          cout << "No DDR3 Controller " << moduleId
               << " available in Firmware. Skipping..." << endl;
        }
        continue;
      }

      if (sInitModule) {
        /**
         * check module state first:
         * - if in reset, just trigger reinitialization
         * - if not initialized correctly, set reset and trigger
         *   reinitialization
         **/
        uint32_t pre_reset_val = ddr->getReset();
        if ((pre_reset_val != 0) || !ddr->initSuccessful()) {
          if (pre_reset_val == 0) {
            if (verbose) {
              cout << "DDR3 C" << moduleId
                   << " not initialized correctly, triggering reinitialization..."
                   << endl;
            }
            ddr->setReset(1);
            usleep(1000);
          }
          sReset = 1;
          sResetVal = 0;
        }
      }

      if (sReset) {
        bool ensure_good = false;
        if(sResetVal==0 && ddr->getReset()==1) {
          ensure_good = true; //this is a deassert request
        }
        ddr->setReset(sResetVal);
        if (ensure_good) {
          for (int i = 0; i < DDR3_MAX_RESET_RETRIES; i++) {
            uint32_t timeout = DDR3_INIT_TIMEOUT_RETRIES;
            while (!ddr->initPhaseDone() && (timeout>0)) {
              timeout--;
              usleep(DDR3_INIT_TIMEOUT_PERIOD);
            }
            if(timeout==0 && verbose) {
              cout << "DDR3 C" << moduleId << " Initialization timeout." << endl;
            }
            if (!ddr->initSuccessful()) {
              if (verbose) {
                cout << "DDR3 C" << moduleId << " Initialization failed - "
                     << "retrying..." << endl;
              }
              ddr->setReset(1);
              usleep(1000);
              ddr->setReset(0);
            } else {
              break;
            }
          }
          if (!ddr->initSuccessful()) {
            cerr << "DDR3 C" << moduleId << " Initialization failed after "
                 << DDR3_MAX_RESET_RETRIES << " retries - aborting." << endl;
            ret = -1;
          } else {
            ret = 0;
          }
        }
      }
      if (sStatus) {
        printControllerStatus(ddr, sm);
      }
      if (sSpdTimings) {
        printSpdTiming(sm, moduleId);
      }
      delete ddr;
    }
  }

  uint32_t startChannel = 0, endChannel = 0;
  uint64_t module_size[2] = {0, 0};
  uint64_t max_ctrl_size[2] = {0, 0};
  bool module_ready[2] = {false, false};

  // any DataReplay related options set?
  if (isChannelOp) {

    uint32_t nchannels = getNumberOfReplayChannels(bar, sm);

    if (nchannels == 0) {
        /* no replay channels available (e.g. HLT_OUT...) */
        return 0;
    }

    if (channelId == 0xffffffff) {
      startChannel = 0;
      endChannel = nchannels - 1;
    } else if (channelId >= nchannels) {
      cerr << "ERROR: invalid channel selected: " << channelId << endl;
      return -1;
    } else {
      startChannel = channelId;
      endChannel = channelId;
    }

#ifdef MODELSIM
    /** wait for phy_init_done in simulation */
    while (!(bar->get32(RORC_REG_DDR3_CTRL) & (1 << 1))) {
      usleep(100);
    }
#endif

    if (sFileToDdr3) {
      /**
       * get size of installed RAM modules and max supported size from
       * controller. This is required to calculate the DDR3 addresses.
       **/
      if (startChannel < 6 && sm->ddr3Bitrate(0) != 0) {
        module_size[0] = getDdr3ModuleCapacity(sm, 0);
        max_ctrl_size[0] = sm->ddr3ControllerMaxModuleSize(0);
        module_ready[0] = sm->ddr3ModuleInitReady(0);
      }
      if (endChannel > 5 && sm->ddr3Bitrate(1) != 0) {
        module_size[1] = getDdr3ModuleCapacity(sm, 1);
        max_ctrl_size[1] = sm->ddr3ControllerMaxModuleSize(1);
        module_ready[1] = sm->ddr3ModuleInitReady(1);
      }
    }
  }

  // any DataReplay related options set?
  if (sSetOneshot || sSetContinuous || sSetChannelEnable || sSetDisableReplay ||
      sFileToDdr3 || sSetReplayStatus || sSetChannelReset || sSetEventLimit ) {

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

      /** create link instance */
      librorc::link *link = new librorc::link(bar, chId);

      if (!link->isDdlDomainReady()) {
        cerr << "WARNING: Channel " << chId << " clock not ready - skipping..."
             << endl;
        delete link;
        continue;
      }

      /** create data replay channel instance */
      librorc::datareplaychannel *dr = new librorc::datareplaychannel(link);

      if (sFileToDdr3) {

        if (!module_ready[controllerId]) {
          cerr << "DDR3 Controller/Module " << controllerId
               << " not ready or not available - skipping Ch " << chId << "."
               << endl;
          continue;
        }
        int ret = 0;
        uint32_t ddr3_ch_start_addr = getDataReplayStartAddress(
            chId, max_ctrl_size[controllerId], module_size[controllerId]);
        uint32_t ddr3_ch_max_addr = getDataReplayMaxAddress(
            chId, max_ctrl_size[controllerId], module_size[controllerId]);
        uint32_t next_addr = ddr3_ch_start_addr;
        vector<string>::iterator iter, end;
        iter = list_of_filenames.begin();
        end = list_of_filenames.end();

        /** iterate over list of files */
        while (iter != end) {
          bool is_last_event = (iter == (end - 1));
          const char *filename = (*iter).c_str();
          ret = fileToRam(sm, chId, filename, next_addr, is_last_event,
                          next_addr);
          if (next_addr > ddr3_ch_max_addr) {
            int idx = (iter - list_of_filenames.begin()) + 1;
            size_t overlap = (next_addr - ddr3_ch_max_addr) * 8; // 8B per addr
            cerr << "ERROR: Channel " << chId << ", Input file no. " << idx
                 << " (" << filename << ") - Replay data exceeds channel "
                 << "storage capacity by " << overlap << " Bytes ("
                 << (overlap >> 20) << " MB)." << endl;
            ret = -1;
          }
          if (ret) {
            break;
          }
          ++iter;
        }
        if (ret) {
          cerr << "ERROR: Failed to load File to RAM" << endl;
        } else {
          dr->setStartAddress(ddr3_ch_start_addr);
          uint64_t packets_to_ram = (next_addr - ddr3_ch_start_addr) / 8;
          uint64_t payload_to_ram = packets_to_ram * 15 * 4;
          uint64_t bytes_to_ram = packets_to_ram * 16 * 4;
          uint32_t fill_state = 100 * (next_addr - ddr3_ch_start_addr) /
                                (ddr3_ch_max_addr - ddr3_ch_start_addr);
          uint64_t max_avg_rate =
              list_of_filenames.size() * 212500000 / payload_to_ram;
          if (verbose) {
            cout << "Ch " << chId << ": wrote " << list_of_filenames.size()
                 << " file(s) to RAM using " << (bytes_to_ram >> 20) << " MB ("
                 << fill_state << "%) - Max avg. rate: " << max_avg_rate
                 << " Hz" << endl;
          }
        }
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

      if (sSetEventLimit) {
        dr->setEventLimit(sEventLimitVal);
      }

      if (sSetChannelEnable) {
        if(sChannelResetVal==1) {
          link->setDataSourceDdr3DataReplay();
        }
        dr->setEnable(sChannelEnableVal);
      }

      if (sSetDisableReplay) {
        if (dr->isInReset()) {
          // already in reset: disable and release reset
          dr->setEnable(0);
          dr->setReset(0);
        } else if (!dr->isEnabled()) {
          // not in reset, not enabled, this is where we want to end
          cout << "Channel " << chId << " is not enabled, skipping..." << endl;
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
        printChannelStatus(chId, dr, verbose);
      }
      delete dr;
      delete link;

    } // for-loop over selected channels
  }   // any DataReplay related options set

  if (sSetWait) {
    if (verbose) {
      cout << "Waiting..." << endl;
    }
    bool replayDone[endChannel];
    bool allDone = false;
    struct timeval start, now;
    gettimeofday(&start, NULL);
    bool timeout = false;
    while (!allDone && !timeout) {
      for (uint32_t chId = startChannel; chId <= endChannel; chId++) {
        librorc::link *link = new librorc::link(bar, chId);
        librorc::datareplaychannel *dr = new librorc::datareplaychannel(link);
        replayDone[chId] = dr->isDone();
        delete dr;
        delete link;
      }
      allDone = true;
      for (uint32_t i = startChannel; i <= endChannel; i++) {
        allDone &= replayDone[i];
      }
      if (sSetTimeout) {
        gettimeofday(&now, NULL);
        double tdiff = librorc::gettimeofdayDiff(start, now);
        if (tdiff > sTimeoutVal) {
          timeout = true;
        }
      }
      usleep(1000);
    } // while

    if (!allDone) {
      ret = -1;
    }

    for (uint32_t i = startChannel; i <= endChannel; i++) {
      if (replayDone[i]) {
        if (verbose) {
          cout << "Ch" << i << " Replay done." << endl;
        }
      } else {
        cerr << "*** Ch" << i << " Replay Timeout! ***" << endl;
      }
    }
  } // sSetWait

  delete sm;
  delete bar;
  delete dev;
  return ret;
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
    cerr << "WARNING: Failed to read from DDR3 SPD on SO-DIMM "
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
int fileToRam(librorc::sysmon *sm, uint32_t channelId, const char *filename,
              uint32_t addr, bool is_last_event, uint32_t &next_addr) {
  int fd_in = open(filename, O_RDONLY);
  if (fd_in == -1) {
    cerr << "ERROR: Failed to open input file" << filename << endl;
    return -1;
  }
  struct stat fd_in_stat;
  fstat(fd_in, &fd_in_stat);

  uint32_t *event = (uint32_t *)mmap(NULL, fd_in_stat.st_size, PROT_READ,
                                     MAP_SHARED, fd_in, 0);
  close(fd_in);
  if (event == MAP_FAILED) {
    cerr << "ERROR: failed to mmap input file" << filename << endl;
    return -1;
  }

  try {
    next_addr =
        sm->ddr3DataReplayEventToRam(event,
                                     (fd_in_stat.st_size >> 2), // num_dws
                                     addr,           // ddr3 start address
                                     channelId,      // channel
                                     is_last_event); // last event
  } catch (int e) {
    const char *reason;
    switch (e) {
    case LIBRORC_SYSMON_ERROR_DATA_REPLAY_TIMEOUT:
      reason = "Timeout";
      errno = EBUSY;
      break;
    case LIBRORC_SYSMON_ERROR_DATA_REPLAY_INVALID:
      reason = "Invalid Channel";
      errno = EINVAL;
      break;
    default:
      reason = "unknown";
      errno = EIO;
      break;
    }

    cerr << reason << " Exception (" << e
         << ") while writing event to RAM:" << endl << "File " << filename
         << " Channel " << channelId << " Addr " << hex << addr << dec
         << " LastEvent " << is_last_event << endl;
    munmap(event, fd_in_stat.st_size);
    return -1;
  }

  munmap(event, fd_in_stat.st_size);
  return 0;
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
                        int verbose) {
  if (verbose) {
    cout << "Channel " << ChannelId << " Config:" << endl
         << "\tStart Address: 0x" << hex << dr->startAddress() << dec << endl
         << "\tReset: " << dr->isInReset() << endl
         << "\tContinuous: " << dr->isContinuousEnabled() << endl
         << "\tOneshot: " << dr->isOneshotEnabled() << endl
         << "\tEnabled: " << dr->isEnabled() << endl
         << "\tEvent Limit: " << dr->eventLimit() << endl;
    cout << "Channel " << ChannelId << " Status:" << endl
         << "\tNext Address: 0x" << hex << dr->nextAddress() << dec << endl
         << "\tWaiting: " << dr->isWaiting() << endl << "\tDone: " << dr->isDone()
         << endl;
  } else {
    cout << "Ch" << setw(2) << ChannelId << " CFG -"
         << " R:" << dr->isInReset()
         << " E:" << dr->isEnabled()
         << " O:" << dr->isOneshotEnabled()
         << " C:" << dr->isContinuousEnabled()
         << " StartAddr:0x" << hex << setw(8) << setfill('0') << dr->startAddress()
         << " L:" << dec << dr->eventLimit()
         << setfill(' ') << endl;

    cout << "Ch" << setw(2) << ChannelId << " STS -"
         << " Done:" << dr->isDone()
         << " Waiting:" << dr->isWaiting()
         << " NextAddr:0x" << hex << setw(8) << setfill('0') << dr->nextAddress()
         << dec << setfill(' ') << endl;
  }
}

/**
 * print controller status
 **/
void printControllerStatus(librorc::ddr3 *ddr, librorc::sysmon *sm) {
  uint16_t state = ddr->controllerState();
  bool OK = ddr->initSuccessful();
  bool RESET = (ddr->getReset() != 0);
  const char *status = (RESET) ? "RESET" : ((OK) ? "OK" : "ERROR");
  cout << endl << "Controller " << ddr->id() << " Status: " << status << endl;
  cout << "\tReset: " << ((state >> LIBRORC_DDR3_RESET) & 1) << endl;
  cout << "\tPhyInitDone: " << ((state >> LIBRORC_DDR3_PHYINITDONE) & 1) << endl;
  cout << "\tPLL Lock: " << ((state >> LIBRORC_DDR3_PLLLOCK) & 1) << endl;
  cout << "\tRead Levelling Started: " << ((state >> LIBRORC_DDR3_RDLVL_START) & 1) << endl;
  cout << "\tRead Levelling Done: " << ((state >> LIBRORC_DDR3_RDLVL_DONE) & 1) << endl;
  cout << "\tRead Levelling Error: " << ((state >> LIBRORC_DDR3_RDLVL_ERROR) & 1) << endl;
  cout << "\tWrite Levelling Started: " << ((state >> LIBRORC_DDR3_WRLVL_START) & 1) << endl;
  cout << "\tWrite Levelling Done: " << ((state >> LIBRORC_DDR3_WRLVL_DONE) & 1) << endl;
  cout << "\tWrite Levelling Error: " << ((state >> LIBRORC_DDR3_WRLVL_ERROR) & 1) << endl;
  cout << "\tMax. Controller Capacity: "
       << (ddr->maxModuleSize() >> 20) << " MB" << endl;

  if (sm->firmwareIsHltHardwareTest()) {
    cout << "\tRead Count: " << ddr->getTgRdCount() << endl;
    cout << "\tWrite Count: " << ddr->getTgWrCount() << endl;
    cout << "\tTG Error: " << ddr->getTgErrorFlag() << endl;
  }
  cout << endl;
}

/**
 * print the SPD Timings of a module stored in the modules EEPROM
 **/
void printSpdTiming(librorc::sysmon *sm, int moduleId) {
  cout << endl << "Module " << moduleId << " SPD Readings:" << endl;
  try {
    cout << "Part Number      : " << sm->ddr3SpdReadString(moduleId, 128, 145)
         << endl;

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
    uint32_t trfcmin =
        (sm->ddr3SpdRead(moduleId, 25) << 8) | sm->ddr3SpdRead(moduleId, 24);
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
    uint32_t cassupport =
        (sm->ddr3SpdRead(moduleId, 15) << 8) | sm->ddr3SpdRead(moduleId, 14);
    cout << "CAS Latencies    : ";
    for (int i = 0; i < 14; i++) {
      if ((cassupport >> i) & 1) {
        cout << "CL" << (i + 4) << " ";
      }
    }
  } catch (...) {
    cerr << "Failed to read from DDR3 C" << moduleId << " EEPROM" << endl;
  }
  cout << endl;
}

uint32_t getDataReplayStartAddress(uint32_t chId, uint32_t max_ctrl_size,
                                   uint32_t module_size) {
  /**
   * set default channel start address:
   * divide module size into 8 partitions. We can have up to 6
   * channels per module, so dividing by 6 is also fine here,
   * but /8 gives the nicer boundaries.
   * The additional factor 8 comes from the data width of the
   * DDR3 interface: 64bit = 8 byte for each address.
   * 1/(8*8) = 2^(-6) => shift right by 6 bit
   **/
  uint32_t controllerId = (chId < 6) ? 0 : 1;
  uint32_t module_ch = (controllerId == 0) ? chId : (chId - 6);
  if (max_ctrl_size >= module_size) {
    return module_ch * (module_size >> 6);
  } else {
    return module_ch * (max_ctrl_size >> 6);
  }
}

uint32_t getDataReplayMaxAddress(uint32_t chId, uint32_t max_ctrl_size,
                                 uint32_t module_size) {
  uint32_t firstModuleChannel = (chId < 6) ? 0 : 6;
  uint32_t channelRange = getDataReplayStartAddress(firstModuleChannel + 1,
                                                  max_ctrl_size, module_size);
  return getDataReplayStartAddress(chId, max_ctrl_size, module_size) +
         channelRange - 1;
}
