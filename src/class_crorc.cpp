/**
 *  class_crorc.cpp
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

#include "class_crorc.hpp"

crorc::crorc(uint32_t deviceId) {
  m_dev = NULL;
  m_bar = NULL;
  m_sm = NULL;
  m_refclk = NULL;
  for (uint32_t i = 0; i < LIBRORC_MAX_LINKS; i++) {
    m_link[i] = NULL;
    m_ch[i] = NULL;
    m_gtx[i] = NULL;
    m_diu[i] = NULL;
    m_siu[i] = NULL;
    m_ddl[i] = NULL;
    m_filter[i] = NULL;
  }
  try {
    m_dev = new librorc::device(deviceId);
  } catch (...) {
    throw;
  }

  try {
    m_bar = new librorc::bar(m_dev, 1);
  } catch (...) {
    throw;
  }

  try {
    m_sm = new librorc::sysmon(m_bar);
  } catch (...) {
    throw;
  }

  try {
    m_refclk = new librorc::refclk(m_sm);
  } catch (...) {
    throw;
  }

  m_nchannels = m_sm->numberOfChannels();

  for (uint32_t i = 0; i < m_nchannels; i++) {
    try {
      m_link[i] = new librorc::link(m_bar, i);
      m_ch[i] = new librorc::dma_channel(m_link[i]);
      m_gtx[i] = new librorc::gtx(m_link[i]);
      m_ddl[i] = new librorc::ddl(m_link[i]);
      m_linkType[i] = m_link[i]->linkType();

      switch (m_linkType[i]) {
      case RORC_CFG_LINK_TYPE_DIU:
        m_diu[i] = new librorc::diu(m_link[i]);
        m_filter[i] = new librorc::eventfilter(m_link[i]);
        break;
      case RORC_CFG_LINK_TYPE_SIU:
        m_siu[i] = new librorc::siu(m_link[i]);
        break;
      case RORC_CFG_LINK_TYPE_VIRTUAL:
        m_filter[i] = new librorc::eventfilter(m_link[i]);
        break;
      default:
        break;
      }
    } catch (...) {
      throw;
    }
  }
}

crorc::~crorc() {
  for (uint32_t i = 0; i < LIBRORC_MAX_LINKS; i++) {
    if (m_filter[i]) {
      delete m_filter[i];
    }
    if (m_ddl[i]) {
      delete m_ddl[i];
    }
    if (m_siu[i]) {
      delete m_siu[i];
    }
    if (m_diu[i]) {
      delete m_diu[i];
    }
    if (m_gtx[i]) {
      delete m_gtx[i];
    }
    if (m_ch[i]) {
      delete m_ch[i];
    }
    if (m_link[i]) {
      delete m_link[i];
    }
  }
  if (m_refclk) {
    delete m_refclk;
  }
  if (m_sm) {
    delete m_sm;
  }
  if (m_bar) {
    delete m_bar;
  }
  if (m_dev) {
    delete m_dev;
  }
}

const char *crorc::getFanState() {
  if (m_sm->systemFanIsAutoMode()) {
    return "auto";
  } else if (m_sm->systemFanIsEnabled()) {
    return "on";
  } else {
    return "off";
  }
}

void crorc::setFanState(uint32_t state) {
  if (state == FAN_ON) {
    m_sm->systemFanSetEnable(1, 1);
  } else if (state == FAN_OFF) {
    m_sm->systemFanSetEnable(1, 0);
  } else {
    m_sm->systemFanSetEnable(0, 1);
  }
}

const char *crorc::getLedState() {
  if (m_sm->bracketLedInBlinkMode()) {
    return "blink";
  } else {
    return "auto";
  }
}

void crorc::setLedState(uint32_t state) {
  uint32_t mode = (state == LED_BLINK) ? 1 : 0;
  m_sm->setBracketLedMode(mode);
}

uint32_t crorc::getLinkmask() { return m_sm->getLinkmask(); }

void crorc::setLinkmask(uint32_t mask) { m_sm->setLinkmask(mask); }

void crorc::setAllQsfpReset(uint32_t reset) {
  m_sm->qsfpSetReset(0, reset);
  m_sm->qsfpSetReset(1, reset);
  m_sm->qsfpSetReset(2, reset);
}

void crorc::setAllGtxReset(uint32_t reset) {
  for (uint32_t i = 0; i < m_nchannels; i++) {
    if (isOpticalLink(i)) {
      m_gtx[i]->setReset(reset);
    }
  }
}

void crorc::configAllGtxPlls(librorc::gtxpll_settings pllcfg) {
  for (uint32_t i = 0; i < m_nchannels; i++) {
    if (isOpticalLink(i)) {
      m_gtx[i]->drpSetPllConfig(pllcfg);
    }
  }
}

t_linkStatus crorc::getLinkStatus(uint32_t i) {
  t_linkStatus ls;
  memset(&ls, 0, sizeof(ls));
  ls.gtx_inReset = (m_gtx[i]->getReset() == 1);
  ls.gtx_domainReady = m_gtx[i]->isDomainReady();
  if (!ls.gtx_domainReady) {
      return ls;
  }
  ls.gtx_linkUp = m_gtx[i]->isLinkUp();
  ls.gtx_dispErrCnt = m_gtx[i]->getDisparityErrorCount();
  ls.gtx_realignCnt = m_gtx[i]->getRealignCount();
  ls.gtx_nitCnt = m_gtx[i]->getRxNotInTableErrorCount();
  ls.gtx_losCnt = m_gtx[i]->getRxLossOfSyncErrorCount();

  ls.ddl_domainReady = m_link[i]->isDdlDomainReady();
  if (!ls.ddl_domainReady) {
    return ls;
  }
  if (m_linkType[i] == RORC_CFG_LINK_TYPE_DIU) {
    ls.ddl_linkUp = m_diu[i]->linkUp();
    ls.ddl_linkFull = m_diu[i]->linkFull();
  } else if (m_linkType[i] == RORC_CFG_LINK_TYPE_SIU) {
    ls.ddl_linkUp = ls.gtx_linkUp;
    ls.ddl_linkFull = m_siu[i]->linkFull();
  } else {
    ls.ddl_linkUp = true;
    ls.ddl_linkFull = false;
  }
  return ls;
}

bool crorc::isOpticalLink(uint32_t i) {
  bool isOptical = false;
  switch (m_linkType[i]) {
  case RORC_CFG_LINK_TYPE_SIU:
  case RORC_CFG_LINK_TYPE_DIU:
  case RORC_CFG_LINK_TYPE_LINKTEST:
  case RORC_CFG_LINK_TYPE_GBT:
    isOptical = true;
    break;
  default:
    // RORC_CFG_LINK_TYPE_VIRTUAL
    // RORC_CFG_LINK_TYPE_IBERT
    isOptical = false;
    break;
  }
  return isOptical;
}

const char *crorc::linkTypeDescr(uint32_t i) {
  const char *descr;
  switch (m_linkType[i]) {
  case RORC_CFG_LINK_TYPE_IBERT:
    descr = "IBERT";
    break;
  case RORC_CFG_LINK_TYPE_VIRTUAL:
    descr = "RAW-COPY";
    break;
  case RORC_CFG_LINK_TYPE_LINKTEST:
    descr = "LINK-TEST";
    break;
  case RORC_CFG_LINK_TYPE_DIU:
    descr = "DIU";
    break;
  case RORC_CFG_LINK_TYPE_SIU:
    descr = "SIU";
    break;
  case RORC_CFG_LINK_TYPE_GBT:
    descr = "GBT";
    break;
  default:
    descr = "UNKNOWN";
    break;
  }
  return descr;
}
