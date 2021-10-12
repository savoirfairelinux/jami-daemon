/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion@savoirfairelinux.com>
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

/* Std */
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <vector>

/* Guile */
#include <libguile.h>

/* Jami */
#include "account_const.h"
#include "jami/callmanager_interface.h"
#include "jami/configurationmanager_interface.h"
#include "jami/conversation_interface.h"
#include "jami/presencemanager_interface.h"

/* Agent */
#include "bindings/bindings.h"
#include "utils.h"

static SCM signal_alist = SCM_EOL;

template<typename... Args>
class Handler
{
    std::mutex mutex_;
    std::vector<SCM> callbacks_;

public:

    Handler(const char *symbol_name) {

	signal_alist = scm_cons(
	    scm_cons(scm_from_utf8_symbol(symbol_name),
		     scm_cons(scm_from_pointer(static_cast<void*>(&callbacks_), nullptr),
			      scm_from_pointer(static_cast<void*>(&mutex_), nullptr))),
	    signal_alist);
    }

    struct cb_ctx {
	Handler<Args...>& me;
	std::tuple<Args...>& args;
    };

    void doExecuteInGuile(Args... args) {

	std::unique_lock lck(mutex_);
	std::vector<SCM> old;
	std::vector<SCM> to_keep;

	old = std::move(callbacks_);

	lck.unlock();

	for (SCM cb : old) {

	    using namespace std::placeholders;
	    using std::bind;

	    SCM ret = apply_to_guile(cb, args...);
	    if (scm_is_true(ret)) {
		to_keep.emplace_back(cb);
	    }
	}

	lck.lock();

	for (SCM cb : to_keep) {
	    callbacks_.push_back(cb);
	}
    }

    static void *executeInGuile(void *ctx_raw) {

	cb_ctx *ctx = static_cast<cb_ctx*>(ctx_raw);

	auto apply_wrapper = [&](Args... args){
	    ctx->me.doExecuteInGuile(args...);
	};

	std::apply(apply_wrapper, ctx->args);

	return nullptr;
    }

    void execute(Args... args) {

	    std::tuple<Args...> tuple(args...);

	    cb_ctx ctx = {*this, tuple};

	    scm_with_guile(executeInGuile, &ctx);
        }
};

static SCM
on_signal_binding(SCM signal_sym, SCM handler_proc)
{
    SCM handler_pair;

    std::vector<SCM> *callbacks;
    std::mutex *mutex;

    AGENT_ASSERT(scm_is_true(scm_procedure_p(handler_proc)),
		 "handler_proc must be a procedure");

    handler_pair = scm_assq_ref(signal_alist, signal_sym);

    if (scm_is_false(handler_pair)) {

    }

    callbacks = static_cast<std::vector<SCM>*>(scm_to_pointer(scm_car(handler_pair)));
    mutex     = static_cast<std::mutex*>(scm_to_pointer(scm_cdr(handler_pair)));

    std::unique_lock lck(*mutex);
    callbacks->push_back(handler_proc);

    return SCM_UNDEFINED;
}


void
install_signal_primitives(void *)
    {
    static Handler<const std::string&, const std::string&, std::map<std::string, std::string>>
	onMessageReceived("message-received");

    static Handler<const std::string&, const std::string&, std::map<std::string, std::string>>
	onConversationRequestReceived("conversation-request-received");

    static Handler<const std::string&, const std::string&>
	onConversationReady("conversation-ready");

    static Handler<const std::string&, const std::string&, signed>
	onCallStateChanged("call-state-changed");

    static Handler<const std::string&, const std::string&, const std::string&, const std::vector<DRing::MediaMap>>
	onIncomingCall("incomming-call");

    static Handler<const std::string&, const std::string&, bool> onContactAdded("contact-added");

    static Handler<const std::string&, const std::string&, int, const std::string&>
	onRegistrationStateChanged("registration-state-changed");

    static Handler<const std::string&, const std::map<std::string, std::string>&>
	onVolatileDetailsChanged("volatile-details-changed");

    define_primitive("on-signal", 2, 0, 0, (void*) on_signal_binding);

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> handlers;

    using namespace std::placeholders;
    using std::bind;

    handlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCallWithMedia>(
			bind(&Handler<const std::string&,
			     const std::string&,
			     const std::string&,
			     const std::vector<DRing::MediaMap>>::execute,
			     &onIncomingCall,
			     _1,
			     _2,
			     _3,
			     _4)));

    handlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
			bind(&Handler<const std::string&, const std::string&, signed>::execute,
			     &onCallStateChanged,
			     _1,
			     _2,
			     _3)));

    handlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
			bind(&Handler<const std::string&,
			     const std::string&,
			     std::map<std::string, std::string>>::execute,
			     &onMessageReceived,
			     _1,
			     _2,
			     _3)));

    handlers.insert(
	DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
	    bind(&Handler<const std::string&,
		 const std::string&,
		 std::map<std::string, std::string>>::execute,
		 &onConversationRequestReceived,
		 _1,
		 _2,
		 _3)));

    handlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
			bind(&Handler<const std::string&, const std::string&>::execute,
			     &onConversationReady,
			     _1,
			     _2)));

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::ContactAdded>(
			bind(&Handler<const std::string&, const std::string&, bool>::execute,
			     &onContactAdded,
			     _1,
			     _2,
			     _3)));

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::RegistrationStateChanged>(
			bind(&Handler<const std::string&, const std::string&, int, const std::string&>::execute,
			     &onRegistrationStateChanged,
			     _1,
			     _2,
			     _3,
			     _4)));

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
			bind(&Handler<const std::string&, const std::map<std::string, std::string>&>::execute,
			     &onVolatileDetailsChanged,
			     _1,
			     _2)));

    DRing::registerSignalHandlers(handlers);
}
