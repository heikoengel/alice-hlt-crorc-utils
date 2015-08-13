/**
 *  crorc_reset.cpp
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

#include <unistd.h>
#include <librorc.h>

using namespace std;

#define HELP_TEXT "crorc_reset usage: \n\
crorc_reset [parameters] \n\
Parameters: \n\
        -h              Print this help \n\
        -n [0...255]    Target device \n\
        -c [channelID]  (optional) channel ID \n\
        -f              (optional) do full reset incl. GTXs\n\
"

int
main
(
    int argc,
    char *argv[]
)
{
    int32_t device_number = -1;
    uint32_t channel_number = 0xffffffff;
    int arg;
    int do_full_reset = 0;
    int ret = 0;

    /** parse command line arguments */
    while ( (arg = getopt(argc, argv, "hn:c:f")) != -1 )
    {
        switch (arg)
        {
            case 'h':
                cout << HELP_TEXT;
                return 0;
                break;
            case 'n':
                device_number = strtol(optarg, NULL, 0);
                break;
            case 'c':
                channel_number = strtol(optarg, NULL, 0);
                break;
            case 'f':
                do_full_reset = 1;
                break;
            default:
                cout << "Unknown parameter (" << arg << ")!" << endl;
                cout << HELP_TEXT;
                return -1;
                break;
        } //switch
    } //while

    if ( device_number < 0 || device_number > 255 )
    {
        cerr << "No or invalid device selected: " << device_number << endl;
        cout << HELP_TEXT;
        return -1;
    }

    /** Instantiate C-RORC **/
    librorc::device *dev = NULL;
    librorc::bar *bar = NULL;
    librorc::sysmon *sm = NULL;

    try {
        dev = new librorc::device(device_number);
        bar = new librorc::bar(dev, 1);
        sm = new librorc::sysmon(bar);
    } catch (int e) {
      cerr << "Failed to intialize device" << device_number
           << ": " << librorc::errMsg(e) << endl;
      if (sm) {
          delete sm;
      }
      if (bar) {
          delete bar;
      }
      if (dev) {
          delete dev;
      }
      return -1;
    }

    sm->clearAllErrorCounters();

    uint32_t start_channel = (channel_number!=0xffffffff) ?
        channel_number : 0;
    uint32_t end_channel = sm->numberOfChannels()-1;

    uint32_t firmware_type = sm->firmwareType();

    for ( uint32_t i=start_channel; i<=end_channel; i++ )
    {
        librorc::link *link = new librorc::link(bar, i);
        uint32_t link_type = link->linkType();
        librorc::dma_channel *ch = new librorc::dma_channel(link);

        link->setChannelActive(0);
        link->setFlowControlEnable(0);
        ch->disable();
        ch->clearStallCount();
        ch->clearEventCount();
        ch->readAndClearPtrStallFlags();
        ch->setRateLimit(0);


        if (link_type == RORC_CFG_LINK_TYPE_SIU ||
                link_type == RORC_CFG_LINK_TYPE_DIU ||
                link_type == RORC_CFG_LINK_TYPE_LINKTEST ) {
            librorc::gtx *gtx = new librorc::gtx(link);

            bool clocks_ready = link->isGtxDomainReady() &&
                                link->isDdlDomainReady();

            if ( do_full_reset || !clocks_ready)
            {
                gtx->setReset(1);
                usleep(1000);
                gtx->setReset(0);
                if (link->waitForGTXDomain() == -1) {
                    cerr << "Device " << device_number << " GTX" << i
                        << ": Clock failed to initialize correctly" << endl;
                    ret = -1;
                    continue;
                }
            }

            gtx->clearErrorCounters();
            delete gtx;
        }

        if(link->isDdlDomainReady())
        {
            link->setFlowControlEnable(0);

            switch( link_type )
            {
                case RORC_CFG_LINK_TYPE_DIU:
                    {
                        librorc::diu *diu = new librorc::diu(link);
                        diu->setReset(1);
                        diu->setEnable(0);
                        diu->clearEventcount();
                        diu->clearDdlDeadtime();
                        diu->clearDmaDeadtime();
                        diu->clearAllLastStatusWords();
                        diu->setReset(0);
                        delete diu;

                        if( link->fastClusterFinderAvailable() )
                        {
                            librorc::fastclusterfinder *fcf = 
                                new librorc::fastclusterfinder(link);
                            fcf->setReset(1); // reset
                            fcf->setEnable(0); // not enabled
                            delete fcf;
                        }
                    }
                    break;

                case RORC_CFG_LINK_TYPE_SIU:
                    {
                        librorc::siu *siu = new librorc::siu(link);
                        siu->setReset(0);
                        siu->setEnable(0);
                        siu->clearEventcount();
                        siu->clearDdlDeadtime();
                        siu->clearDmaDeadtime();
                        delete siu;
                    }
                    break;

                case RORC_CFG_LINK_TYPE_VIRTUAL:
                    {
                        librorc::ddl *ddlraw = new librorc::ddl(link);
                        ddlraw->setEnable(0);
                        ddlraw->clearDmaDeadtime();
                        delete ddlraw;
                    }
                    break;

                default: // LINK_TEST, IBERT
                    {
                    }
                    break;
            }

            // PG on HLT_IN, HLT_OUT, HWTEST
            if( link->patternGeneratorAvailable() )
            {
                librorc::patterngenerator *pg =
                    new librorc::patterngenerator(link);
                pg->disable();
                delete pg;
            }

            // reset DDR3 Data Replay if available
            if( link->ddr3DataReplayAvailable() )
            {
                librorc::datareplaychannel *dr =
                    new librorc::datareplaychannel(link);
                dr->setReset(1);
                delete dr;
            }

            if( link_type == RORC_CFG_LINK_TYPE_DIU ||
                    link_type == RORC_CFG_LINK_TYPE_VIRTUAL )
            {
                // EventFilter
                librorc::eventfilter *filter = new librorc::eventfilter(link);
                filter->setFilterMask(0);
                delete filter;
            }
        }

        delete ch;
        delete link;
    }

    delete sm;
    delete bar;
    delete dev;

    return ret;
}
