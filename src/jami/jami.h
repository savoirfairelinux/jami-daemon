/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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
#ifndef LIBJAMI_H
#define LIBJAMI_H

#include "def.h"

#include <vector>
#include <functional>
#include <string>
#include <map>
#include <memory>
#include <type_traits>
#include <filesystem>

#include "trace-tools.h"

namespace libjami {

/* flags for initialization */
enum InitFlag {
    LIBJAMI_FLAG_DEBUG = 1 << 0,
    LIBJAMI_FLAG_CONSOLE_LOG = 1 << 1,
    LIBJAMI_FLAG_AUTOANSWER = 1 << 2,
    LIBJAMI_FLAG_IOS_EXTENSION = 1 << 4,
    LIBJAMI_FLAG_NO_LOCAL_AUDIO = 1 << 6,
    LIBJAMI_FLAG_NO_LOCAL_VIDEO = 1 << 7,
    LIBJAMI_FLAG_NO_LOCAL_MEDIA = LIBJAMI_FLAG_NO_LOCAL_AUDIO | LIBJAMI_FLAG_NO_LOCAL_VIDEO,
    LIBJAMI_FLAG_NO_AUTOSYNC = 1 << 8
};

/**
 * Return the library version as string.
 */
LIBJAMI_PUBLIC const char* version() noexcept;

/**
 * Return the target platform (OS) as a string.
 */
LIBJAMI_PUBLIC const char* platform() noexcept;

/**
 * Initialize globals, create underlaying daemon.
 *
 * @param flags  Flags to customize this initialization
 * @returns      true if initialization succeed else false.
 */
LIBJAMI_PUBLIC bool init(enum InitFlag flags) noexcept;

/**
 * Start asynchronously daemon created by init().
 * @returns true if daemon started successfully
 */
LIBJAMI_PUBLIC bool start(const std::filesystem::path& config_file = {}) noexcept;

/**
 * Stop and freeing any resource allocated by daemon
 */
LIBJAMI_PUBLIC void fini() noexcept;

LIBJAMI_PUBLIC bool initialized() noexcept;

/**
 * Control log handlers.
 *
 * @param whom  Log handler to control
 */
LIBJAMI_PUBLIC void logging(const std::string& whom, const std::string& action) noexcept;

/* External Callback Dynamic Utilities
 *
 * The library provides to users a way to be acknowledged
 * when daemon's objects have a state change.
 * The user is aware of this changement when the deamon calls
 * a user-given callback.
 * Daemon handles many of these callbacks, one per event type.
 * The user registers his callbacks using registerXXXXHandlers() functions.
 * As each callback has its own function signature,
 * to keep compatibility over releases we don't let user directly provides
 * his callbacks as it or through a structure.
 * This way brings ABI violation if we need to change the order
 * and/or the existence of any callback type.
 * Thus the user have to pass them using following template classes
 * and functions, that wraps user-callback in a generic and ABI-compatible way.
 */

/* Generic class to transit user callbacks to daemon library.
 * Used conjointly with std::shared_ptr to hide the concrete class.
 * See CallbackWrapper template for details.
 */
class LIBJAMI_PUBLIC CallbackWrapperBase
{
protected:
    // Because post() needs Manager, it should be defined in a .cpp
    // so not in a templated class.
    // Also we do not want this method to be public in the API.
    void post(std::function<void()> cb);
};

/* Concrete class of CallbackWrapperBase.
 * This class wraps callbacks of a specific signature.
 * Also used to obtain the user callback from a CallbackWrapperBase shared ptr.
 *
 * This class is CopyConstructible, CopyAssignable, MoveConstructible
 * and MoveAssignable.
 */
template<typename TProto>
class CallbackWrapper : public CallbackWrapperBase
{
private:
    using TFunc = std::function<TProto>;
    TFunc cb_; // The user-callback

public:
    const char* file_;
    uint32_t linum_;

    // Empty wrapper: no callback associated.
    // Used to initialize internal callback arrays.
    CallbackWrapper() noexcept {}

    // Create and initialize a wrapper to given callback.
    CallbackWrapper(TFunc&& func, const char* filename, uint32_t linum) noexcept
        : cb_(std::forward<TFunc>(func))
        , file_(filename)
        , linum_(linum)
    {}

