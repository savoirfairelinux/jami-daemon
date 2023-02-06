/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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

#pragma once

/* Jami */
#include "jami/conversation_interface.h"

/* Agent */
#include "utils.h"

static SCM
get_conversations_binding(SCM accountID_str)
{
    LOG_BINDING();

    return to_guile(libjami::getConversations(from_guile(accountID_str)));
}

static SCM
get_conversation_members_binding(SCM accountID_str, SCM conversationID_str)
{
    LOG_BINDING();

    return  to_guile(libjami::getConversationMembers(from_guile(accountID_str),
                                                   from_guile(conversationID_str)));
}

static SCM
accept_conversation_binding(SCM accountID_str, SCM conversationID_str)
{
    LOG_BINDING();

    libjami::acceptConversationRequest(from_guile(accountID_str),
                                     from_guile(conversationID_str));

    return SCM_UNDEFINED;
}

static SCM
send_message_binding(SCM accountID_str, SCM conversationID_str, SCM message_str,
                     SCM parent_str_optional)
{
    LOG_BINDING();

    if (SCM_UNBNDP(parent_str_optional)) {
        libjami::sendMessage(from_guile(accountID_str),
                           from_guile(conversationID_str),
                           from_guile(message_str),
                           "");

    } else {
        libjami::sendMessage(from_guile(accountID_str),
                           from_guile(conversationID_str),
                           from_guile(message_str),
                           from_guile(parent_str_optional));
    }


    return SCM_UNDEFINED;
}

static void
install_conversation_primitives(void *)
{
    define_primitive("get-conversations", 1, 0, 0,
                     (void*) get_conversations_binding);

    define_primitive("get-conversation-members", 2, 0, 0,
                     (void*) get_conversation_members_binding);

    define_primitive("accept-conversation", 2, 0, 0,
                     (void*) accept_conversation_binding);

    define_primitive("send-message", 3, 1, 0,
                     (void*) send_message_binding);
}
