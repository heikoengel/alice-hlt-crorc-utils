/**
 *  crorc_dma_in.cpp
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

#include <iostream>
#include <librorc.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <vector>
#include "event_checker.hh"
#include "file_writer.hh"
#include "fcf_mapping.hh"

using namespace std;

/** Default event buffer size (in Bytes) **/
#ifndef MODELSIM
#define EBUFSIZE (((uint64_t)1) << 30)
#else
#define EBUFSIZE (((uint64_t)1) << 19)
#endif

bool done = false;
enum t_dataSource {
  DS_PG,
  DS_DDR3,
  DS_DIU,
  DS_SIU,
  DS_RAW
};

// Prototypes
bool fileExists(char *filename);
int configureDdl(librorc::event_stream *es, t_dataSource dataSource);
void unconfigureDdl(librorc::event_stream *es, t_dataSource dataSource);
int configureFcf(librorc::event_stream *es, char *tpcRowMappingFile,
                 uint32_t patch, uint32_t rcuVersion);
void unconfigureFcf(librorc::event_stream *es);
int configurePg(librorc::event_stream *es, uint32_t pgSize);
void unconfigurePg(librorc::event_stream *es);
int configureRawReadout(librorc::event_stream *es);
void unconfigureRawReadout(librorc::event_stream *es);
int64_t timediff_us(struct timeval from, struct timeval to);
void printStatusLine(librorc::ChannelStatus *cs_cur,
                     librorc::ChannelStatus *cs_last, int64_t tdiff_us,
                     int error_mask);

// Signal handler
void abort_handler(int s) {
  cerr << "Caught signal " << s << endl;
  if (done == true) {
    exit(-1);
  } else {
    done = true;
  }
}

