/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

%header %{
#include "dring/dring.h"
#include "dring/conversation_interface.h"

class ConversationCallback {
public:
    virtual ~ConversationCallback(){}
    virtual void conversationLoaded(uint32_t /* id */, const std::string& /*accountId*/, const std::string& /* conversationId */, std::vector<std::map<std::string, std::string>> /*messages*/){}
    virtual void messageReceived(const std::string& /*accountId*/, const std::string& /* conversationId */, std::map<std::string, std::string> /*message*/){}
    virtual void conversationRequestReceived(const std::string& /*accountId*/, const std::string& /* conversationId */, std::map<std::string, std::string> /*metadatas*/){}
    virtual void conversationReady(const std::string& /*accountId*/, const std::string& /* conversationId */){}
    virtual void conversationRemoved(const std::string& /*accountId*/, const std::string& /* conversationId */){}
    virtual void conversationMemberEvent(const std::string& /*accountId*/, const std::string& /* conversationId */, const std::string& /* memberUri */, int /* event */){}
    virtual void onConversationError(const std::string& /*accountId*/, const std::string& /* conversationId */, uint32_t /* code */, const std::string& /* what */){}
};
%}

%feature("director") ConversationCallback;

namespace DRing {

  // Conversation management
  std::string startConversation(const std::string& accountId);
  void acceptConversationRequest(const std::string& accountId, const std::string& conversationId);
  void declineConversationRequest(const std::string& accountId, const std::string& conversationId);
  bool removeConversation(const std::string& accountId, const std::string& conversationId);
  std::vector<std::string> getConversations(const std::string& accountId);
  std::vector<std::map<std::string, std::string>> getConversationRequests(const std::string& accountId);
  void updateConversationInfos(const std::string& accountId, const std::string& conversationId, const std::map<std::string, std::string>& infos);
  std::map<std::string, std::string> conversationInfos(const std::string& accountId, const std::string& conversationId);

  // Member management
  void addConversationMember(const std::string& accountId, const std::string& conversationId, const std::string& contactUri);
  void removeConversationMember(const std::string& accountId, const std::string& conversationId, const std::string& contactUri);
  std::vector<std::map<std::string, std::string>> getConversationMembers(const std::string& accountId, const std::string& conversationId);

  // Message send/load
  void sendMessage(const std::string& accountId, const std::string& conversationId, const std::string& message, const std::string& parent);
  uint32_t loadConversationMessages(const std::string& accountId, const std::string& conversationId, const std::string& fromMessage, size_t n);

}

class ConversationCallback {
public:
    virtual ~ConversationCallback(){}
    virtual void conversationLoaded(uint32_t /* id */, const std::string& /*accountId*/, const std::string& /* conversationId */, std::vector<std::map<std::string, std::string>> /*messages*/){}
    virtual void messageReceived(const std::string& /*accountId*/, const std::string& /* conversationId */, std::map<std::string, std::string> /*message*/){}
    virtual void conversationRequestReceived(const std::string& /*accountId*/, const std::string& /* conversationId */, std::map<std::string, std::string> /*metadatas*/){}
    virtual void conversationReady(const std::string& /*accountId*/, const std::string& /* conversationId */){}
    virtual void conversationRemoved(const std::string& /*accountId*/, const std::string& /* conversationId */){}
    virtual void conversationMemberEvent(const std::string& /*accountId*/, const std::string& /* conversationId */, const std::string& /* memberUri */, int /* event */){}
    virtual void onConversationError(const std::string& /*accountId*/, const std::string& /* conversationId */, uint32_t /* code */, const std::string& /* what */){}
};