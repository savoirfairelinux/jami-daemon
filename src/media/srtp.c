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

#include <stdlib.h>
#include <libavutil/common.h>
#include <libavutil/base64.h>
#include <libavutil/aes.h>
#include <libavutil/hmac.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/log.h>
#include "srtp.h"

#ifdef TEST_SRTP_AEAD
static void dump_memory(const uint8_t *ptr, size_t len)
{
    for (size_t i=0; i < len; ++i) {
        fprintf(stderr, "%02x", ptr[i]);
        if (((i+1) % 16) == 0)
            fputc('\n', stderr);
        else if (((i+1) % 4) == 0)
            fputc(' ', stderr);
    }
    fputc('\n', stderr);
}
#endif

void ff_srtp_free(struct SRTPContext *s)
{
    if (!s)
        return;
    av_freep(&s->aes);
    if (s->hmac)
        av_hmac_free(s->hmac);
    //s->hmac = NULL;
    memset(s, 0, sizeof(*s));
}

static void encrypt_counter(struct AVAES *aes, uint8_t *iv, uint8_t *outbuf,
                            int outlen)
{
    int i, j, outpos;
    for (i = 0, outpos = 0; outpos < outlen; i++) {
        uint8_t keystream[16];
        AV_WB16(&iv[14], i);
        av_aes_crypt(aes, keystream, iv, 1, NULL, 0);
        for (j = 0; j < 16 && outpos < outlen; j++, outpos++)
            outbuf[outpos] ^= keystream[j];
    }
}

static void derive_key(struct AVAES *aes, const uint8_t *salt, int label,
                       uint8_t *out, int outlen)
{
    uint8_t input[16] = { 0 };
    memcpy(input, salt, 14);
    // Key derivation rate assumed to be zero
    input[14 - 7] ^= label;
    memset(out, 0, outlen);
    encrypt_counter(aes, input, out, outlen);
}

static int strp_aead_init(struct SRTPContext *s, gnutls_cipher_algorithm_t cipher,
                          uint8_t master_key_size, const uint8_t *params)
{
    s->aes = av_aes_alloc();
    if (!s->aes)
        return AVERROR(ENOMEM);

    memcpy(s->master_key, params, master_key_size);
    memcpy(s->master_salt, params + master_key_size, 12);

    av_aes_init(s->aes, s->master_key, master_key_size * 8, 0);

    derive_key(s->aes, s->master_salt, 0x00, s->rtp_key, master_key_size);
    derive_key(s->aes, s->master_salt, 0x02, s->rtp_salt, 12);

    derive_key(s->aes, s->master_salt, 0x03, s->rtcp_key, master_key_size);
    derive_key(s->aes, s->master_salt, 0x05, s->rtcp_salt, 12);

#ifdef TEST_SRTP_AEAD
    memcpy(s->rtp_key, s->master_key, master_key_size);
    memcpy(s->rtp_salt, s->master_salt, 12);
    memcpy(s->rtcp_key, s->master_key, master_key_size);
    memcpy(s->rtcp_salt, s->master_salt, 12);
#endif

    gcm_aes256_set_key(&s->aead_rtp_ctx, s->rtp_key);
    gcm_aes256_set_key(&s->aead_rtcp_ctx, s->rtcp_key);

    return 0;
}

