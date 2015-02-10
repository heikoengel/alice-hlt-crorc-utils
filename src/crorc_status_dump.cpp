/**
 *  crorc_status_dump.cpp
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

#include <cstdio>
#include <iomanip>
#include <unistd.h>
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
        bar = new librorc::bar(dev, 1);
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

        if (link->isDdlDomainReady()) {
          /** print DDL registers */
          for (int i = 0; i < 32; i++) {
            cout << "CH" << ch << " DDL reg " << setw(2) << i << ": "
                 << HEXSTR(link->ddlReg(i), 8) << endl;
          }
        }

        if (link->isGtxDomainReady()) {
          /** print GTX registers */
          for (int i = 0; i < 7; i++) {
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
