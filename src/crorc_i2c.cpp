/**
 *  crorc_i2c.cpp
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

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <unistd.h>

#include <librorc.h>

using namespace std;

#define HELP_TEXT "access onboard i2c devices \n\
usage: crorc_i2c [parameters] \n\
parameters: \n\
        -h              Print this help screen \n\
        -d [0..255]     Target device ID \n\
        -c [0..5]       Target I2C Chain \n\
                        0: QSFP0 \n\
                        1: QSFP1 \n\
                        2: QSFP2 \n\
                        3: DDR3 \n\
                        4: RefClkGen \n\
                        5: FMC \n\
        -s [0x0..0x7f]  Slave address \n\
        -a [0x0..0xff]  Memory address \n\
        -w [value]      Value to be written to slave address \n\
Examples: \n\
Read from device 0, QSFP0, slave address 5: \n\
        crorc_i2c -d 0 -c 0 -a 0x5 \n\
Write 0x12 to device 1, FMC chain, slave address 0x27: \n\
        crorc_i2c -d 1 -c 5 -a 0x27 -w 0x12 \n\
"


typedef struct
{
    int32_t device_number;
    int32_t i2c_chain;
    int32_t slave_addr;
    int32_t mem_addr;
    uint8_t value;
} confopts;


int
    main
(
 int argc,
 char *argv[]
 )
{

    confopts options =
    {
        -1, -1, -1, -1, 0
    };

    int arg;
    int do_write = 0;
    uint8_t data_read = 0;

    /** parse command line arguments **/
    while ( (arg = getopt(argc, argv, "d:c:s:a:w:")) != -1 )
    {
        switch (arg)
        {
            case 'd':
                options.device_number = strtol(optarg, NULL, 0);
                break;
            case 'c':
                options.i2c_chain = strtol(optarg, NULL, 0);
                break;
            case 's':
                options.slave_addr = strtol(optarg, NULL, 0);
                break;
            case 'a':
                options.mem_addr = strtol(optarg, NULL, 0);
                break;
            case 'w':
                options.value = strtol(optarg, NULL, 0);
                do_write = 1;
                break;
            case 'h':
                cout << HELP_TEXT;
                return 0;
        }
    }

    /** make sure all required parameters are provided and valid **/
    if ( options.device_number == -1 )
    {
        cout << "ERROR: device ID was not provided." << endl;
        cout << HELP_TEXT;
        return -1;
    }
    else if ( options.device_number > 255 || options.device_number < 0 )
    {
        cout << "ERROR: invalid device ID selected: "
             << options.device_number << endl;
        return -1;
    }

    if ( options.i2c_chain == -1 )
    {
        cout << "ERROR: I2C chain was not provided." << endl;
        cout << HELP_TEXT;
        return -1;
    }
    else if ( options.i2c_chain > 5 || options.i2c_chain < 0)
    {
        cout << "ERROR: invalid I2C chain selected: " << options.i2c_chain
             << endl;
    }

    if ( options.slave_addr == -1 )
    {
        cout << "ERROR: I2C slave address was not provided." << endl;
        cout << HELP_TEXT;
        return -1;
    }
    else if ( options.slave_addr > 0x7f || options.slave_addr < 0)
    {
        cout << "ERROR: I2C slave address out of bounds [0x0..0x7f]: 0x"
             << hex << options.slave_addr << endl;
        cout << HELP_TEXT;
        return -1;
    }

    if ( options.mem_addr == -1 )
    {
        cout << "ERROR: I2C mem address was not provided." << endl;
        cout << HELP_TEXT;
        return -1;
    }
    else if ( options.mem_addr > 0xff || options.mem_addr < 0)
    {
        cout << "ERROR: I2C mem address out of bounds [0x0..0xff]: 0x"
             << hex << options.mem_addr << endl;
        cout << HELP_TEXT;
        return -1;
    }


    /** Instantiate device **/
    librorc::device *dev = NULL;
    try
    {
        dev = new librorc::device(options.device_number);
    }
    catch(...)
    {
        cout << "Failed to intialize device " << options.device_number
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

    /** Instantiate a new sysmon */
    librorc::sysmon *sm;
    try
    {
        sm = new librorc::sysmon(bar);
    }
    catch(...)
    {
        cout << "Sysmon init failed!" << endl;
        delete bar;
        delete dev;
        abort();
    }

    if ( do_write )
    {
        try
        {
            sm->i2c_write_mem(
                options.i2c_chain,
                options.slave_addr,
                options.mem_addr,
                options.value );
        }
        catch (...)
        {
            cout << "Failed to write to Slave 0x" << hex
                 << options.slave_addr << " Mem 0x" << options.mem_addr
                 << endl;
        }
    }
    else
    {
        try
        {
            data_read = sm->i2c_read_mem(
                    options.i2c_chain,
                    options.slave_addr,
                    options.mem_addr);
        }
        catch (...)
        {
            cout << "Failed to read from Slave 0x" << hex
                << options.slave_addr << " Mem 0x" << options.mem_addr
                << endl;
        }
        cout << "0x" << hex << setfill('0') << setw(2) << (int)data_read
             << endl;
    }

    delete sm;
    delete bar;
    delete dev;

    return 0;
}
