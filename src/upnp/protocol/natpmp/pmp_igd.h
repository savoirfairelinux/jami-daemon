/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *	Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../igd.h"
#include "noncopyable.h"
#include "ip_utils.h"

#include <map>
#include <atomic>
#include <string>
#include <chrono>
#include <functional>
#include <natpmp.h>

namespace jami {
namespace upnp {

class PMPIGD : public IGD
{
    friend class NatPmp;

public:
    PMPIGD();
    PMPIGD(const PMPIGD&);
    ~PMPIGD() = default;

    PMPIGD& operator=(PMPIGD&& other) = delete;
    PMPIGD& operator=(PMPIGD& other) = delete;

    bool operator==(IGD& other) const;
    bool operator==(PMPIGD& other) const;

private:
    // Clear all the mappings.
    void clearMappings();

    // Get handle.
    natpmp_t& getHandle() { return natpmpHdl_; }

    // Clear the natpmp handle.
    void clearNatPmpHdl() { memset(&natpmpHdl_, 0, sizeof(natpmpHdl_)); };

    natpmp_t natpmpHdl_; // NatPmp handle.
};

} // namespace upnp
} // namespace jami
