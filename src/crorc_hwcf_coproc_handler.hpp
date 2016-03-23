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

#include <stdlib.h>
#include <stdint.h>
#define LIBRORC_INTERNAL
#include <librorc.h>


class crorc_hwcf_coproc_handler {
public:
  crorc_hwcf_coproc_handler(librorc::device *dev, librorc::bar *bar, int channelId);
  ~crorc_hwcf_coproc_handler();
  int initializeDmaToHost(ssize_t bufferSize);
  int initializeDmaToDevice(ssize_t bufferSize);
  int initializeClusterFinder(const char *tpcMappingFile, uint32_t tpcPatch, uint32_t rcuVersion);

  int enqueueEventToDevice(const char *filename);
  int pollForEventToDeviceCompletion();
  bool pollForEventToHost(librorc::EventDescriptor **report,
                         const uint32_t **event, uint64_t *reference);
  void releaseEventToHost(uint64_t reference);

protected:
  int64_t m_es2host_id;
  int64_t m_es2dev_id;
  uint64_t m_eb2dev_writeptr;
  uint64_t m_eb2dev_readptr;
  librorc::event_stream *m_es2host;
  librorc::event_stream *m_es2dev;
  librorc::fastclusterfinder *m_fcf;
};

#endif
