/*
 * SRTP encryption/decryption
 * Copyright (c) 2012 Martin Storsjo
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef SRTP_H
#define SRTP_H

#include <stdint.h>

struct AVAES;
struct AVHMAC;

enum {
    SRTP_MAX_MASTER_KEY_SIZE = 32,
    SRTP_MASTER_SALT_SIZE = 14,
    SRTP_MAX_SESSION_KEY_SIZE = 32,
    SRTP_AUTH_KEY_SIZE = 20,
};

struct SRTPContext
{
    struct AVAES* aes;
    struct AVHMAC* hmac;
    int rtp_hmac_size, rtcp_hmac_size;
    int master_key_size;
    int master_salt_size;
    int session_key_size;
    uint8_t master_key[SRTP_MAX_MASTER_KEY_SIZE];
    uint8_t master_salt[SRTP_MASTER_SALT_SIZE];
    uint8_t rtp_key[SRTP_MAX_SESSION_KEY_SIZE], rtcp_key[SRTP_MAX_SESSION_KEY_SIZE];
    uint8_t rtp_salt[SRTP_MASTER_SALT_SIZE], rtcp_salt[SRTP_MASTER_SALT_SIZE];
    uint8_t rtp_auth[SRTP_AUTH_KEY_SIZE], rtcp_auth[SRTP_AUTH_KEY_SIZE];
    int seq_largest, seq_initialized;
    uint32_t roc;

    uint32_t rtcp_index;
};

int ff_srtp_set_crypto(struct SRTPContext* s, const char* suite, const char* params);
void ff_srtp_free(struct SRTPContext* s);
int ff_srtp_decrypt(struct SRTPContext* s, uint8_t* buf, int* lenptr);
int ff_srtp_encrypt(struct SRTPContext* s, const uint8_t* in, int len, uint8_t* out, int outlen);

/* RTCP packet types */
enum RTCPType { RTCP_FIR = 192, RTCP_IJ = 195, RTCP_SR = 200, RTCP_TOKEN = 210, RTCP_REMB = 206 };

#define RTP_PT_IS_RTCP(x) (((x) >= RTCP_FIR && (x) <= RTCP_IJ) || ((x) >= RTCP_SR && (x) <= RTCP_TOKEN))

#endif /* SRTP_H */
