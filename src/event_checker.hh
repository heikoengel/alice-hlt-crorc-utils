/**
 *  event_checker.hh
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

#ifndef _EVENT_CHECKER_H
#define _EVENT_CHECKER_H

#include <vector>
#include <stdint.h>
#include <librorc.h>

/** sanity checks **/
#define CHK_SIZES   (1<<0)
#define CHK_PATTERN (1<<1)
#define CHK_SOE     (1<<2)
#define CHK_EOE     (1<<3)
#define CHK_ID      (1<<4)
#define CHK_DIU_ERR (1<<5)
#define CHK_FILE    (1<<8)
#define CHK_CMPL    (1<<9)

struct refFileEntry {
  uint32_t *map;
  size_t size;
};

class event_checker {
public:
  event_checker(uint32_t deviceId, uint32_t channelId, char *logDir);
  ~event_checker();

  int addRefFile(char *filename);
  int check(librorc::EventDescriptor *report, const uint32_t *event, uint32_t checkMask);

private:
  uint32_t checkDiuError(librorc::EventDescriptor *report);
  uint32_t checkReportSizes(librorc::EventDescriptor *report);
  uint32_t checkCompletionStatus(librorc::EventDescriptor *report);
  uint32_t checkReferenceFile(librorc::EventDescriptor *report, const uint32_t *event);

  void selectNextRefFile();
  void dumpToFile(librorc::EventDescriptor *report, const uint32_t *event,
                  uint32_t checkResult);

  uint32_t m_deviceId;
  uint32_t m_channelId;
  char *m_logDir;
  uint64_t m_errorCount;
  std::vector<refFileEntry> m_refList;
  std::vector<refFileEntry>::iterator m_refListIter;
};
#endif
