/*
 *  Copyright (C) 2024 Savoir-faire Linux Inc.
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

#include "typers.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"

static constexpr std::chrono::steady_clock::duration COMPOSING_TIMEOUT {std::chrono::seconds(12)};

namespace jami {

Typers::Typers(const std::shared_ptr<JamiAccount>& acc, const std::string& convId)
    : ioContext_(Manager::instance().ioContext())
    , acc_(acc)
    , accountId_(acc->getAccountID())
    , convId_(convId)
    , selfUri_(acc->getUsername())
{

}

Typers::~Typers()
{
    for (auto& watcher : watcher_) {
        watcher.second->cancel();
    }
    watcher_.clear();
}

std::string
getIsComposing(const std::string& conversationId, bool isWriting)
{
    // implementing https://tools.ietf.org/rfc/rfc3994.txt
    return fmt::format("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                       "<isComposing><state>{}</state>{}</isComposing>",
                       isWriting ? "active"sv : "idle"sv,
                       conversationId.empty()
                           ? ""
                           : "<conversation>" + conversationId + "</conversation>");
}

void
Typers::addTyper(const std::string &typer)
{
    auto acc = acc_.lock();
    if (!acc || !acc->isComposingEnabled())
        return;
    auto [it, res] = watcher_.insert({typer, std::make_unique<asio::steady_timer>(*ioContext_)});
    if (res) {
        auto& watcher = it->second;
        // Check next member
        watcher->expires_at(std::chrono::steady_clock::now() + COMPOSING_TIMEOUT);
        watcher->async_wait(
            std::bind(&Typers::onTyperTimeout,
                        shared_from_this(),
                        std::placeholders::_1,
                        typer));

        if (typer != selfUri_)
            emitSignal<libjami::ConfigurationSignal::ComposingStatusChanged>(accountId_,
                                                                            convId_,
                                                                            typer,
                                                                            1);
    }
    if (typer == selfUri_) {
        acc->sendInstantMessage(convId_,
                               {{MIME_TYPE_IM_COMPOSING, getIsComposing(convId_, true)}});
    }
}

void
Typers::removeTyper(const std::string &typer)
{
    auto acc = acc_.lock();
    if (!acc || !acc->isComposingEnabled())
        return;
    if (watcher_.erase(typer)) {
        if (typer == selfUri_) {
            acc->sendInstantMessage(convId_,
                                {{MIME_TYPE_IM_COMPOSING, getIsComposing(convId_, false)}});
        } else {
            emitSignal<libjami::ConfigurationSignal::ComposingStatusChanged>(accountId_,
                                                                                convId_,
                                                                                typer,
                                                                                0);
        }
    }
}

void
Typers::onTyperTimeout(const asio::error_code& ec, const std::string &typer)
{
    if (ec == asio::error::operation_aborted)
        return;
    removeTyper(typer);
}

} // namespace jami
