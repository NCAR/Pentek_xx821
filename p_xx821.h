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
 * p_xx821.h
 *
 *  Created on: Mar 27, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#ifndef PENTEK_XX821_P_XX821_H_
#define PENTEK_XX821_P_XX821_H_

#include <nav_common.h>
#include <820_include/nav820.h>

#include <boost/thread/recursive_mutex.hpp>

#include <stdint.h>
#include <atomic>
#include <exception>
#include <string>

/// @brief Class which encapsulates access to a Pentek xx821-series transceiver
/// card
class p_xx821 {
public:
    /// @brief constructor
    /// @param boardNum number of the xx821 board to open (1 = first board,
    ///        2 = second board, etc.) [default = 1]j
    /// @throws ConstructError on error in construction
    p_xx821(uint boardNum = 1);

    /// @brief destructor
    virtual ~p_xx821();

    class ConstructError : public virtual std::runtime_error {
    public:
        ConstructError(std::string msg) : std::runtime_error(msg) {}
    };
protected:
    /// @brief Close the Navigator BSP if there are no instantiated objects
    /// which need it.
    ///
    /// This is called from the constructor before throwing an exception, and
    /// from the destructor.
    static void _CloseNavigatorOnLastInstance();

    /// @brief Class-wide count of how many p_xx821 objects are
    /// instantiated
    ///
    /// The instance count is kept so that Pentek's Navigator board support
    /// package can be closed as the last object using it is destroyed.
    ///
    /// This member is atomic to allow for thread-safe access.
    static std::atomic<uint> _InstanceCount;

    /// @brief Return the generic (void*) board handle pointer reinterpreted
    /// as a pointer to NAV_BOARD_RESRC.
    /// @return the generic board handle pointer reinterpreted as a pointer to
    /// NAV_BOARD_RESRC.
    NAV_BOARD_RESRC * _boardResource() const {
        return(reinterpret_cast<NAV_BOARD_RESRC *>(_boardHandle));
    }

    /// @brief Return the base address for the board information registers
    /// @return the base address for the board information registers
    volatile uint32_t * _boardInfoRegBase() const {
        return(_boardResource()->ipBaseAddr.boardInfo);
    }

    /// @brief Return the base address of USER BLOCK 1
    /// @return the base address of USER BLOCK 1
    volatile uint32_t * _userBlock1Base() const {
        return(_boardResource()->ipBaseAddr.userBlock[0]);
    }

    /// @brief Return the base address of USER BLOCK 2
    /// @return the base address of USER BLOCK 2
    volatile uint32_t * _userBlock2Base() const {
        return(_boardResource()->ipBaseAddr.userBlock[1]);
    }
    /// @brief Log board info
    void _logBoardInfo();

    /// @brief Mutex for thread-safe access to instance members.
    mutable boost::recursive_mutex _mutex;

    /// @brief Number of the associated xx821 board
    uint _boardNum;

    /// @brief Board handle pointer returned by Navigator
    void * _boardHandle;

};

#endif /* PENTEK_XX821_P_XX821_H_ */
