/**
 * @file crorc_dma_out.h
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2016-07-26
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * */

#ifndef _CRORC_DMA_OUT_H
#define _CRORC_DMA_OUT_H

#include <librorc.h>

#define SHM_BASE 9999

struct t_sts {
  uint64_t n_events;
  uint64_t bytes_received;
  uint32_t deviceId;
  uint32_t channelId;
  uint32_t eventSize;
  uint32_t rcvEventSize;
};

#endif
