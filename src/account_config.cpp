#include "account_config.h"
#include "yamlparser.h"

namespace jami {

constexpr const char* ALL_CODECS_KEY = "allCodecs";
constexpr const char* VIDEO_CODEC_ENABLED = "enabled";
constexpr const char* VIDEO_CODEC_NAME = "name";
constexpr const char* VIDEO_CODEC_PARAMETERS = "parameters";
constexpr const char* VIDEO_CODEC_BITRATE = "bitrate";
constexpr const char* RINGTONE_PATH_KEY = "ringtonePath";
constexpr const char* RINGTONE_ENABLED_KEY = "ringtoneEnabled";
constexpr const char* VIDEO_ENABLED_KEY = "videoEnabled";
constexpr const char* DISPLAY_NAME_KEY = "displayName";
constexpr const char* ALIAS_KEY = "alias";
constexpr const char* TYPE_KEY = "type";
constexpr const char* ID_KEY = "id";
constexpr const char* USERNAME_KEY = "username";
constexpr const char* AUTHENTICATION_USERNAME_KEY = "authenticationUsername";
constexpr const char* PASSWORD_KEY = "password";
constexpr const char* HOSTNAME_KEY = "hostname";
constexpr const char* ACCOUNT_ENABLE_KEY = "enable";
constexpr const char* ACCOUNT_AUTOANSWER_KEY = "autoAnswer";
constexpr const char* ACCOUNT_READRECEIPT_KEY = "sendReadReceipt";
constexpr const char* ACCOUNT_ISRENDEZVOUS_KEY = "rendezVous";
constexpr const char* ACCOUNT_ACTIVE_CALL_LIMIT_KEY = "activeCallLimit";
constexpr const char* MAILBOX_KEY = "mailbox";
constexpr const char* USER_AGENT_KEY = "useragent";
constexpr const char* HAS_CUSTOM_USER_AGENT_KEY = "hasCustomUserAgent";
constexpr const char* PRESENCE_MODULE_ENABLED_KEY = "presenceModuleEnabled";
constexpr const char* UPNP_ENABLED_KEY = "upnpEnabled";
constexpr const char* ACTIVE_CODEC_KEY = "activeCodecs";
constexpr const char* DISABLED_CODECS_KEY = "disabledCodecs";
constexpr const char* DEFAULT_MODERATORS_KEY = "defaultModerators";
constexpr const char* LOCAL_MODERATORS_ENABLED_KEY = "localModeratorsEnabled";
constexpr const char* ALL_MODERATORS_ENABLED_KEY = "allModeratorsEnabled";
constexpr const char* PROXY_PUSH_TOKEN_KEY = "proxyPushToken";
constexpr const char* PROXY_PUSH_TOPIC_KEY = "proxyPushiOSTopic";

void
AccountConfig::serialize(YAML::Emitter& out) const
{
    out << YAML::Key << ALIAS_KEY << YAML::Value << alias;
    out << YAML::Key << ACCOUNT_ENABLE_KEY << YAML::Value << enabled;
    out << YAML::Key << TYPE_KEY << YAML::Value << type;
    out << YAML::Key << DISABLED_CODECS_KEY << YAML::Value << disabledCodecs;
    out << YAML::Key << MAILBOX_KEY << YAML::Value << mailbox;
    out << YAML::Key << ACCOUNT_AUTOANSWER_KEY << YAML::Value << autoAnswerEnabled;
    out << YAML::Key << ACCOUNT_READRECEIPT_KEY << YAML::Value << sendReadReceipt;
    out << YAML::Key << ACCOUNT_ISRENDEZVOUS_KEY << YAML::Value << isRendezVous;
    out << YAML::Key << ACCOUNT_ACTIVE_CALL_LIMIT_KEY << YAML::Value << activeCallLimit;
    out << YAML::Key << RINGTONE_ENABLED_KEY << YAML::Value << ringtoneEnabled;
    out << YAML::Key << RINGTONE_PATH_KEY << YAML::Value << ringtonePath;
    out << YAML::Key << USER_AGENT_KEY << YAML::Value << customUserAgent;
    out << YAML::Key << DISPLAY_NAME_KEY << YAML::Value << displayName;
    out << YAML::Key << UPNP_ENABLED_KEY << YAML::Value << upnpEnabled;
    out << YAML::Key << DEFAULT_MODERATORS_KEY << YAML::Value << defaultModerators;
    out << YAML::Key << LOCAL_MODERATORS_ENABLED_KEY << YAML::Value << localModeratorsEnabled;
    out << YAML::Key << ALL_MODERATORS_ENABLED_KEY << YAML::Value << allModeratorsEnabled;
    out << YAML::Key << PROXY_PUSH_TOKEN_KEY << YAML::Value << deviceKey;
    out << YAML::Key << PROXY_PUSH_TOPIC_KEY << YAML::Value << notificationTopic;
}

void
AccountConfig::unserialize(const YAML::Node& node)
{
    using yaml_utils::parseValue;
    using yaml_utils::parseValueOptional;

    parseValueOptional(node, ALIAS_KEY, alias);
    parseValueOptional(node, ACCOUNT_ENABLE_KEY, enabled);
    parseValueOptional(node, ACCOUNT_AUTOANSWER_KEY, autoAnswerEnabled);
    parseValueOptional(node, ACCOUNT_READRECEIPT_KEY, sendReadReceipt);
    parseValueOptional(node, ACCOUNT_ISRENDEZVOUS_KEY, isRendezVous);
    parseValueOptional(node, ACCOUNT_ACTIVE_CALL_LIMIT_KEY, activeCallLimit);
    parseValueOptional(node, MAILBOX_KEY, mailbox);

    std::string codecs;
    if (parseValueOptional(node, ACTIVE_CODEC_KEY, codecs))
        activeCodecs = split_string_to_unsigned(codecs, '/');

    parseValueOptional(node, DISPLAY_NAME_KEY, displayName);
    //parseValueOptional(node, HOSTNAME_KEY, hostname);

    parseValueOptional(node, USER_AGENT_KEY, customUserAgent);
    parseValueOptional(node, RINGTONE_PATH_KEY, ringtonePath);
    parseValueOptional(node, RINGTONE_ENABLED_KEY, ringtoneEnabled);

    parseValue(node, UPNP_ENABLED_KEY, upnpEnabled);

    std::string defMod;
    parseValueOptional(node, DEFAULT_MODERATORS_KEY, defMod);
    defaultModerators = string_split_set(defMod);
    parseValueOptional(node, LOCAL_MODERATORS_ENABLED_KEY, localModeratorsEnabled);
    parseValueOptional(node, ALL_MODERATORS_ENABLED_KEY, allModeratorsEnabled);
    parseValueOptional(node, PROXY_PUSH_TOKEN_KEY, deviceKey);
    parseValueOptional(node, PROXY_PUSH_TOPIC_KEY, notificationTopic);
}

}

