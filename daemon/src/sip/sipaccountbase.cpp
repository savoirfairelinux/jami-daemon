#include "sipaccountbase.h"
#include "sipvoiplink.h"

bool SIPAccountBase::portsInUse_[HALF_MAX_PORT];

SIPAccountBase::SIPAccountBase(const std::string& accountID)
    : Account(accountID), link_(getSIPVoIPLink())
{}


void
SIPAccountBase::setTransport(pjsip_transport* transport, pjsip_tpfactory* lis)
{
    // release old transport
    if (transport_ && transport_ != transport) {
        pjsip_transport_dec_ref(transport_);
    }
    if (tlsListener_ && tlsListener_ != lis)
        tlsListener_->destroy(tlsListener_);
    // set new transport
    transport_ = transport;
    tlsListener_ = lis;
}

// returns even number in range [lower, upper]
uint16_t
SIPAccountBase::getRandomEvenNumber(const std::pair<uint16_t, uint16_t> &range)
{
    const uint16_t halfUpper = range.second * 0.5;
    const uint16_t halfLower = range.first * 0.5;
    uint16_t result;
    do {
        result = 2 * (halfLower + rand() % (halfUpper - halfLower + 1));
    } while (portsInUse_[result / 2]);

    portsInUse_[result / 2] = true;
    return result;
}

void
SIPAccountBase::releasePort(uint16_t port)
{
    portsInUse_[port / 2] = false;
}

uint16_t
SIPAccountBase::generateAudioPort() const
{
    return getRandomEvenNumber(audioPortRange_);
}

#ifdef SFL_VIDEO
uint16_t
SIPAccountBase::generateVideoPort() const
{
    return getRandomEvenNumber(videoPortRange_);
}
#endif