int ff_srtp_set_crypto(struct SRTPContext *s, const char *suite,
                       const char *params)
{
    uint8_t buf[32+12];
    int params_length;
    gnutls_cipher_algorithm_t aead_cipher;

    ff_srtp_free(s);

    // RFC 5764 and RFC 7714
    if (!strcmp(suite, "AEAD_AES_256_GCM") ||
        !strcmp(suite, "SRTP_AEAD_AES_256_GCM")) {
        s->aead = 1;
        aead_cipher = GNUTLS_CIPHER_AES_256_GCM;
        params_length = 32+12;
    } else if (!strcmp(suite, "AES_CM_128_HMAC_SHA1_80") ||
        !strcmp(suite, "SRTP_AES128_CM_HMAC_SHA1_80")) {
        s->rtp_hmac_size = s->rtcp_hmac_size = 10;
        params_length = 16+14;
    } else if (!strcmp(suite, "AES_CM_128_HMAC_SHA1_32")) {
        s->rtp_hmac_size = s->rtcp_hmac_size = 4;
        params_length = 16+14;
    } else if (!strcmp(suite, "SRTP_AES128_CM_HMAC_SHA1_32")) {
        // RFC 5764 section 4.1.2
        s->rtp_hmac_size  = 4;
        s->rtcp_hmac_size = 10;
        params_length = 16+14;
    } else {
        av_log(NULL, AV_LOG_WARNING, "SRTP Crypto suite %s not supported\n",
                                     suite);
        return AVERROR(EINVAL);
    }
    int ret;
    if ((ret = av_base64_decode(buf, params, sizeof(buf))) != params_length) {
        av_log(NULL, AV_LOG_WARNING, "Incorrect amount of SRTP params (%d)\n", ret);
        return AVERROR(EINVAL);
    }

    // AEAD algorithms setup
    if (s->aead)
        return strp_aead_init(s, aead_cipher, 32, buf);

    // MKI and lifetime not handled yet
    s->aes  = av_aes_alloc();
    s->hmac = av_hmac_alloc(AV_HMAC_SHA1);
    if (!s->aes || !s->hmac)
        return AVERROR(ENOMEM);
    memcpy(s->master_key, buf, 16);
    memcpy(s->master_salt, buf + 16, 14);

    // RFC 3711
    av_aes_init(s->aes, s->master_key, 128, 0);

    derive_key(s->aes, s->master_salt, 0x00, s->rtp_key, sizeof(s->rtp_key));
    derive_key(s->aes, s->master_salt, 0x02, s->rtp_salt, sizeof(s->rtp_salt));
    derive_key(s->aes, s->master_salt, 0x01, s->rtp_auth, sizeof(s->rtp_auth));

    derive_key(s->aes, s->master_salt, 0x03, s->rtcp_key, sizeof(s->rtcp_key));
    derive_key(s->aes, s->master_salt, 0x05, s->rtcp_salt, sizeof(s->rtcp_salt));
    derive_key(s->aes, s->master_salt, 0x04, s->rtcp_auth, sizeof(s->rtcp_auth));
    return 0;
}

static void create_iv(uint8_t *iv, const uint8_t *salt, uint64_t index,
                      uint32_t ssrc)
{
    uint8_t indexbuf[8];
    int i;
    memset(iv, 0, 16);
    AV_WB32(&iv[4], ssrc);
    AV_WB64(indexbuf, index);
    for (i = 0; i < 8; i++) // index << 16
        iv[6 + i] ^= indexbuf[i];
    for (i = 0; i < 14; i++)
        iv[i] ^= salt[i];
}

static void create_eaed_iv(uint8_t *iv, int rtcp, const uint8_t *salt, uint64_t index,
                           uint32_t ssrc, uint32_t roc, uint16_t seq)
{
    int i;

    memset(iv, 0, 12);
    AV_WB32(&iv[2], ssrc);
    if (rtcp) {
        AV_WB32(&iv[8], index);
    } else {
        AV_WB32(&iv[6], roc);
        AV_WB16(&iv[10], seq);
    }
#ifdef TEST_SRTP_AEAD
    fprintf(stderr, "base: ");
    dump_memory(iv, 12);
    fprintf(stderr, "salt: ");
    dump_memory(salt, 12);
#endif

    for (i = 0; i < 12; i++)
        iv[i] ^= salt[i];

#ifdef TEST_SRTP_AEAD
    fprintf(stderr, "iv:   ");
    dump_memory(iv, 12);
#endif
}

