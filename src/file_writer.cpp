/**
 *  file_writer.cpp
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

#include <sstream>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "file_writer.hh"


/****************** Helpers *******************/
// Source: 'Patrick', https://stackoverflow.com/a/12904145
int mkpath(std::string s, mode_t mode) {
  size_t pre = 0, pos;
  std::string dir;
  int mdret = -1;
  if ((s.size() > 0) && (s[s.size() - 1] != '/')) {
    s += '/'; // force trailing '/'
  }
  while ((pos = s.find_first_of('/', pre)) != std::string::npos) {
    dir = s.substr(0, pos++);
    pre = pos;
    if (dir.size() == 0)
      continue; // first leading '/'
    if ((mdret = mkdir(dir.c_str(), mode)) && errno != EEXIST) {
      return mdret;
    }
  }
  return mdret;
}

/****************** Public *******************/
file_writer::file_writer(std::string basedir, uint32_t device, uint32_t channel,
                         uint64_t eventlimit) {
  m_basedir = basedir;
  m_eventcount = 0;
  m_eventlimit = 0;
  m_device = device;
  m_channel = channel;
  struct stat dirstat;
  int ret = stat(basedir.c_str(), &dirstat);
  if (ret < 0 && errno == ENOENT) {
    ret = mkpath(basedir, 0755);
    if (ret < 0) {
      throw FILE_WRITER_CONSTRUCTOR_FAILED;
    }
  } else {
    throw FILE_WRITER_CONSTRUCTOR_FAILED;
  }
}

file_writer::~file_writer(){};

int file_writer::dump(librorc::EventDescriptor *report, const uint32_t *event) {
  if ((m_eventlimit > 0) && (m_eventcount > m_eventlimit)) {
    m_eventcount++;
    return 0;
  }
  std::string filename = create_file_name();
  int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return -1;
  }
  ssize_t event_size = (report->calc_event_size & 0x3fffffff) << 2;
  ssize_t ret = write(fd, event, event_size);
  close(fd);
  if (ret != event_size) {
    return -1;
  }
  m_eventcount++;
  return 0;
}

/****************** Private *******************/
std::string file_writer::create_file_name() {
  std::stringstream ss;
  ss << m_basedir << "/"
     << "dev" << m_device << "_ch" << m_channel << "_" << m_eventcount;
  return ss.str();
}
