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
    ILOG << "Opened Pentek xx821 board " << _boardNum;

    // DMA reads by Pentek boards apparently always fail if PCIe 'max read
    // request size' is 4096 bytes. (E.g., Pentek Navigator's 'transmit_dma'
    // example program will fail with DMA timeouts). Detect that case now,
    // and warn the user that if the board attempts any DMA reads, they will
    // fail.
    uint32_t maxRdReqSzStat;
    status = NAVip_BrdInfoRegs_PcieLinkStat_GetLinkStat(_baseBAR0(),
            NAV_IP_BRD_INFO_PCIE_LINK_STAT_MAX_RD_REQ_SZ,
            &maxRdReqSzStat);
    if (maxRdReqSzStat == NAV_IP_BRD_INFO_PCIE_LINK_STAT_MAX_RD_REQ_SZ_4096_BYTES) {
        WLOG << "";
        WLOG << "XXXXXXXXXXXXXXXXXXXXXX";
        WLOG << "PCIe 'max read request size' for Pentek " << _boardNum;
        WLOG << "is 4096 bytes. A Pentek bug will cause any";
        WLOG << "DMA reads initiated by the board to time out";
        WLOG << "in this case. If DMA reads are required,";
        WLOG << "adjust PCIe 'max read request size' in the";
        WLOG << "computer's BIOS settings to be 2048 bytes or";
        WLOG << "smaller.";
        WLOG << "XXXXXXXXXXXXXXXXXXXXXX";
        WLOG << "";
    } else {
        DLOG << "PCIe 'max read request size' bits for board " << _boardNum <<
                ": 0x" << std::hex << maxRdReqSzStat;
    }
    // We're good, so increment the _InstanceCount.
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