int main(int argc, char *argv[]) {
  char logdir[] = "/tmp";
  int deviceId = 0;
  int channelId = 0;
  t_dataSource dataSource = DS_DIU;
  char *refFile = NULL;
  char *tpcRowMappingFile = NULL;
  uint32_t pgSize = 0x1000;
  char *dumpDir = NULL;
  uint32_t tpcPatch = 0;
  uint32_t rcuVersion = 1;
  uint32_t pciPacketSize = 0;

  static struct option long_options[] = {
    { "device", required_argument, 0, 'n' },
    { "channel", required_argument, 0, 'c' },
    { "file", required_argument, 0, 'f' },
    { "size", required_argument, 0, 'S' },
    { "source", required_argument, 0, 's' },
    { "fcfmapping", required_argument, 0, 'm' },
    { "tpcpatch", required_argument, 0, 'p' },
    { "dump", required_argument, 0, 'd' },
    { "reffile", required_argument, 0, 'f' },
    { "rcuversion", required_argument, 0, 'r' },
    { "packetsize", required_argument, 0, 'P' },
    { "help", no_argument, 0, 'h' },
    { 0, 0, 0, 0 }
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "n:c:f:S:s:m:p:hd:r:P:", long_options,
                            NULL)) != -1) {
    switch (opt) {
    case 'n':
      deviceId = strtoul(optarg, NULL, 0);
      break;
    case 'c':
      channelId = strtoul(optarg, NULL, 0);
      break;
    case 'f':
      refFile = optarg;
      break;
    case 'p':
      tpcPatch = strtoul(optarg, NULL, 0);
      break;
    case 'r':
      rcuVersion = strtoul(optarg, NULL, 0);
      break;
    case 'P':
      pciPacketSize = strtoul(optarg, NULL, 0);
      break;
    case 's':
      if (strcmp(optarg, "diu") == 0) {
        dataSource = DS_DIU;
      } else if (strcmp(optarg, "siu") == 0) {
        dataSource = DS_SIU;
      } else if (strcmp(optarg, "pg") == 0) {
        dataSource = DS_PG;
      } else if (strcmp(optarg, "ddr3") == 0) {
        dataSource = DS_DDR3;
      } else if (strcmp(optarg, "raw") == 0) {
        dataSource = DS_RAW;
      } else {
        cerr << "Invalid data source: " << optarg << endl
             << "Supported values: diu, pg, ddr3, raw." << endl;
        return -1;
      }
      break;
    case 'S':
      pgSize = strtoul(optarg, NULL, 0);
      break;
    case 'm':
      tpcRowMappingFile = optarg;
      break;
    case 'd':
      dumpDir = optarg;
      break;
    case 'h': {
      cout << argv[0] << " command line parameters: " << endl;
      int long_opt_count = (sizeof(long_options) / sizeof(struct option)) - 1;
      for (int i = 0; i < long_opt_count; i++) {
        struct option cur = long_options[i];
        cout << "  ";
        if (cur.val != 0) {
          cout << "-" << char(cur.val) << "/";
        }
        cout << "--" << cur.name;
        if (cur.has_arg == required_argument) {
          cout << " [arg]";
        } else if (cur.has_arg == optional_argument) {
          cout << " ([arg])";
        }
        cout << endl;
      }
    }
      return 0;
      break;
    default:
      return -1;
      break;
    }
  }

  if (refFile && !fileExists(refFile)) {
    perror("Failed to access reference file: ");
    return -1;
  }

  if (tpcRowMappingFile && !fileExists(tpcRowMappingFile)) {
    perror("Failed to access FCF mapping file: ");
    return -1;
  }

  if (rcuVersion < 1 || rcuVersion > 2) {
    cerr << "Invalid RCU version: " << rcuVersion << ", allowed values: 1, 2"
         << endl;
    return -1;
  }

  file_writer *dumper = NULL;
  if (dumpDir) {
    try {
      dumper = new file_writer(dumpDir, deviceId, channelId, 100);
    }
    catch (int e) {
      cerr << "ERROR initializing file writer: " << e << endl;
      return -1;
    }
  }

  librorc::event_stream *es = NULL;
  try {
    es = new librorc::event_stream(deviceId, channelId,
                                   librorc::kEventStreamToHost);
  }
  catch (int e) {
    cerr << "ERROR: Exception while setting up event stream: "
         << librorc::errMsg(e) << endl;
    return -1;
  }
  int result = es->initializeDma(2 * channelId, EBUFSIZE);
  if (result != 0) {
    cerr << "ERROR: failed to initialize DMA: " << librorc::errMsg(result)
         << endl;
    delete es;
    return -1;
  }

  // wait for DDL clock domain to be ready
  while (!es->m_link->isDdlDomainReady()) {
    usleep(100);
  }
  es->m_link->setFlowControlEnable(0);
  es->m_link->setChannelActive(0);
  es->m_channel->clearEventCount();
  es->m_channel->clearStallCount();
  es->m_channel->readAndClearPtrStallFlags();
  if (pciPacketSize) {
    es->m_channel->setPciePacketSize(pciPacketSize);
  }
  es->m_link->setFlowControlEnable(1);
  es->m_link->setChannelActive(1);

  switch (dataSource) {
  case DS_DIU:
  case DS_SIU:
    if (configureDdl(es, dataSource) != 0) {
      es->m_link->setFlowControlEnable(0);
      es->m_link->setChannelActive(0);
      delete es;
      return -1;
    }
    break;
  case DS_RAW:
    configureRawReadout(es);
    break;
  case DS_PG:
    configurePg(es, pgSize);
    break;
  case DS_DDR3:
    es->m_link->setDataSourceDdr3DataReplay();
    break;
  }

  // check if FCF exists and configure it
  if (configureFcf(es, tpcRowMappingFile, tpcPatch, rcuVersion) < 0) {
    delete es;
    return -1;
  }

  // register signal handler for event loop
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = abort_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  struct timeval tcur, tlast;
  gettimeofday(&tcur, NULL);
  tlast = tcur;
  librorc::ChannelStatus cs_last;
  memset(&cs_last, 0, sizeof(librorc::ChannelStatus));

  // set up event checker
  event_checker *checker = new event_checker(deviceId, channelId, logdir);
  uint32_t check_mask = EC_CHK_SIZES | EC_CHK_DIU_ERR;
  if (refFile) {
    checker->addRefFile(refFile);
    check_mask |= EC_CHK_FILE;
  }
  int error_mask = 0;

  // Main event loop
  while (!done) {
    gettimeofday(&tcur, NULL);
    librorc::EventDescriptor *report;
    const uint32_t *event;
    uint64_t reference;

    if (es->getNextEvent(&report, &event, &reference)) {
      es->updateChannelStatus(report);
      int ret = checker->check(report, event, check_mask);
      if (ret != 0) {
        es->m_channel_status->error_count++;
        error_mask |= ret;
      }
      if (dumper) {
        dumper->dump(report, event);
      }
      es->releaseEvent(reference);
    }

    int64_t tdiff_us = timediff_us(tlast, tcur);
    if (tdiff_us > 1000000) {
      printStatusLine(es->m_channel_status, &cs_last, tdiff_us, error_mask);
      memcpy(&cs_last, es->m_channel_status, sizeof(librorc::ChannelStatus));
      tlast = tcur;
      if (error_mask) {
        error_mask = 0;
      }
    }
  }

  es->m_link->setFlowControlEnable(0);
  es->m_link->setChannelActive(0);
  switch (dataSource) {
  case DS_DIU:
  case DS_SIU:
    unconfigureDdl(es, dataSource);
    break;
  case DS_RAW:
    unconfigureRawReadout(es);
    break;
  case DS_PG:
    unconfigurePg(es);
    break;
  case DS_DDR3:
    break;
  }
  unconfigureFcf(es);
  es->m_link->setDefaultDataSource();

  if (dumper) {
    delete dumper;
  }
  delete checker;
  delete es;
  return 0;
}

