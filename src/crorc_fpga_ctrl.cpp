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
 *     * Neither the name of University Frankfurt, CERN nor the
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
 * TODO:
 * - check GTX/DDL Domain is up before accessing
 **/

#include <getopt.h>
#include <sstream>
#include <iomanip>

#include <librorc.h>
#include "class_crorc.hpp"

using namespace ::std;

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
}

void print_linkpllstate(librorc::gtxpll_settings pllcfg) {
  uint32_t linkspeed =
      2 * pllcfg.refclk * pllcfg.n1 * pllcfg.n2 / pllcfg.m / pllcfg.d;
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
       << "\tTxEqMix      : " << gtx->getRxEqMix() << endl
       << "\tDfeEye       : " << gtx->dfeEye() << endl;
  if (gtx->isDomainReady()) {
    cout << "\tDomain       : up" << endl;
    cout << "\tLink Up      : " << gtx->isLinkUp() << endl;
    cout << "\tDisp.Errors  : " << gtx->getDisparityErrorCount() << endl
         << "\tRealignCount : " << gtx->getRealignCount() << endl
         << "\tRX NIT Errors: " << gtx->getRxNotInTableErrorCount() << endl
         << "\tRX LOS Errors: " << gtx->getRxLossOfSignalErrorCount() << endl;
  } else {
    cout << "\tDomain       : DOWN!" << endl;
  }
}