int srtp_aead_decrypt(struct SRTPContext *s, uint8_t *buf, int *lenptr)
{
    uint8_t iv[12];
    uint8_t *in = buf;
    int len = *lenptr;
#warning seq_largest and roc may be unitialized
    int av_uninit(seq_largest);
    uint32_t ssrc, av_uninit(roc);
    uint64_t index;
    int rtcp, seq;
    size_t cipher_len;

    // TODO: Missing replay protection

    if (len < 12)
        return AVERROR_INVALIDDATA;

    rtcp = RTP_PT_IS_RTCP(buf[1]);
    if (!rtcp) {
        seq = AV_RB16(buf + 2);
        uint32_t v;

        // RFC 3711 section 3.3.1, appendix A
        seq_largest = s->seq_initialized ? s->seq_largest : seq;
        v = roc = s->roc;
        if (seq_largest < 32768) {
            if (seq - seq_largest > 32768)
                v = roc - 1;
        } else {
            if (seq_largest - 32768 > seq)
                v = roc + 1;
        }
        if (v == roc) {
            seq_largest = FFMAX(seq_largest, seq);
        } else if (v == roc + 1) {
            seq_largest = seq;
            roc = v;
        }
        index = seq + (((uint64_t)v) << 16);
    }

    if (rtcp) {
        uint32_t srtcp_index = AV_RB32(buf + len - 4);
        len -= 4;
        *lenptr = len;

        ssrc = AV_RB32(buf + 4);
        index = srtcp_index & 0x7fffffff;

        buf += 8;
        len -= 8;
        if (!(srtcp_index & 0x80000000))
            cipher_len = 0;
        else
            cipher_len = len - 16;
    } else {
        int ext, csrc;
        s->seq_initialized = 1;
        s->seq_largest     = seq_largest;
        s->roc             = roc;

        csrc = buf[0] & 0x0f;
        ext  = buf[0] & 0x10;
        ssrc = AV_RB32(buf + 8);

        *lenptr = len;
        buf += 12;
        len -= 12;

        buf += 4 * csrc;
        len -= 4 * csrc;
        if (len < 0)
            return AVERROR_INVALIDDATA;

        if (ext) {
            if (len < 4)
                return AVERROR_INVALIDDATA;
            ext = (AV_RB16(buf + 2) + 1) * 4;
            if (len < ext)
                return AVERROR_INVALIDDATA;
            len -= ext;
            buf += ext;
        }

        cipher_len = len - 16; // remove auth tag length
    }

    create_eaed_iv(iv, rtcp, rtcp ? s->rtcp_salt : s->rtp_salt, index, ssrc, s->roc, seq);
    gcm_aes256_set_iv(rtcp ? &s->aead_rtcp_ctx : &s->aead_rtp_ctx, 12, iv);

    size_t ptext_len = *lenptr - (buf - in);
    uint8_t auth_tag[16];

    if (rtcp) {
        uint8_t auth_data[12]; // need to use a temporary buffer for AAD as update need to be called only one time
        *(uint64_t*)auth_data = *(uint64_t*)in;
        ((uint32_t*)auth_data)[2] = *(uint32_t*)(buf + ptext_len);
        gcm_aes256_update(&s->aead_rtcp_ctx, sizeof(auth_data), auth_data);
        gcm_aes256_decrypt(&s->aead_rtcp_ctx, cipher_len, buf, buf);
        gcm_aes256_digest(&s->aead_rtcp_ctx, 16, auth_tag);
        ptext_len = cipher_len;
    } else {
        gcm_aes256_update(&s->aead_rtp_ctx, buf - in, in);
        gcm_aes256_decrypt(&s->aead_rtp_ctx, cipher_len, buf, buf);
        gcm_aes256_digest(&s->aead_rtp_ctx, 16, auth_tag);
        ptext_len = cipher_len;
    }

    if (memcmp(buf + cipher_len, auth_tag, 16) != 0) {
        av_log(NULL, AV_LOG_WARNING, "Authentification failed\n");
        return AVERROR_INVALIDDATA;
    }

    *lenptr = ptext_len + (buf - in); // plus AAD
    return 0;
}