int configureDdl(librorc::event_stream *es, t_dataSource dataSource) {
  librorc::diu *diu = es->getDiu();
  if (!diu) {
    cerr << "ERROR: DIU not available for this channel!" << endl;
    return -1;
  }
  diu->useAsDataSource();
  if (dataSource == DS_DIU) {
    if (diu->prepareForDiuData() < 0) {
      cerr << "ERROR: prepareForDiuData failed!" << endl;
      delete diu;
      return -1;
    }
  } else {
    if (diu->prepareForSiuData() < 0) {
      cerr << "ERROR: prepareForSiuData failed!" << endl;
      delete diu;
      return -1;
    }
  }
  diu->setEnable(1);
  if (dataSource == DS_SIU) {
    if (diu->sendFeeReadyToReceiveCmd() < 0) {
      cerr << "ERROR: failed to send RDYRX to FEE!" << endl;
      delete diu;
      return -1;
    }
  }
  delete diu;
  return 0;
}

void unconfigureDdl(librorc::event_stream *es, t_dataSource dataSource) {
  librorc::diu *diu = es->getDiu();
  if (!diu) {
    return;
  }
  if (dataSource == DS_SIU && diu->linkUp()) {
    diu->sendFeeEndOfBlockTransferCmd();
  }
  diu->setEnable(0);
  delete diu;
}

int configureFcf(librorc::event_stream *es, char *tpcRowMappingFile,
                 uint32_t patch, uint32_t rcuVersion) {
  librorc::fastclusterfinder *fcf = es->getFastClusterFinder();
  if (!fcf) {
    // FCF doesn't exist on this channel, so nothing to configure
    return 0;
  }
  fcf->setReset(1);
  fcf->setEnable(0);
  fcf->setBypass((tpcRowMappingFile != NULL) ? 0 : 1);
  fcf->clearErrors();
  if (tpcRowMappingFile) {
    if (patch > 5) {
      cerr << "ERROR: invalid TPC patch: " << patch << endl;
      delete fcf;
      return -1;
    }
    fcf_mapping map = fcf_mapping(patch);
    if (map.readMappingFile(tpcRowMappingFile, rcuVersion) < 0) {
      cerr << "ERROR: Invalid TPC row mapping file: " << tpcRowMappingFile
           << endl;
      delete fcf;
      return -1;
    }
    for (uint32_t i = 0; i < gkConfigWordCnt; i++) {
      fcf->writeMappingRamEntry(i, map[i]);
    }
    fcf->setBypass(0);
  } else {
    fcf->setBypass(1);
  }
  fcf->setSinglePadSuppression(0);
  fcf->setBypassMerger(0);
  fcf->setDeconvPad(1);
  fcf->setSingleSeqLimit(0);
  fcf->setClusterLowerLimit(10);
  fcf->setMergerDistance(4);
  fcf->setMergerAlgorithm(1);
  fcf->setChargeTolerance(0);
  fcf->setNoiseSuppression(0);
  fcf->setNoiseSuppressionMinimum(0);
  fcf->setNoiseSuppressionNeighbor(0);
  fcf->setTagBorderClusters(0);
  fcf->setCorrectEdgeClusters(0);
  if (rcuVersion == 2) {
    fcf->setBranchOverride(1);
  } else {
    fcf->setBranchOverride(0);
  }
  fcf->setReset(0);
  fcf->setEnable(1);
  delete fcf;
  return 0;
}

