/**
 *  crorc_hwcf_coproc_handler.hpp
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
#ifndef CRORC_HWCF_COPROC_HANDLER_HPP
#define CRORC_HWCF_COPROC_HANDLER_HPP

#include <list>
#include <stdlib.h>
#include <stdint.h>
#include <zmq.h>
#define LIBRORC_INTERNAL
#include <librorc.h>

struct streamStatus_t {
  uint64_t nInputsQueued;
  uint64_t nInputsDone;
  uint64_t nOutputsQueued;
  uint64_t nOutputsDone;
  uint64_t nRefsQueued;
  uint64_t nRefsDone;
};

class crorc_hwcf_coproc_handler {
public:
  crorc_hwcf_coproc_handler(librorc::device *dev, librorc::bar *bar,
                            int channelId, ssize_t bufferSize);
  ~crorc_hwcf_coproc_handler();
  // int initializeDmaToHost(ssize_t bufferSize);
  // int initializeDmaToDevice(ssize_t bufferSize);
  int initializeClusterFinder(const char *tpcMappingFile, uint32_t tpcPatch,
                              uint32_t rcuVersion);
  int initializeZmq(int port);
  int pollZmq();

  int enqueueEventToDevice(const char *filename);
  int enqueueNextEventToDevice();
  int pollForEventToDeviceCompletion();
  bool pollForEventToHost(librorc::EventDescriptor **report,
                          const uint32_t **event, uint64_t *reference);
  void releaseEventToHost(uint64_t reference);

  void addInputFile(std::string filename);
  void addOutputFile(std::string filename);
  void addRefFile(std::string filename);
  bool inputFilesPending();
  bool outputFilesPending();
  bool refFilesPending();
  const char *nextInputFile() { return m_input_iter->c_str(); };
  const char *nextRefFile() { return m_ref_iter->c_str(); };
  const char *nextOutputFile() { return m_output_iter->c_str(); };

  int writeEventToNextOutputFile(librorc::EventDescriptor *report,
                                 const uint32_t *event);
  int compareEventWithNextRefFile(librorc::EventDescriptor *report,
                                  const uint32_t *event);

protected:
  int64_t m_es2host_id;
  int64_t m_es2dev_id;
  uint64_t m_eb2dev_writeptr;
  uint64_t m_eb2dev_readptr;
  librorc::event_stream *m_es2host;
  librorc::event_stream *m_es2dev;
  librorc::fastclusterfinder *m_fcf;

  std::list<std::string> m_input_file_list;
  std::list<std::string>::iterator m_input_end;
  std::list<std::string>::iterator m_input_iter;

  std::list<std::string> m_output_file_list;
  std::list<std::string>::iterator m_output_end;
  std::list<std::string>::iterator m_output_iter;

  std::list<std::string> m_ref_file_list;
  std::list<std::string>::iterator m_ref_end;
  std::list<std::string>::iterator m_ref_iter;

  void *m_zmq_ctx;
  void *m_zmq_skt;
  zmq_pollitem_t m_zmq_pi;
  int m_stop_rcvd;

  struct streamStatus_t m_status;
};

#endif
