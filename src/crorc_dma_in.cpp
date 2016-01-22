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

using namespace std;

/** Default event buffer size (in Bytes) **/
#ifndef MODELSIM
#define EBUFSIZE (((uint64_t)1) << 30)
#else
#define EBUFSIZE (((uint64_t)1) << 19)
#endif

bool done = false;
enum t_dataSource { DS_PG, DS_DDR3, DS_DIU, DS_SIU, DS_RAW };

// Prototypes
bool fileExists(char *filename);
int configureDdl(librorc::event_stream *es, t_dataSource dataSource);
void unconfigureDdl(librorc::event_stream *es, t_dataSource dataSource);
int configureFcf(librorc::event_stream *es, char *fcfMappingFile);
void unconfigureFcf(librorc::event_stream *es);
int configurePg(librorc::event_stream *es, uint32_t pgSize);
void unconfigurePg(librorc::event_stream *es);
int64_t timediff_us(struct timeval from, struct timeval to);
void printStatusLine(librorc::ChannelStatus *cs_cur,
                     librorc::ChannelStatus *cs_last, int64_t tdiff_us);

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
  char *fcfMappingFile = NULL;
  uint32_t pgSize = 0x1000;
  
  static struct option long_options[] = {
      {"device", required_argument, 0, 'n'},
      {"channel", required_argument, 0, 'c'},
      {"file", required_argument, 0, 'f'},
      {"size", required_argument, 0, 'S'},
      {"source", required_argument, 0, 's'},
      {"fcfmapping", required_argument, 0, 'm'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, "n:c:f:S:s:m:h", long_options, NULL)) !=
         -1) {
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
      fcfMappingFile = optarg;
      break;
    case 'h':
      cout << argv[0] << " command line parameters: " << endl;
      for (int i = 0; i < sizeof(long_options) / sizeof(struct option); i++) {
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

  if (fcfMappingFile && !fileExists(fcfMappingFile)) {
    perror("Failed to access FCF mapping file: ");
    return -1;
  }

  librorc::event_stream *es = NULL;
  try {
    es = new librorc::event_stream(deviceId, channelId,
                                   librorc::kEventStreamToHost);
  } catch (int e) {
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
    // configureRaw(es);
    break;
  case DS_PG:
    configurePg(es, pgSize);
    break;
  case DS_DDR3:
    es->m_link->setDataSourceDdr3DataReplay();
    break;
  }

  // check if FCF exists and configure it
  if (configureFcf(es, fcfMappingFile) < 0) {
    delete es;
    return -1;
  }

  es->m_link->setFlowControlEnable(1);
  es->m_link->setChannelActive(1);

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
  uint32_t check_mask = CHK_SIZES | CHK_DIU_ERR;
  if (refFile) {
    checker->addRefFile(refFile);
    check_mask |= CHK_FILE;
  }

  // Main event loop
  while (!done) {
    gettimeofday(&tcur, NULL);
    librorc::EventDescriptor *report;
    const uint32_t *event;
    uint64_t reference;

    if (es->getNextEvent(&report, &event, &reference)) {
      es->updateChannelStatus(report);
      if (checker->check(report, event, check_mask) != 0) {
	es->m_channel_status->error_count++;
      }
      es->releaseEvent(reference);
    }

    int64_t tdiff_us = timediff_us(tlast, tcur);
    if (tdiff_us > 1000000) {
      printStatusLine(es->m_channel_status, &cs_last, tdiff_us);
      memcpy(&cs_last, es->m_channel_status, sizeof(librorc::ChannelStatus));
      tlast = tcur;
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
    // unconfigureRaw(es);
    break;
  case DS_PG:
    unconfigurePg(es);
    break;
  case DS_DDR3:
    break;
  }
  unconfigureFcf(es);
  es->m_link->setDefaultDataSource();

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

int configureFcf(librorc::event_stream *es, char *fcfMappingFile) {
  librorc::fastclusterfinder *fcf = es->getFastClusterFinder();
  if (!fcf) {
    // FCF doesn't exist on this channel, so nothing to configure
    return 0;
  }
  fcf->setReset(1);
  fcf->setEnable(0);
  fcf->setBypass((fcfMappingFile != NULL) ? 0 : 1);
  fcf->clearErrors();
  if (fcfMappingFile) {
    if (fcf->loadMappingRam(fcfMappingFile) < 0) {
      cerr << "ERROR: failed to load FCF mapping file " << fcfMappingFile
           << endl;
      delete fcf;
      return -1;
    }
  }
  fcf->setSinglePadSuppression(0);
  fcf->setBypassMerger(0);
  fcf->setDeconvPad(1);
  fcf->setSingleSeqLimit(0);
  fcf->setClusterLowerLimit(10);
  fcf->setMergerDistance(4);
  fcf->setMergerAlgorithm(1);
  fcf->setChargeTolerance(0);
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

int64_t timediff_us(struct timeval from, struct timeval to) {
  return ((int64_t)(to.tv_sec - from.tv_sec) * 1000000LL +
          (int64_t)(to.tv_usec - from.tv_usec));
}

void printStatusLine(librorc::ChannelStatus *cs_cur,
                     librorc::ChannelStatus *cs_last, int64_t tdiff_us) {
  uint64_t events_diff = cs_cur->n_events - cs_last->n_events;
  float event_rate_khz = (events_diff * 1000000.0) / tdiff_us / 1000.0;
  uint64_t bytes_diff = cs_cur->bytes_received - cs_last->bytes_received;
  float mbytes_rate = (bytes_diff * 1000000.0) / tdiff_us / (float)(1 << 20);
  float total_receiced_GB = (cs_cur->bytes_received / (float)(1 << 30));
  cout << "Ch" << cs_cur->channel << " -  #Events: " << cs_cur->n_events
       << ", Size: " << total_receiced_GB << " GB, ";
  if (events_diff) {
    cout << "Data Rate: " << mbytes_rate
         << " MB/s, Event Rate: " << event_rate_khz << " kHz";
  } else {
    cout << "Data Rate: - , Event Rate: - ";
  }
  cout << ", Errors: " << cs_cur->error_count << endl;
}

bool fileExists(char *filename) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    return false;
  }
  close(fd);
  return true;
}
