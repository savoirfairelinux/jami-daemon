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

#pragma once

#include <iostream>
#include <iomanip>
#include <array>
#include <vector>

#include <cstring>

// bytes
#define HASH_LEN 20

namespace dht {

class DhtException : public std::runtime_error {
    public:
        DhtException(const std::string &str = "") :
            std::runtime_error("DhtException occured: " + str) {}
};


/**
 * Represents an InfoHash.
 * An InfoHash is a byte array of HASH_LEN bytes.
 * InfoHashes identify nodes and values in the Dht.
 */
class InfoHash final : public std::array<uint8_t, HASH_LEN> {
public:
    constexpr InfoHash() : std::array<uint8_t, HASH_LEN>() {}
    constexpr InfoHash(const std::array<uint8_t, HASH_LEN>& h) : std::array<uint8_t, HASH_LEN>(h) {}
    InfoHash(const uint8_t* h, size_t h_len=HASH_LEN) : std::array<uint8_t, HASH_LEN>() {
        memcpy(data(), h, std::min((size_t)HASH_LEN, h_len));
    }

    /**
     * Constructor from an hexadecimal string (without "0x").
     * hex must be at least 2.HASH_LEN characters long.
     * If too long, only the first 2.HASH_LEN characters are read.
     */
    InfoHash(const std::string& hex);

    /**
     * Find the lowest 1 bit in an id.
     * Result will allways be lower than 8*HASH_LEN
     */
    inline unsigned lowbit() const {
        int i, j;
        for(i = HASH_LEN-1; i >= 0; i--)
            if((*this)[i] != 0)
                break;
        if(i < 0)
            return -1;
        for(j = 7; j >= 0; j--)
            if(((*this)[i] & (0x80 >> j)) != 0)
                break;
        return 8 * i + j;
    }

    /**
     * Forget about the ``XOR-metric''.  An id is just a path from the
     * root of the tree, so bits are numbered from the start.
     */
    static inline int cmp(const InfoHash& __restrict__ id1, const InfoHash& __restrict__ id2) {
        return std::memcmp(id1.data(), id2.data(), HASH_LEN);
    }

    /** Find how many bits two ids have in common. */
    static inline unsigned
    commonBits(const InfoHash& id1, const InfoHash& id2)
    {
        unsigned i, j;
        uint8_t x;
        for(i = 0; i < HASH_LEN; i++) {
            if(id1[i] != id2[i])
                break;
        }

        if(i == HASH_LEN)
            return 8*HASH_LEN;

        x = id1[i] ^ id2[i];

        j = 0;
        while((x & 0x80) == 0) {
            x <<= 1;
            j++;
        }

        return 8 * i + j;
    }

    /** Determine whether id1 or id2 is closer to this */
    int
    xorCmp(const InfoHash& id1, const InfoHash& id2) const
    {
        unsigned i;
        for(i = 0; i < HASH_LEN; i++) {
            uint8_t xor1, xor2;
            if(id1[i] == id2[i])
                continue;
            xor1 = id1[i] ^ (*this)[i];
            xor2 = id2[i] ^ (*this)[i];
            if(xor1 < xor2)
                return -1;
            else
                return 1;
        }
        return 0;
    }

    static inline InfoHash get(const std::string& data) {
        return get((const uint8_t*)data.data(), data.size());
    }

    static inline InfoHash get(const std::vector<uint8_t>& data) {
        return get(data.data(), data.size());
    }

    /**
     * Computes the hash from a given data buffer of size data_len.
     */
    static InfoHash get(const uint8_t* data, size_t data_len);

    friend std::ostream& operator<< (std::ostream& s, const InfoHash& h);

    std::string toString() const;
};

}
