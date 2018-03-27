// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* 
// ** Copyright UCAR (c) 2018                                         
// ** University Corporation for Atmospheric Research (UCAR)                 
// ** National Center for Atmospheric Research (NCAR)                        
// ** Boulder, Colorado, USA                                                 
// ** BSD licence applies - redistribution and use in source and binary      
// ** forms, with or without modification, are permitted provided that       
// ** the following conditions are met:                                      
// ** 1) If the software is modified to produce derivative works,            
// ** such modified software should be clearly marked, so as not             
// ** to confuse it with the version available from UCAR.                    
// ** 2) Redistributions of source code must retain the above copyright      
// ** notice, this list of conditions and the following disclaimer.          
// ** 3) Redistributions in binary form must reproduce the above copyright   
// ** notice, this list of conditions and the following disclaimer in the    
// ** documentation and/or other materials provided with the distribution.   
// ** 4) Neither the name of UCAR nor the names of its contributors,         
// ** if any, may be used to endorse or promote products derived from        
// ** this software without specific prior written permission.               
// ** DISCLAIMER: THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS  
// ** OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED      
// ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.    
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

int _nToggles = 50;     ///< number of times leds are toggled
double _waitSecs = 0.2; ///< delay between toggles (secs)
bool _exitNow = false;  ///< early exit if this becomes true


/// Parse the command line options
void parseOptions(int argc, char** argv)
{
  
  // get the options
  po::options_description descripts("Options");
  descripts.add_options()
    ("help", "Describe options")
    ("nToggles", po::value<int>(&_nToggles),
     "Number of times lights are toggled [50]")
    ("waitSecs", po::value<double>(&_waitSecs), 
     "Toggle period in seconds [0.2]")
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

// Interrupt handler to trigger early exit
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
    void * boardHandle = NAV_BoardSelect(numBoards, 0, boardList[0], 0);
    if (boardHandle) {
        DLOG << "Opened xx821 board";
    } else {
        ELOG << "Failed to open board!";
        exit(1);
    }

    // Alternately toggle the USR LED and MAS LED _nToggles times.
    NAV_BOARD_RESRC * boardResource = reinterpret_cast<NAV_BOARD_RESRC *>(boardHandle);
    volatile uint32_t * BAR0Base =
            reinterpret_cast<volatile uint32_t *>(boardResource->pciInfo.BAR0Base);
    for (int t = 0; t < _nToggles; t++) {
        ILOG << "Cycle " << t + 1 << " of " << _nToggles;
        // USR LED on/MAS LED off for half of the toggle period
        NAVip_BrdInfoRegs_UserLed_SetUserLedEnable(BAR0Base,
            NAV_IP_BRD_INFO_USER_LED_CTRL_USER_LED_ON);
        NAV820_BusSetup(boardResource,
                        NAV_BUS_MSTR_STAND_ALONE,
                        NAV_BUS_MSTR_STAND_ALONE);
        usleep(1000000 * (0.5 * _waitSecs));
        if (_exitNow) {
            break;
        }

        // USR LED off/MAS LED on for half of the toggle period
        NAVip_BrdInfoRegs_UserLed_SetUserLedEnable(BAR0Base,
            NAV_IP_BRD_INFO_USER_LED_CTRL_USER_LED_OFF);
        NAV820_BusSetup(boardResource, NAV_BUS_MSTR_STAND_ALONE,
                        NAV_BUS_MSTR_STAND_ALONE);
        status = NAV820_BusSetup(boardResource,
                                 NAV_BUS_MSTR_MASTER,
                                 NAV_BUS_MSTR_MASTER);
        usleep(1000000 * (0.5 * _waitSecs));
        if (_exitNow) {
            break;
        }
    }

    // Make sure the LEDs are off before we exit
    NAVip_BrdInfoRegs_UserLed_SetUserLedEnable(BAR0Base,
        NAV_IP_BRD_INFO_USER_LED_CTRL_USER_LED_OFF);
    NAV820_BusSetup(boardResource, NAV_BUS_MSTR_STAND_ALONE,
                    NAV_BUS_MSTR_STAND_ALONE);

    // Release the board and close Navigator cleanly
    NAV_BoardClose(boardHandle);
    NAV_BoardFinish();
}