int ff_srtp_decrypt(struct SRTPContext *s, uint8_t *buf, int *lenptr)
{
    /* AEAD algorithm contains MAC algorithm, has it's own IV and remove usage of some SRTP field */
    if (s->aead == 1)
        return srtp_aead_decrypt(s, buf, lenptr);

    uint8_t iv[16] = { 0 }, hmac[20];
    int len = *lenptr;
#warning seq_largest and roc may be unitialized
    int av_uninit(seq_largest);
    uint32_t ssrc, av_uninit(roc);
    uint64_t index;
    int rtcp, hmac_size;

    // TODO: Missing replay protection

    if (len < 2)
        return AVERROR_INVALIDDATA;

    rtcp = RTP_PT_IS_RTCP(buf[1]);
    hmac_size = rtcp ? s->rtcp_hmac_size : s->rtp_hmac_size;

    if (len < hmac_size)
        return AVERROR_INVALIDDATA;

    // Authentication HMAC
    av_hmac_init(s->hmac, rtcp ? s->rtcp_auth : s->rtp_auth, sizeof(s->rtp_auth));
    // If MKI is used, this should exclude the MKI as well
    av_hmac_update(s->hmac, buf, len - hmac_size);

    if (!rtcp) {
        int seq = AV_RB16(buf + 2);
        uint32_t v;
        uint8_t rocbuf[4];

        // RFC 3711 section 3.3.1, appendix A
        seq_largest = s->seq_initialized ? s->seq_largest : seq;
        v = roc = s->roc;
        if (seq_largest < 32768) {
            if (seq - seq_largest > 32768)
                v = roc - 1;
        } else {
            if (seq_largest - 32768 > seq)
                v = roc + 1;
        }
        if (v == roc) {
            seq_largest = FFMAX(seq_largest, seq);
        } else if (v == roc + 1) {
            seq_largest = seq;
            roc = v;
        }
        index = seq + (((uint64_t)v) << 16);

        AV_WB32(rocbuf, roc);
        av_hmac_update(s->hmac, rocbuf, 4);
    }

    av_hmac_final(s->hmac, hmac, sizeof(hmac));
    if (memcmp(hmac, buf + len - hmac_size, hmac_size)) {
        av_log(NULL, AV_LOG_WARNING, "HMAC mismatch\n");
        return AVERROR_INVALIDDATA;
    }

    len -= hmac_size;
    *lenptr = len;

    if (len < 12)
        return AVERROR_INVALIDDATA;

    if (rtcp) {
        uint32_t srtcp_index = AV_RB32(buf + len - 4);
        len -= 4;
        *lenptr = len;

        ssrc = AV_RB32(buf + 4);
        index = srtcp_index & 0x7fffffff;

        buf += 8;
        len -= 8;
        if (!(srtcp_index & 0x80000000))
            return 0;
    } else {
        int ext, csrc;
        s->seq_initialized = 1;
        s->seq_largest     = seq_largest;
        s->roc             = roc;

        csrc = buf[0] & 0x0f;
        ext  = buf[0] & 0x10;
        ssrc = AV_RB32(buf + 8);

        buf += 12;
        len -= 12;

        buf += 4 * csrc;
        len -= 4 * csrc;
        if (len < 0)
            return AVERROR_INVALIDDATA;

        if (ext) {
            if (len < 4)
                return AVERROR_INVALIDDATA;
            ext = (AV_RB16(buf + 2) + 1) * 4;
            if (len < ext)
                return AVERROR_INVALIDDATA;
            len -= ext;
            buf += ext;
        }
    }

    create_iv(iv, rtcp ? s->rtcp_salt : s->rtp_salt, index, ssrc);
    av_aes_init(s->aes, rtcp ? s->rtcp_key : s->rtp_key, 128, 0);
    encrypt_counter(s->aes, iv, buf, len);

    return 0;
}

