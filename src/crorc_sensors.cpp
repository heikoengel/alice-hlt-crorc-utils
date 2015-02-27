/**
 *  crorc_sensors.cpp
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
#include <string>
#include <sstream>
#include <cstdio>
#include <iomanip>

#include <librorc.h>

#define HELP_TEXT                                                              \
  "crorc_sensors usage: \n"                                                    \
  "  -h|--help            print this help text\n"                              \
  "  -d|--device [id]     select target C-RORC device. Uses device 0 if not\n" \
  "                       specified\n"                                         \
  "  -l|--list            list all available sensors\n"                        \
  "  -a|--all             print all sensor values\n"                           \
  "  -q|--qsfp [id]       select QSFP module. [id] can be 0..2\n"              \
  "  --fpga_temp          print FPGA Temperature in Degree Celsius\n"          \
  "  --fpga_fw_rev        print FPGA Firmware Revision\n"                      \
  "  --fpga_fw_date       print FPGA Firmware Date\n"                          \
  "  --fpga_uptime        print FPGA Uptime in Seconds\n"                      \
  "  --fpga_fw_descr      print Firmware Description string\n"                 \
  "  --fpga_vcc_int       print FPGA Core Voltage in V\n"                      \
  "  --fpga_vcc_aux       print FPGA Auxilliary Voltage in V\n"                \
  "  --fan_speed          print FPGA Fan RPM or 0 if stopped\n"                \
  "  --pcie_slot          print PCIe Domain:Bus:Slot.Funtion numbers\n"        \
  "  --pcie_lanes         print number of PCIe lanes used for C-RORC\n"        \
  "  --pcie_gen           print PCIe Generation used for C-RORC\n"             \
  "  --pcie_tx_error      print PCIe TX Error Count\n"                         \
  "  --pcie_ill_req       print PCIe Illegal Request Count\n"                  \
  "  --smbus_slv_addr     print SMBus Slave Address for Configuration\n"       \
  "                       Controller access\n"                                 \
  "  --num_qsfps          print number of QSFP modules installed\n"            \
  "  --num_dma_ch         print number of DMA channels supported by\n"         \
  "                       firmware\n"                                          \
  "  --num_ddls           print number of DDLs supported by firmware\n"        \
  "  --qsfp_temp          print QSFP Module temperature. requires --qsfp\n"    \
  "  --ddr_ctrl0_bitrate  print DDR3 Controller 0 bitrate\n"                   \
  "  --ddr_mod0_available check if DDR3 module is installed in socket 0\n"     \
  "  --ddr_mod1_available check if DDR3 module is installed in socket 1\n"     \
  "\n"

std::string composeFormat(const char *name, const char *fmt, int withName) {
  std::string full_fmt = "";
  if (withName) {
    full_fmt += "%s: ";
  }
  full_fmt += std::string(fmt) + "\n";
  return full_fmt;
}

#define PRINT_METRIC(name, fmt, value, withName)                               \
  std::string full_fmt = composeFormat(name, fmt, withName);                   \
  if (withName) {                                                              \
    printf(full_fmt.c_str(), name, value);                                     \
  } else {                                                                     \
    printf(full_fmt.c_str(), value);                                           \
  }

int main(int argc, char *argv[]) {
  int sAll = 0;
  int sFpgaTemp = 0;
  int sFpgaFwDate = 0;
  int sFpgaFwRev = 0;
  int sFpgaUptime = 0;
  int sFpgaFwDesrc = 0;
  int sFpgaVccInt = 0;
  int sFpgaVccAux = 0;
  int sFpgaSysclkActive = 0;
  int sFpgaFan = 0;
  int sPcieSlot = 0;
  int sPcieLanes = 0;
  int sPcieGen = 0;
  int sPcieTxErr = 0;
  int sPcieIllReq = 0;
  int sSmbusSlvAddr = 0;
  int sNumQsfps = 0;
  int sQsfpTemp = 0;
  int sNumDmaCh = 0;
  int sDdrCtrl0Bitrate = 0;
  int sDdrCtrl1Bitrate = 0;
  int sDdrMod0Available = 0;
  int sDdrMod1Available = 0;
  uint32_t deviceId = 0;
  uint32_t qsfpId = 0xffffffff;

  static struct option long_options[] = {
    { "help", no_argument, 0, 'h' },
    { "device", required_argument, 0, 'd' },
    { "list", no_argument, 0, 'l' },
    { "all", no_argument, 0, 'a' },
    { "qsfp", required_argument, 0, 'q' },
    { "fpga_temp", no_argument, &sFpgaTemp, 1 },
    { "fpga_fw_rev", no_argument, &sFpgaFwRev, 1 },
    { "fpga_fw_date", no_argument, &sFpgaFwDate, 1 },
    { "fpga_uptime", no_argument, &sFpgaUptime, 1 },
    { "fpga_fw_descr", no_argument, &sFpgaFwDesrc, 1 },
    { "fpga_vcc_int", no_argument, &sFpgaVccInt, 1 },
    { "fpga_vcc_aux", no_argument, &sFpgaVccAux, 1 },
    { "fpga_sysclk_active", no_argument, &sFpgaSysclkActive, 1 },
    { "fan_speed", no_argument, &sFpgaFan, 1 },
    { "pcie_slot", no_argument, &sPcieSlot, 1 },
    { "pcie_lanes", no_argument, &sPcieLanes, 1 },
    { "pcie_gen", no_argument, &sPcieGen, 1 },
    { "pcie_tx_error", no_argument, &sPcieTxErr, 1 },
    { "pcie_ill_req", no_argument, &sPcieIllReq, 1 },
    { "smbus_slv_addr", no_argument, &sSmbusSlvAddr, 1 },
    { "num_qsfps", no_argument, &sNumQsfps, 1 },
    { "num_dma_ch", no_argument, &sNumDmaCh, 1 },
    { "ddr_ctrl0_bitrate", no_argument, &sDdrCtrl0Bitrate, 1 },
    { "ddr_ctrl1_bitrate", no_argument, &sDdrCtrl1Bitrate, 1 },
    { "ddr_mod0_available", no_argument, &sDdrMod0Available, 1 },
    { "ddr_mod1_available", no_argument, &sDdrMod1Available, 1 },
    { "qsfp_temp", no_argument, &sQsfpTemp, 1 },
    { 0, 0, 0, 0 }
  };

  /** Parse command line arguments **/
  if (argc > 1) {
    while (1) {
      int opt = getopt_long(argc, argv, "hld:aq:", long_options, NULL);
      if (opt == -1) {
        break;
      }

      switch (opt) {
      case 'h':
        printf(HELP_TEXT);
        return 0;
      case 'd':
        deviceId = strtol(optarg, NULL, 0);
        break;
      case 'a':
        sAll = 1;
        break;
      case 'q':
        qsfpId = strtol(optarg, NULL, 0);
        break;
      case 'l': {
        int iter = 0;
        while (long_options[iter].name != 0) {
          if (long_options[iter].val == 1) {
            printf("--%s\n", long_options[iter].name);
          }
          iter++;
        }
        return 0;
      }
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

  librorc::device *dev = NULL;
  try {
    dev = new librorc::device(deviceId);
  }
  catch (...) {
    std::cerr << "Failed to initialize device " <<  deviceId << std::endl;
    return -1;
  }

  librorc::bar *bar = NULL;
  try {
    bar = new librorc::bar(dev, 1);
  }
  catch (...) {
    std::cerr << "Failed to initialize BAR 1" << std::endl;
    delete dev;
    return -1;
  }

  librorc::sysmon *sm = NULL;
  try {
    sm = new librorc::sysmon(bar);
  }
  catch (...) {
    std::cerr << "Failed to initialize SystemMonitor" << std::endl;
    delete bar;
    delete dev;
    return -1;
  }

  if (sFpgaTemp || sAll) {
    PRINT_METRIC("fpga_temp", "%d", (uint32_t)sm->FPGATemperature(), sAll);
  }
  if (sFpgaFwRev || sAll) {
    PRINT_METRIC("fpga_fw_rev", "0x%08x", sm->FwRevision(), sAll);
  }
  if (sFpgaFwDate || sAll) {
    PRINT_METRIC("fpga_fw_date", "%08x", sm->FwBuildDate(), sAll);
  }
  if (sFpgaUptime || sAll) {
    PRINT_METRIC("fpga_uptime", "%ld", sm->uptimeSeconds(), sAll);
  }
  if (sFpgaFwDesrc || sAll) {
    PRINT_METRIC("fpga_fw_descr", "%s", sm->firmwareDescription(), sAll);
  }
  if (sFpgaVccInt || sAll) {
    PRINT_METRIC("fpga_vcc_int", "%.2f", sm->VCCINT(), sAll);
  }
  if (sFpgaVccAux || sAll) {
    PRINT_METRIC("fpga_vcc_aux", "%.2f", sm->VCCAUX(), sAll);
  }
  if (sFpgaSysclkActive || sAll) {
    PRINT_METRIC("fpga_sysclk_active", "%d",
                 (uint32_t)sm->systemClockIsRunning(), sAll);
  }
  if (sFpgaFan || sAll) {
    uint32_t fan_speed =
        (sm->systemFanIsRunning()) ? (uint32_t)sm->systemFanSpeed() : 0;
    PRINT_METRIC("fan_speed", "%d", fan_speed, sAll);
  }
  if (sPcieSlot || sAll) {
    uint32_t domain = (uint32_t)dev->getDomain();
    uint32_t bus = (uint32_t)dev->getBus();
    uint32_t slot = (uint32_t)dev->getSlot();
    uint32_t func = (uint32_t)dev->getFunc();

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(4) << domain << ":"
       << std::setw(2) << bus << ":" << std::setw(2) << slot << "."
       << std::setw(1) << func;
    std::string pcie_slot = ss.str();
    PRINT_METRIC("pcie_slot", "%s", pcie_slot.c_str(), sAll);
  }
  if (sPcieLanes || sAll) {
    PRINT_METRIC("pcie_lanes", "%d", sm->pcieNumberOfLanes(), sAll);
  }
  if (sPcieGen || sAll) {
    PRINT_METRIC("pcie_gen", "%d", sm->pcieGeneration(), sAll);
  }
  if (sSmbusSlvAddr || sAll) {
    PRINT_METRIC("smbus_slv_addr", "0x%02x", sm->dipswitch(), sAll);
  }
  if (sPcieTxErr || sAll) {
    PRINT_METRIC("pcie_tx_error", "%d", sm->pcieTransmissionErrorCounter(),
                 sAll);
  }
  if (sPcieIllReq || sAll) {
    PRINT_METRIC("pcie_ill_req", "%d", sm->pcieIllegalRequestCounter(), sAll);
  }
  if (sNumQsfps || sAll) {
    uint32_t num_qsfps = 0;
    for (int i = 0; i < LIBRORC_MAX_QSFP; i++) {
      if (sm->qsfpIsPresent(i)) {
        num_qsfps++;
      }
    }
    PRINT_METRIC("num_qsfps", "%d", num_qsfps, sAll);
  }
  if (sNumDmaCh) {
    PRINT_METRIC("num_dma_ch", "%d", sm->numberOfChannels(), sAll);
  }

  if (sDdrCtrl0Bitrate || sAll) {
    PRINT_METRIC("ddr_ctrl0_bitrate", "%d", sm->ddr3Bitrate(0), sAll);
  }
  if (sDdrCtrl1Bitrate || sAll) {
    PRINT_METRIC("ddr_ctrl1_bitrate", "%d", sm->ddr3Bitrate(1), sAll);
  }

  if (sDdrMod0Available || sAll) {
    int mod_available = 1;
    try {
      sm->ddr3SpdRead(0, 0x03);
    }
    catch (...) {
      mod_available = 0;
    }
    PRINT_METRIC("ddr_mod0_available", "%d", mod_available, sAll);
  }

  if (sDdrMod1Available || sAll) {
    int mod_available = 1;
    try {
      sm->ddr3SpdRead(1, 0x03);
    }
    catch (...) {
      mod_available = 0;
    }
    PRINT_METRIC("ddr_mod1_available", "%d", mod_available, sAll);
  }

  if (sQsfpTemp) {
    if (qsfpId >= LIBRORC_MAX_QSFP) {
      std::cerr << "No or invalid QSFP module selected. Use --qsfp [0-2]."
                << std::endl;
      return -1;
    } else if (!sm->qsfpIsPresent(qsfpId)) {
      std::cerr << "Selected QSFP module not found." << std::endl;
      return -1;
    } else {
      PRINT_METRIC("qsfp_temp", "%d", (uint32_t)sm->qsfpTemperature(qsfpId),
                   sAll);
    }
  }

  if (sm) {
    delete sm;
  }
  if (bar) {
    delete bar;
  }
  if (dev) {
    delete dev;
  }

  return 0;
}