//  // Initialize the library
//
//  DWORD dwStatus = PTK714X_LibInit();
//  if (PTK714X_STATUS_OK != dwStatus) {
//    return (exitHandler (1, NULL));
//  }
//  
//  // Find and open the next PTK714X device
//  // user will be asked to pick the device num
//  
//  DWORD BAR0Base;
//  DWORD BAR2Base;
//  DWORD slot = -1; // forces user interaction if more than 1 pentek
//  void *hDev = PTK714X_DeviceFindAndOpen(&slot, &BAR0Base, &BAR2Base);
//  if (hDev == NULL) {
//    return (exitHandler (2, hDev));
//  }
//  
//  // Initialize the 7142 register address table
//  
//  P7142_REG_ADDR p7142Regs;  /* 7142 register table */
//  P7142InitRegAddr(BAR0Base, BAR2Base, &p7142Regs);
//
//  // check if module is a 7142
//
//  unsigned int moduleId;
//  P7142_GET_MODULE_ID(p7142Regs.BAR2RegAddr.idReadout, moduleId);
//  if (moduleId != P7142_MODULE_ID) {
//    cerr << "Pentek card " << slot + 1
//         << " is not a 7142!" << endl;
//    cerr << "Expected 0x" << hex << P7142_MODULE_ID << 
//      ", and got 0x" << moduleId << dec << endl;
//    return -1;
//  }
//
//  // print status
//
//  cerr << "Found Pentek 7142 device";
//  cerr << "  device addr: " << hex << hDev << endl;
//  cerr << "  slot: " << dec << slot << endl;
//  cerr << "  BAR0Base: " << hex << (void *)BAR0Base;
//  cerr << "  BAR2Base: " << hex << (void *)BAR2Base;
//  cerr << dec;
//  cerr << endl;
//
//  // get master bus controls
//
//  uint32_t masterBusAControl;
//  P7142_REG_READ(BAR2Base + P7142_MASTER_BUS_A_CONTROL, masterBusAControl);
//  
//  uint32_t masterBusBControl;
//  P7142_REG_READ(BAR2Base + P7142_MASTER_BUS_B_CONTROL, masterBusBControl);
//
//  cerr << "Initial state:" << endl;
//  cerr << "  masterBusAControl: " << hex << masterBusAControl << endl;
//  cerr << "  masterBusBControl: " << hex << masterBusBControl << endl;
//  cerr << dec;
//
//  // toggle master control on and off, to toggle LEDs
//
//  uint32_t bothOnA = masterBusAControl | 0x00000003;
//  uint32_t bothOnB = masterBusBControl | 0x00000003;
//  
//  uint32_t masterOnA = bothOnA | 0x00000001;
//  uint32_t masterOnB = bothOnB | 0x00000001;
//
//  uint32_t masterOffA = bothOnA & 0xfffffffe;
//  uint32_t masterOffB = bothOnB & 0xfffffffe;
//
//  uint32_t termOnA = bothOnA | 0x00000002;
//  uint32_t termOnB = bothOnB | 0x00000002;
//  
//  uint32_t termOffA = bothOnA & 0xfffffffd;
//  uint32_t termOffB = bothOnB & 0xfffffffd;
//
//  useconds_t sleepTime = (int) (_waitSecs * 1.0e6);
//
//  cerr << "--->> Setting all on" << endl;
//    
//  P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_A_CONTROL, bothOnA);
//  P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_B_CONTROL, bothOnB);
//
//  for (int ii = 0; ii < _nToggles; ii++) {
//
//    cerr << "--->> Toggling master off" << endl;
//    
//    P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_A_CONTROL, masterOffA);
//    P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_B_CONTROL, masterOffB);
//
//    usleep(sleepTime);
//
//    cerr << "--->> Toggling master on" << endl;
//    
//    P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_A_CONTROL, masterOnA);
//    P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_B_CONTROL, masterOnB);
//    
//    usleep(sleepTime);
//    
//    cerr << "--->> Toggling term off" << endl;
//    
//    P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_A_CONTROL, termOffA);
//    P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_B_CONTROL, termOffB);
//
//    usleep(sleepTime);
//
//    cerr << "--->> Toggling term on" << endl;
//    
//    P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_A_CONTROL, termOnA);
//    P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_B_CONTROL, termOnB);
//    
//    usleep(sleepTime);
//    
//  }
//
//  // reset to the initial state
//    
//  // cerr << "--->> Resetting to initial state" << endl;
//  // P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_A_CONTROL, masterBusAControl);
//  // P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_B_CONTROL, masterBusBControl);
//
//  // set all on to exit
//    
//  cerr << "--->> Setting all on" << endl;
//    
//  P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_A_CONTROL, bothOnA);
//  P7142_REG_WRITE(BAR2Base + P7142_MASTER_BUS_B_CONTROL, bothOnB);
//
//  // return
//
//  return (exitHandler (0, hDev));

