/**
 *  class_crorc.hpp
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

#ifndef CLASS_CRORC_HPP
#define CLASS_CRORC_HPP

#include <librorc.h>
#define LIBRORC_MAX_LINKS 12
#define LIBRORC_CH_UNDEF 0xffffffff

#define FAN_OFF 0
#define FAN_ON 1
#define FAN_AUTO 2

#define LED_AUTO 0
#define LED_BLINK 1

typedef struct {
    bool gtx_inReset;
    bool gtx_domainReady;
    bool gtx_linkUp;
    uint32_t gtx_dispErrCnt;
    uint32_t gtx_realignCnt;
    uint32_t gtx_nitCnt;
    uint32_t gtx_losCnt;
    bool ddl_domainReady;
    bool ddl_linkUp;
    bool ddl_linkFull;
} t_linkStatus;

class crorc {
public:
  crorc(uint32_t deviceId);
  ~crorc();
  const char *getFanState();
  void setFanState(uint32_t state);
  const char *getLedState();
  void setLedState(uint32_t state);
  uint32_t getLinkmask();
  void setLinkmask(uint32_t mask);
  void setAllQsfpReset(uint32_t reset);
  void setAllGtxReset(uint32_t reset);
  void configAllGtxPlls(librorc::gtxpll_settings pllcfg);
  t_linkStatus getLinkStatus(uint32_t linkId);
  bool isOpticalLink(uint32_t i);
  const char *linkTypeDescr(uint32_t i);

  librorc::device *m_dev;
  librorc::bar *m_bar;
  librorc::sysmon *m_sm;
  librorc::refclk *m_refclk;
  librorc::link *m_link[LIBRORC_MAX_LINKS];
  librorc::dma_channel *m_ch[LIBRORC_MAX_LINKS];
  librorc::gtx *m_gtx[LIBRORC_MAX_LINKS];
  librorc::diu *m_diu[LIBRORC_MAX_LINKS];
  librorc::siu *m_siu[LIBRORC_MAX_LINKS];
  librorc::ddl *m_ddl[LIBRORC_MAX_LINKS];
  librorc::eventfilter *m_filter[LIBRORC_MAX_LINKS];
  uint32_t m_linkType[LIBRORC_MAX_LINKS];
  uint32_t m_nchannels;
};


#endif
