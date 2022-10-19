#include "sip/sip_utils.h"
#include "serializable.h"

#include <yaml-cpp/yaml.h>
#include <string>
#include <string_view>
#include <utility>

using namespace std::literals;

namespace jami {
constexpr static const auto OVERRTP_STR = "overrtp"sv;
constexpr static const auto SIPINFO_STR = "sipinfo"sv;

struct AccountConfig: public Serializable {
    virtual void serialize(YAML::Emitter& out) const;
    virtual void unserialize(const YAML::Node& node);

    virtual std::map<std::string, std::string> toMap() const;
    virtual void fromMap(const std::map<std::string, std::string>&);

    /** Account type */
    std::string type {};

    /** A user-defined name for this account */
    std::string alias {};

    /** SIP hostname (SIP account) or DHT bootstrap nodes (Jami account) */
    std::string hostname {};

    /** True if the account is enabled. */
    bool enabled {true};

    /** If true, automatically answer calls to this account */
    bool autoAnswerEnabled {false};

    /** If true, send displayed status (and emit to the client) */
    bool sendReadReceipt {true};

    /** If true mix calls into a conference */
    bool isRendezVous {false};

    /**
     * The number of concurrent calls for the account
     * -1: Unlimited
     *  0: Do not disturb
     *  1: Single call
     *  +: Multi line
     */
    int activeCallLimit {-1};

    [[deprecated]]
    std::vector<unsigned> activeCodecs {};

    std::vector<unsigned> disabledCodecs {};

    /**
     * Play ringtone when receiving a call
     */
    bool ringtoneEnabled {true};

    /**
     * Ringtone .au file used for this account
     */
    std::string ringtonePath {};

    /**
     * Allows user to temporarily disable video calling
     */
    bool videoEnabled {true};

    /**
     * Display name when calling
     */
    std::string displayName {};

    /**
     * User-agent used for registration
     */
    std::string customUserAgent {};

    /**
     * Account mail box
     */
    std::string mailbox {};

    /**
     * UPnP IGD controller and the mutex to access it
     */
    bool upnpEnabled {};

    std::set<std::string> defaultModerators {};
    bool localModeratorsEnabled {true};
    bool allModeratorsEnabled {true};

    bool multiStreamEnabled {false};
    bool iceForMediaEnabled {true};
    bool iceCompIdRfc5245Compliant {false};

    /**
     * Device push notification token.
     */
    std::string deviceKey {};

    /**
     * Push notification topic.
     */
    std::string notificationTopic {};
};

}

/*
namespace YAML {
template<>
struct convert<jami::AccountConfig> {
    static Node encode(const jami::AccountConfig& config) {
        return config.serialize();
    }
    static bool decode(const Node& node, jami::AccountConfig& config) {
        
    }
};
}
*/