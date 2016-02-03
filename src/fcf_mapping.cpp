/**
 *  fcf_mapping.cpp
 *  based on 
 *  $ALIHLT_DC_SRCDIR/Components/CRORCInterfacing/ALIHLTCRORCTPCMapping.cpp
 *  by T. M. Steinbeck, 2001
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

#include <fstream>
#include <vector>
#include <errno.h>
#include <string.h>
#include "fcf_mapping.hh"

static const unsigned gkPatchStartRow[] = { 0, 30, 63, 90, 117, 139 };

fcf_mapping::fcf_mapping(unsigned patchNr) {
  memset(fConfigWords, 0, gkConfigWordCnt * sizeof(uint32_t));
  fPatchNr = patchNr;
}

fcf_mapping::~fcf_mapping(){};

uint32_t fcf_mapping::operator[](unsigned ndx) {
  return (ndx < gkConfigWordCnt) ? fConfigWords[ndx] : 0xffffffff;
}

std::vector<std::string> splitString( std::string &s, char token) {
  std::vector<std::string> ret;
  typedef std::string::size_type string_size;
  string_size i = 0;
  // invariant: we have processed characters [original value of i, i)
  while (i != s.size()) {
    // ignore leading blanks
    // invariant: characters in range [original i, current i) are all spaces
    while (i != s.size() && isspace(s[i]))
      ++i;
    // find end of next word
    string_size j = i;
    // invariant: none of the characters in range [original j, current j)is a
    // space
    while (j != s.size() && !isblank(s[j]))
      j++;
    // if we found some nonwhitespace characters
    if (i != j) {
      // copy from s starting at i and taking j - i chars
      ret.push_back(s.substr(i, j - i));
      i = j;
    }
  }
  return ret;
}

int fcf_mapping::readMappingFile(const char *filename) {
  if (!filename) {
    errno = EINVAL;
    return -1;
  }
  std::ifstream infile(filename);
  if (infile.bad() or infile.fail()) {
    return -1;
  }
  const unsigned bufSize = 4096;
  char buf[bufSize];
  std::string line;
  unsigned long lineCnt = 0;
  while (infile.good()) {
    line = "";
    unsigned count = 0;
    do {
      infile.getline(buf, bufSize);
      count = infile.gcount();
      if (count > 0)
        line += buf;
    } while (count == bufSize - 1 && infile.fail() && !infile.eof() &&
             !infile.bad());
    // fail bit set if function stops extracing because
    // buffer size limit is reached...

    ++lineCnt;
    if (line.length() <= 0)
      continue;
    char lineStr[1024];
    snprintf(lineStr, 1024, "%lu", lineCnt);
    std::vector<std::string> tokens;
    //line.split(tokens, " \t");
    tokens = splitString(line, '\t');
    if (tokens.size() == 0) {
      continue;
    }
    if (tokens.size() < 2) {
      errno = EINVAL;
      return -1;
    }
    char *cpErr = NULL;
    // first word is number of this row
    unsigned long rowNr = strtoul(tokens[0].c_str(), &cpErr, 0);
    if (*cpErr != '\0') {
      errno = EINVAL;
      return -1;
    }
    rowNr -= gkPatchStartRow[fPatchNr];
    // second word is number of pads in this row
    unsigned long padCnt = strtoul(tokens[1].c_str(), &cpErr, 0);
    if (*cpErr != '\0') {
      errno = EINVAL;
      return -1;
    }
    if (padCnt + 2 != tokens.size()) {
      errno = EINVAL;
      return -1;
    }
    unsigned long lastHWAddress = ~(0UL);
    // Currently all channels are always active
    bool active = true;

    // Gain calib identical for all pads currently, 1.0 as 13 bit
    // fixed point, with 1 bit position before decimal point
    uint32_t gainCalib = (1 << 12);

    for (unsigned pad = 0; pad < padCnt; ++pad) {
      char padStr[1024];
      snprintf(padStr, 1024, "%ud", pad);
      unsigned long hwAddress = strtoul(tokens[pad + 2].c_str(), &cpErr, 0);
      if (*cpErr != '\0') {
        errno = EINVAL;
        return -1;
      }
      unsigned long patchNr = (hwAddress & ~0xFFF) >> 12;
      unsigned long lastPatchNr = (lastHWAddress & ~0xFFF) >> 12;
      if (patchNr != fPatchNr) {
        if (lastHWAddress != ~(0UL) && lastPatchNr == fPatchNr) {
          lastHWAddress &= 0xFFF;
          fConfigWords[lastHWAddress] |=
              (1 << 14); // Change in patch means this pad (which we ignore
                         // since it is in the wrong patch) and the preceeding
                         // one, must be border pads
        }
        lastHWAddress = hwAddress;
        continue;
      }
      bool isBorderPad =
          (lastHWAddress == ~(0UL)) ||
          (((hwAddress >= lastHWAddress) ? (hwAddress - lastHWAddress)
                                         : (lastHWAddress - hwAddress)) >=
           2048); // First pad in a padrow is always border pad, or if hwaddress
                  // differs by at least 2048 from preceeding hw address.
      hwAddress &= 0xFFF; // Clear patch nr from HW address, do this only after
                          // the difference has been checked above.
      lastHWAddress &= 0xFFF;
      uint32_t configWord = (pad & 0xFF) | ((rowNr & 0x3F) << 8);
      if (active) {
        configWord |= (1 << 15);
      }
      if (isBorderPad) {
        configWord |= (1 << 14);
      }
      // Only do this, if there was a previous pad and it belongs to this
      // patch
      if (isBorderPad && lastHWAddress != ~(0UL) && lastPatchNr == fPatchNr) {
        // This pad is a border pad, therefore the previous one
        // has to have been one as well...
        fConfigWords[lastHWAddress] |= (1 << 14);
      }
      configWord |= (gainCalib & 0x1FFF) << 16;
      fConfigWords[hwAddress] = configWord;
      lastHWAddress = hwAddress | (fPatchNr << 12);
    }
    unsigned long lastPatchNr = (lastHWAddress & ~0xFFF) >> 12;
    if (lastHWAddress != ~(0UL) && lastPatchNr == fPatchNr) {
      lastHWAddress &= 0xFFF;
      // Last pad is always border pad by definition
      fConfigWords[lastHWAddress] |= (1 << 14);
    }
  }
  return 0;
}
