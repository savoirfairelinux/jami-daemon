%header %{
#include "jami/circle_interface.h"
%}

namespace libjami {
    std::string createCircle(const std::string& accountId, const std::string& defaults = "");
    std::vector<std::string> getCircles(const std::string& accountId);
    bool removeCircle(const std::string& accountId, const std::string& circleId, const std::string& successor = "");
    bool inviteCircleMember(const std::string& accountId, const std::string& circleId, const std::string& memberUri);
    bool banCircleMember(const std::string& accountId, const std::string& circleId, const std::string& memberUri);
    bool unbanCircleMember(const std::string& accountId, const std::string& circleId, const std::string& memberUri);
    std::vector<std::map<std::string, std::string>> getCircleMembers(const std::string& accountId, const std::string& circleId);
    std::vector<std::map<std::string, std::string>> getCircleRequests(const std::string& accountId);
    void acceptCircleRequest(const std::string& accountId, const std::string& circleId);
    void declineCircleRequest(const std::string& accountId, const std::string& circleId);
    std::string getCirclePolicy(const std::string& accountId, const std::string& circleId);
    bool updateCirclePolicy(const std::string& accountId, const std::string& circleId, const std::string& change);
    std::map<std::string, std::string> getCircleProfile(const std::string& accountId, const std::string& circleId, const std::string& channelId = "");
    bool updateCircleProfile(const std::string& accountId, const std::string& circleId, const std::map<std::string, std::string>& profile, const std::string& channelId = "");
    std::string createCircleChannel(const std::string& accountId, const std::string& circleId, const std::string& access = "");
    bool archiveCircleChannel(const std::string& accountId, const std::string& circleId, const std::string& channelId);
    std::vector<std::map<std::string, std::string>> getCircleChannels(const std::string& accountId, const std::string& circleId);
    void joinCircleChannel(const std::string& accountId, const std::string& circleId, const std::string& channelId);
    void leaveCircleChannel(const std::string& accountId, const std::string& circleId, const std::string& channelId);
}