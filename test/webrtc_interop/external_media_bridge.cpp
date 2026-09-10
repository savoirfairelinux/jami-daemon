/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

// External media interop bridge: exercises the libjami external media
// endpoint API (placeCallWithExternalMedia / RemoteSdpReceived) against a
// real Jami peer. A WebRTC endpoint (driven by external_media_interop.js)
// provides the SDP offer; the callee is a regular Jami account whose media
// stack connects directly to the WebRTC endpoint.
//
// Protocol (stdin/stdout, one line per message, SDP payloads in base64):
//   -> READY bob=<uri>
//   <- CALL offer <base64 sdp>
//   -> SDP type=answer payload=<base64 sdp>
//   <- DONE
//   -> RESULT status=ok
//
// Must be run from the test/unitTest directory (uses actors/alice-bob.yml).

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "base64.h"
#include "jami.h"
#include "jami/callmanager_interface.h"
#include "jami/media_const.h"
#include "jamidht/jamiaccount.h"
#include "manager.h"
#include "../unitTest/common.h"

using namespace std::literals::chrono_literals;
using namespace jami;

namespace {

struct Command
{
    std::string name {};
    std::string type {};
    std::string payload {};
};

std::optional<Command>
readCommand()
{
    std::string line;
    if (!std::getline(std::cin, line))
        return std::nullopt;

    std::istringstream iss(line);
    Command command;
    if (!(iss >> command.name))
        return std::nullopt;
    iss >> command.type;
    if (!command.type.empty())
        iss >> command.payload;
    return command;
}

void
emitEvent(const std::string& name, const std::map<std::string, std::string>& fields = {})
{
    std::cout << name;
    for (const auto& [key, value] : fields) {
        std::cout << ' ' << key << '=' << value;
    }
    std::cout << std::endl;
}

std::string
decodePayload(const std::string& payload)
{
    const auto decoded = base64::decode(payload);
    return {decoded.begin(), decoded.end()};
}

} // namespace

int
main()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (!Manager::instance().initialized && !libjami::start("jami-sample.yml")) {
        std::cerr << "failed to start libjami" << std::endl;
        return 1;
    }

    int exitCode = 1;
    std::string aliceId, bobId;
    try {
        auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
        aliceId = actors["alice"];
        bobId = actors["bob"];

        auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
        const auto bobUri = bobAccount->getUsername();

        std::mutex mtx;
        std::condition_variable cv;
        std::string bobCallId;
        std::string aliceAnswer;
        std::atomic<int> callStopped {0};

        std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers;
        handlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCall>(
            [&](const std::string& accountId,
                const std::string& callId,
                const std::string&,
                const std::vector<std::map<std::string, std::string>>&) {
                if (accountId == bobId) {
                    {
                        std::lock_guard lk {mtx};
                        bobCallId = callId;
                    }
                    // Answer with regular local media: Bob's media stack
                    // connects directly to the WebRTC endpoint.
                    std::vector<std::map<std::string, std::string>> media;
                    media.emplace_back(
                        std::map<std::string, std::string> {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
                                                             libjami::Media::MediaAttributeValue::AUDIO},
                                                            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
                                                            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}});
                    libjami::acceptWithMedia(accountId, callId, media);
                }
                cv.notify_all();
            }));
        handlers.insert(libjami::exportable_callback<libjami::CallSignal::RemoteSdpReceived>(
            [&](const std::string& accountId, const std::string&, const std::string& sdp) {
                if (accountId == aliceId) {
                    std::lock_guard lk {mtx};
                    aliceAnswer = sdp;
                }
                cv.notify_all();
            }));
        handlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
            [&](const std::string&, const std::string&, const std::string& state, signed) {
                if (state == "OVER")
                    callStopped += 1;
                cv.notify_all();
            }));
        libjami::registerSignalHandlers(handlers);

        emitEvent("READY", {{"bob", bobUri}});

        const auto callCmd = readCommand();
        if (!callCmd || callCmd->name != "CALL" || callCmd->payload.empty())
            throw std::runtime_error("expected CALL command with an SDP offer");
        const auto offer = decodePayload(callCmd->payload);

        const auto aliceCallId = libjami::placeCallWithExternalMedia(aliceId, bobUri, offer);
        if (aliceCallId.empty())
            throw std::runtime_error("placeCallWithExternalMedia failed");

        {
            std::unique_lock lk {mtx};
            if (!cv.wait_for(lk, 60s, [&] { return !aliceAnswer.empty(); }))
                throw std::runtime_error("timed out waiting for the SDP answer");
        }

        emitEvent("SDP", {{"type", "answer"}, {"payload", base64::encode(aliceAnswer)}});

        const auto doneCmd = readCommand();
        if (!doneCmd || doneCmd->name != "DONE")
            throw std::runtime_error("expected DONE command");

        libjami::hangUp(aliceId, aliceCallId);
        {
            std::unique_lock lk {mtx};
            cv.wait_for(lk, 30s, [&] { return callStopped >= 2; });
        }

        emitEvent("RESULT", {{"status", "ok"}});
        libjami::unregisterSignalHandlers();
        exitCode = 0;
    } catch (const std::exception& e) {
        std::cerr << "external media bridge error: " << e.what() << std::endl;
        emitEvent("RESULT", {{"status", "error"}, {"message", e.what()}});
    }

    if (!aliceId.empty())
        wait_for_removal_of({aliceId, bobId});
    libjami::fini();
    return exitCode;
}