int srtp_aead_encrypt(struct SRTPContext *s, const uint8_t *in, int len,
                      uint8_t *out, int outlen)
{
    uint8_t iv[12];
    int i, rtcp;
    uint32_t ssrc, index=0;
    uint16_t seq;
    uint8_t *buf, *salt;

    if (len < 8)
        return AVERROR_INVALIDDATA;

    memcpy(out, in, len);
    buf = out;

    rtcp = RTP_PT_IS_RTCP(in[1]);
    if (rtcp) {
        salt = s->rtcp_salt;
        ssrc = AV_RB32(buf + 4);
        index = s->rtcp_index++;
        index = 0x5d4;
    } else {
        salt = s->rtp_salt;
        seq = AV_RB16(in + 2);
        ssrc = AV_RB32(in + 8);
        if (seq < s->seq_largest)
            s->roc++;
        s->seq_largest = seq;
        index = seq + (((uint64_t)s->roc) << 16);
    }

    if (rtcp) {
        buf += 8;
        len -= 8;
    } else {
        // Compute auth and plaintext sizes
        int csrc = buf[0] & 0x0f;
        int ext = buf[0] & 0x10;

        buf += 12;
        len -= 12;

        buf += 4 * csrc;
        len -= 4 * csrc;
        if (len < 0)
            return AVERROR_INVALIDDATA;

        if (ext) {
            if (len < 4)
                return AVERROR_INVALIDDATA;
            ext = (AV_RB16(buf + 2) + 1) * 4;
            if (len < ext)
                return AVERROR_INVALIDDATA;
            len -= ext;
            buf += ext;
        }
    }

    create_eaed_iv(iv, rtcp, rtcp ? s->rtcp_salt : s->rtp_salt, index, ssrc, s->roc, seq);
    gcm_aes256_set_iv(rtcp ? &s->aead_rtcp_ctx : &s->aead_rtp_ctx, 12, iv);

    size_t ctext_len = outlen - (buf - out);
    uint8_t auth_tag[16];

    if (rtcp) {
        AV_WB32(buf + ctext_len - 4, 0x80000000 | index);
        uint8_t auth_data[12]; // need to use a temporary buffer for AAD as update need to be called only one time
        *(uint64_t*)auth_data = *(uint64_t*)out;
        ((uint32_t*)auth_data)[2] = *(uint32_t*)(buf + ctext_len - 4);
        gcm_aes256_update(&s->aead_rtcp_ctx, sizeof(auth_data), auth_data);
        gcm_aes256_encrypt(&s->aead_rtcp_ctx, len, buf, buf);
        gcm_aes256_digest(&s->aead_rtcp_ctx, 16, auth_tag);
        memcpy(buf+len, auth_tag, 16);
        ctext_len = len + 16 + 4;
    } else {
        gcm_aes256_update(&s->aead_rtp_ctx, buf - out, out);
        gcm_aes256_encrypt(&s->aead_rtp_ctx, len, buf, buf);
        gcm_aes256_digest(&s->aead_rtp_ctx, 16, auth_tag);
        memcpy(buf+len, auth_tag, 16);
        ctext_len = len + 16;
    }

    return buf + ctext_len - out;
}

int ff_srtp_encrypt(struct SRTPContext *s, const uint8_t *in, int len,
                    uint8_t *out, int outlen)
{
    /* AEAD algorithm contains MAC algorithm, has it's own IV and remove usage of some SRTP field */
    if (s->aead == 1)
        return srtp_aead_encrypt(s, in, len, out, outlen);

    uint8_t iv[16] = { 0 }, hmac[20];
    uint64_t index;
    uint32_t ssrc;
    int rtcp, hmac_size, padding;
    uint8_t *buf;

    if (len < 8)
        return AVERROR_INVALIDDATA;

    rtcp = RTP_PT_IS_RTCP(in[1]);
    hmac_size = rtcp ? s->rtcp_hmac_size : s->rtp_hmac_size;
    padding = hmac_size;
    if (rtcp)
        padding += 4; // For the RTCP index

    if (len + padding > outlen)
        return 0;

    memcpy(out, in, len);
    buf = out;

    if (rtcp) {
        ssrc = AV_RB32(buf + 4);
        index = s->rtcp_index++;

        buf += 8;
        len -= 8;
    } else {
        int ext, csrc;
        int seq = AV_RB16(buf + 2);

        if (len < 12)
            return AVERROR_INVALIDDATA;

        ssrc = AV_RB32(buf + 8);

        if (seq < s->seq_largest)
            s->roc++;
        s->seq_largest = seq;
        index = seq + (((uint64_t)s->roc) << 16);

        csrc = buf[0] & 0x0f;
        ext = buf[0] & 0x10;

        buf += 12;
        len -= 12;

        buf += 4 * csrc;
        len -= 4 * csrc;
        if (len < 0)
            return AVERROR_INVALIDDATA;

        if (ext) {
            if (len < 4)
                return AVERROR_INVALIDDATA;
            ext = (AV_RB16(buf + 2) + 1) * 4;
            if (len < ext)
                return AVERROR_INVALIDDATA;
            len -= ext;
            buf += ext;
        }
    }

    create_iv(iv, rtcp ? s->rtcp_salt : s->rtp_salt, index, ssrc);
    av_aes_init(s->aes, rtcp ? s->rtcp_key : s->rtp_key, 128, 0);
    encrypt_counter(s->aes, iv, buf, len);

    if (rtcp) {
        AV_WB32(buf + len, 0x80000000 | index);
        len += 4;
    }

    av_hmac_init(s->hmac, rtcp ? s->rtcp_auth : s->rtp_auth, sizeof(s->rtp_auth));
    av_hmac_update(s->hmac, out, buf + len - out);
    if (!rtcp) {
        uint8_t rocbuf[4];
        AV_WB32(rocbuf, s->roc);
        av_hmac_update(s->hmac, rocbuf, 4);
    }
    av_hmac_final(s->hmac, hmac, sizeof(hmac));

    memcpy(buf + len, hmac, hmac_size);
    len += hmac_size;
    return buf + len - out;
}