void print_ddlstate(uint32_t i, crorc *rorc) {
  if (rorc->m_link[i]->isDdlDomainReady()) {
    cout << "DDL" << i << " Status" << endl;
    cout << "\tReset       : " << rorc->m_ddl[i]->getReset() << endl;
    cout << "\tIF-Enable   : " << rorc->m_ddl[i]->getEnable() << endl;
    cout << "\tLink Full   : " << rorc->m_ddl[i]->linkFull() << endl;
    cout << "\tDMA Deadtime: " << rorc->m_ddl[i]->getDmaDeadtime() << endl;
    cout << "\tEventcount  : " << rorc->m_ddl[i]->getEventcount() << endl;
    if (rorc->m_diu[i] != NULL) {
      cout << "\tLink Up     : " << rorc->m_diu[i]->linkUp() << endl;
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
      cout << "\tDDL Deadtime: " << rorc->m_siu[i]->getDdlDeadtime() << endl;
      cout << "\tLast FECW   : 0x" << hex
           << rorc->m_siu[i]->lastFrontEndCommandWord() << dec << endl;
      cout << "IFFIFOEmpty   : " << rorc->m_siu[i]->isInterfaceFifoEmpty()
           << endl;
      cout << "IFFIFOFull    : " << rorc->m_siu[i]->isInterfaceFifoFull()
           << endl;
      cout << "SourceEmpty   : " << rorc->m_siu[i]->isSourceEmpty() << endl;
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
  ss << days << "d " << setfill('0') << setw(2) << hours << ":" << minutes
     << ":" << seconds;
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
    }
    cout << endl;
}


typedef struct {
  bool set;
  bool get;
  uint32_t value;
} tControlSet;

typedef struct {
  uint32_t dev;
  uint32_t ch;
  int boardReset;
  bool listRorcs;
  bool listLinkSpeeds;
  int linkStatus;
  int gtxClearCounters;
  int gtxStatus;
  int ddlClearCounters;
  int refclkReset;
  int ddlStatus;
  int diuInitRemoteDiu;
  int diuInitRemoteSiu;
  int dmaClearErrorFlags;
  tControlSet fan;
  tControlSet led;
  tControlSet linkmask;
  tControlSet linkspeed;
  tControlSet gtxReset;
  tControlSet gtxLoopback;
  tControlSet gtxRxeqmix;
  tControlSet gtxTxdiffctrl;
  tControlSet gtxTxpreemph;
  tControlSet gtxTxpostemph;
  tControlSet ddlReset;
  tControlSet diuSendCommand;
  tControlSet dmaRateLimit;
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
      {"fan", optional_argument, 0, 'f'},
      {"linkmask", optional_argument, 0, 'm'},
      {"bracketled", optional_argument, 0, 'b'},
      {"device", required_argument, 0, 'n'},
      {"channel", required_argument, 0, 'c'},
      {"listrorcs", no_argument, 0, 'l'},
      {"boardreset", no_argument, &(cmd.boardReset), 1},
      {"linkstatus", no_argument, &(cmd.linkStatus), 1},
      {"linkspeed", optional_argument, 0, 's'},
      {"listlinkspeeds", no_argument, 0, 'S'},
      {"gtxreset", optional_argument, 0, 'R'},
      {"gtxloopback", optional_argument, 0, 'L'},
      {"gtxrxeqmix", optional_argument, 0, 'M'},
      {"gtxtxdiffctrl", optional_argument, 0, 'D'},
      {"gtxtxpreemph", optional_argument, 0, 'P'},
      {"gtxtxpostemph", optional_argument, 0, 'O'},
      {"gtxclearcounters", no_argument, &(cmd.gtxClearCounters), 1},
      {"gtxstatus", no_argument, &(cmd.gtxStatus), 1},
      {"ddlreset", optional_argument, 0, 'd'},
      {"ddlclearcounters", no_argument, &(cmd.ddlClearCounters), 1},
      {"ddlstatus", no_argument, &(cmd.ddlStatus), 1},
      {"diuinitremotesiu", no_argument, &(cmd.diuInitRemoteSiu), 1},
      {"diuinitremotediu", no_argument, &(cmd.diuInitRemoteDiu), 1},
      {"diusendcmd", optional_argument, 0, 'C'},
      {"refclkreset", no_argument, &(cmd.refclkReset), 1},
      {"dmaclearerrorflags", no_argument, &(cmd.dmaClearErrorFlags), 1},
      {"dmaratelimit", optional_argument, 0, 't'},
      {0, 0, 0, 0}};
  int nargs = sizeof(long_options) / sizeof(option);

  /** Parse command line arguments **/
  if (argc > 1) {
    while (1) {
      int opt =
          getopt_long(argc, argv, "hf::lm::b::n:c:s::S::R::L::M::D::P::O::d::C::t::",
                      long_options, NULL);
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
      }
      catch (...) {
        return 0;
      }

      cout << "[" << idx << "] " << setfill('0') << hex << setw(4) << (uint32_t)
          dev->getDomain() << ":" << hex << setw(2) << (uint32_t)
          dev->getBus() << ":" << hex << setw(2) << (uint32_t)
          dev->getSlot() << "." << hex << setw(1) << (uint32_t) dev->getFunc();

      try {
        bar = new librorc::bar(dev, 1);
      }
      catch (...) {
        cout << " - BAR1 access failed!" << endl;
        delete dev;
        return 0;
      }

      try {
        sm = new librorc::sysmon(bar);
      }
      catch (...) {
        cout << " - Sysmon access failed!" << endl;
        delete bar;
        delete dev;
        return 0;
      }

      cout << " " << setw(10) << setfill(' ') << left
           << sm->firmwareDescription() << " Date: "
           << date2string(sm->FwBuildDate()) << ", Rev: 0x" << hex << setw(7)
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
    cerr << "Failed to intialize RORC0" << endl;
    abort();
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

  if (cmd.boardReset) {
    rorc->doBoardReset();
    // TODO
  }

  uint32_t ch_start, ch_end;
  if (cmd.ch == LIBRORC_CH_UNDEF) {
    ch_start = 0;
    ch_end = rorc->m_nchannels - 1;
  } else {
    ch_start = cmd.ch;
    ch_end = cmd.ch;
  }

  if (cmd.linkspeed.get) {
    librorc::refclkopts clkopts = rorc->m_refclk->getCurrentOpts(0);
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      librorc::gtxpll_settings pllsts = rorc->m_gtx[i]->drpGetPllConfig();
      pllsts.refclk = rorc->m_refclk->getFout(clkopts);
      cout << "Ch" << i << " ";
      print_linkpllstate(pllsts);
    }
  } else if (cmd.linkspeed.set) {
    if (cmd.linkspeed.value > nPllCfgs) {
      cout << "ERROR: invalid PLL config selected." << endl;
      return -1;
    }
    librorc::gtxpll_settings pllcfg =
        librorc::gtxpll_supported_cfgs[cmd.linkspeed.value];

    // set QSFPs and GTX to reset
    rorc->setAllQsfpReset(1);
    rorc->setAllGtxReset(1);

    // reconfigure reference clock
    rorc->m_refclk->reset();
    if (pllcfg.refclk != LIBRORC_REFCLK_DEFAULT_FOUT) {
      librorc::refclkopts refclkopts =
          rorc->m_refclk->getCurrentOpts(LIBRORC_REFCLK_DEFAULT_FOUT);
      librorc::refclkopts new_refclkopts =
          rorc->m_refclk->calcNewOpts(pllcfg.refclk, refclkopts.fxtal);
      rorc->m_refclk->writeOptsToDevice(new_refclkopts);
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

  if (cmd.linkStatus) {
      for (uint32_t i = ch_start; i <= ch_end; i++) {
          printLinkStatus(rorc->getLinkStatus(i), i);
      }
  }


  if (cmd.gtxReset.get) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      cout << "Ch" << i << " GTX Reset: " << rorc->m_gtx[i]->getReset() << endl;
    }
  } else if (cmd.gtxReset.set) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      rorc->m_gtx[i]->setReset(cmd.gtxReset.value);
    }
  }

  if (cmd.gtxLoopback.get) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      cout << "Ch" << i << " GTX Loopback: " << rorc->m_gtx[i]->getLoopback()
           << endl;
    }
  } else if (cmd.gtxLoopback.set) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      rorc->m_gtx[i]->setLoopback(cmd.gtxLoopback.value);
    }
  }

  if (cmd.gtxRxeqmix.get) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      cout << "Ch" << i << " GTX RxEqMix: " << rorc->m_gtx[i]->getRxEqMix()
           << endl;
    }
  } else if (cmd.gtxRxeqmix.set) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      rorc->m_gtx[i]->setRxEqMix(cmd.gtxRxeqmix.value);
    }
  }

  if (cmd.gtxTxpreemph.get) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      cout << "Ch" << i << " GTX TxPreEmph: " << rorc->m_gtx[i]->getTxPreEmph()
           << endl;
    }
  } else if (cmd.gtxTxpreemph.set) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      rorc->m_gtx[i]->setTxPreEmph(cmd.gtxTxpreemph.value);
    }
  }

  if (cmd.gtxTxpostemph.get) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      cout << "Ch" << i
           << " GTX TxPostEmph: " << rorc->m_gtx[i]->getTxPostEmph() << endl;
    }
  } else if (cmd.gtxTxpostemph.set) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      rorc->m_gtx[i]->setTxPostEmph(cmd.gtxTxpostemph.value);
    }
  }

  if (cmd.gtxClearCounters) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      rorc->m_gtx[i]->clearErrorCounters();
    }
  }

  if (cmd.gtxStatus){
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      print_gtxstate(i, rorc->m_gtx[i]);
    }
  }

  if (cmd.ddlReset.get) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      cout << "Ch" << i << "DDL Reset: " << rorc->m_ddl[i]->getReset() << endl;
    }
  } else if (cmd.ddlReset.set) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      rorc->m_ddl[i]->setReset(cmd.ddlReset.value);
    }
  }

  if (cmd.ddlClearCounters) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      if (rorc->m_diu[i] != NULL) {
        rorc->m_diu[i]->clearAllLastStatusWords();
        rorc->m_diu[i]->clearDdlDeadtime();
      } else if (rorc->m_siu[i] != NULL) {
        rorc->m_siu[i]->clearLastFrontEndCommandWord();
        rorc->m_siu[i]->clearDdlDeadtime();
      }
      rorc->m_ddl[i]->clearEventcount();
      rorc->m_ddl[i]->clearDmaDeadtime();
    }
  }

  if (cmd.ddlStatus) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      print_ddlstate(i, rorc);
    }
  }

  if (cmd.diuInitRemoteDiu) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      if (rorc->m_diu[i] != NULL) {
        if (rorc->m_diu[i]->prepareForDiuData()) {
          cout << "DIU" << i << " Failed to init remote DIU" << endl;
        }
      } else {
        cout << "Link" << i << " has no local DIU, cannot init remote DIU"
             << endl;
      }
    }
  }

  if (cmd.diuInitRemoteSiu) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      if (rorc->m_diu[i] != NULL) {
        if (rorc->m_diu[i]->prepareForSiuData()) {
          cout << "DIU" << i << " Failed to init remote SIU" << endl;
        }
      } else {
        cout << "Link" << i << " has no local DIU, cannot init remote SIU"
             << endl;
      }
    }
  }

  // note: cmd.diuSendCommand.get is not handled here
  if (cmd.diuSendCommand.set) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      if (rorc->m_diu[i] != NULL) {
        rorc->m_diu[i]->sendCommand(cmd.diuSendCommand.value);
      } else {
        cout << "Link" << i << " has no local DIU, cannot send command" << endl;
      }
    }
  }

  if (cmd.refclkReset) {
    rorc->m_refclk->reset();
  }

  if (cmd.dmaClearErrorFlags) {
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      rorc->m_ch[i]->readAndClearPtrStallFlags();
    }
  }

  if (cmd.dmaRateLimit.get) {
    uint32_t pcie_gen = rorc->m_sm->pcieGeneration();
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      cout << "Link" << i << " Rate Limit: "
           << rorc->m_ch[i]->rateLimit(pcie_gen) << " Hz" << endl;
    }
  }
  else if (cmd.dmaRateLimit.set) {
    uint32_t pcie_gen = rorc->m_sm->pcieGeneration();
    for (uint32_t i = ch_start; i <= ch_end; i++) {
      rorc->m_ch[i]->setRateLimit(cmd.dmaRateLimit.value, pcie_gen);
    }
  }

  if (rorc) {
    delete rorc;
  }
  return 0;
}
