#include "SdesNegotiator.h"
#include <vector>
#include <iostream>

int main (void)
{
    std::vector<sfl::CryptoSuiteDefinition> localCapabilities;
    std::vector<std::string> remoteOffer;

    remoteOffer.push_back ("a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj|2^20|1:32");
    remoteOffer.push_back ("a=crypto:1 AES_CM_128_HMAC_SHA1_32 inline:NzB4d1BINUAvLEw6UzF3WSJ+PSdFcGdUJShpX1Zj|2^20|1:32");

    sfl::SdesNegotiator sdesNegotiator (localCapabilities, remoteOffer);

    sdesNegotiator.negotiate();

    return 0;
}
