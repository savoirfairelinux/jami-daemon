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
#include <stdexcept>

typedef std::map<std::string, std::shared_ptr<Call> > CallMap;

class CallFactory {
    public:
        CallFactory() {};

        /**
         * Create a new call instance of Call class T, using Account class A.
         * @param id Unique identifier of the call
         * @param type set definitely this call as incoming/outgoing
         * @param account account useed to create this call
         */
        template <class T, class A>
        std::shared_ptr<T> newCall(A& account, const std::string& id, Call::CallType type) {
            // Trick: std::make_shared<T> can't build as T constructor is protected
            // and not accessible from std::make_shared.
            // We use a concrete class to bypass this restriction.
            struct ConcreteCall : T {
                    ConcreteCall(A& account, const std::string& id, Call::CallType type)
                        : T(account, id, type) {}
            };
            auto call = std::make_shared<ConcreteCall>(account, id, type);
            registerCall(call);
            return std::dynamic_pointer_cast<T>(call);
        }

        bool hasCall(const std::string& id) {
            for (const auto& item : callMaps_) {
                const auto& map = item.second;
                if (map.find(id) != map.cend())
                    return true;
            }

            return false;
        }

        void removeCall(const std::string& id);

        void removeCall(Call& call);

        std::vector<std::string> getCallIDs() {
            std::vector<std::string> v;

            for (const auto& item : callMaps_) {
                const CallMap& map = item.second;
                for (const auto& it : map)
                    v.push_back(it.first);
            }

            v.shrink_to_fit();
            return v;
        }

        void registerCall(std::shared_ptr<Call> call) {
            if (hasCall(call->getCallId())) {
                std::stringstream msg;
                msg << "Call " << call->getCallId() << " is already in the call map";
                throw std::runtime_error(msg.str());
            }

            // Auto recording of Call pointer
            const auto& pair = std::make_pair(call->getCallId(), call);
            callMaps_[call->getAccount().getAccountType()].insert(pair);
            call->getAccount().attachCall(call);
        }

        // Specializations of following template methods for T = Call
        // are defined in call.cpp

        template <class T = Call>
        bool empty() {
            const auto& iter = callMaps_.find(T::LINK_TYPE);
            if (iter != callMaps_.cend())
                return iter->second.empty();
            return true; // no map = no calls
        }

        template <class T = Call>
        std::shared_ptr<T> getCall(const std::string& id) {
            const auto& itermap = callMaps_.find(T::LINK_TYPE);
            if (itermap == callMaps_.cend())
                return nullptr;

            const auto& map = itermap->second;
            const auto& iter = map.find(id);

            if (iter != map.cend())
                return std::dynamic_pointer_cast<T>(iter->second);

            return nullptr;
        }

        template <class T = Call>
        std::vector<std::shared_ptr<T> > getAllCalls() {
            std::vector<std::shared_ptr<T> > v;
            const auto& itermap = callMaps_.find(T::LINK_TYPE);

            if (itermap != callMaps_.cend()) {
                const auto& map = itermap->second;
                std::vector<std::shared_ptr<T> > v;

                for (const auto& item : map)
                    v.push_back(std::dynamic_pointer_cast<T>(item.second));
                v.shrink_to_fit();
            }

            return v;
        }

    private:
        std::map<std::string, CallMap> callMaps_ = {};
};

// Specializations defined in call_factory.cpp

template <> bool
CallFactory::empty<Call>();

template <> std::shared_ptr<Call>
CallFactory::getCall<Call>(const std::string& id);

template <> std::vector<std::shared_ptr<Call> >
CallFactory::getAllCalls<Call>();

#endif // CALL_FACTORY_H
