/**
 *  crorc_fpga_ctrl.cpp
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

#include <getopt.h>
#include <sstream>
#include <iomanip>

#include <librorc.h>
#include "class_crorc.hpp"

using namespace ::std;

#define DATASOURCE_DDL 0
#define DATASOURCE_DDR 1
#define DATASOURCE_PG 2
#define DATASOURCE_PCI 4

#define LOG_HEX_DEC(x) hex << "0x" << (x) << "(" << dec << (x) << ")"
#define LOG_DEC_HEX(x) dec << (x) << "(0x" << hex << (x) << ")" << dec

void list_options(const struct option *long_options, int nargs) {
  cout << "Available arguments:" << endl;
  for (int i = 0; i < nargs; i++) {
    if (long_options[i].name) {
      if(long_options[i].flag==0) {
        cout << "-" << (char)long_options[i].val << "|";
      }
      cout << "--" << long_options[i].name;
    }
    switch (long_options[i].has_arg) {
    case optional_argument:
      cout << "(=[value])";
      break;
    default:
      break;
    }
    cout << endl;
  }
  cout << endl << "Parameters with optional value parameter can be used to"
      " get the current value without specifying a value or set a new value"
      " by adding the value parameter." << endl;
}

uint32_t pllcfg2linkspeed(librorc::gtxpll_settings pllcfg) {
  return 2 * pllcfg.refclk * pllcfg.n1 * pllcfg.n2 / pllcfg.m / pllcfg.d;
}

void print_linkpllstate(librorc::gtxpll_settings pllcfg) {
  uint32_t linkspeed = pllcfg2linkspeed(pllcfg);
  cout << "GTX Link Speed: " << linkspeed << " Mbps, RefClk: " << pllcfg.refclk
       << " MHz" << endl;
}

void print_gtxstate(uint32_t i, librorc::gtx *gtx) {
  cout << "GTX" << i << endl;
  cout << "\tReset        : " << gtx->getReset() << endl
       << "\tLoopback     : " << gtx->getLoopback() << endl
       << "\tTxDiffCtrl   : " << gtx->getTxDiffCtrl() << endl
       << "\tTxPreEmph    : " << gtx->getTxPreEmph() << endl
       << "\tTxPostEmph   : " << gtx->getTxPostEmph() << endl
       << "\tRxEqMix      : " << gtx->getRxEqMix() << endl;
  if (gtx->isDomainReady()) {
    cout << "\tDomain       : up" << endl;
    cout << "\tLink Up      : " << gtx->isLinkUp() << endl;
    cout << "\tDisp.Errors  : " << gtx->getDisparityErrorCount() << endl
         << "\tRealignCount : " << gtx->getRealignCount() << endl
         << "\tRX NIT Errors: " << gtx->getRxNotInTableErrorCount() << endl
         << "\tRX LOS Errors: " << gtx->getRxLossOfSyncErrorCount() << endl;
  } else {
    cout << "\tDomain       : DOWN!" << endl;
  }
}

void print_ddlstate(uint32_t i, crorc *rorc) {
  if (rorc->m_link[i]->isDdlDomainReady()) {
    cout << "DDL" << i << " Status" << endl;
    cout << "\tType        : " << rorc->linkTypeDescr(i) << endl;
    cout << "\tReset       : " << rorc->m_ddl[i]->getReset() << endl;
    cout << "\tIF-Enable   : " << rorc->m_ddl[i]->getEnable() << endl;
    cout << "\tDMA Deadtime: " << rorc->m_ddl[i]->getDmaDeadtime() << endl;
    if (rorc->m_diu[i] != NULL) {
      cout << "\tLink Up     : " << rorc->m_diu[i]->linkUp() << endl;
      cout << "\tLink Full   : " << rorc->m_diu[i]->linkFull() << endl;
      cout << "\tEventcount  : " << rorc->m_diu[i]->getEventcount() << endl;
      cout << "\tWordcount   : " << rorc->m_diu[i]->totalWordsReceived() << endl;
      cout << "\tDDL Deadtime: " << rorc->m_diu[i]->getDdlDeadtime() << endl;
      cout << "\tLast Command: 0x" << hex << rorc->m_diu[i]->lastDiuCommand()
           << dec << endl;
      cout << "\tLast FESTW  : 0x" << hex
           << rorc->m_diu[i]->lastFrontEndStatusWord() << dec << endl;
      cout << "\tLast CTSTW  : 0x" << hex
           << rorc->m_diu[i]->lastCommandTransmissionStatusWord() << dec
           << endl;
      cout << "\tLast DTSTW  : 0x" << hex
           << rorc->m_diu[i]->lastDataTransmissionStatusWord() << dec << endl;
      cout << "\tLast IFSTW  : 0x" << hex
           << rorc->m_diu[i]->lastInterfaceStatusWord() << dec << endl;
    }
    if (rorc->m_siu[i] != NULL) {
      cout << "\tLink Full   : " << rorc->m_siu[i]->linkFull() << endl;
      cout << "\tLink Open   : " << rorc->m_siu[i]->linkOpen() << endl;
      cout << "\tEventcount  : " << rorc->m_siu[i]->getEventcount() << endl;
      cout << "\tWordcount   : " << rorc->m_siu[i]->totalWordsTransmitted() << endl;
      cout << "\tDDL Deadtime: " << rorc->m_siu[i]->getDdlDeadtime() << endl;
      cout << "\tLast FECW   : 0x" << hex
           << rorc->m_siu[i]->lastFrontEndCommandWord() << dec << endl;
      cout << "\tIFFIFOEmpty   : " << rorc->m_siu[i]->isInterfaceFifoEmpty()
           << endl;
      cout << "\tIFFIFOFull    : " << rorc->m_siu[i]->isInterfaceFifoFull()
           << endl;
      cout << "\tSourceEmpty   : " << rorc->m_siu[i]->isSourceEmpty() << endl;
      cout << "\tErrorFlags    : " << hex << rorc->m_siu[i]->errorFlags()
           << dec <<endl;
    }
  } else {
    cout << "DDL" << i << " Clock DOWN!" << endl;
  }
}

string date2string(uint32_t fwdate) {
  stringstream ss;
  ss << hex << setfill('0') << setw(4) << (fwdate >> 16) << "-" << setw(2)
     << ((fwdate >> 8) & 0xff) << "-" << setw(2) << (fwdate & 0xff);
  return ss.str();
}

string uptime2string(uint32_t uptime_seconds) {
  stringstream ss;
  uint32_t days = uptime_seconds / 60 / 60 / 24;
  uint32_t hours = (uptime_seconds / 60 / 60) % 24;
  uint32_t minutes = (uptime_seconds / 60) % 60;
  uint32_t seconds = uptime_seconds % 60;
  ss << days << "d " << setfill('0') << setw(2) << hours << ":" << setfill('0')
     << setw(2) << minutes << ":" << setfill('0') << setw(2) << seconds;
  return ss.str();
}

void printLinkStatus(t_linkStatus ls, uint32_t linkId) {
    cout << "Ch" << setw(2) << linkId << ":";
    if (ls.gtx_inReset) {
        cout << " IN RESET!" << endl;
    } else if (!ls.gtx_domainReady) {
        cout << " NO CLOCK!" << endl;
    } else if (!ls.gtx_linkUp) {
        cout << " GTX DOWN!" << endl;
    } else {
        cout << " GTX UP,";
        if (!ls.ddl_domainReady) {
            cout << " DDL CLK STOPPED";
        } else if (ls.ddl_linkUp) {
            cout << " DDL UP";
        } else {
            cout << " DDL DOWN";
        }

        if (ls.gtx_dispErrCnt || ls.gtx_realignCnt ||
                ls.gtx_nitCnt || ls.gtx_losCnt) {
            cout << ", GTX_ERRORS";
        }

        if (ls.ddl_linkFull) {
            cout << ", DDL_FULL";
        }
        cout << endl;
    }
}

void printMetric (uint32_t ch, const char *descr, uint32_t value, const char* unit = "") {
    cout << "Ch" << ch << " " << descr << ": " << value << unit << endl;
}

void printMetric (uint32_t ch, const char *descr, const char* value, const char* unit = "") {
    cout << "Ch" << ch << " " << descr << ": " << value << unit << endl;
}

void print_dmastate(librorc::dma_channel *ch, uint32_t chId) {
  cout << "DMA Ch" << setw(2) << chId << " DMA: E=" << ch->getEnable()
       << ", StallFlag:" << ch->ptrStallFlags()
       << ", #Events:" << LOG_DEC_HEX(ch->eventCount())
       << ", RateLimit:" << ch->rateLimit() << " Hz"
       << ", PktSize:" << (ch->pciePacketSize()<<2) << " Bytes" << endl;
  cout << "DMA Ch" << setw(2) << chId << " DMA: B=" << ch->getDMABusy()
       << " EB_SW=" << LOG_DEC_HEX(ch->getEBOffset())
       << " EB_DMA=" << LOG_DEC_HEX(ch->getEBDMAOffset())
       << " EB_SIZE=" << LOG_DEC_HEX(ch->getEBSize())
       << " RB_SW=" << LOG_DEC_HEX(ch->getRBOffset())
       << " RB_DMA=" << LOG_DEC_HEX(ch->getRBDMAOffset())
       << " RB_SIZE=" << LOG_DEC_HEX(ch->getRBSize())
       << endl;
}

void drpUpdateField(librorc::gtx *gtx, uint8_t drp_addr, uint16_t field_data,
                    uint16_t field_bit, uint16_t field_width) {
  // read current value at drp_addr
  uint16_t drp_data_orig = gtx->drpRead(drp_addr);
  uint16_t bitmask = (((uint32_t)1) << field_width) - 1;
  // clear previous field data
  uint16_t drp_data_new = drp_data_orig & ~(bitmask << field_bit);
  // set new field data at field_bit
  drp_data_new |= ((field_data & bitmask) << field_bit);
  // write new  register value back to drp_addr
  if (drp_data_orig != drp_data_new) {
    gtx->drpWrite(drp_addr, drp_data_new);
    gtx->drpRead(0);
  }
}

typedef struct {
  bool set;
  bool get;
  uint32_t value;
} tControlSet;

typedef struct {
  uint32_t dev;
  uint32_t ch;
  bool listRorcs;
  bool listLinkSpeeds;
  int linkStatus;
  int gtxClearCounters;
  int gtxStatus;
  int ddlClearCounters;
  int refclkReset;
  int refclkStatus;
  int ddlStatus;
  int ddlDeadtime;
  int diuInitRemoteDiu;
  int diuInitRemoteSiu;
  int diuLastIFSTW;
  int dmaClearErrorFlags;
  int dmaStatus;
  int gtxRxInit;
  int pcieDeadtime;
  tControlSet fan;
  tControlSet flowControl;
  tControlSet channelActive;
  tControlSet led;
  tControlSet linkmask;
  tControlSet linkspeed;
  tControlSet gtxReset;
  tControlSet gtxTxReset;
  tControlSet gtxRxReset;
  tControlSet gtxLoopback;
  tControlSet gtxRxeqmix;
  tControlSet gtxTxdiffctrl;
  tControlSet gtxTxpreemph;
  tControlSet gtxTxpostemph;
  tControlSet gtxRxLosFsm;
  tControlSet gtxRxTerm;
  tControlSet gtxRxAcCapDisable;
  tControlSet ddlReset;
  tControlSet diuSendCommand;
  tControlSet diuSendXOFF;
  tControlSet ddlFilterMask;
  tControlSet ddlFilterAll;
  tControlSet dmaRateLimit;
  tControlSet dataSource;
} tRorcCmd;

tControlSet evalParam(char *param) {
  tControlSet cs = {false, false, 0};
  if (param) {
    cs.value = strtol(param, NULL, 0);
    cs.set = true;
  } else {
    cs.get = true;
  }
  return cs;
}

int main(int argc, char *argv[]) {

  crorc *rorc = NULL;
  tRorcCmd cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.ch = LIBRORC_CH_UNDEF;

  static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"bracketled", optional_argument, 0, 'b'},
      {"channel", required_argument, 0, 'c'},
      {"channelactive", optional_argument, 0, 'E'},
      {"datasource", optional_argument, 0, 'T'},
      {"ddlclearcounters", no_argument, &(cmd.ddlClearCounters), 1},
      {"ddlfilterall", optional_argument, 0, 'A'},
      {"ddlfiltermask", optional_argument, 0, 'F'},
      {"ddlreset", optional_argument, 0, 'd'},
      {"ddlstatus", no_argument, &(cmd.ddlStatus), 1},
      {"ddldeadtime", no_argument, &(cmd.ddlDeadtime), 1},
      {"device", required_argument, 0, 'n'},
      {"diuinitremotediu", no_argument, &(cmd.diuInitRemoteDiu), 1},
      {"diuinitremotesiu", no_argument, &(cmd.diuInitRemoteSiu), 1},
      {"diusendcmd", optional_argument, 0, 'C'},
      {"diusendxoff", optional_argument, 0, 'X'},
      {"diulastifstw", no_argument, &(cmd.diuLastIFSTW), 1},
      {"dmaclearerrorflags", no_argument, &(cmd.dmaClearErrorFlags), 1},
      {"dmaratelimit", optional_argument, 0, 't'},
      {"dmastatus", no_argument, &(cmd.dmaStatus), 1},
      {"fan", optional_argument, 0, 'f'},
      {"flowcontrol", optional_argument, 0, 'B'},
      {"gtxclearcounters", no_argument, &(cmd.gtxClearCounters), 1},
      {"gtxloopback", optional_argument, 0, 'L'},
      {"gtxreset", optional_argument, 0, 'R'},
      {"gtxrxeqmix", optional_argument, 0, 'M'},
      {"gtxrxinit", no_argument, &(cmd.gtxRxInit), 1},
      {"gtxrxlosfsm", optional_argument, 0, 'o'},
      {"gtxrxterm", optional_argument, 0, 'p'},
      {"gtxrxaccapdisable", optional_argument, 0, 'q'},
      {"gtxrxreset", optional_argument, 0, 'e'},
      {"gtxstatus", no_argument, &(cmd.gtxStatus), 1},
      {"gtxtxdiffctrl", optional_argument, 0, 'D'},
      {"gtxtxpostemph", optional_argument, 0, 'O'},
      {"gtxtxpreemph", optional_argument, 0, 'P'},
      {"gtxtxreset", optional_argument, 0, 'a'},
      {"linkmask", optional_argument, 0, 'm'},
      {"linkspeed", optional_argument, 0, 's'},
      {"linkstatus", no_argument, &(cmd.linkStatus), 1},
      {"listlinkspeeds", no_argument, 0, 'S'},
      {"listrorcs", no_argument, 0, 'l'},
      {"pciedeadtime", no_argument, &(cmd.pcieDeadtime), 1},
      {"refclkreset", no_argument, &(cmd.refclkReset), 1},
      {"refclkstatus", no_argument, &(cmd.refclkStatus), 1},
      {0, 0, 0, 0}};
  int nargs = sizeof(long_options) / sizeof(option);

  // generate optstring for getopt_long()
  string optstring = "";
  for (int i = 0; i < nargs; i++) {
    if (long_options[i].flag == 0) {
      optstring += long_options[i].val;
    }
    switch (long_options[i].has_arg) {
    case required_argument:
      optstring += ":";
      break;
    case optional_argument:
      optstring += "::";
      break;
    default:
      break;
    }
  }

  /** Parse command line arguments **/
  if (argc > 1) {
    while (1) {
      int opt = getopt_long(argc, argv, optstring.c_str(), long_options, NULL);
      if (opt == -1) {
        break;
      }

      switch (opt) {
      case 'h':
        // help
        list_options(long_options, nargs);
        return 0;
        break;

      case 'f':
        // fan: on, off, auto
        if (optarg) {
          if (strcmp(optarg, "on") == 0) {
            cmd.fan.value = FAN_ON;
            cmd.fan.set = true;
          } else if (strcmp(optarg, "off") == 0) {
            cmd.fan.value = FAN_OFF;
            cmd.fan.set = true;
          } else if (strcmp(optarg, "auto") == 0) {
            cmd.fan.value = FAN_AUTO;
            cmd.fan.set = true;
          } else {
            cout << "Invalid argument to 'fan', allowed are 'on', 'off' and "
                    "'auto'." << endl;
            return -1;
          }
        } else {
          cmd.fan.get = true;
        }
        break;

      case 'm':
        // linkmask
        cmd.linkmask = evalParam(optarg);
        break;

      case 'b':
        // bracketled: auto, blink
        if (optarg) {
          if (strcmp(optarg, "auto") == 0) {
            cmd.led.value = LED_AUTO;
            cmd.led.set = true;
          } else if (strcmp(optarg, "blink") == 0) {
            cmd.led.value = LED_BLINK;
            cmd.led.set = true;
          } else {
            cout << "Invalid argument to 'bracketled', allowed are 'auto' and "
                    "'blink'." << endl;
            return -1;
          }
        } else {
          cmd.led.get = true;
        }
        break;

      case 'n':
        // device
        cmd.dev = strtol(optarg, NULL, 0);
        break;

      case 'c':
        // channel
        cmd.ch = strtol(optarg, NULL, 0);
        break;

      case 'T':
        // datasource
        if (optarg) {
          if (strcmp(optarg, "ddl") == 0) {
            cmd.dataSource.value = DATASOURCE_DDL;
            cmd.dataSource.set = true;
          } else if (strcmp(optarg, "ddr") == 0) {
            cmd.dataSource.value = DATASOURCE_DDR;
            cmd.dataSource.set = true;
          } else if (strcmp(optarg, "pg") == 0) {
            cmd.dataSource.value = DATASOURCE_PG;
            cmd.dataSource.set = true;
          } else if (strcmp(optarg, "pci") == 0) {
            cmd.dataSource.value = DATASOURCE_PCI;
            cmd.dataSource.set = true;
          } else {
            cout << "Invalid argument to 'datasource', allowed are 'ddl', "
                    "'ddr', 'pg' and 'pci'." << endl;
            return -1;
          }
        } else {
          cmd.dataSource.get = true;
        }
        break;

      case 's':
        // linkspeed [specifier number]
        cmd.linkspeed = evalParam(optarg);
        break;

      case 'l':
        // listrorcs
        cmd.listRorcs = true;
        break;

      case 'S':
        // listlinkspeeds
        cmd.listLinkSpeeds = true;
        break;

      case 'R':
        // gtxreset
        cmd.gtxReset = evalParam(optarg);
        break;

      case 'B':
        // flowcontrol
        cmd.flowControl = evalParam(optarg);
        break;

      case 'E':
        // channelactive
        cmd.channelActive = evalParam(optarg);
        break;

      case 'a':
        // gtxtxreset
        cmd.gtxTxReset = evalParam(optarg);
        break;

      case 'e':
        // gtxtxreset
        cmd.gtxRxReset = evalParam(optarg);
        break;

      case 'L':
        // gtxloopback
        cmd.gtxLoopback = evalParam(optarg);
        break;

      case 'M':
        // gtxrxeqmix
        cmd.gtxRxeqmix = evalParam(optarg);
        break;

      case 'D':
        // gtxtxdiffctrl
        cmd.gtxTxdiffctrl = evalParam(optarg);
        break;

      case 'P':
        // gtxtxpreemph
        cmd.gtxTxpreemph = evalParam(optarg);
        break;

      case 'O':
        // gtxtxpostemph
        cmd.gtxTxpostemph = evalParam(optarg);
        break;

      case 'o':
        // gtxrxlosfsm
        cmd.gtxRxLosFsm = evalParam(optarg);
        break;

      case 'p':
        // gtxrxterm
        cmd.gtxRxTerm = evalParam(optarg);
        if (cmd.gtxRxTerm.set &&
            (cmd.gtxRxTerm.value < 0 || cmd.gtxRxTerm.value > 2)) {
          cerr << "Invalid GTX RX Term Value " << cmd.gtxRxTerm.value
               << ", allowed: 0,1,2" << endl;
          return -1;
        }
        break;

      case 'q':
        // gtxrxaccapdisable
        cmd.gtxRxAcCapDisable = evalParam(optarg);
        if (cmd.gtxRxAcCapDisable.set && cmd.gtxRxAcCapDisable.value > 1) {
          cerr << "Invalid GTX RX AC_CAP_DIS Value "
               << cmd.gtxRxAcCapDisable.value << ", allows: 0,1" << endl;
          return -1;
        }
        break;

      case 'd':
        // ddlreset
        cmd.ddlReset = evalParam(optarg);
        break;

      case 'C':
        // ddlreset
        cmd.diuSendCommand = evalParam(optarg);
        break;

      case 't':
        // dmaratelimit
        cmd.dmaRateLimit = evalParam(optarg);
        break;

      case 'F':
        // ddlFilterMask
        cmd.ddlFilterMask = evalParam(optarg);
        break;

      case 'A':
        cmd.ddlFilterAll = evalParam(optarg);
        break;

      case 'X':
        cmd.diuSendXOFF = evalParam(optarg);
        break;

      case '?':
        return -1;
      default:
        continue;
      }
    }
  } else {
    list_options(long_options, nargs);
    return -1;
  }

  uint32_t nPllCfgs =
      sizeof(librorc::gtxpll_supported_cfgs) / sizeof(librorc::gtxpll_settings);

  if (cmd.listLinkSpeeds) {
    cout << "Number of supported configurations: " << nPllCfgs << endl;
    for (uint32_t i = 0; i < nPllCfgs; i++) {
      librorc::gtxpll_settings pll = librorc::gtxpll_supported_cfgs[i];
      cout << i << ") ";
      print_linkpllstate(pll);
    }
    return 0;
  }

  if (cmd.listRorcs) {
    librorc::device *dev = NULL;
    librorc::bar *bar = NULL;
    librorc::sysmon *sm = NULL;

    for (uint32_t idx = 0; idx < 32; idx++) {
      try {
        dev = new librorc::device(idx);
      } catch (int e) {
        if (e != LIBRORC_DEVICE_ERROR_PDADEV_FAILED) {
          cerr << "Failed to initizalize device " << idx << ": "
               << librorc::errMsg(e) << endl;
          return -1;
        } else if (idx == 0) {
            cout << "No C-RORC found." << endl;
        }
        return 0;
      }

      cout << "[" << idx << "] " << setfill('0') << hex << setw(4) << (uint32_t)
          dev->getDomain() << ":" << hex << setw(2) << (uint32_t)
          dev->getBus() << ":" << hex << setw(2) << (uint32_t)
          dev->getSlot() << "." << hex << setw(1) << (uint32_t) dev->getFunc();

      try {
        bar = new librorc::bar(dev, 1);
        sm = new librorc::sysmon(bar);
      }
      catch (int e) {
        cerr << "Failed to intialize device " << idx
             << ": " << librorc::errMsg(e) << endl;
        if (sm) {
            delete sm;
        }
        if (bar) {
            delete bar;
        }
        delete dev;
        return -1;
      }

      cout << " " << setw(10) << setfill(' ') << left
           << sm->firmwareDescription() << right << " Date: "
           << date2string(sm->FwBuildDate()) << ", Rev: 0x"
           << hex << setw(7) << setfill('0')
           << sm->FwRevision() << ", Uptime: "
           << uptime2string(sm->uptimeSeconds()) << endl;

      delete sm;
      delete bar;
      delete dev;
    }
    return 0;
  }

  try {
    rorc = new crorc(cmd.dev);
  } catch (...) {
    cerr << "Failed to intialize RORC" << cmd.dev << endl;
    return -1;
  }

  if (cmd.fan.get) {
    cout << "Fan State: " << rorc->getFanState() << endl;
  } else if (cmd.fan.set) {
    rorc->setFanState(cmd.fan.value);
  }

  if (cmd.led.get) {
    cout << "Bracket LED State: " << rorc->getLedState() << endl;
  } else if (cmd.led.set) {
    rorc->setLedState(cmd.led.value);
  }

  if (cmd.linkmask.get) {
    cout << "Linkmask 0x" << hex << rorc->getLinkmask() << dec << endl;
  } else if (cmd.linkmask.set) {
    rorc->setLinkmask(cmd.linkmask.value);
  }

  if (cmd.pcieDeadtime) {
    cout << "PCIe max. Deadtime: " << rorc->m_sm->maxPcieDeadtime() << " us"
         << endl;
  }

  int ch_start, ch_end;
  if (cmd.ch == LIBRORC_CH_UNDEF) {
    ch_start = 0;
    ch_end = rorc->m_nchannels - 1;
  } else {
    ch_start = cmd.ch;
    ch_end = cmd.ch;
  }

  int gtx_start = ch_start;
  int gtx_end = -1;
  for (int i = ch_start; i <= ch_end; i++) {
    if (rorc->isOpticalLink(i)) {
      gtx_end = i;
    }
  }

  if (cmd.linkspeed.get) {
    librorc::refclkopts clkopts;
    try {
      clkopts = rorc->m_refclk->getCurrentOpts(0);
    } catch (int e) {
      cout << "ERROR: Failed to read RefClk configuration: "
           << librorc::errMsg(e) << endl;
      return -1;
    }
    for (int i = gtx_start; i <= gtx_end; i++) {
      librorc::gtxpll_settings pllsts = rorc->m_gtx[i]->drpGetPllConfig();
      pllsts.refclk = rorc->m_refclk->getFout(clkopts);
      cout << "Ch" << i << " ";
      print_linkpllstate(pllsts);
    }
  } else if (cmd.linkspeed.set) {
    uint32_t linkspeed_index = cmd.linkspeed.value;

    // try to detect the command line value as link speed in Mbps
    if (linkspeed_index > nPllCfgs) {
      for (uint32_t idx = 0; idx < nPllCfgs; idx++) {
        uint32_t pll_ls = pllcfg2linkspeed(librorc::gtxpll_supported_cfgs[idx]);
        if (pll_ls == linkspeed_index) {
          linkspeed_index = idx;
          break;
        }
      }
    }

    if (linkspeed_index > nPllCfgs) {
      cout << "ERROR: invalid PLL config selected." << endl;
      return -1;
    }
    librorc::gtxpll_settings pllcfg =
        librorc::gtxpll_supported_cfgs[linkspeed_index];

    // set QSFPs and GTX to reset
    rorc->setAllQsfpReset(1);
    rorc->setAllGtxReset(1);

    // reconfigure reference clock
    try {
      rorc->m_refclk->reset();
      if (pllcfg.refclk != LIBRORC_REFCLK_DEFAULT_FOUT) {
        librorc::refclkopts refclkopts =
            rorc->m_refclk->getCurrentOpts(LIBRORC_REFCLK_DEFAULT_FOUT);
        librorc::refclkopts new_refclkopts =
            rorc->m_refclk->calcNewOpts(pllcfg.refclk, refclkopts.fxtal);
        rorc->m_refclk->writeOptsToDevice(new_refclkopts);
      }
    } catch (int e) {
      cout << "ERROR: Failed to reconfigure RefClk: " << librorc::errMsg(e) << endl;
      cout << "Keeping QSFPs and GTXs in reset! Don't release reset "
              "unless you know what you're doing! It's probably best "
              "to power cycle..." << endl;
      return -1;
    }

    // reconfigure GTX
    rorc->configAllGtxPlls(pllcfg);

    // release GTX and QSFP resets
    rorc->setAllGtxReset(0);
    rorc->setAllQsfpReset(0);

    print_linkpllstate(pllcfg);
  }

  if (cmd.listLinkSpeeds) {
    cout << "Number of supported configurations: " << nPllCfgs << endl;
    for (uint32_t i = 0; i < nPllCfgs; i++) {
      librorc::gtxpll_settings pll = librorc::gtxpll_supported_cfgs[i];
      cout << i << ") ";
      print_linkpllstate(pll);
    }
  }

  if (cmd.refclkReset) {
    try {
      rorc->m_refclk->reset();
    } catch (int e) {
      cout << "ERROR: Failed to reset RefClk: " << librorc::errMsg(e) << endl;
      return -1;
    }
  }

  if (cmd.refclkStatus) {
    cout << "Stored RefClk Freq.: " << rorc->m_sm->refclkFreq() << " Hz"
         << endl;
    try {
      librorc::refclkopts refclkopts = rorc->m_refclk->getCurrentOpts(0);
      cout << "Read RefClk Freq.  : " << rorc->m_refclk->getFout(refclkopts)
           << " MHz" << endl;
    } catch (int e) {
      cout << "ERROR: Failed to read from RefClk: " << librorc::errMsg(e)
           << endl;
    }
  }

  // GTX-DRP setters require GTX and QSFP to be in reset
  if (cmd.gtxRxTerm.set || cmd.gtxRxAcCapDisable.set) {
    rorc->setAllGtxReset(1);
    rorc->setAllQsfpReset(1);
  }

  // iterate over all or selectedGTX instances
  for (int32_t i = gtx_start; i <= gtx_end; i++) {

    if (cmd.linkStatus) {
      printLinkStatus(rorc->getLinkStatus(i), i);
    }

    if (cmd.gtxReset.get) {
      uint32_t val = rorc->m_gtx[i]->getReset();
      cout << "Ch" << i << "\tGTX Full Reset: " << (val & 1) << endl
           << "\tGTX RX Reset: " << ((val >> 1) & 1) << endl
           << "\tGTX TX Reset: " << ((val >> 2) & 1) << endl;
    } else if (cmd.gtxReset.set) {
      // NOTE: this overrides any RxReset or RxReset value
      rorc->m_gtx[i]->setReset(cmd.gtxReset.value & 1);
    }

    if (cmd.gtxRxReset.get) {
      printMetric(i, "GTX RX Reset", rorc->m_gtx[i]->getRxReset());
    } else if (cmd.gtxRxReset.set) {
      rorc->m_gtx[i]->setRxReset(cmd.gtxRxReset.value);
    }

    if (cmd.gtxTxReset.get) {
      printMetric(i, "GTX TX Reset", rorc->m_gtx[i]->getTxReset());
    } else if (cmd.gtxTxReset.set) {
      rorc->m_gtx[i]->setTxReset(cmd.gtxTxReset.value);
    }

    if (cmd.gtxLoopback.get) {
        printMetric(i, "GTX Loopback", rorc->m_gtx[i]->getLoopback());
    } else if (cmd.gtxLoopback.set) {
      rorc->m_gtx[i]->setLoopback(cmd.gtxLoopback.value);
    }

    if (cmd.gtxRxeqmix.get) {
        printMetric(i, "GTX RxEqMix", rorc->m_gtx[i]->getRxEqMix());
    } else if (cmd.gtxRxeqmix.set) {
      rorc->m_gtx[i]->setRxEqMix(cmd.gtxRxeqmix.value);
    }

    if (cmd.gtxTxdiffctrl.get) {
      printMetric(i, "GTX TxDiffCtrl", rorc->m_gtx[i]->getTxDiffCtrl());
    } else if (cmd.gtxTxdiffctrl.set) {
      rorc->m_gtx[i]->setTxDiffCtrl(cmd.gtxTxdiffctrl.value);
    }

    if (cmd.gtxTxpreemph.get) {
      printMetric(i, "GTX TxPreEmph", rorc->m_gtx[i]->getTxPreEmph());
    } else if (cmd.gtxTxpreemph.set) {
      rorc->m_gtx[i]->setTxPreEmph(cmd.gtxTxpreemph.value);
    }

    if (cmd.gtxTxpostemph.get) {
      printMetric(i, "GTX TxPostEmph", rorc->m_gtx[i]->getTxPostEmph());
    } else if (cmd.gtxTxpostemph.set) {
      rorc->m_gtx[i]->setTxPostEmph(cmd.gtxTxpostemph.value);
    }

    if (cmd.gtxRxLosFsm.get) {
      uint16_t state = ((rorc->m_gtx[i]->drpRead(0x04) >> 15) & 1);
      cout << "Ch" << i << " RX LossOfSync FSM: ";
      if (state) {
        cout << "ON (1)";
      } else {
        cout << "OFF (0)";
      }
      cout << endl;
    } else if (cmd.gtxRxLosFsm.set) {
      uint16_t state = rorc->m_gtx[i]->drpRead(0x04);
      state &= ~(1 << 15);
      state |= ((cmd.gtxRxLosFsm.value & 1) << 15);
      rorc->m_gtx[i]->drpWrite(0x04, state);
    }

    if (cmd.gtxRxTerm.get) {
      uint16_t state = rorc->m_gtx[i]->drpRead(0x2e);
      uint16_t term = ((state >> 7) & 0x03);
      const char *termstr = NULL;
      switch (term) {
      case 0:
        termstr = "FLOAT (0)";
        break;
      case 1:
        termstr = "GND (1)";
        break;
      case 2:
        termstr = "MGTAVTT (2)";
        break;
      default:
        termstr = "INVALID (3)";
        break;
      }
      cout << "Ch" << i << " GTX RX Termination: " << termstr << endl;
    } else if (cmd.gtxRxTerm.set) {
      drpUpdateField(rorc->m_gtx[i], 0x2e, cmd.gtxRxTerm.value, 7, 2);
    }

    if (cmd.gtxRxAcCapDisable.get) {
      uint16_t state = ((rorc->m_gtx[i]->drpRead(0x17) >> 4) & 0x01);
      cout << "Ch" << i << " GTX AC_CAP_DIS=" << ((state) ? "TRUE" : "FALSE")
           << endl;
    } else if (cmd.gtxRxAcCapDisable.set) {
      drpUpdateField(rorc->m_gtx[i], 0x17, cmd.gtxRxAcCapDisable.value, 4, 1);
    }

    if (cmd.gtxClearCounters) {
      rorc->m_gtx[i]->clearErrorCounters();
    }

    if (cmd.gtxRxInit) {
      int result = rorc->m_gtx[i]->rxInitialize();
      if (result < 0) {
        cerr << "Ch" << i << " failed to initialize GTX RX Interface" << endl;
      } else {
        cout << "Ch" << i << " initialized GTX RX Interface after " << result
             << " retires" << endl;
        rorc->m_gtx[i]->clearErrorCounters();
      }
    }

    if (cmd.gtxStatus) {
      print_gtxstate(i, rorc->m_gtx[i]);
    }
  }

  // release resets for GTX-DRP setters
  if (cmd.gtxRxTerm.set || cmd.gtxRxAcCapDisable.set) {
    rorc->setAllGtxReset(0);
    rorc->setAllQsfpReset(0);
  }


  // iterate over all or selected DMA channels
  for (int32_t i = ch_start; i <= ch_end; i++) {

    if (cmd.dataSource.get) {
      printMetric(i, "Datasource", rorc->m_link[i]->getDataSourceDescr());
    } else if (cmd.dataSource.set) {
      switch (rorc->m_linkType[i]) {
      case RORC_CFG_LINK_TYPE_VIRTUAL:
        cout << "Ch" << i << " cannot change datasource of a Raw-Copy channel."
             << endl;
        break;
      case RORC_CFG_LINK_TYPE_DIU:
        switch (cmd.dataSource.value) {
        case DATASOURCE_DDL:
          rorc->m_link[i]->setDefaultDataSource();
          break;
        case DATASOURCE_DDR:
          rorc->m_link[i]->setDataSourceDdr3DataReplay();
          break;
        case DATASOURCE_PG:
          rorc->m_link[i]->setDataSourcePatternGenerator();
          break;
        default:
          cout << "Ch" << i << " invalid data source" << endl;
          break;
        }
        break;
      case RORC_CFG_LINK_TYPE_SIU:
        switch (cmd.dataSource.value) {
        case DATASOURCE_PCI:
          rorc->m_link[i]->setDefaultDataSource();
          break;
        case DATASOURCE_PG:
          rorc->m_link[i]->setDataSourcePatternGenerator();
          break;
        default:
          cout << "Ch" << i << " invalid data source" << endl;
          break;
        }
        break;
      }
    }

    if (cmd.ddlReset.get) {
      printMetric(i, "DDL Reset", rorc->m_ddl[i]->getReset());
    } else if (cmd.ddlReset.set) {
      rorc->m_ddl[i]->setReset(cmd.ddlReset.value);
    }

    if (cmd.ddlClearCounters) {
      if (rorc->m_diu[i] != NULL) {
        rorc->m_diu[i]->clearAllLastStatusWords();
        rorc->m_diu[i]->clearDdlDeadtime();
        rorc->m_diu[i]->clearEventcount();
      } else if (rorc->m_siu[i] != NULL) {
        rorc->m_siu[i]->clearLastFrontEndCommandWord();
        rorc->m_siu[i]->clearDdlDeadtime();
        rorc->m_siu[i]->clearEventcount();
      }
      rorc->m_ddl[i]->clearDmaDeadtime();
    }

    if (cmd.ddlStatus) {
      print_ddlstate(i, rorc);
    }

    if (cmd.ddlDeadtime) {
      if (rorc->m_diu[i] != NULL) {
        cout << rorc->m_diu[i]->getDdlDeadtime() << endl;
      }
    }

    if (cmd.diuInitRemoteDiu) {
      if (rorc->m_diu[i] != NULL) {
        if (rorc->m_diu[i]->prepareForDiuData()) {
          cout << "DIU" << i << " Failed to init remote DIU" << endl;
        }
      } else {
        cout << "Link" << i << " has no local DIU, cannot init remote DIU"
             << endl;
      }
    }

    if (cmd.diuInitRemoteSiu) {
      if (rorc->m_diu[i] != NULL) {
        if (rorc->m_diu[i]->prepareForSiuData()) {
          cout << "DIU" << i << " Failed to init remote SIU" << endl;
        }
      } else {
        cout << "Link" << i << " has no local DIU, cannot init remote SIU"
             << endl;
      }
    }

    if (cmd.diuSendCommand.get) {
      if (rorc->m_diu[i] != NULL) {
        cout << "Link" << i << " Last DIU command: 0x" << setw(8)
             << setfill('0') << rorc->m_diu[i]->lastDiuCommand() << endl;
      } else {
        cout << "Link" << i << " has no local DIU, no last command" << endl;
      }
    } else if (cmd.diuSendCommand.set) {
      if (rorc->m_diu[i] != NULL) {
        rorc->m_diu[i]->sendCommand(cmd.diuSendCommand.value);
      } else {
        cout << "Link" << i << " has no local DIU, cannot send command" << endl;
      }
    }

    if (cmd.diuSendXOFF.get) {
      if (rorc->m_diu[i] != NULL) {
        cout << "Link" << i << " forced-XOFF: " << rorc->m_diu[i]->getForcedXoff()
             << endl;
      } else {
        cout << "Link" << i << " has no local DIU, no last command" << endl;
      }
    } else if (cmd.diuSendXOFF.set) {
      if (rorc->m_diu[i] != NULL) {
        rorc->m_diu[i]->setForcedXoff(cmd.diuSendXOFF.value);
      } else {
        cout << "Link" << i << " has no local DIU, cannot send XOFF" << endl;
      }
    }

    if (cmd.diuLastIFSTW) {
      if (rorc->m_diu[i] != NULL) {
        uint32_t lastIFSTW = rorc->m_diu[i]->lastInterfaceStatusWord();
        bool linkUp = rorc->m_diu[i]->linkUp();
        cout << "Link " << i << " last IFSTW: 0x" << hex << lastIFSTW
             << dec << " linkUp: " << linkUp << endl;
      } else {
        cout << "Link" << i << " has no local DIU so no last IFSTW" << endl;
      }
    }

    if (cmd.dmaClearErrorFlags) {
      rorc->m_ch[i]->readAndClearPtrStallFlags();
    }

    if (cmd.dmaStatus) {
      print_dmastate(rorc->m_ch[i], i);
    }

    if (cmd.dmaRateLimit.get) {
      uint32_t pcie_gen = rorc->m_sm->pcieGeneration();
      printMetric(i, "Rate Limit", rorc->m_ch[i]->rateLimit(pcie_gen), " Hz");
    } else if (cmd.dmaRateLimit.set) {
      uint32_t pcie_gen = rorc->m_sm->pcieGeneration();
      rorc->m_ch[i]->setRateLimit(cmd.dmaRateLimit.value, pcie_gen);
    }

    if (cmd.ddlFilterAll.get) {
      if (rorc->m_filter[i]) {
        printMetric(i, "Filter-All", rorc->m_filter[i]->getFilterAll());
      } else {
        cout << "Link" << i << " has no EventFilter" << endl;
      }
    } else if (cmd.ddlFilterAll.set) {
      if (rorc->m_filter[i]) {
        rorc->m_filter[i]->setFilterAll(cmd.ddlFilterAll.value);
      } else {
        cout << "Link" << i << " has no EventFilter" << endl;
      }
    }

    if (cmd.ddlFilterMask.get) {
      if (rorc->m_filter[i]) {
        printMetric(i, "Filter-Mask", rorc->m_filter[i]->getFilterMask());
      } else {
        cout << "Link" << i << " has no EventFilter" << endl;
      }
    } else if (cmd.ddlFilterMask.set) {
      if (rorc->m_filter[i]) {
        rorc->m_filter[i]->setFilterMask(cmd.ddlFilterMask.value);
      } else {
        cout << "Link" << i << " has no EventFilter" << endl;
      }
    }

    if (cmd.flowControl.get) {
      printMetric(i, "Flow Control", rorc->m_link[i]->flowControlIsEnabled());
    } else if (cmd.flowControl.set) {
      rorc->m_link[i]->setFlowControlEnable(cmd.flowControl.value);
    }

    if (cmd.channelActive.get) {
      printMetric(i, "Channel Active", rorc->m_link[i]->channelIsActive());
    } else if (cmd.channelActive.set) {
      rorc->m_link[i]->setChannelActive(cmd.channelActive.value);
    }
  }

  if (rorc) {
    delete rorc;
  }
  return 0;
}
