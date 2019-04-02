/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#include "callmanager_interface.h"
#include "configurationmanager_interface.h"
#include "presencemanager_interface.h"
#include "datatransfer_interface.h"

#ifdef ENABLE_VIDEO
#include "videomanager_interface.h"
#endif

#include "dring.h"
#include "logger.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include <exception>
#include <memory>
#include <map>
#include <utility>
#include <string>

namespace jami {

using SignalHandlerMap = std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>;
extern SignalHandlerMap& getSignalHandlers();

/*
 * Find related user given callback and call it with given
 * arguments.
 */
template <typename Ts, typename ...Args>
static void emitSignal(Args...args) {
    const auto& handlers = getSignalHandlers();
    if (auto cb = *DRing::CallbackWrapper<typename Ts::cb_type>(handlers.at(Ts::name))) {
        try {
            cb(args...);
        } catch (std::exception& e) {
            JAMI_ERR("Exception during emit signal %s:\n%s", Ts::name, e.what());
        }
    }
}

template <typename Ts>
std::pair<std::string, std::shared_ptr<DRing::CallbackWrapper<typename Ts::cb_type>>>
exported_callback() {
    return std::make_pair((const std::string&)Ts::name, std::make_shared<DRing::CallbackWrapper<typename Ts::cb_type>>());
}

} // namespace jami
