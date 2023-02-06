/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
 *  Author: Mohamed Chibani <mohamed.chibani@savoirfairelinux.com>
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
#include "connectivity/ip_utils.h"

#include <map>
#include <atomic>
#include <string>
#include <chrono>
#include <functional>

namespace jami {
namespace upnp {

class PMPIGD : public IGD
{
public:
    PMPIGD();
    PMPIGD(const PMPIGD&);
    ~PMPIGD() = default;

    PMPIGD& operator=(PMPIGD&& other) = delete;
    PMPIGD& operator=(PMPIGD& other) = delete;

    bool operator==(IGD& other) const;
    bool operator==(PMPIGD& other) const;

    const std::string toString() const override;
};

} // namespace upnp
} // namespace jami
