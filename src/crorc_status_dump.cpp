/**
 *  crorc_status_dump.cpp
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
#include <unistd.h>
#include <librorc.h>

using namespace std;

#define HELP_TEXT "Dump the C-RORC status to stdout. \n\
usage: crorc_status_dump -n [DeviceID] \n\
"

#define HEXSTR(x, width) "0x" << setw(width) << setfill('0') << hex << x << setfill(' ') << dec

#define PRINT_SM_REG(reg) __print_reg( reg, bar->get32(reg.addr), "SYSMON", 0)
#define PRINT_DMA_CH_REG(reg, ch) __print_reg( reg, link->pciReg(reg.addr), "DMA", ch)
#define PRINT_GTX_REG(reg, ch) __print_reg( reg, link->gtxReg(reg.addr), "GTX", ch)
#define PRINT_DDL_REG(reg, ch) __print_reg( reg, link->ddlReg(reg.addr), "DDL", ch)
#define REG_ENTRY(reg) {reg, #reg}

uint32_t getVal(uint32_t addr) { return 0; }

struct reg {
  uint32_t addr;
  const char *name;
};

struct reg sm_regs[] = {
    REG_ENTRY(RORC_REG_FIRMWARE_REVISION),
    REG_ENTRY(RORC_REG_FIRMWARE_DATE),
    REG_ENTRY(RORC_REG_FPGA_TEMPERATURE),
    REG_ENTRY(RORC_REG_FPGA_VCCINT),
    REG_ENTRY(RORC_REG_FPGA_VCCAUX),
    REG_ENTRY(RORC_REG_SC_REQ_CANCELED),
    REG_ENTRY(RORC_REG_QSFP_LED_CTRL),
    REG_ENTRY(RORC_REG_UPTIME),
    REG_ENTRY(RORC_REG_FPGA_ID_LOW),
    REG_ENTRY(RORC_REG_FPGA_ID_HIGH),
    REG_ENTRY(RORC_REG_DMA_TX_TIMEOUT),
    REG_ENTRY(RORC_REG_I2C_CONFIG),
    REG_ENTRY(RORC_REG_I2C_OPERATION),
    REG_ENTRY(RORC_REG_ILLEGAL_REQ),
    REG_ENTRY(RORC_REG_MULTIDWREAD),
    REG_ENTRY(RORC_REG_PCIE_CTRL),
    REG_ENTRY(RORC_REG_TYPE_CHANNELS),
    REG_ENTRY(RORC_REG_PCIE_DST_BUSY),
    REG_ENTRY(RORC_REG_BRACKET_LED_CTRL),
    REG_ENTRY(RORC_REG_FAN_CTRL),
    REG_ENTRY(RORC_REG_PCIE_TERR_DROP),
    REG_ENTRY(RORC_REG_UC_SPI_DATA),
    REG_ENTRY(RORC_REG_UC_SPI_CTRL),
    REG_ENTRY(RORC_REG_DDR3_CTRL),
    REG_ENTRY(RORC_REG_DDR3_C0_TESTER_RDCNT),
    REG_ENTRY(RORC_REG_DDR3_C0_TESTER_WRCNT),
    REG_ENTRY(RORC_REG_DDR3_C1_TESTER_RDCNT),
    REG_ENTRY(RORC_REG_DDR3_C1_TESTER_WRCNT),
    REG_ENTRY(RORC_REG_DDR3_MODULE),
    REG_ENTRY(RORC_REG_LVDS_CTRL),
    REG_ENTRY(RORC_REG_FMC_CTRL_LOW),
    REG_ENTRY(RORC_REG_FMC_CTRL_MID),
    REG_ENTRY(RORC_REG_FMC_CTRL_HIGH),
    REG_ENTRY(RORC_REG_DATA_REPLAY_CTRL),
};

struct reg dma_ch_regs[] = {
    REG_ENTRY(RORC_REG_EBDM_N_SG_CONFIG),
    REG_ENTRY(RORC_REG_EBDM_BUFFER_SIZE_L),
    REG_ENTRY(RORC_REG_EBDM_BUFFER_SIZE_H),
    REG_ENTRY(RORC_REG_RBDM_N_SG_CONFIG),
    REG_ENTRY(RORC_REG_RBDM_BUFFER_SIZE_L),
    REG_ENTRY(RORC_REG_RBDM_BUFFER_SIZE_H),
    REG_ENTRY(RORC_REG_EBDM_SW_READ_POINTER_L),
    REG_ENTRY(RORC_REG_EBDM_SW_READ_POINTER_H),
    REG_ENTRY(RORC_REG_RBDM_SW_READ_POINTER_L),
    REG_ENTRY(RORC_REG_RBDM_SW_READ_POINTER_H),
    REG_ENTRY(RORC_REG_DMA_CTRL),
    REG_ENTRY(RORC_REG_DMA_N_EVENTS_PROCESSED),
    REG_ENTRY(RORC_REG_EBDM_FPGA_WRITE_POINTER_H),
    REG_ENTRY(RORC_REG_EBDM_FPGA_WRITE_POINTER_L),
    REG_ENTRY(RORC_REG_RBDM_FPGA_WRITE_POINTER_L),
    REG_ENTRY(RORC_REG_RBDM_FPGA_WRITE_POINTER_H),
    REG_ENTRY(RORC_REG_SGENTRY_ADDR_LOW),
    REG_ENTRY(RORC_REG_SGENTRY_ADDR_HIGH),
    REG_ENTRY(RORC_REG_SGENTRY_LEN),
    REG_ENTRY(RORC_REG_SGENTRY_CTRL),
    REG_ENTRY(RORC_REG_DMA_STALL_CNT),
    REG_ENTRY(RORC_REG_GTX_ASYNC_CFG),
    REG_ENTRY(RORC_REG_DMA_ELFIFO),
    REG_ENTRY(RORC_REG_DMA_PKT_SIZE),
    REG_ENTRY(RORC_REG_GTX_DRP_CTRL),
    REG_ENTRY(RORC_REG_DMA_RATE_LIMITER_WAITTIME),
};

struct reg gtx_regs[] = {
    REG_ENTRY(RORC_REG_GTX_CTRL),
    REG_ENTRY(RORC_REG_GTX_RXDFE),
    REG_ENTRY(RORC_REG_GTX_DISPERR_REALIGN_CNT),
    REG_ENTRY(RORC_REG_GTX_RXNIT_RXLOS_CNT),
    REG_ENTRY(RORC_REG_GTX_ERROR_CNT),
};

struct reg ddl_regs[] = {
    REG_ENTRY(RORC_REG_FCF_LIMITS),
    REG_ENTRY(RORC_REG_FCF_STS_RCU),
    REG_ENTRY(RORC_REG_FCF_STS_CFD),
    REG_ENTRY(RORC_REG_FCF_STS_CFM_A),
    REG_ENTRY(RORC_REG_FCF_STS_CFM_B),
    REG_ENTRY(RORC_REG_FCF_STS_DIV),
    REG_ENTRY(RORC_REG_FCF_MP_TOTAL_TIME),
    REG_ENTRY(RORC_REG_FCF_MP_IDLE_TIME),
    REG_ENTRY(RORC_REG_DDL_CTRL),
    REG_ENTRY(RORC_REG_DDL_PG_EVENT_LENGTH),
    REG_ENTRY(RORC_REG_DDL_PG_PATTERN),
    REG_ENTRY(RORC_REG_DDL_PG_NUM_EVENTS),
    REG_ENTRY(RORC_REG_FCF_CTRL),
    REG_ENTRY(RORC_REG_DDL_CMD),
    REG_ENTRY(RORC_REG_DDL_SERIAL),
    REG_ENTRY(RORC_REG_DDL_EC),
    REG_ENTRY(RORC_REG_DDL_DEADTIME),
    REG_ENTRY(RORC_REG_DDL_CTSTW),
    REG_ENTRY(RORC_REG_DDL_FESTW),
    REG_ENTRY(RORC_REG_DDL_DTSTW),
    REG_ENTRY(RORC_REG_DDL_IFSTW),
    REG_ENTRY(RORC_REG_FCF_RAM_CTRL),
    REG_ENTRY(RORC_REG_FCF_RAM_DATA),
    REG_ENTRY(RORC_REG_DDL_FILTER_CTRL),
    REG_ENTRY(RORC_REG_DDL_DMA_DEADTIME),
    REG_ENTRY(RORC_REG_DDR3_DATA_REPLAY_CHANNEL_CTRL),
    REG_ENTRY(RORC_REG_DDR3_DATA_REPLAY_CHANNEL_STS),
    REG_ENTRY(RORC_REG_FCF_MP_TIMER_IDLE),
    REG_ENTRY(RORC_REG_FCF_MP_NUM_CLUSTERS),
};

void __print_reg(struct reg reg, uint32_t val, const char *prefix, uint32_t ch) {
  cout << "[" << prefix << ch << " " << setw(2) << right << reg.addr << "] " << setw(40)
       << left << reg.name << ": " << right << HEXSTR(val, 8) << endl;
}

void printSgEntry(librorc::dma_channel *ch, uint32_t ram_sel,
                  uint32_t ram_addr) {
  uint64_t sg_addr;
  uint32_t sg_len;
  ch->readSgListEntry(ram_sel, ram_addr, sg_addr, sg_len);
  const char *ramname = (ram_sel) ? "RBDM" : "EBDM";
  cout << ramname << " RAM"
       << " entry " << HEXSTR(ram_addr, 4) << ": " << HEXSTR(sg_addr, 16) << " "
       << HEXSTR(sg_len, 8) << endl;
}

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
  } catch (int e) {
    cout << "Failed to intialize device " << device_number << ": "
         << librorc::errMsg(e) << endl;
    return -1;
  }

  /** Instantiate a new bar */
  librorc::bar *bar = NULL;
  try {
    bar = new librorc::bar(dev, 1);
  } catch (int e) {
    cout << "ERROR: failed to initialize BAR:" << librorc::errMsg(e) << endl;
    delete dev;
    return -1;
  }

  /** Instantiate a new sysmon */
  librorc::sysmon *sm;
  try {
    sm = new librorc::sysmon(bar);
  } catch (...) {
    cout << "Sysmon init failed!" << endl;
    delete bar;
    delete dev;
    return -1;
  }

  /** print system monitor registers */
  for (int i = 0; i < sizeof(sm_regs) / sizeof(reg); i++) {
    PRINT_SM_REG(sm_regs[i]);
  }

  uint32_t nChannels = sm->numberOfChannels();
  for (uint32_t chId = 0; chId < nChannels; chId++) {
    librorc::link *link = new librorc::link(bar, chId);

    /** print DMA registers */
    for (int i = 0; i < sizeof(dma_ch_regs) / sizeof(reg); i++) {
      PRINT_DMA_CH_REG(dma_ch_regs[i], chId);
    }

    if (link->isGtxDomainReady()) {
      /** print GTX registers */
      for (int i = 0; i < sizeof(gtx_regs) / sizeof(reg); i++) {
        PRINT_GTX_REG(gtx_regs[i], chId);
      }
    }

    if (link->isDdlDomainReady()) {
      /** print DDL registers */
      for (int i = 0; i < sizeof(ddl_regs) / sizeof(reg); i++) {
        PRINT_DDL_REG(ddl_regs[i], chId);
      }
    }

    librorc::dma_channel *ch = new librorc::dma_channel(link);
    uint32_t ebdmNSgEntries = ch->getEBDMNumberOfSgEntries();
    uint32_t rbdmNSgEntries = ch->getRBDMNumberOfSgEntries();

    /** print EBDM sglist */
    for (uint32_t i = 0; i <= ebdmNSgEntries; i++) {
      printSgEntry(ch , 0, i);
    }
    /** print RBDM sglist */
    for (uint32_t i = 0; i <= rbdmNSgEntries; i++) {
      printSgEntry(ch , 1, i);
    }

    delete ch;
    delete link;
  }

  delete bar;
  delete dev;
  return 0;
}
