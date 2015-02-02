/**
 * Copyright (c) 2014, Heiko Engel <hengel@cern.ch>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of University Frankfurt, CERN nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL A COPYRIGHT HOLDER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
  void doBoardReset();
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
