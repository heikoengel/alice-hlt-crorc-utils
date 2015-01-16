/**
 * Copyright (c) 2014, Heiko Engel <hengel@cern.ch>
 * Copyright (c) 2014, Dominic Eschweiler <dominic.eschweiler@cern.ch>
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
 *     * Neither the name of University Frankfurt, FIAS, CERN nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL A COPYRIGHT HOLDER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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
        cout << "No or invalid device selected: " << device_number << endl;
        cout << HELP_TEXT;
        abort();
    }

    /** Instantiate device **/
    librorc::device *dev = NULL;
    try
    {
        dev = new librorc::device(device_number);
    }
    catch(...)
    {
        cout << "Failed to intialize device " << device_number
            << endl;
        return -1;
    }

    /** Instantiate a new bar */
    librorc::bar *bar = NULL;
    try
    {
        bar = new librorc::bar(dev, 1);
    }
    catch(...)
    {
        cout << "ERROR: failed to initialize BAR." << endl;
        delete dev;
        abort();
    }

    librorc::sysmon *sm = NULL;
    try
    {
        sm = new librorc::sysmon(bar);
    }
    catch(...)
    {
        cout << "ERROR: failed to initialize System Manager." << endl;
        delete bar;
        delete dev;
        abort();
    }

    sm->clearAllErrorCounters();

    uint32_t start_channel = (channel_number!=0xffffffff) ?
        channel_number : 0;
    uint32_t end_channel = sm->numberOfChannels()-1;

    uint32_t firmware_type = sm->firmwareType();

    for ( uint32_t i=start_channel; i<=end_channel; i++ )
    {
        librorc::link *link = new librorc::link(bar, i);
        librorc::gtx *gtx = new librorc::gtx(link);
        uint32_t link_type = link->linkType();
        librorc::dma_channel *ch = new librorc::dma_channel(link);


        if ( do_full_reset )
        {
            gtx->setReset(1);
            usleep(1000);
            gtx->setReset(0);
            link->waitForGTXDomain();
        }

        if(link->isGtxDomainReady())
        {
            link->setFlowControlEnable(0);
            gtx->clearErrorCounters();

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
                        delete diu;

                        if( firmware_type == RORC_CFG_PROJECT_hlt_in_fcf )
                        {
                            librorc::fastclusterfinder *fcf = 
                                new librorc::fastclusterfinder(link);
                            fcf->setState(1, 0); // reset, not enabled
                            delete fcf;
                        }
                    }
                    break;

                case RORC_CFG_LINK_TYPE_SIU:
                    {
                        librorc::siu *siu = new librorc::siu(link);
                        siu->setReset(1);
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

        link->setChannelActive(0);
        link->setFlowControlEnable(0);
        ch->disable();
        ch->clearStallCount();
        ch->clearEventCount();
        ch->readAndClearPtrStallFlags();
        ch->setRateLimit(0);

        delete gtx;
        delete ch;
        delete link;
    }

    return 0;
}