namespace YAML {

Node
convert<jami::AccountConfig>::encode(const jami::AccountConfig& config)
{

}

bool
convert<jami::AccountConfig>::decode(const Node& node, jami::AccountConfig& config)
{

    parseValue(node, ALIAS_KEY, alias_);
    parseValue(node, ACCOUNT_ENABLE_KEY, enabled_);
    parseValue(node, ACCOUNT_AUTOANSWER_KEY, autoAnswerEnabled_);
    parseValueOptional(node, ACCOUNT_READRECEIPT_KEY, sendReadReceipt_);
    parseValueOptional(node, ACCOUNT_ISRENDEZVOUS_KEY, isRendezVous_);
    parseValue(node, ACCOUNT_ACTIVE_CALL_LIMIT_KEY, activeCallLimit_);
    // parseValue(node, PASSWORD_KEY, password_);

    parseValue(node, MAILBOX_KEY, mailBox_);

    std::string activeCodecs;
    if (parseValueOptional(node, ACTIVE_CODEC_KEY, activeCodecs))
        setActiveCodecs(split_string_to_unsigned(activeCodecs, '/'));
    else {
        std::string allCodecs;
        if (parseValueOptional(node, ALL_CODECS_KEY, allCodecs)) {
            JAMI_WARN("Converting deprecated codec list");
            auto list = convertIdToAVId(split_string_to_unsigned(allCodecs, '/'));
            auto codec = searchCodecByName("H265", MEDIA_ALL);
            // set H265 as first active codec if found
            if (codec)
                list.emplace(list.begin(), codec->systemCodecInfo.id);
            setActiveCodecs(list);
            runOnMainThread([id = getAccountID()] {
                if (auto sthis = Manager::instance().getAccount(id))
                    Manager::instance().saveConfig(sthis);
            });
        }
    }

    parseValue(node, DISPLAY_NAME_KEY, displayName_);
    parseValue(node, HOSTNAME_KEY, hostname_);

    parseValue(node, HAS_CUSTOM_USER_AGENT_KEY, hasCustomUserAgent_);
    parseValue(node, USER_AGENT_KEY, customUserAgent_);
    parseValue(node, RINGTONE_PATH_KEY, ringtonePath_);
    parseValue(node, RINGTONE_ENABLED_KEY, ringtoneEnabled_);
    if (ringtonePath_.empty()) {
        ringtonePath_ = DEFAULT_RINGTONE_PATH;
    } else {
        // If the user defined a custom ringtone, the file may not exists
        // In this case, fallback on the default ringtone path (this will be set during the next
        // setAccountDetails)
        auto pathRingtone = fmt::format("{}/{}/{}", JAMI_DATADIR, RINGDIR, ringtonePath_);
        if (!fileutils::isFile(ringtonePath_) && !fileutils::isFile(pathRingtone)) {
            JAMI_WARN("Ringtone %s is not a valid file", pathRingtone.c_str());
            ringtonePath_ = DEFAULT_RINGTONE_PATH;
        }
    }

    parseValue(node, UPNP_ENABLED_KEY, upnpEnabled_);
    updateUpnpController();

    std::string defMod;
    parseValueOptional(node, DEFAULT_MODERATORS_KEY, defMod);
    defaultModerators_ = string_split_set(defMod);
    parseValueOptional(node, LOCAL_MODERATORS_ENABLED_KEY, localModeratorsEnabled_);
    parseValueOptional(node, ALL_MODERATORS_ENABLED_KEY, allModeratorsEnabled_);
    parseValueOptional(node, PROXY_PUSH_TOKEN_KEY, deviceKey_);
    parseValueOptional(node, PROXY_PUSH_TOPIC_KEY, notificationTopic_);
}

}
