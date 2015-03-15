/*
 *  Copyright (C) 2014-2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */
#ifndef DRING_H
#define DRING_H

#include <vector>
#include <functional>
#include <string>
#include <map>
#include <memory>
#include <type_traits>

namespace DRing {

/* flags for initialization */
enum InitFlag {
    DRING_FLAG_DEBUG=1,
    DRING_FLAG_CONSOLE_LOG=2,
};

/**
 * Return the library version as string.
 */
const char* version() noexcept;

/**
 * Initialize globals, create underlaying daemon.
 *
 * @param flags  Flags to customize this initialization
 * @returns      true if initialization succeed else false.
 */
bool init(enum InitFlag flags) noexcept;

/**
 * Start asynchronously daemon created by init().
 * @returns true if daemon started successfuly
 */
bool start(const std::string& config_file={}) noexcept;

/**
 * Stop and freeing any resource allocated by daemon
 */
void fini() noexcept;

/**
 * Poll daemon events.
 * This function has to be called by user at a fixed frequency
 * to let daemon checks its internal ressources and io and
 * manages events reported by them.
 */
void pollEvents() noexcept;

/* External Callback Dynamic Utilities
 *
 * The library provides to users a way to be acknowledged
 * when daemon's objects have a state change.
 * The user is aware of this changement wen the deamon calls
 * a user-given callback.
 * Daemon handles many of these callbacks, one per event type.
 * The user registers his callback when he calls daemon DRing:init().
 * Each callback has its own function signature.
 * To keep ABI compatibility we don't let user directly provides
 * his callbacks as it or through a structure.
 * This way bring ABI violation if we need to change the order
 * and/or the existance of any callback type.
 * Thus the user have to pass them using following template classes
 * and functions, that wraps user-callback in a generic and ABI-compatible way.
 */

/* Generic class to transit user callbacks to daemon library.
 * Used conjointly with std::shared_ptr to hide the concrete class.
 * See CallbackWrapper template for details.
 */
class CallbackWrapperBase {};

/* Concrete class of CallbackWrapperBase.
 * This class wraps callbacks of a specific signature.
 * Also used to obtain the user callback from a CallbackWrapperBase shared ptr.
 *
 * This class is CopyConstructible, CopyAssignable, MoveConstructible
 * and MoveAssignable.
 */
template <typename TProto>
class CallbackWrapper : public CallbackWrapperBase {
    private:
        using TFunc = std::function<TProto>;
        TFunc cb_; // The user-callback

    public:
        // Empty wrapper: no callback associated.
        // Used to initialize internal callback arrays.
        CallbackWrapper() noexcept {}

        // Create and initialize a wrapper to given callback.
        CallbackWrapper(TFunc&& func) noexcept {
            cb_ = std::forward<TFunc>(func);
        }

        // Create and initialize a wrapper from a generic CallbackWrapperBase
        // shared pointer.
        // Note: the given callback is copied into internal storage.
        CallbackWrapper(std::shared_ptr<CallbackWrapperBase> p) noexcept {
            if (p)
                cb_ = ((CallbackWrapper<TProto>*)p.get())->cb_;
        }

        // Return user-callback reference.
        // The returned std::function can be null-initialized if no callback
        // has been set.
        constexpr const TFunc& operator *() const noexcept {
            return cb_;
        }

        // Return boolean true value if a non-null callback has been set
        constexpr explicit operator bool() const noexcept {
            return static_cast<bool>(cb_);
        }
};

/**
 * Return an exportable callback object.
 * This object is a std::pair of a string and a CallbackWrapperBase shared_ptr.
 * This last wraps given callback in a ABI-compatible way.
 * Note: this version accepts callbacks as rvalue only.
 */
template <typename Ts>
std::pair<std::string, std::shared_ptr<CallbackWrapperBase>>
exportable_callback(std::function<typename Ts::cb_type>&& func) {
    return std::make_pair((const std::string&)Ts::name,
                          std::make_shared<CallbackWrapper<typename Ts::cb_type>>
                          (std::forward<std::function<typename Ts::cb_type>>(func)));
}

} // namespace DRing

#endif /* DRING_H */