    // Create and initialize a wrapper from a generic CallbackWrapperBase
    // shared pointer.
    // Note: the given callback is copied into internal storage.
    CallbackWrapper(const std::shared_ptr<CallbackWrapperBase>& p) noexcept
    {
        if (p) {
            auto other = (CallbackWrapper<TProto>*) p.get();

            cb_ = other->cb_;
            file_ = other->file_;
            linum_ = other->linum_;
        }
    }

    // Return user-callback reference.
    // The returned std::function can be null-initialized if no callback
    // has been set.
    constexpr const TFunc& operator*() const noexcept { return cb_; }

    // Return boolean true value if a non-null callback has been set
    constexpr explicit operator bool() const noexcept { return static_cast<bool>(cb_); }
};

/* Concrete class of CallbackWrapperBase.
 * This class wraps callbacks of a specific signature.
 * Used to retrigger callbacks on a io context to avoid lock if signals cannot
 * be emitted while a method is called.
 * Also used to obtain the user callback from a CallbackWrapperBase shared ptr.
 *
 * This class is CopyConstructible, CopyAssignable, MoveConstructible
 * and MoveAssignable.
 */
template<typename TProto>
class SerializedCallbackWrapper : public CallbackWrapperBase
{
private:
    using TFunc = std::function<TProto>;
    TFunc cb_; // The user-callback

    // This is quite a ugly method used to transmit templated TFunc with their arguments in the
    // ioContext of the manager to avoid locks for signals.
    template <typename TCallback>
    auto ioContextWrapper(TCallback&& fun)
    {
        return [this, fun{std::move(fun)}](auto&&... args) -> decltype(fun(std::forward<decltype(args)>(args)...))
        {
            post([fun{std::move(fun)}, forwardArgs=std::make_tuple(std::move(args)...)]() mutable {
                std::apply(std::move(fun), std::move(forwardArgs));
            });
        };
    }

public:
    const char* file_;
    uint32_t linum_;

    // Empty wrapper: no callback associated.
    // Used to initialize internal callback arrays.
    SerializedCallbackWrapper() noexcept {}

    // Create and initialize a wrapper to given callback.
    SerializedCallbackWrapper(TFunc&& func, const char* filename, uint32_t linum) noexcept
        : file_(filename)
        , linum_(linum)
    {
        cb_ = ioContextWrapper(func);
    }

    // Create and initialize a wrapper from a generic CallbackWrapperBase
    // shared pointer.
    // Note: the given callback is copied into internal storage.
    SerializedCallbackWrapper(const std::shared_ptr<CallbackWrapperBase>& p) noexcept
    {
        if (p) {
            auto other = (CallbackWrapper<TProto>*) p.get();

            cb_ = ioContextWrapper(other.cb_);
            file_ = other->file_;
            linum_ = other->linum_;
        }
    }

    // Return user-callback reference.
    // The returned std::function can be null-initialized if no callback
    // has been set.
    constexpr const TFunc& operator*() const noexcept { return cb_; }

    // Return boolean true value if a non-null callback has been set
    constexpr explicit operator bool() const noexcept { return static_cast<bool>(cb_); }
};

/**
 * Return an exportable callback object.
 * This object is a std::pair of a string and a CallbackWrapperBase shared_ptr.
 * This last wraps given callback in a ABI-compatible way.
 * Note: this version accepts callbacks as rvalue only.
 */
template<typename Ts>
std::pair<std::string, std::shared_ptr<CallbackWrapperBase>>
exportable_callback(std::function<typename Ts::cb_type>&& func,
                    const char* file = CURRENT_FILENAME(),
                    uint32_t linum = CURRENT_LINE())
{
    return std::make_pair((const std::string&) Ts::name,
                          std::make_shared<CallbackWrapper<typename Ts::cb_type>>(
                              std::forward<std::function<typename Ts::cb_type>>(func), file, linum));
}

template<typename Ts>
std::pair<std::string, std::shared_ptr<CallbackWrapperBase>>
exportable_serialized_callback(std::function<typename Ts::cb_type>&& func,
                    const char* file = CURRENT_FILENAME(),
                    uint32_t linum = CURRENT_LINE())
{
    return std::make_pair((const std::string&) Ts::name,
                          std::make_shared<SerializedCallbackWrapper<typename Ts::cb_type>>(
                              std::forward<std::function<typename Ts::cb_type>>(func), file, linum));
}

LIBJAMI_PUBLIC void registerSignalHandlers(
    const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);
LIBJAMI_PUBLIC void unregisterSignalHandlers();

using MediaMap = std::map<std::string, std::string>;

} // namespace libjami

#endif /* LIBJAMI_H */
