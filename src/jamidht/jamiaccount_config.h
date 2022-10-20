#include "sip/sipaccountbase_config.h"

namespace jami {

struct JamiAccountConfig : public SipAccountBaseConfig {
    virtual void serialize(YAML::Emitter& out) const;
    virtual void unserialize(const YAML::Node& node);

};

}
