/**
 *  crorc_hwcf_coproc_handler.cpp
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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "crorc_hwcf_coproc_handler.hpp"
#include "fcf_mapping.hh"

#define ZMQ_BUF_SIZE (3 * 4096)

crorc_hwcf_coproc_handler::crorc_hwcf_coproc_handler(librorc::device *dev,
                                                     librorc::bar *bar,
                                                     int channelId,
                                                     ssize_t bufferSize) {
  m_es2host = NULL;
  m_es2dev = NULL;
  m_fcf = NULL;
  m_eb2dev_writeptr = 0;
  m_eb2dev_readptr = 0;
  m_es2dev_id = channelId;
  m_zmq_skt = NULL;
  m_zmq_ctx = NULL;
  m_status.nInputsQueued = 0;
  m_status.nInputsDone = 0;
  m_status.nOutputsQueued = 0;
  m_status.nOutputsDone = 0;
  m_status.nRefsQueued = 0;
  m_status.nRefsDone = 0;
  m_status.stopReceived = false;

  m_es2dev = new librorc::event_stream(dev, bar, m_es2dev_id,
                                       librorc::kEventStreamToDevice);
  m_es2host_id = m_es2dev->m_sm->numberOfChannels() / 2 + channelId;

  m_es2host = new librorc::event_stream(dev, bar, m_es2host_id,
                                        librorc::kEventStreamToHost);
  // initialize DMA to host
  m_es2host->m_channel->clearEventCount();
  m_es2host->m_channel->clearStallCount();
  int result = m_es2host->initializeDma(2 * m_es2host_id, bufferSize);
  if (result) {
    throw result;
  }

  // initialize DMA to device
  m_es2dev->overridePciePacketSize(128);
  m_es2dev->m_channel->clearEventCount();
  m_es2dev->m_channel->clearStallCount();
  result = m_es2dev->initializeDma(2 * m_es2dev_id, bufferSize);
  if (result) {
    throw result;
  }
  m_eb2dev_readptr = m_es2dev->m_eventBuffer->size();
}

crorc_hwcf_coproc_handler::~crorc_hwcf_coproc_handler() {
  if (m_zmq_skt) {
    zmq_close(m_zmq_skt);
  }
  if (m_zmq_ctx) {
    zmq_term(m_zmq_ctx);
  }
  if (m_fcf) {
    delete m_fcf;
  }
  if (m_es2host) {
    delete m_es2host;
  }
  if (m_es2dev) {
    delete m_es2dev;
  }
}

int crorc_hwcf_coproc_handler::initializeClusterFinder(
    const char *tpcRowMappingFile, uint32_t tpcPatch, uint32_t rcuVersion) {

  if (tpcPatch > 5 || rcuVersion < 1 || rcuVersion > 2) {
    errno = EINVAL;
    return -1;
  }

#ifdef MODELSIM
  while (!m_es2host->m_link->isDdlDomainReady())
    ;
  while (!m_es2dev->m_link->isDdlDomainReady())
    ;
#endif
  m_fcf = m_es2host->getFastClusterFinder();
  if (!m_fcf) {
    errno = ENODEV;
    return -1;
  }

  m_es2dev->m_link->setChannelActive(0);
  m_es2dev->m_link->setFlowControlEnable(1);

  m_es2host->m_link->setChannelActive(0);
  m_es2host->m_link->setFlowControlEnable(1);

  m_fcf->setReset(1);
  m_fcf->setEnable(0);
  m_fcf->setBypass(0);
  m_fcf->clearErrors();

  fcf_mapping map = fcf_mapping(tpcPatch);
  if (map.readMappingFile(tpcRowMappingFile, rcuVersion)) {
    return -1;
  }
  for (uint32_t i = 0; i < gkConfigWordCnt; i++) {
    m_fcf->writeMappingRamEntry(i, map[i]);
  }

  m_fcf->setSinglePadSuppression(0);
  m_fcf->setBypassMerger(0);
  m_fcf->setDeconvPad(1);
  m_fcf->setSingleSeqLimit(0);
  m_fcf->setClusterLowerLimit(10);
  m_fcf->setMergerDistance(4);
  m_fcf->setMergerAlgorithm(1);
  m_fcf->setChargeTolerance(0);

  if (rcuVersion == 2) {
    m_fcf->setBranchOverride(1);
  } else {
    m_fcf->setBranchOverride(0);
  }

  m_fcf->setReset(0);
  m_fcf->setEnable(1);
  return 0;
}

int crorc_hwcf_coproc_handler::enqueueEventToDevice(const char *filename) {
  // open DDL file
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return errno;
  }

  // map event data
  struct stat fd_stat;
  fstat(fd, &fd_stat);
  void *event_in = mmap(NULL, fd_stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (event_in == MAP_FAILED) {
    return errno;
  }
  if (fd_stat.st_size < 0) {
    return EIO;
  }

  uint64_t buffersize = m_es2dev->m_eventBuffer->size();
  // check if file fits into buffer at all
  if ((uint64_t)fd_stat.st_size > buffersize) {
    return EFBIG;
  }

  // check if file fits into free buffer
  uint64_t free_buffer =
      (m_eb2dev_writeptr > m_eb2dev_readptr)
          ? (buffersize - m_eb2dev_writeptr + m_eb2dev_readptr)
          : (m_eb2dev_readptr - m_eb2dev_writeptr);
  if ((uint64_t)fd_stat.st_size > free_buffer) {
    munmap(event_in, fd_stat.st_size);
    return EAGAIN;
  }

  // copy event to buffer
  char *event_dst =
      (char *)m_es2dev->m_eventBuffer->getMem() + m_eb2dev_writeptr;
  memcpy(event_dst, event_in, fd_stat.st_size);
  munmap(event_in, fd_stat.st_size);

  // create scatter-gather-list
  std::vector<librorc::ScatterGatherEntry> sglist;
  if (m_es2dev->m_eventBuffer->composeSglistFromBufferSegment(
          m_eb2dev_writeptr, fd_stat.st_size, &sglist) == false) {
    return EINVAL;
  }

  // check if scatter-gather-list fits into DMA channel FIFO at all
  uint32_t outFifoDepth = m_es2dev->m_channel->outFifoDepth();
  if (outFifoDepth < sglist.size()) {
    return EIO;
  }

  // check if scatter-gather-list fits into DMA channel FIFO
  uint32_t availFifoEntries =
      outFifoDepth - m_es2dev->m_channel->outFifoFillState();
  if (sglist.size() > availFifoEntries) {
    return EAGAIN;
  }

  // announce event to C-RORC
  m_es2dev->m_channel->announceEvent(sglist);

  // adjust write pointer
  m_eb2dev_writeptr += fd_stat.st_size;
  if (m_eb2dev_writeptr >= buffersize) {
    m_eb2dev_writeptr -= buffersize;
  }

  return 0;
}

int crorc_hwcf_coproc_handler::enqueueNextEventToDevice() {
  int result = enqueueEventToDevice(m_input_iter->c_str());
  if (result) {
    return result;
  }
  m_input_iter = m_input_file_list.erase(m_input_iter);
  m_status.nInputsDone++;
  return 0;
}

int crorc_hwcf_coproc_handler::pollForEventToDeviceCompletion() {
  librorc::EventDescriptor *report = NULL;
  uint64_t librorcEventReference = 0;
  const uint32_t *event = NULL;
  if (!m_es2dev->getNextEvent(&report, &event, &librorcEventReference)) {
    return EAGAIN;
  }
  m_es2dev->updateChannelStatus(report);
  m_eb2dev_readptr =
      report->offset + ((report->calc_event_size & 0x3fffffff) << 2);
  if (m_es2dev->releaseEvent(librorcEventReference) != 0) {
    printf("pollForEventToDeviceCompletion: failed to release reference %ld\n",
           librorcEventReference);
  }
  uint64_t buffersize = m_es2dev->m_eventBuffer->size();
  if (m_eb2dev_readptr >= buffersize) {
    m_eb2dev_readptr -= buffersize;
  }
  return 0;
}

bool crorc_hwcf_coproc_handler::pollForEventToHost(
    librorc::EventDescriptor **report, const uint32_t **event,
    uint64_t *reference) {
  bool result = m_es2host->getNextEvent(report, event, reference);
  if (result) {
    m_es2host->updateChannelStatus(*report);
  }
  return result;
}

void crorc_hwcf_coproc_handler::releaseEventToHost(uint64_t reference) {
  if (m_es2host->releaseEvent(reference) != 0) {
    printf("failed to release reference %ld\n", reference);
  }
}

void crorc_hwcf_coproc_handler::addInputFile(std::string filename) {
  m_input_file_list.push_back(filename);
  m_input_iter = m_input_file_list.begin();
  m_input_end = m_input_file_list.end();
  m_status.nInputsQueued++;
}

void crorc_hwcf_coproc_handler::addOutputFile(std::string filename) {
  m_output_file_list.push_back(filename);
  m_output_iter = m_output_file_list.begin();
  m_output_end = m_output_file_list.end();
  m_status.nOutputsQueued++;
}

void crorc_hwcf_coproc_handler::addRefFile(std::string filename) {
  m_ref_file_list.push_back(filename);
  m_ref_iter = m_ref_file_list.begin();
  m_ref_end = m_ref_file_list.end();
  m_status.nRefsQueued++;
}

bool crorc_hwcf_coproc_handler::inputFilesPending() {
  return (m_input_iter != m_input_end);
}

bool crorc_hwcf_coproc_handler::outputFilesPending() {
  return (m_output_iter != m_output_end);
}

bool crorc_hwcf_coproc_handler::refFilesPending() {
  return (m_ref_iter != m_ref_end);
}

int crorc_hwcf_coproc_handler::writeEventToNextOutputFile(
    librorc::EventDescriptor *report, const uint32_t *event) {
  const char *filename = m_output_iter->c_str();
  int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return -1;
  }
  uint32_t dmaWords = (report->calc_event_size & 0x3fffffff);
  ssize_t nbytes = write(fd, event, (dmaWords << 2));
  close(fd);
  if (nbytes == -1) {
    return -1;
  } else if (nbytes != (dmaWords << 2)) {
    return -1;
  }
  m_output_iter = m_output_file_list.erase(m_output_iter);
  m_status.nOutputsDone++;
  return 0;
}

int crorc_hwcf_coproc_handler::compareEventWithNextRefFile(
    librorc::EventDescriptor *report, const uint32_t *event) {
  // open DDL file
  const char *filename = m_ref_iter->c_str();
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  // map reference data
  struct stat fd_stat;
  fstat(fd, &fd_stat);
  void *event_ref = mmap(NULL, fd_stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (event_ref == MAP_FAILED) {
    return -1;
  }
  uint32_t dmaWords = (report->calc_event_size & 0x3fffffff);
  ssize_t eventSize = (dmaWords << 2);

  if (fd_stat.st_size != eventSize) {
    munmap(event_ref, fd_stat.st_size);
    errno = EFBIG;
    return -1;
  }

  int result = memcmp(event, event_ref, eventSize);
  munmap(event_ref, fd_stat.st_size);
  if (result != 0) {
    errno = EILSEQ;
    return -1;
  }
  m_ref_iter = m_ref_file_list.erase(m_ref_iter);
  m_status.nRefsDone++;
  return 0;
}

int crorc_hwcf_coproc_handler::initializeZmq(int port) {
  m_zmq_ctx = zmq_ctx_new();
  m_zmq_skt = zmq_socket(m_zmq_ctx, ZMQ_PULL);
  if (!m_zmq_skt) {
    return -1;
  }
  char zmq_bind_addr[1024];
  snprintf(zmq_bind_addr, 1024, "tcp://*:%d", port);
  if (zmq_bind(m_zmq_skt, zmq_bind_addr)) {
    return -1;
  }
  m_zmq_pi.socket = m_zmq_skt;
  m_zmq_pi.events = ZMQ_POLLIN;
  return 0;
}

int crorc_hwcf_coproc_handler::pollZmq() {
  if (!m_zmq_ctx || !m_zmq_skt) {
    return -1;
  }
  int zmq_ret = zmq_poll(&m_zmq_pi, 1, 0);
  if (zmq_ret < 0) {
    return -1;
  } else if (zmq_ret > 0) {
    char zmq_buffer[ZMQ_BUF_SIZE];
    memset(zmq_buffer, 0, ZMQ_BUF_SIZE);
    int ret = zmq_recv(m_zmq_skt, zmq_buffer, ZMQ_BUF_SIZE, 0);
    if (ret == -1) {
      return -1;
    }
    std::string rcvd = std::string(zmq_buffer);
    if (rcvd.compare(";;;") == 0) {
      m_status.stopReceived = true;
      return 0;
    }
    size_t found = rcvd.find_first_of(";");
    if (found == std::string::npos) {
      return -1;
    }
    std::string inputfile = rcvd.substr(0, found);
    rcvd = rcvd.substr(found + 1, -1);
    found = rcvd.find_first_of(";");
    if (found == std::string::npos) {
      return -1;
    }
    std::string outputfile = rcvd.substr(0, found);
    std::string reffile = rcvd.substr(found + 1, -1);

    addInputFile(inputfile);
    if (!outputfile.empty()) {
      addOutputFile(outputfile);
    }
    if (!reffile.empty()) {
      addRefFile(reffile);
    }
  }
  return 0;
}

struct streamStatus_t crorc_hwcf_coproc_handler::getStatus() {
  struct streamStatus_t statuscopy;
  memcpy(&statuscopy, &m_status, sizeof(struct streamStatus_t));
  return statuscopy;
}

bool crorc_hwcf_coproc_handler::isDone() {
  return m_status.stopReceived && !inputFilesPending() &&
         !outputFilesPending() && !refFilesPending();
}
