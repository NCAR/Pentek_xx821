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
 * p_xx821.cpp
 *
 *  Created on: Mar 27, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#include <sstream>
#include <logx/Logging.h>

#include "p_xx821.h"

LOGGING("p_xx821")

// Class-wide count of how many objects of this class are instantiated
std::atomic<uint32_t> p_xx821::_InstanceCount(0);

p_xx821::p_xx821(uint boardNum) :
    _mutex(),
    _boardNum(boardNum),
    _boardHandle(NULL)
{
    boost::recursive_mutex::scoped_lock guard(_mutex);

    // Initialize the Navigator board support package
    int32_t status = NAV_BoardStartup();
    if (status != NAV_STAT_OK) {
        _CloseNavigatorOnLastInstance();
        std::ostringstream os;
        os << "Error initializing Navigator BSP: " << NavApiStatus[status];
        throw ConstructError(os.str());
    }

    // Find all Pentek boards in the system
    NAV_DEVICE_INFO *boardList[NAV_MAX_BOARDS];
    int32_t numBoards;
    status = NAV_BoardFind(0, boardList, &numBoards);
    if (status != NAV_STAT_OK)
    {
        _CloseNavigatorOnLastInstance();
        std::ostringstream os;
        os << "Error from NAV_BoardFind: " << NavApiStatus[status];
        throw ConstructError(os.str());
    }

    DLOG << numBoards << ((numBoards == 1) ? " board " : " boards ") << "found";

    // Make sure the requested board number is valid
    if (_boardNum > numBoards) {
        _CloseNavigatorOnLastInstance();
        std::ostringstream os;
        os << "Cannot open Pentex_xx821 board " << _boardNum << " (only " <<
              numBoards << " installed)";
        throw ConstructError(os.str());
    }

    // Open the board
    _boardHandle = NAV_BoardOpen(boardList[_boardNum - 1], 0);
    if (_boardHandle == NULL) {
        _CloseNavigatorOnLastInstance();
        std::ostringstream os;
        os << "NAV_BoardOpen error opening board " << _boardNum;
        throw ConstructError(os.str());
    }

    // Log information about the board
    _logBoardInfo();

    // DMA reads by Pentek boards apparently always fail if PCIe 'max read
    // request size' is 4096 bytes. (E.g., Pentek Navigator's 'transmit_dma'
    // example program will fail with DMA timeouts). Detect that case now,
    // and warn the user that if the board attempts any DMA reads, they will
    // fail.
    uint32_t junk;
    uint32_t maxReadReqSize;
    status = NAV_GetPcieLinkStatus(_boardHandle, &junk, &junk, &junk,
                                   &maxReadReqSize, &junk);
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

    // We're good, so increment the _InstanceCount.
    ILOG << "Opened Pentek xx821 board " << _boardNum;
    _InstanceCount++;
}

p_xx821::~p_xx821() {
    boost::recursive_mutex::scoped_lock guard(_mutex);

    // Decrement the instance count.
    _InstanceCount--;

    // Close the board
    int32_t status = NAV_BoardClose(_boardHandle);
    if (status == NAV_STAT_OK) {
        ILOG << "Closed Pentek xx821 board " << _boardNum;
    } else {
        ELOG << "Error closing Pentek xx821 board " << _boardNum << ": " <<
                NavApiStatus[status];
    }

    // Close Navigator BSP if this is the last p_xx821 instance
    _CloseNavigatorOnLastInstance();
}

void
p_xx821::_CloseNavigatorOnLastInstance() {
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

void
p_xx821::_logBoardInfo() {
    DLOG << "Pentek xx821 board " << _boardNum << " info:";
    DLOG << "    board info register base: " <<
            printablePtr(_boardInfoRegBase());
    ILOG << "    RAM DMA write base: " <<
            printablePtr(_boardResource()->ipBaseAddr.ramDmaWrite);

	// Log the offset (in 32-bit words) from _boardInfoBase() to the start of
	// Pentek USER BLOCK 1
    int wordOffset =
            (_boardResource()->ipBaseAddr.userBlock[0] - _boardInfoRegBase()) / 4;
    ILOG << "    User block 1 register base offset: 0x" << std::hex <<
            wordOffset << "32-bit words";

	// Log the offset (in 32-bit words) from _boardInfoBase() to the start of
	// Pentek USER BLOCK 2
    wordOffset =
            (_boardResource()->ipBaseAddr.userBlock[1] - _boardInfoRegBase()) / 4;
    ILOG << "    User block 2 register base offset: 0x" << std::hex <<
            wordOffset << "32-bit words";
}
