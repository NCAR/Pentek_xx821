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
    NAV_BOARD_RESRC * _boardResource() {
        return(reinterpret_cast<NAV_BOARD_RESRC *>(_boardHandle));
    }

    /// @brief Return the BAR0 base address for card registers
    /// @return the BAR0 base address for card registers
    volatile uint32_t * _baseBAR0() {
        return(reinterpret_cast<volatile uint32_t *>
                    (_boardResource()->pciInfo.BAR0Base));
    }

    /// @brief Return the BAR2 base address for card registers
    /// @return the BAR2 base address for card registers
    volatile uint32_t * _baseBAR2() {
        return(reinterpret_cast<volatile uint32_t *>
                    (_boardResource()->pciInfo.BAR2Base));
    }

    /// @brief Return the BAR4 base address for card registers
    /// @return the BAR4 base address for card registers
    volatile uint32_t * _baseBAR4() {
        return(reinterpret_cast<volatile uint32_t *>
                    (_boardResource()->pciInfo.BAR4Base));
    }

    /// @brief Mutex for thread-safe access to instance members.
    mutable boost::recursive_mutex _mutex;

    /// @brief Number of the associated xx821 board
    uint _boardNum;

    /// @brief Board handle pointer returned by Navigator
    void * _boardHandle;

};

#endif /* PENTEK_XX821_P_XX821_H_ */