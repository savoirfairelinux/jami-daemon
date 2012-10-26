/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author: Emeric Vigier <emeric.vigier@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

%header %{

#include <android-jni/callmanagerJNI.h>


typedef struct callmanager_callback
{
    void (*on_new_call_created)(const std::string& accountID,
                                const std::string& callID,
                                const std::string& to);

    void (*on_call_state_changed)(const std::string& callID,
                                  const std::string& state);

    void (*on_incoming_call)(const std::string& accountID,
                             const std::string& callID,
                             const std::string& from);
} callmanager_callback_t;


class Callback {
public:
    virtual ~Callback() {}

    virtual void on_new_call_created(const std::string& arg1,
                                     const std::string& arg2,
                                     const std::string& arg3) {}

    virtual void on_call_state_changed(const std::string& arg1,
                                       const std::string& arg2) {}

    virtual void on_incoming_call(const std::string& arg1,
                                  const std::string& arg2,
                                  const std::string& arg3) {}
};


static Callback* registeredCallbackObject = NULL;

void on_new_call_created_wrapper (const std::string& accountID,
                                  const std::string& callID,
                                  const std::string& to) {
    registeredCallbackObject->on_new_call_created(accountID, callID, to);
}

void on_call_state_changed_wrapper(const std::string& callID,
                           const std::string& state) {
    registeredCallbackObject->on_call_state_changed(callID, state);
}

void on_incoming_call_wrapper (const std::string& accountID,
                               const std::string& callID,
                               const std::string& from) {
    registeredCallbackObject->on_incoming_call(accountID, callID, from);
}

static struct callmanager_callback wrapper_callback_struct = {
    &on_new_call_created_wrapper,
    &on_call_state_changed_wrapper,
    &on_incoming_call_wrapper,
};

void setCallbackObject(Callback* callback) {
    registeredCallbackObject = callback;
}

%}

%feature("director") Callback;

class CallManagerJNI {
public:
    /* Manager::instance().outgoingCall */
    void placeCall(const std::string& accountID,
                   const std::string& callID,
                   const std::string& to);
    /* Manager::instance().refuseCall */
    void refuse(const std::string& callID);
    /* Manager::instance().answerCall */
    void accept(const std::string& callID);
    /* Manager::instance().hangupCall */
    void hangUp(const std::string& callID);
    void hold(const std::string& callID);
    void unhold(const std::string& callID);
};

class Callback {
public:
    virtual ~Callback();

    virtual void on_new_call_created(const std::string& arg1,
                                     const std::string& arg2,
                                     const std::string& arg3);

    virtual void on_call_state_changed(const std::string& arg1,
                                       const std::string& arg2);

    virtual void on_incoming_call(const std::string& arg1,
                                  const std::string& arg2,
                                  const std::string& arg3);
};

static Callback* registeredCallbackObject = NULL;

void setCallbackObject(Callback* callback) {
    registeredCallbackObject = callback;
}

