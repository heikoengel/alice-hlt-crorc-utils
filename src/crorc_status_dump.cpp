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

#include <librorc.h>

using namespace std;

#define HELP_TEXT "Dump the C-RORC status to stdout. \n\
usage: crorc_status_dump -n [DeviceID] \n\
"

#define HEXSTR(x, width) "0x" << setw(width) << setfill('0') << hex << x << setfill(' ') << dec

void printSgEntry
(
    librorc::link *link,
    uint32_t ram_sel,
    uint32_t ram_addr
)
{
    link->setPciReg(RORC_REG_SGENTRY_CTRL, ((ram_sel<<31) | ram_addr) );
    uint64_t sg_addr = (uint64_t)(link->pciReg(RORC_REG_SGENTRY_ADDR_HIGH))<<32;
    sg_addr |= link->pciReg(RORC_REG_SGENTRY_ADDR_LOW);
    uint32_t sg_len = link->pciReg(RORC_REG_SGENTRY_LEN);
    cout << "RAM" << ram_sel << " entry " << HEXSTR(ram_addr, 4)
         << ": " << HEXSTR(sg_addr, 16) << " " << HEXSTR(sg_len, 8) << endl;
}



int
main
(
    int argc,
    char *argv[]
)
{
    int32_t device_number = -1;
    int arg;

    /** parse command line arguments **/
    while ( (arg = getopt(argc, argv, "n:h")) != -1 )
    {
        switch (arg)
        {
            case 'n':
            { device_number = strtol(optarg, NULL, 0); }
            break;

            case 'h':
            {
                cout << HELP_TEXT;
                return 0;
            }
            break;

            default:
            {
                cout << "Unknown parameter" << endl;
                return -1;
            }
            break;
        }
    }

    /** make sure all required parameters are provided and valid **/
    if ( device_number == -1 )
    {
        cout << "ERROR: device ID was not provided." << endl;
        cout << HELP_TEXT;
        abort();
    }

    /** Instantiate device **/
    librorc::device *dev = NULL;
    try
    { dev = new librorc::device(device_number); }
    catch(...)
    {
        cout << "Failed to intialize device " << device_number
             << endl;
        abort();
    }

    /** Instantiate a new bar */
    librorc::bar *bar = NULL;
    try
    {
#ifdef MODELSIM
        bar = new librorc::sim_bar(dev, 1);
#else
        bar = new librorc::rorc_bar(dev, 1);
#endif
    }
    catch(...)
    {
        cout << "ERROR: failed to initialize BAR." << endl;
        delete dev;
        abort();
    }

    /** Instantiate a new sysmon */
    librorc::sysmon *sm;
    try
    { sm = new librorc::sysmon(bar); }
    catch(...)
    {
        cout << "Sysmon init failed!" << endl;
        delete bar;
        delete dev;
        abort();
    }

    /** print system monitor registers */
    for( int i=0; i<32; i++ )
    {
        cout << "SM reg " << setw(2) << i << ": "
             << HEXSTR(bar->get32(i), 8) << endl;
    }

    uint32_t nChannels = sm->numberOfChannels();
    for( uint32_t ch=0; ch<nChannels; ch++ )
    {
        librorc::link *link = new librorc::link(bar, ch);

        /** print DMA registers */
        for( int i=0; i<32; i++ )
        {
            cout << "CH" << ch << " DMA reg " << setw(2) << i << ": "
                 << HEXSTR(link->pciReg(i), 8) << endl;
        }

        if( link->isGtxDomainReady() )
        {
            /** print DDL registers */
            for( int i=0; i<32; i++ )
            {
                cout << "CH" << ch << " DDL reg " << setw(2) << i << ": "
                    << HEXSTR(link->ddlReg(i), 8) << endl;
            }

            /** print GTX registers */
            for( int i=0; i<7; i++ )
            {
                cout << "CH" << ch << " GTX reg " << setw(2) << i << ": "
                    << HEXSTR(link->gtxReg(i), 8) << endl;
            }
        }

        /** print EBDM sglist */
        uint32_t nSgConfig = link->pciReg(RORC_REG_EBDM_N_SG_CONFIG);
        uint32_t nEntries = ((nSgConfig&0xffff)>=(nSgConfig>>16)) ?
            (nSgConfig>>16)-1 : (nSgConfig&0xffff);
        for( uint32_t i=0; i<=nEntries; i++ )
        { printSgEntry(link, 0, i); }

        /** print RBDM sglist */
        nSgConfig = link->pciReg(RORC_REG_RBDM_N_SG_CONFIG);
        nEntries = ((nSgConfig&0xffff)>=(nSgConfig>>16)) ?
            (nSgConfig>>16)-1 : (nSgConfig&0xffff);
        for( uint32_t i=0; i<=nEntries; i++ )
        { printSgEntry(link, 1, i); }

        delete link;
    }

    delete bar;
    delete dev;
    return 0;
}
