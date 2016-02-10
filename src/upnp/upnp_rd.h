#ifndef UPNP_RD_H_
#define UPNP_RD_H_

#include <string>
#include <map>
#include <functional>

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

namespace ring { namespace upnp {

/* defines a UPnP capable Ring Device (any device running Ring) */
class RD
{
public:

    /* local device address of RD */
    IpAddr localIp;

    /* constructors */
    RD() {}
    RD(std::string UDN,
        std::string deviceType,
        std::string friendlyName,
        std::string baseURL,
        std::string relURL)
        : UDN_(UDN)
        , deviceType_(deviceType)
        , friendlyName_(friendlyName)
        , baseURL_(baseURL)
        , relURL_(relURL)
        {}

    /* move constructor and operator */
    RD(RD&&) = default;
    RD& operator=(RD&&) = default;

    ~RD() = default;

    const std::string& getUDN() const { return UDN_; };
    const std::string& getDeviceType() const { return deviceType_; };
    const std::string& getFriendlyName() const { return friendlyName_; };
    const std::string& getBaseURL() const { return baseURL_; };
    const std::string& getrelURL() const { return relURL_; };

private:
    NON_COPYABLE(RD);

    /* root device info */
    std::string UDN_ {}; /* used to uniquely identify this UPnP device */
    std::string deviceType_ {};
    std::string friendlyName_ {};
    std::string baseURL_ {};
    std::string relURL_ {};

};

}} // namespace ring::upnp

#endif // UPNP_RD_H_
