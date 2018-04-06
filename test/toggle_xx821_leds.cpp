// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* 
// Copyright UCAR (c) 2018
// University Corporation for Atmospheric Research (UCAR)
// National Center for Atmospheric Research (NCAR)
// Boulder, Colorado, USA
// BSD licence applies - redistribution and use in source and binary
// forms, with or without modification, are permitted provided that
// the following conditions are met:
// 1) If the software is modified to produce derivative works,
// such modified software should be clearly marked, so as not
// to confuse it with the version available from UCAR.
// 2) Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// 3) Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// 4) Neither the name of UCAR nor the names of its contributors,
// if any, may be used to endorse or promote products derived from
// this software without specific prior written permission.
// DISCLAIMER: THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* 
#include <iostream>
#include <cstring>
#include <boost/program_options.hpp>
#include <csignal>
#include <logx/Logging.h>
#include <nav_common.h>
#include <820_include/nav820.h>

using namespace std;
namespace po = boost::program_options;

LOGGING("toggle_xx821_leds")

int _nCycles = 50;          ///< number of times leds are cycled
double _cyclePeriod = 0.2;  ///< cycle period (secs)
bool _exitNow = false;      ///< early exit if this becomes true

/// Parse the command line options
void parseOptions(int argc, char** argv)
{

    // get the options
    po::options_description descripts("Options");
    descripts.add_options()
            ("help", "Describe options")
            ("nCycles", po::value<int>(&_nCycles), "# of times to cycle LEDs [50]")
            ("cyclePeriod", po::value<double>(&_cyclePeriod), "Cycle period, s [0.2]")
            ;

    po::variables_map vm;
    po::command_line_parser parser(argc, argv);
    po::positional_options_description pd;
    po::store(parser.options(descripts).positional(pd).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << "Usage: " << argv[0] <<
                " [OPTION]..." << endl;
        cout << descripts << endl;
        exit(0);
    }
}

/// Interrupt handler to trigger early exit
void
onInterrupt(int signal) {
    ILOG << "Exiting early on " << strsignal(signal) << " signal";
    _exitNow = true;
}

int
main(int argc, char** argv)
{
    // Let logx get and strip out its arguments
    logx::ParseLogArgs(argc, argv);

    // parse the command line options, substituting for config params.
    parseOptions(argc, argv);

    // Exit early if an interrupt signal (^C) is received
    signal(SIGINT, onInterrupt);

    // Initialize Pentek's Navigator board support package
    int32_t status = NAV_BoardStartup();
    if (status != NAV_STAT_OK)
    {
        ELOG << "Error initializing Navigator board support package!";
        exit(1);
    }

    status = NAV_BoardStartup();
    if (status != NAV_STAT_OK)
    {
        ELOG << "Error initializing Navigator board support package!";
        exit(1);
    }

    // Find all Pentek boards in the system
    NAV_DEVICE_INFO *boardList[NAV_MAX_BOARDS];

    int32_t numBoards;
    status = NAV_BoardFind(0, boardList, &numBoards);
    DLOG << numBoards << ((numBoards == 1) ? " board " : " boards ") << "found";

    if (status != NAV_STAT_OK)
    {
        ELOG << "No Pentek boards detected!";
        exit(1);
    }

    // Open a board, providing for user selection if there's more than one
    // board.
    void * boardPtr = NAV_BoardSelect(numBoards, 0, boardList[0], 0);
    NAV_BOARD_RESRC * boardResource = reinterpret_cast<NAV_BOARD_RESRC *>(boardPtr);

    if (boardResource) {
        DLOG << "Opened xx821 board";
    } else {
        ELOG << "Failed to open board!";
        exit(1);
    }

    // BAR0 base for register access
    volatile uint32_t * baseBAR0 =
                reinterpret_cast<volatile uint32_t *>(boardResource->pciInfo.BAR0Base);

    // Calculate the 'on' time for each LED in the cycle, in microseconds
    int nLeds = 2;
    int onTimeUsecs = int(1.0e6 * _cyclePeriod / nLeds);

    // Alternately light the user (USR) LED and clock master (MAS) LED
    // _nCycles times.
    ILOG << "LEDs will cycle " << _nCycles << " times @ " << _cyclePeriod <<
            " s/cycle";
    ILOG << "(Total time " << _nCycles * _cyclePeriod << " s)";
    ILOG << "Running...";

    for (int t = 0; t < _nCycles; t++) {
        // user LED
        NAVip_BrdInfoRegs_UserLed_SetUserLedEnable(baseBAR0,
            NAV_IP_BRD_INFO_USER_LED_CTRL_USER_LED_ON);
        usleep(onTimeUsecs);
        NAVip_BrdInfoRegs_UserLed_SetUserLedEnable(baseBAR0,
            NAV_IP_BRD_INFO_USER_LED_CTRL_USER_LED_OFF);
        if (_exitNow) {
            break;
        }

        // clock master LED
        NAV820_BusSetup(boardResource,
                        NAV_BUS_MSTR_MASTER, NAV_BUS_MSTR_MASTER);
        usleep(onTimeUsecs);
        NAV820_BusSetup(boardResource,
                        NAV_BUS_MSTR_STAND_ALONE, NAV_BUS_MSTR_STAND_ALONE);
        if (_exitNow) {
            break;
        }
    }
    ILOG << "Done";

    // Release the board and close Navigator cleanly
    NAV_BoardClose(boardResource);
    NAV_BoardFinish();
}
