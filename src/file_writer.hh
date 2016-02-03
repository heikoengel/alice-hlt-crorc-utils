/**
 *  file_writer.hh
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

#ifndef FILE_WRITER_HH
#define FILE_WRITER_HH

#include <string>
#include <librorc.h>

#define FILE_WRITER_CONSTRUCTOR_FAILED 1

class file_writer {
public:
  file_writer(std::string basedir, uint32_t device, uint32_t channel,
              uint64_t eventlimit = 0);
  ~file_writer();

  int dump(librorc::EventDescriptor *report, const uint32_t *event);

private:
  std::string create_file_name();
  
  std::string m_basedir;
  uint64_t m_eventcount;
  uint64_t m_eventlimit;
  uint32_t m_device;
  uint32_t m_channel;  
};

#endif // FILE_WRITER_HH