void unconfigureFcf(librorc::event_stream *es) {
  librorc::fastclusterfinder *fcf = es->getFastClusterFinder();
  if (!fcf) {
    return;
  }
  fcf->setReset(1);
  fcf->setEnable(0);
  delete fcf;
}

int configurePg(librorc::event_stream *es, uint32_t pgSize) {
  librorc::patterngenerator *pg = es->getPatternGenerator();
  if (!pg) {
    return 0;
  }
  pg->disable();
  pg->useAsDataSource();
  pg->configureMode(PG_PATTERN_INC, 0 /*pattern*/, 0 /*#events*/);
  pg->setStaticEventSize(pgSize);
  pg->enable();
  delete pg;
  return 0;
}

void unconfigurePg(librorc::event_stream *es) {
  librorc::patterngenerator *pg = es->getPatternGenerator();
  if (!pg) {
    return;
  }
  pg->disable();
  delete pg;
}

int configureRawReadout(librorc::event_stream *es) {
  librorc::ddl *rawddl = es->getRawReadout();
  if (!rawddl) {
    return 0;
  }
  rawddl->setEnable(1);
  delete rawddl;
  return 0;
}

void unconfigureRawReadout(librorc::event_stream *es) {
  librorc::ddl *rawddl = es->getRawReadout();
  if (!rawddl) {
    return;
  }
  rawddl->setEnable(0);
  delete rawddl;
}

int64_t timediff_us(struct timeval from, struct timeval to) {
  return ((int64_t)(to.tv_sec - from.tv_sec) * 1000000LL +
          (int64_t)(to.tv_usec - from.tv_usec));
}

void printStatusLine(librorc::ChannelStatus *cs_cur,
                     librorc::ChannelStatus *cs_last, int64_t tdiff_us,
                     int error_mask) {
  uint64_t events_diff = cs_cur->n_events - cs_last->n_events;
  float event_rate_khz = (events_diff * 1000000.0) / tdiff_us / 1000.0;
  uint64_t bytes_diff = cs_cur->bytes_received - cs_last->bytes_received;
  float mbytes_rate = (bytes_diff * 1000000.0) / tdiff_us / (float)(1 << 20);
  float total_receiced_GB = (cs_cur->bytes_received / (float)(1 << 30));
  cout.precision(2);
  cout.setf(ios::fixed, ios::floatfield);
  cout << "Ch" << cs_cur->channel << " -  #Events: " << cs_cur->n_events
       << ", Size: " << total_receiced_GB << " GB, ";
  if (events_diff) {
    cout << "Data Rate: " << mbytes_rate
         << " MB/s, Event Rate: " << event_rate_khz << " kHz";
  } else {
    cout << "Data Rate: - , Event Rate: - ";
  }
  cout << ", Errors: " << cs_cur->error_count;
  if (error_mask) {
    cout << " mask: 0x" << hex << error_mask << dec;
  }
  cout << endl;
}

bool fileExists(char *filename) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    return false;
  }
  close(fd);
  return true;
}