#ifdef TEST_SRTP_AEAD
int test_gcm256(const uint8_t* plain, int plain_len, const uint8_t* cipher, int cipher_len, int rtcp)
{
    uint8_t master_salt[32 + 12];
    char params[sizeof(master_salt)*2];
    uint8_t tmp_output[FFMAX(plain_len, cipher_len)];
    int i, res;

    struct SRTPContext ctx;

    memset(&ctx, 0, sizeof(ctx));
    memset(&params, 0, sizeof(params));
    memset(&tmp_output, 0, sizeof(tmp_output));

    /* master key */
    for (i=0; i < 32; i++)
        master_salt[i] = i;

    /* append master salt */
    strncpy((char*)&master_salt[32], "Quid pro quo", 12);

    /* base64 encode master key|salt */
    if (!av_base64_encode(params, sizeof(params), master_salt, sizeof(master_salt))) {
        fprintf(stderr, "!! Base64 encoding failed\n");
        return -1;
    }

    if (1) {
        /* init crypto */
        printf("[encryption]\n");
        if (ff_srtp_set_crypto(&ctx, "SRTP_AEAD_AES_256_GCM", params) < 0) {
            fprintf(stderr, "!! Initialisation failed\n");
            return -1;
        }

        /* test encrypt */
        res = ff_srtp_encrypt(&ctx, plain, plain_len, tmp_output, sizeof(tmp_output));
        if (res < 0){
            fprintf(stderr, "!! Encryption failed\n");
            return -1;
        }

        printf("result:\n");
        dump_memory(tmp_output, res);
        printf("waited:\n");
        dump_memory(cipher, cipher_len);

        if (res != cipher_len) {
            fprintf(stderr, "!! Encryption failed: lengths differ (get %d, waited %d)\n",
                    res, cipher_len);
            return -1;
        }

        if (memcmp(tmp_output, cipher, cipher_len)) {
            fprintf(stderr, "!! Encryption failed: wrong result\n");
            return -1;
        }

        ff_srtp_free(&ctx);
    }

    /* Reset data for decryption */
    memset(&ctx, 0, sizeof(ctx));
    memset(&tmp_output, 0, sizeof(tmp_output));

    /* init crypto */
    printf("\n[decryption]\n");
    if (ff_srtp_set_crypto(&ctx, "SRTP_AEAD_AES_256_GCM", params) < 0) {
        fprintf(stderr, "!! Initialisation failed\n");
        return -1;
    }

    /* test decrypt */
    memcpy(tmp_output, cipher, cipher_len);
    res = ff_srtp_decrypt(&ctx, tmp_output, &cipher_len);
    if (res < 0) {
        fprintf(stderr, "!! Decryption failed\n");
        return -1;
    }

    printf("result:\n");
    dump_memory(tmp_output, cipher_len);
    printf("waited:\n");
    dump_memory(plain, plain_len);

    if (cipher_len != plain_len) {
        fprintf(stderr, "!! Decryption failed: lengths differ (get %d, waited %d)\n",
                cipher_len, plain_len);
        return -1;
    }

    if (memcmp(tmp_output, plain, plain_len)) {
        fprintf(stderr, "!! Decryption failed: wrong result\n");
        return -1;
    }

    ff_srtp_free(&ctx);

    return 0;
}

