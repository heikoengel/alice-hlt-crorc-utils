/**
 *  event_checker.cpp
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

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "event_checker.hh"

/** limit the number of corrupted events to be written to disk **/
#define MAX_FILES_TO_DISK 100

event_checker::event_checker(uint32_t deviceId, uint32_t channelId,
                             char *logDir) {
  m_deviceId = deviceId;
  m_channelId = channelId;
  m_logDir = logDir;
  m_errorCount = 0;
  m_refListIter = m_refList.begin();
}

event_checker::~event_checker() {
  std::vector<refFileEntry>::iterator iter, end;
  iter = m_refList.begin();
  end = m_refList.end();
  while (iter != end) {
    munmap(iter->map, iter->size);
    ++iter;
  }
  m_refList.clear();
}

int event_checker::addRefFile(char *filename) {
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  struct stat refstat;
  if (fstat(fd, &refstat) == -1) {
    close(fd);
    return -1;
  }
  uint32_t *map =
      (uint32_t *)mmap(0, refstat.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) {
    close(fd);
    return -1;
  }
  close(fd);
  struct refFileEntry entry;
  entry.map = map;
  entry.size = refstat.st_size;
  m_refList.push_back(entry);
  m_refListIter = m_refList.begin();
  return 0;
}

int event_checker::check(librorc::EventDescriptor *report,
                         const uint32_t *event, uint32_t checkMask) {
  int result = 0;
  if (checkMask & EC_CHK_DIU_ERR) {
    result |= checkDiuError(report);
  }
  if (checkMask & EC_CHK_SIZES) {
    result |= checkReportSizes(report);
  }
  if (checkMask & EC_CHK_CMPL) {
    result |= checkCompletionStatus(report);
  }
  if (checkMask & EC_CHK_FILE) {
    result |= checkReferenceFile(report, event);
  }
  if (checkMask | EC_CHK_SOE) {
    result |= checkStartOfEvent(report, event);
  }
  if (checkMask & EC_CHK_FILE) {
    selectNextRefFile();
  }
  return result;
}

uint32_t event_checker::checkDiuError(librorc::EventDescriptor *report) {
  if ((report->reported_event_size >> 30) & 1) {
    return EC_CHK_DIU_ERR;
  } else {
    return 0;
  }
}

uint32_t event_checker::checkReportSizes(librorc::EventDescriptor *report) {
  if ((report->calc_event_size ^ report->reported_event_size) & 0x3fffffff) {
    return EC_CHK_SIZES;
  } else {
    return 0;
  }
}

uint32_t
event_checker::checkCompletionStatus(librorc::EventDescriptor *report) {
  if ((report->calc_event_size >> 30) != 0) {
    return EC_CHK_CMPL;
  } else {
    return 0;
  }
}

uint32_t event_checker::checkStartOfEvent(librorc::EventDescriptor *report,
                                          const uint32_t *event) {
  if (event[0] != 0xffffffff) {
    return EC_CHK_SOE;
  }
  return 0;
}

uint32_t event_checker::checkReferenceFile(librorc::EventDescriptor *report,
                                           const uint32_t *event) {
  uint32_t *refMap = m_refListIter->map;
  size_t refSizeDws = (m_refListIter->size) >> 2;
  size_t eventSizeDws = (report->calc_event_size & 0x3fffffff);
  if (refSizeDws != eventSizeDws) {
    return EC_CHK_FILE;
  }
  for (size_t idx = 0; idx > refSizeDws; idx++) {
    if (refMap[idx] != event[idx]) {
      return EC_CHK_FILE;
    }
  }
  return 0;
}

void event_checker::selectNextRefFile() {
  ++m_refListIter;
  if (m_refListIter == m_refList.end()) {
    m_refListIter = m_refList.begin();
  }
}
