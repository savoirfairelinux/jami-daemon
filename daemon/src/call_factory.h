/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#ifndef CALL_FACTORY_H
#define CALL_FACTORY_H

#include <call.h>
#include <account.h>

#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <utility>

class CallFactory {
    public:
        /**
         * Forbid creation of new calls.
         */
        void forbid();

        /**
         * Remove given call instance from call list.
         */
        void removeCall(Call& call);

        /**
         * Accessor on removeCall with callID than instance.
         */
        void removeCall(const std::string& id);

        // Specializations of following template methods for T = Call
        // are defined in call.cpp

        /**
         * Return if given call exists. Type can optionally be specified.
         */
        template <class T = Call>
        bool hasCall(const std::string& id) const {
            std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

            const auto map = getMap_<T>();
            return map and map->find(id) != map->cend();
        }

        /**
         * Create a new call instance of Call class T, using Account class A.
         * @param id Unique identifier of the call
         * @param type set definitely this call as incoming/outgoing
         * @param account account useed to create this call
         */
        template <class T, class A>
        std::shared_ptr<T> newCall(A& account, const std::string& id, Call::CallType type) {
            if (!allowNewCall_) {
                SFL_WARN("newCall aborted : CallFactory in forbid state");
                return nullptr;
            }

            // Trick: std::make_shared<T> can't build as T constructor is protected
            // and not accessible from std::make_shared.
            // We use a concrete class to bypass this restriction.
            struct ConcreteCall : T {
                    ConcreteCall(A& account, const std::string& id, Call::CallType type)
                        : T(account, id, type) {}
            };

            if (hasCall(id)) {
                SFL_ERR("Call %s is already created", id.c_str());
                return nullptr;
            }

            auto call = std::make_shared<ConcreteCall>(account, id, type);

            {
                std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
                callMaps_[account.getAccountType()].insert(std::make_pair(id, call));
            }

            return call;
        }

        /**
         * Return if calls exist. Type can optionally be specified.
         */
        template <class T = Call>
        bool empty() const {
            std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

            const auto map = getMap_<T>();
            return !map or map->empty();
        }

        /**
         * Erase all calls. Type can optionally be specified.
         */
        template <class T = Call>
        void clear() {
            std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

            auto map = getMap_<T>();
            if (!map) return;

            map->clear();
        }

        /**
         * Return call pointer associated to given ID. Type can optionally be specified.
         */
        template <class T = Call>
        std::shared_ptr<T> getCall(const std::string& id) {
            std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

            const auto map = getMap_<T>();
            if (!map) return nullptr;

            const auto& it = map->find(id);
            if (it == map->cend())
                return nullptr;

            return std::static_pointer_cast<T>(it->second);
        }

        /**
         * Return all calls. Type can optionally be specified.
         */
        template <class T = Call>
        std::vector<std::shared_ptr<T> > getAllCalls() {
            std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
            std::vector<std::shared_ptr<T> > v;

            const auto map = getMap_<T>();
            if (map) {
                for (const auto& it : *map)
                    v.push_back(std::static_pointer_cast<T>(it.second));
            }

            v.shrink_to_fit();
            return v;
        }

        /**
         * Return all call's IDs. Type can optionally be specified.
         */
        template <class T = Call>
        std::vector<std::string> getCallIDs() const {
            std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
            std::vector<std::string> v;

            const auto map = getMap_<T>();
            if (map) {
                for (const auto& it : *map)
                    v.push_back(it.first);
            }

            v.shrink_to_fit();
            return v;
        }

        /**
         * Return number of calls. Type can optionally be specified.
         */
        template <class T = Call>
        std::size_t callCount() {
            std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

            const auto map = getMap_<T>();
            if (!map) return 0;

            return map->size();
        }

    private:
        mutable std::recursive_mutex callMapsMutex_{};

        std::atomic_bool allowNewCall_{true};

        std::map<std::string, CallMap<Call> > callMaps_{};

        template <class T>
        const CallMap<Call>* getMap_() const {
            const auto& itermap = callMaps_.find(T::LINK_TYPE);

            if (itermap != callMaps_.cend())
                return &itermap->second;

            return nullptr;
        }
};

// Specializations defined in call_factory.cpp

template <> bool
CallFactory::hasCall<Call>(const std::string& id) const;

template <> void
CallFactory::clear<Call>();

template <> bool
CallFactory::empty<Call>() const;

template <> std::shared_ptr<Call>
CallFactory::getCall<Call>(const std::string& id);

template <> std::vector<std::shared_ptr<Call> >
CallFactory::getAllCalls<Call>();

template <> std::vector<std::string>
CallFactory::getCallIDs<Call>() const;

template <> std::size_t
CallFactory::callCount<Call>();

#endif // CALL_FACTORY_H