int main(int argc, char** argv)
{
    /* Following test vectors are comming from RFC 7714, Ch. 16.2.x and 17.2.x
     * Notes: only 256bits version is tested.
     */

    uint8_t rtp_plain[] = {
        0x80, 0x40, 0xf1, 0x7b,
        0x80, 0x41, 0xf8, 0xd3,
        0x55, 0x01, 0xa0, 0xb2,
        0x47, 0x61, 0x6c, 0x6c,
        0x69, 0x61, 0x20, 0x65,
        0x73, 0x74, 0x20, 0x6f,
        0x6d, 0x6e, 0x69, 0x73,
        0x20, 0x64, 0x69, 0x76,
        0x69, 0x73, 0x61, 0x20,
        0x69, 0x6e, 0x20, 0x70,
        0x61, 0x72, 0x74, 0x65,
        0x73, 0x20, 0x74, 0x72,
        0x65, 0x73
    };

    uint8_t rtp_cipher[] = {
        0x80, 0x40, 0xf1, 0x7b,
        0x80, 0x41, 0xf8, 0xd3,
        0x55, 0x01, 0xa0, 0xb2,
        0x32, 0xb1, 0xde, 0x78,
        0xa8, 0x22, 0xfe, 0x12,
        0xef, 0x9f, 0x78, 0xfa,
        0x33, 0x2e, 0x33, 0xaa,
        0xb1, 0x80, 0x12, 0x38,
        0x9a, 0x58, 0xe2, 0xf3,
        0xb5, 0x0b, 0x2a, 0x02,
        0x76, 0xff, 0xae, 0x0f,
        0x1b, 0xa6, 0x37, 0x99,
        0xb8, 0x7b, 0x7a, 0xa3,
        0xdb, 0x36, 0xdf, 0xff,
        0xd6, 0xb0, 0xf9, 0xbb,
        0x78, 0x78, 0xd7, 0xa7,
        0x6c, 0x13
    };

    uint8_t rtcp_plain[] = {
        0x81, 0xc8, 0x00, 0x0d,
        0x4d, 0x61, 0x72, 0x73,
        0x4e, 0x54, 0x50, 0x31,
        0x4e, 0x54, 0x50, 0x32,
        0x52, 0x54, 0x50, 0x20,
        0x00, 0x00, 0x04, 0x2a,
        0x00, 0x00, 0xe9, 0x30,
        0x4c, 0x75, 0x6e, 0x61,
        0xde, 0xad, 0xbe, 0xef,
        0xde, 0xad, 0xbe, 0xef,
        0xde, 0xad, 0xbe, 0xef,
        0xde, 0xad, 0xbe, 0xef,
        0xde, 0xad, 0xbe, 0xef
    };

    uint8_t rtcp_cipher[] = {
        0x81, 0xc8, 0x00, 0x0d,
        0x4d, 0x61, 0x72, 0x73,
        0xd5, 0x0a, 0xe4, 0xd1,
        0xf5, 0xce, 0x5d, 0x30,
        0x4b, 0xa2, 0x97, 0xe4,
        0x7d, 0x47, 0x0c, 0x28,
        0x2c, 0x3e, 0xce, 0x5d,
        0xbf, 0xfe, 0x0a, 0x50,
        0xa2, 0xea, 0xa5, 0xc1,
        0x11, 0x05, 0x55, 0xbe,
        0x84, 0x15, 0xf6, 0x58,
        0xc6, 0x1d, 0xe0, 0x47,
        0x6f, 0x1b, 0x6f, 0xad,
        0x1d, 0x1e, 0xb3, 0x0c,
        0x44, 0x46, 0x83, 0x9f,
        0x57, 0xff, 0x6f, 0x6c,
        0xb2, 0x6a, 0xc3, 0xbe,
        0x80, 0x00, 0x05, 0xd4
    };

    int res, failures = 0;

    printf("**** Testing RTP packet ****\n");
    res = test_gcm256(rtp_plain, sizeof(rtp_plain),
                      rtp_cipher, sizeof(rtp_cipher), 0);
    if (res < 0) {
        fprintf(stderr, "!!!! RTP test failed !!!!\n");
        failures++;
    }

    printf("\n**** Testing RTCP packet ****\n");
    res = test_gcm256(rtcp_plain, sizeof(rtcp_plain),
                      rtcp_cipher, sizeof(rtcp_cipher), 1);
    if (res < 0) {
        fprintf(stderr, "!!!! RTP test failed !!!!\n");
        failures++;
    }

    if (failures) {
        fprintf(stderr, "\n==== %u test(s) FAILED ====\n", failures);
        return -1;
    }

    fprintf(stderr, "\n==== All tests PASSED ====\n");
    return 0;
}
#endif
