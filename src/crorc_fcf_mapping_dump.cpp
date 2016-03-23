/**
 *  crorc_fcf_mapping_dump.cpp
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

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include "fcf_mapping.hh"

#define HELP_TEXT                                                              \
  "crorc_fcf_mapping_dump paramters:\n"                                        \
  " -f [file]     TPC AliRoot RowMapping.txt file\n"                           \
  " -p [patch]    TPC Patch Number, default: 0\n"                              \
  " -r [version]  RCU Version, default: 0\n"

int main(int argc, char *argv[]) {
  uint32_t tpcPatch = 0;
  uint32_t rcuVersion = 1;
  char *filename = NULL;

  int opt;
  while ((opt = getopt(argc, argv, "p:r:f:h")) != -1) {
    switch (opt) {
    case 'p':
      tpcPatch = strtoul(optarg, NULL, 0);
      break;
    case 'r':
      rcuVersion = strtoul(optarg, NULL, 0);
      break;
    case 'f':
      filename = optarg;
      break;
    case 'h':
      printf(HELP_TEXT);
      return 0;
    }
  }

  if (!filename) {
    printf("ERROR: path to AliRoot RowMapping.txt missing!\n");
    return -1;
  }

  fcf_mapping map = fcf_mapping(tpcPatch);
  if (map.readMappingFile(filename, rcuVersion) != 0) {
    perror("Failed to read mapping file");
    return -1;
  }
  for (unsigned i = 0; i < gkConfigWordCnt; i++) {
    printf("%08x\n", map[i]);
  }

  return 0;
}
