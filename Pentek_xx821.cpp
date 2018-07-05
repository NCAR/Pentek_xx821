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

/*
 * Pentek_xx821.cpp
 *
 *  Created on: Mar 27, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#include <sstream>
#include <logx/Logging.h>

#include "Pentek_xx821.h"

LOGGING("Pentek_xx821")

// Class-wide count of how many objects of this class are instantiated
std::atomic<uint32_t> Pentek_xx821::_InstanceCount(0);

Pentek_xx821::Pentek_xx821(uint16_t boardNum) :
    _mutex(),
    _boardNum(boardNum),
    _boardHandle(NULL)
{
    boost::recursive_mutex::scoped_lock guard(_mutex);

    // Holder for status value returned by NAV_xxx() functions
    int32_t status;

    // Initialize the Navigator board support package when the first instance
    // is being constructed.
    if (_InstanceCount == 0) {
        status = NAV_BoardStartup();
        _AbortCtorOnNavStatusError(status, "NAV_BoardStartup");
        DLOG << "Opened Navigator BSP";
    }

    // Find all Pentek boards in the system
    NAV_DEVICE_INFO *boardList[NAV_MAX_BOARDS];
    int32_t numBoards;
    status = NAV_BoardFind(0, boardList, &numBoards);
    _AbortCtorOnNavStatusError(status, "NAV_BoardFind");

    DLOG << numBoards << ((numBoards == 1) ? " board " : " boards ") << "found";

    // Make sure the requested board number is valid
    if (_boardNum >= numBoards) {
        std::ostringstream os;
        os << "Cannot open Pentex_xx821 board " << _boardNum << " (only " <<
              numBoards << " installed)";
        _AbortConstruction(os.str());
    }

    // Open the board
    _boardHandle = NAV_BoardOpen(boardList[_boardNum], 0);
    if (_boardHandle == NULL) {
        std::ostringstream os;
        os << "NAV_BoardOpen error opening board " << _boardNum;
        _AbortConstruction(os.str());
    }

    // Initialize the context for system specific resources (semaphores,
    // signals, etc.)
    status = NAVsys_Init(&_appSysContext);
    _AbortCtorOnNavStatusError(status, "NAVsys_Init");

    // DMA reads by Pentek boards apparently always fail if PCIe 'max read
    // request size' is 4096 bytes. (E.g., Pentek Navigator's 'transmit_dma'
    // example program will fail with DMA timeouts). Detect that case now,
    // and warn the user that if the board attempts any DMA reads, they will
    // fail.
    uint32_t junk;
    uint32_t maxReadReqSize;
    status = NAV_GetPcieLinkStatus(_boardHandle, &junk, &junk, &junk,
                                   &maxReadReqSize, &junk);
    _AbortCtorOnNavStatusError(status, "NAV_GetPcieLinkStatus");

    if (maxReadReqSize > 2048) {
        WLOG << "_______________________";
        WLOG << "|";
        WLOG << "| PCIe 'max read request size' for board " << _boardNum;
        WLOG << "| is " << maxReadReqSize << " bytes. A Pentek bug will cause any";
        WLOG << "| DMA reads initiated by the board to time out";
        WLOG << "| with this setting. If DMA reads are required,";
        WLOG << "| adjust PCIe 'max read request size' in the";
        WLOG << "| computer's BIOS settings to be 2048 bytes or";
        WLOG << "| smaller.";
        WLOG << "|______________________";
        WLOG << "";
    } else {
        DLOG << "PCIe 'max read request size' for board " << _boardNum <<
                " is " << maxReadReqSize << " bytes";
    }

    // Get the ADC channel count for this board and store it in _adcCount
    status = NAV_GetBoardSpec(_boardHandle, NAV_BOARD_SPEC_ADC_CHAN_COUNT,
                              &_adcCount);
    _AbortCtorOnNavStatusError(status, "NAV_GetBoardSpec");

    // Get the DAC channel count for this board and store it in _dacCount
    status = NAV_GetBoardSpec(_boardHandle, NAV_BOARD_SPEC_DAC_CHAN_COUNT,
                              &_dacCount);
    _AbortCtorOnNavStatusError(status, "NAV_GetBoardSpec");

    // We're good, so increment the _InstanceCount.
    ILOG << "Opened Pentek xx821 board " << _boardNum;
    _InstanceCount++;
}

Pentek_xx821::~Pentek_xx821() {
    boost::recursive_mutex::scoped_lock guard(_mutex);

    // Decrement the instance count.
    _InstanceCount--;

    // Uninit system resources
    int32_t status = NAVsys_UnInit(&_appSysContext);
    if (status != NAV_STAT_OK) {
        ELOG << "NAVsys_UnInit error for Pentek xx821 board " << _boardNum <<
                ": " << NavApiStatus[status];
    }

    // Close the board
    status = NAV_BoardClose(_boardHandle);
    if (status == NAV_STAT_OK) {
        ILOG << "Closed Pentek xx821 board " << _boardNum;
    } else {
        ELOG << "Error closing Pentek xx821 board " << _boardNum << ": " <<
                NavApiStatus[status];
    }

    // Close Navigator BSP if this is the last Pentek_xx821 instance
    _CloseNavigatorOnLastInstance();
}

void
Pentek_xx821::_AbortCtorOnNavStatusError(int status,
                                         const std::string & funcName) {
    if (status != NAV_STAT_OK) {
        std::ostringstream os;
        os << "Error in call to " << funcName << "(): " << NavApiStatus[status];
        _AbortConstruction(os.str());
    }
}

void
Pentek_xx821::_AbortConstruction(const std::string & msg) {
    _CloseNavigatorOnLastInstance();
    throw ConstructError(msg);
}

void
Pentek_xx821::_LogNavigatorError(int status, std::string prefix) {
    if (status != NAV_STAT_OK) {
        ELOG << prefix << ": " << NavApiStatus[status];
    }
}
void
Pentek_xx821::_CloseNavigatorOnLastInstance() {
    if (_InstanceCount == 0) {
        DLOG << "Closing Navigator BSP";
        NAV_BoardFinish();
    }
}

// Convenience function which converts a volatile pointer into a static pointer.
// On systems which print zero as the value of a volatile pointer, this returns
// a static address which will print normally. 
static uint32_t*
printablePtr(volatile uint32_t *vptr) {
    return(const_cast<uint32_t*>(vptr));
}

std::string
Pentek_xx821::boardInfoString() const {
    std::ostringstream os;
    os << "Pentek_xx821 Board " << _boardNum << std::endl;
    os << "    register base addr: " <<
          printablePtr(_boardInfoRegBase()) << std::endl;

    // Log the offset (in 32-bit words) from _boardInfoRegBase() to the start of
    // Pentek USER BLOCK 1
    int wordOffset =
            (_boardResource()->ipBaseAddr.userBlock[0] - _boardInfoRegBase()) / 4;
    os << "    User block 1 offset from register base: 0x" << std::hex <<
          wordOffset << " 32-bit words" << std::endl;

    // Log the offset (in 32-bit words) from _boardInfoBase() to the start of
    // Pentek USER BLOCK 2
    wordOffset =
            (_boardResource()->ipBaseAddr.userBlock[1] - _boardInfoRegBase()) / 4;
    os << "    User block 2 offset from register base: 0x" << std::hex <<
          wordOffset << " 32-bit words" << std::endl;

    // ADC and DAC channel counts
    os << "    " << _adcCount << " ADC channels" << std::endl;
    os << "    " << _dacCount << " DAC channels" << std::endl;

    return(os.str());
}
