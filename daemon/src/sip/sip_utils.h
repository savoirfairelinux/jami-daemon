/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef SIP_UTILS_H_
#define SIP_UTILS_H_

#include "ip_utils.h"
#include "media_codec.h"
#include "media/audio/audiobuffer.h"

#include <pjsip/sip_msg.h>
#include <pjlib.h>

#include <utility>
#include <string>
#include <vector>

struct pjsip_msg;

namespace ring {


//typedef std::vector<std::string> CryptoOffer;

enum class MediaType { AUDIO, VIDEO };


class CryptoAttribute {
    public:
        CryptoAttribute() {}
        CryptoAttribute(const std::string &tag,
                        const std::string &cryptoSuite,
                        const std::string &srtpKeyMethod,
                        const std::string &srtpKeyInfo,
                        const std::string &lifetime,
                        const std::string &mkiValue,
                        const std::string &mkiLength) :
            tag_(tag),
            cryptoSuite_(cryptoSuite),
            srtpKeyMethod_(srtpKeyMethod),
            srtpKeyInfo_(srtpKeyInfo),
            lifetime_(lifetime),
            mkiValue_(mkiValue),
            mkiLength_(mkiLength) {}

        std::string getTag() const {
            return tag_;
        }
        std::string getCryptoSuite() const {
            return cryptoSuite_;
        }
        std::string getSrtpKeyMethod() const {
            return srtpKeyMethod_;
        }
        std::string getSrtpKeyInfo() const {
            return srtpKeyInfo_;
        }
        std::string getLifetime() const {
            return lifetime_;
        }
        std::string getMkiValue() const {
            return mkiValue_;
        }
        std::string getMkiLength() const {
            return mkiLength_;
        }

        operator bool() const {
            return not tag_.empty();
        }

        std::string to_string() const {
            return tag_+" "+cryptoSuite_+" "+srtpKeyMethod_+":"+srtpKeyInfo_;
        }

    private:
        std::string tag_;
        std::string cryptoSuite_;
        std::string srtpKeyMethod_;
        std::string srtpKeyInfo_;
        std::string lifetime_;
        std::string mkiValue_;
        std::string mkiLength_;
};


struct MediaDescription {
    MediaType type {};
    bool enabled {false};
    bool holding {false};
    IpAddr addr {};

    MediaCodec* codec {};
    std::string payload_type {};
    std::string receiving_sdp {};
    unsigned bitrate {};

    //audio
    AudioFormat audioformat {AudioFormat::NONE()};

    //video
    unsigned width {};
    unsigned height {};
    std::string parameters {};

    //crypto
    //CryptoOffer crypto {};
    CryptoAttribute crypto {};
};

namespace sip_utils {

    enum class KeyExchangeProtocol { NONE, SDES, ZRTP };

    /**
     * Helper function to parser header from incoming sip messages
     * @return Header from SIP message
     */
    std::string fetchHeaderValue(pjsip_msg *msg, const std::string &field);

    pjsip_route_hdr *
    createRouteSet(const std::string &route, pj_pool_t *hdr_pool);

    void stripSipUriPrefix(std::string& sipUri);

    std::string parseDisplayName(const char * buffer);

    std::string getHostFromUri(const std::string& sipUri);

    void addContactHeader(const pj_str_t *contactStr, pjsip_tx_data *tdata);

    void sip_strerror(pj_status_t code);

}} // namespace ring::sip_utils

#endif // SIP_UTILS_H_
