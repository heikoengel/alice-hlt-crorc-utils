/**
 *  fcf_mapping.hh
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

#ifndef FCF_MAPPING_HH
#define FCF_MAPPING_HH

#include <stdint.h>

const unsigned gkConfigWordCnt = 4096;

class fcf_mapping {
public:
  fcf_mapping(unsigned patchNr);
  ~fcf_mapping();
  int readMappingFile(const char *filename);
  uint32_t operator[](unsigned ndx);

private:
  uint32_t fConfigWords[gkConfigWordCnt];
  unsigned fPatchNr;
};

#endif // FCF_MAPPING_HH
