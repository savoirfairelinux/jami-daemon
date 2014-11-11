/*
Copyright (c) 2014 Savoir-Faire Linux Inc.

Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "infohash.h"

extern "C" {
#include <gnutls/gnutls.h>
}

#include <sstream>

namespace dht {

InfoHash::InfoHash(const std::string& hex) {
    unsigned in = std::min((size_t)HASH_LEN, hex.size()/2);
    for (unsigned i = 0; i < in; i++) {
        sscanf(hex.data() + 2*i, "%02x", (unsigned*)(&((*this)[i])));
    }
}

InfoHash
InfoHash::get(const uint8_t* data, size_t data_len)
{
    InfoHash h;
    size_t s = h.size();
    const gnutls_datum_t gnudata = {(uint8_t*)data, (unsigned)data_len};
    const gnutls_digest_algorithm_t algo =  (HASH_LEN == 64) ? GNUTLS_DIG_SHA512 : (
                                            (HASH_LEN == 32) ? GNUTLS_DIG_SHA256 : (
                                            (HASH_LEN == 20) ? GNUTLS_DIG_SHA1   :
                                            GNUTLS_DIG_NULL ));
    static_assert(algo != GNUTLS_DIG_NULL, "Can't find hash function to use.");
    int rc = gnutls_fingerprint(algo, &gnudata, h.data(), &s);
    if (rc == 0 && s == HASH_LEN)
        return h;
    throw std::string("Error while hashing");
}

std::string
InfoHash::toString() const
{
    std::stringstream ss;
    ss << *this;
    return ss.str();
}

std::ostream& operator<< (std::ostream& s, const InfoHash& h)
{
    s << std::hex;
    for (unsigned i=0; i<HASH_LEN; i++)
        s << std::setfill('0') << std::setw(2) << (unsigned)h[i];
    s << std::dec;
    return s;
}

}
