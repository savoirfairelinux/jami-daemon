/* location:    ring-daemon/src/logger.h
   purpose:     output directly into ring-daemon
*/
#include "logger.h"

// D-Bus client example using anonymous function

const std::map<std::string, SharedCallback> configEvHandlers = {
    exportable_callback<ConfigurationSignal::IncomingAccountMessage>([]
        (const std::string& accountID, const std::string& from,
            const std::map<std::string, std::string>& payloads)
        {
            // body
            RING_INFO("Account Id : %s", accountID.c_str());
            RING_INFO("From : %s", from.c_str());
            RING_INFO("Payloads: ");
            for(auto& it : payloads)
                RING_INFO("%s : %s", it.first.c_str(), it.second.c_str());

        }),
};

if (!DRing::init(static_cast<DRing::InitFlag>(flags)))
    return -1;

registerConfHandlers(configEvHandlers);
