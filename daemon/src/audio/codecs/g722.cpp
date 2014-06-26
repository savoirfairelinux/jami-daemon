/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author:  Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  Mostly borrowed from asterisk's sources (Steve Underwood <steveu@coppice.org>)
 *  See:  http://svnview.digium.com/svn/asterisk?view=revision&revision=194722
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

#include "audiocodec.h"
#include "sfl_types.h"
#include "g722.h"

#include <cstdlib>
#include <cstring>

class G722 : public sfl::AudioCodec {

    public:
        G722() : sfl::AudioCodec(9, "G722", 16000, 320, 1), decode_state_(),
            encode_state_() {
            bitrate_ = 64;
            hasDynamicPayload_ = false;

            g722_state_init(decode_state_);
            g722_state_init(encode_state_);
        }

    private:
        AudioCodec *
        clone()
        {
            return new G722;
        }

        int decode(SFLAudioSample *pcm, unsigned char *data, size_t len)
        {
            return g722_decode(pcm, data, len);
        }

        int encode(unsigned char *data, SFLAudioSample *pcm, size_t max_data_bytes)
        {
            int out = g722_encode(data, pcm, std::min<size_t>(frameSize_, max_data_bytes));
            return out;
        }

        static void g722_state_init(g722_state_t &state) {
            state.itu_test_mode = false;

            // 8 => 64 kbps;  7 => 56 kbps;  6 => 48 kbps
            state.bits_per_sample = 8;

            // Enable 8khz mode, encode using lower subband only
            state.eight_k = false;

            // Never set packed true when using 64 kbps
            state.packed = false;

            memset(state.band, 0, sizeof(state.band));
            state.band[0].det = 32;
            state.band[1].det = 8;

            memset(state.x, 0, sizeof(state.x));

            state.in_buffer = 0;
            state.in_bits = 0;
            state.out_buffer = 0;
            state.out_bits = 0;
        }

        SFLAudioSample saturate(int32_t amp)
        {
            SFLAudioSample amp16 = 0;

            /* Hopefully this is optimised for the common case - not clipping */
            amp16 = (SFLAudioSample) amp;

            if (amp == amp16)
                return amp16;

            if (amp > INT16_MAX)
                return INT16_MAX;

            return INT16_MIN;
        }

        void block4_encode(int band, int d) {
            int wd1 = 0;
            int wd2 = 0;
            int wd3 = 0;
            int i = 0;

            /* Block 4, RECONS */
            encode_state_.band[band].d[0] = d;
            encode_state_.band[band].r[0] = saturate(encode_state_.band[band].s + d);

            /* Block 4, PARREC */
            encode_state_.band[band].p[0] = saturate(encode_state_.band[band].sz + d);

            /* Block 4, UPPOL2 */

            for (i = 0;  i < 3;  i++)
                encode_state_.band[band].sg[i] = encode_state_.band[band].p[i] >> 15;

            wd1 = saturate(encode_state_.band[band].a[1] << 2);

            wd2 = (encode_state_.band[band].sg[0] == encode_state_.band[band].sg[1])  ?  -wd1  :  wd1;

            if (wd2 > 32767)
                wd2 = 32767;

            wd3 = (wd2 >> 7) + ((encode_state_.band[band].sg[0] == encode_state_.band[band].sg[2])  ?  128  :  -128);

            wd3 += (encode_state_.band[band].a[2]*32512) >> 15;

            if (wd3 > 12288)
                wd3 = 12288;
            else if (wd3 < -12288)
                wd3 = -12288;

            encode_state_.band[band].ap[2] = wd3;

            /* Block 4, UPPOL1 */
            encode_state_.band[band].sg[0] = encode_state_.band[band].p[0] >> 15;

            encode_state_.band[band].sg[1] = encode_state_.band[band].p[1] >> 15;

            wd1 = (encode_state_.band[band].sg[0] == encode_state_.band[band].sg[1])  ?  192  :  -192;

            wd2 = (encode_state_.band[band].a[1]*32640) >> 15;

            encode_state_.band[band].ap[1] = saturate(wd1 + wd2);

            wd3 = saturate(15360 - encode_state_.band[band].ap[2]);

            if (encode_state_.band[band].ap[1] > wd3)
                encode_state_.band[band].ap[1] = wd3;
            else if (encode_state_.band[band].ap[1] < -wd3)
                encode_state_.band[band].ap[1] = -wd3;

            /* Block 4, UPZERO */
            wd1 = (d == 0)  ?  0  :  128;

            encode_state_.band[band].sg[0] = d >> 15;

            for (i = 1;  i < 7;  i++) {
                encode_state_.band[band].sg[i] = encode_state_.band[band].d[i] >> 15;
                wd2 = (encode_state_.band[band].sg[i] == encode_state_.band[band].sg[0])  ?  wd1  :  -wd1;
                wd3 = (encode_state_.band[band].b[i]*32640) >> 15;
                encode_state_.band[band].bp[i] = saturate(wd2 + wd3);
            }

            /* Block 4, DELAYA */
            for (i = 6;  i > 0;  i--) {
                encode_state_.band[band].d[i] = encode_state_.band[band].d[i - 1];
                encode_state_.band[band].b[i] = encode_state_.band[band].bp[i];
            }

            for (i = 2;  i > 0;  i--) {
                encode_state_.band[band].r[i] = encode_state_.band[band].r[i - 1];
                encode_state_.band[band].p[i] = encode_state_.band[band].p[i - 1];
                encode_state_.band[band].a[i] = encode_state_.band[band].ap[i];
            }

            /* Block 4, FILTEP */
            wd1 = saturate(encode_state_.band[band].r[1] + encode_state_.band[band].r[1]);

            wd1 = (encode_state_.band[band].a[1]*wd1) >> 15;

            wd2 = saturate(encode_state_.band[band].r[2] + encode_state_.band[band].r[2]);

            wd2 = (encode_state_.band[band].a[2]*wd2) >> 15;

            encode_state_.band[band].sp = saturate(wd1 + wd2);

            /* Block 4, FILTEZ */
            encode_state_.band[band].sz = 0;

            for (i = 6;  i > 0;  i--) {
                wd1 = saturate(encode_state_.band[band].d[i] + encode_state_.band[band].d[i]);
                encode_state_.band[band].sz += (encode_state_.band[band].b[i]*wd1) >> 15;
            }

            encode_state_.band[band].sz = saturate(encode_state_.band[band].sz);

            /* Block 4, PREDIC */
            encode_state_.band[band].s = saturate(encode_state_.band[band].sp + encode_state_.band[band].sz);

        }

        void block4_decode(int band, int d) {
            int wd1 = 0;
            int wd2 = 0;
            int wd3 = 0;
            int i = 0;

            /* Block 4, RECONS */
            decode_state_.band[band].d[0] = d;
            decode_state_.band[band].r[0] = saturate(decode_state_.band[band].s + d);

            /* Block 4, PARREC */
            decode_state_.band[band].p[0] = saturate(decode_state_.band[band].sz + d);

            /* Block 4, UPPOL2 */

            for (i = 0;  i < 3;  i++)
                decode_state_.band[band].sg[i] = decode_state_.band[band].p[i] >> 15;

            wd1 = saturate(decode_state_.band[band].a[1] << 2);

            wd2 = (decode_state_.band[band].sg[0] == decode_state_.band[band].sg[1])  ?  -wd1  :  wd1;

            if (wd2 > 32767)
                wd2 = 32767;

            wd3 = (decode_state_.band[band].sg[0] == decode_state_.band[band].sg[2])  ?  128  :  -128;

            wd3 += (wd2 >> 7);

            wd3 += (decode_state_.band[band].a[2]*32512) >> 15;

            if (wd3 > 12288)
                wd3 = 12288;
            else if (wd3 < -12288)
                wd3 = -12288;

            decode_state_.band[band].ap[2] = wd3;

            /* Block 4, UPPOL1 */
            decode_state_.band[band].sg[0] = decode_state_.band[band].p[0] >> 15;

            decode_state_.band[band].sg[1] = decode_state_.band[band].p[1] >> 15;

            wd1 = (decode_state_.band[band].sg[0] == decode_state_.band[band].sg[1])  ?  192  :  -192;

            wd2 = (decode_state_.band[band].a[1]*32640) >> 15;

            decode_state_.band[band].ap[1] = saturate(wd1 + wd2);

            wd3 = saturate(15360 - decode_state_.band[band].ap[2]);

            if (decode_state_.band[band].ap[1] > wd3)
                decode_state_.band[band].ap[1] = wd3;
            else if (decode_state_.band[band].ap[1] < -wd3)
                decode_state_.band[band].ap[1] = -wd3;

            /* Block 4, UPZERO */
            wd1 = (d == 0)  ?  0  :  128;

            decode_state_.band[band].sg[0] = d >> 15;

            for (i = 1;  i < 7;  i++) {
                decode_state_.band[band].sg[i] = decode_state_.band[band].d[i] >> 15;
                wd2 = (decode_state_.band[band].sg[i] == decode_state_.band[band].sg[0])  ?  wd1  :  -wd1;
                wd3 = (decode_state_.band[band].b[i]*32640) >> 15;
                decode_state_.band[band].bp[i] = saturate(wd2 + wd3);
            }

            /* Block 4, DELAYA */
            for (i = 6;  i > 0;  i--) {
                decode_state_.band[band].d[i] = decode_state_.band[band].d[i - 1];
                decode_state_.band[band].b[i] = decode_state_.band[band].bp[i];
            }

            for (i = 2;  i > 0;  i--) {
                decode_state_.band[band].r[i] = decode_state_.band[band].r[i - 1];
                decode_state_.band[band].p[i] = decode_state_.band[band].p[i - 1];
                decode_state_.band[band].a[i] = decode_state_.band[band].ap[i];
            }

            /* Block 4, FILTEP */
            wd1 = saturate(decode_state_.band[band].r[1] + decode_state_.band[band].r[1]);

            wd1 = (decode_state_.band[band].a[1]*wd1) >> 15;

            wd2 = saturate(decode_state_.band[band].r[2] + decode_state_.band[band].r[2]);

            wd2 = (decode_state_.band[band].a[2]*wd2) >> 15;

            decode_state_.band[band].sp = saturate(wd1 + wd2);

            /* Block 4, FILTEZ */
            decode_state_.band[band].sz = 0;

            for (i = 6;  i > 0;  i--) {
                wd1 = saturate(decode_state_.band[band].d[i] + decode_state_.band[band].d[i]);
                decode_state_.band[band].sz += (decode_state_.band[band].b[i]*wd1) >> 15;
            }

            decode_state_.band[band].sz = saturate(decode_state_.band[band].sz);

            /* Block 4, PREDIC */
            decode_state_.band[band].s = saturate(decode_state_.band[band].sp + decode_state_.band[band].sz);
        }

        int g722_decode(SFLAudioSample amp[], const uint8_t g722_data[], int len)
        {
            static const int wl[8] = {-60, -30, 58, 172, 334, 538, 1198, 3042 };
            static const int rl42[16] = {0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3,  2, 1, 0 };
            static const int ilb[32] = {
                2048, 2093, 2139, 2186, 2233, 2282, 2332,
                2383, 2435, 2489, 2543, 2599, 2656, 2714,
                2774, 2834, 2896, 2960, 3025, 3091, 3158,
                3228, 3298, 3371, 3444, 3520, 3597, 3676,
                3756, 3838, 3922, 4008
            };
            static const int wh[3] = {0, -214, 798};
            static const int rh2[4] = {2, 1, 2, 1};
            static const int qm2[4] = {-7408, -1616,  7408,   1616};
            static const int qm4[16] = {
                0, -20456, -12896,  -8968,
                -6288,  -4240,  -2584,  -1200,
                20456,  12896,   8968,   6288,
                4240,   2584,   1200,      0
            };
            static const int qm5[32] = {
                -280,   -280, -23352, -17560,
                -14120, -11664,  -9752,  -8184,
                -6864,  -5712,  -4696,  -3784,
                -2960,  -2208,  -1520,   -880,
                23352,  17560,  14120,  11664,
                9752,   8184,   6864,   5712,
                4696,   3784,   2960,   2208,
                1520,    880,    280,   -280
            };
            static const int qm6[64] = {
                -136,   -136,   -136,   -136,
                -24808, -21904, -19008, -16704,
                -14984, -13512, -12280, -11192,
                -10232,  -9360,  -8576,  -7856,
                -7192,  -6576,  -6000,  -5456,
                -4944,  -4464,  -4008,  -3576,
                -3168,  -2776,  -2400,  -2032,
                -1688,  -1360,  -1040,   -728,
                24808,  21904,  19008,  16704,
                14984,  13512,  12280,  11192,
                10232,   9360,   8576,   7856,
                7192,   6576,   6000,   5456,
                4944,   4464,   4008,   3576,
                3168,   2776,   2400,   2032,
                1688,   1360,   1040,    728,
                432,    136,   -432,   -136
            };
            static const int qmf_coeffs[12] = {
                3,  -11,   12,   32, -210,  951, 3876, -805,  362, -156,   53,  -11,
            };

            int dlowt = 0;
            int rlow = 0;
            int ihigh = 0;
            int dhigh = 0;
            int rhigh = 0;
            int xout1 = 0;
            int xout2 = 0;
            int wd1 = 0;
            int wd2 = 0;
            int wd3 = 0;
            int code = 0;
            int outlen = 0;
            int i = 0;
            int j = 0;

            outlen = 0;
            rhigh = 0;



            for (j = 0;  j < len;) {
                if (decode_state_.packed) {
                    /* Unpack the code bits */
                    if (decode_state_.in_bits < decode_state_.bits_per_sample) {
                        decode_state_.in_buffer |= (g722_data[j++] << decode_state_.in_bits);
                        decode_state_.in_bits += 8;
                    }

                    code = decode_state_.in_buffer & ((1 << decode_state_.bits_per_sample) - 1);

                    decode_state_.in_buffer >>= decode_state_.bits_per_sample;
                    decode_state_.in_bits -= decode_state_.bits_per_sample;
                } else {
                    code = g722_data[j++];
                }

                switch (decode_state_.bits_per_sample) {

                    default:

                    case 8:
                        wd1 = code & 0x3F;
                        ihigh = (code >> 6) & 0x03;
                        wd2 = qm6[wd1];
                        wd1 >>= 2;
                        break;

                    case 7:
                        wd1 = code & 0x1F;
                        ihigh = (code >> 5) & 0x03;
                        wd2 = qm5[wd1];
                        wd1 >>= 1;
                        break;

                    case 6:
                        wd1 = code & 0x0F;
                        ihigh = (code >> 4) & 0x03;
                        wd2 = qm4[wd1];
                        break;
                }

                /* Block 5L, LOW BAND INVQBL */
                wd2 = (decode_state_.band[0].det*wd2) >> 15;

                /* Block 5L, RECONS */
                rlow = decode_state_.band[0].s + wd2;

                /* Block 6L, LIMIT */
                if (rlow > 16383)
                    rlow = 16383;
                else if (rlow < -16384)
                    rlow = -16384;

                /* Block 2L, INVQAL */
                wd2 = qm4[wd1];

                dlowt = (decode_state_.band[0].det*wd2) >> 15;

                /* Block 3L, LOGSCL */
                wd2 = rl42[wd1];

                wd1 = (decode_state_.band[0].nb*127) >> 7;

                wd1 += wl[wd2];

                if (wd1 < 0)
                    wd1 = 0;
                else if (wd1 > 18432)
                    wd1 = 18432;

                decode_state_.band[0].nb = wd1;

                /* Block 3L, SCALEL */
                wd1 = (decode_state_.band[0].nb >> 6) & 31;

                wd2 = 8 - (decode_state_.band[0].nb >> 11);

                wd3 = (wd2 < 0)  ? (ilb[wd1] << -wd2)  : (ilb[wd1] >> wd2);

                decode_state_.band[0].det = wd3 << 2;

                block4_decode(0, dlowt);

                if (!decode_state_.eight_k) {
                    /* Block 2H, INVQAH */
                    wd2 = qm2[ihigh];
                    dhigh = (decode_state_.band[1].det*wd2) >> 15;
                    /* Block 5H, RECONS */
                    rhigh = dhigh + decode_state_.band[1].s;
                    /* Block 6H, LIMIT */

                    if (rhigh > 16383)
                        rhigh = 16383;
                    else if (rhigh < -16384)
                        rhigh = -16384;

                    /* Block 2H, INVQAH */
                    wd2 = rh2[ihigh];

                    wd1 = (decode_state_.band[1].nb*127) >> 7;

                    wd1 += wh[wd2];

                    if (wd1 < 0)
                        wd1 = 0;
                    else if (wd1 > 22528)
                        wd1 = 22528;

                    decode_state_.band[1].nb = wd1;

                    /* Block 3H, SCALEH */
                    wd1 = (decode_state_.band[1].nb >> 6) & 31;

                    wd2 = 10 - (decode_state_.band[1].nb >> 11);

                    wd3 = (wd2 < 0)  ? (ilb[wd1] << -wd2)  : (ilb[wd1] >> wd2);

                    decode_state_.band[1].det = wd3 << 2;

                    block4_decode(1, dhigh);
                }

                if (decode_state_.itu_test_mode) {
                    amp[outlen++] = (SFLAudioSample)(rlow << 1);
                    amp[outlen++] = (SFLAudioSample)(rhigh << 1);
                } else {
                    if (decode_state_.eight_k) {
                        amp[outlen++] = (SFLAudioSample) (rlow << 1);
                    } else {
                        /* Apply the receive QMF */
                        for (i = 0;  i < 22;  i++)
                            decode_state_.x[i] = decode_state_.x[i + 2];

                        decode_state_.x[22] = rlow + rhigh;

                        decode_state_.x[23] = rlow - rhigh;

                        xout1 = 0;

                        xout2 = 0;

                        for (i = 0;  i < 12;  i++) {
                            xout2 += decode_state_.x[2*i]*qmf_coeffs[i];
                            xout1 += decode_state_.x[2*i + 1]*qmf_coeffs[11 - i];
                        }

                        amp[outlen++] = (SFLAudioSample)(xout1 >> 11);

                        amp[outlen++] = (SFLAudioSample)(xout2 >> 11);
                    }
                }
            }

            return outlen;
        }

        int g722_encode(uint8_t g722_data[], const SFLAudioSample amp[], int len)
        {
            static const int q6[32] = {
                0,   35,   72,  110,  150,  190,  233,  276,
                323,  370,  422,  473,  530,  587,  650,  714,
                786,  858,  940, 1023, 1121, 1219, 1339, 1458,
                1612, 1765, 1980, 2195, 2557, 2919,    0,    0
            };
            static const int iln[32] = {
                0, 63, 62, 31, 30, 29, 28, 27,
                26, 25, 24, 23, 22, 21, 20, 19,
                18, 17, 16, 15, 14, 13, 12, 11,
                10,  9,  8,  7,  6,  5,  4,  0
            };
            static const int ilp[32] = {
                0, 61, 60, 59, 58, 57, 56, 55,
                54, 53, 52, 51, 50, 49, 48, 47,
                46, 45, 44, 43, 42, 41, 40, 39,
                38, 37, 36, 35, 34, 33, 32,  0
            };
            static const int wl[8] = {
                -60, -30, 58, 172, 334, 538, 1198, 3042
            };
            static const int rl42[16] = {
                0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0
            };
            static const int ilb[32] = {
                2048, 2093, 2139, 2186, 2233, 2282, 2332,
                2383, 2435, 2489, 2543, 2599, 2656, 2714,
                2774, 2834, 2896, 2960, 3025, 3091, 3158,
                3228, 3298, 3371, 3444, 3520, 3597, 3676,
                3756, 3838, 3922, 4008
            };
            static const int qm4[16] = {
                0, -20456, -12896, -8968,
                -6288,  -4240,  -2584, -1200,
                20456,  12896,   8968,  6288,
                4240,   2584,   1200,     0
            };
            static const int qm2[4] = {
                -7408,  -1616,   7408,   1616
            };
            static const int qmf_coeffs[12] = {
                3,  -11,   12,   32, -210,  951, 3876, -805,  362, -156,   53,  -11,
            };
            static const int ihn[3] = {0, 1, 0};
            static const int ihp[3] = {0, 3, 2};
            static const int wh[3] = {0, -214, 798};
            static const int rh2[4] = {2, 1, 2, 1};

            int dlow = 0;
            int dhigh = 0;
            int el = 0;
            int wd = 0;
            int wd1 = 0;
            int ril = 0;
            int wd2 = 0;
            int il4 = 0;
            int ih2 = 0;
            int wd3 = 0;
            int eh = 0;
            int mih = 0;
            int i = 0;
            int j = 0;
            /* Low and high band PCM from the QMF */
            int xlow = 0;
            int xhigh = 0;
            int g722_bytes = 0;
            /* Even and odd tap accumulators */
            int sumeven = 0;
            int sumodd = 0;
            int ihigh = 0;
            int ilow = 0;
            int code = 9;

            g722_bytes = 0;
            xhigh = 0;

            for (j = 0;  j < len;) {
                if (encode_state_.itu_test_mode) {
                    xlow =
                        xhigh = amp[j++] >> 1;
                } else {
                    if (encode_state_.eight_k) {
                        xlow = amp[j++] >> 1;
                    } else {
                        /* Apply the transmit QMF */
                        /* Shuffle the buffer down */
                        for (i = 0;  i < 22;  i++)
                            encode_state_.x[i] = encode_state_.x[i + 2];

                        encode_state_.x[22] = amp[j++];

                        encode_state_.x[23] = amp[j++];

                        /* Discard every other QMF output */
                        sumeven = 0;

                        sumodd = 0;

                        for (i = 0;  i < 12;  i++) {
                            sumodd += encode_state_.x[2*i]*qmf_coeffs[i];
                            sumeven += encode_state_.x[2*i + 1]*qmf_coeffs[11 - i];
                        }

                        xlow = (sumeven + sumodd) >> 14;

                        xhigh = (sumeven - sumodd) >> 14;
                    }
                }

                /* Block 1L, SUBTRA */
                el = saturate(xlow - encode_state_.band[0].s);

                /* Block 1L, QUANTL */
                wd = (el >= 0)  ?  el  :  - (el + 1);

                for (i = 1;  i < 30;  i++) {
                    wd1 = (q6[i]*encode_state_.band[0].det) >> 12;

                    if (wd < wd1)
                        break;
                }

                ilow = (el < 0)  ?  iln[i]  :  ilp[i];

                /* Block 2L, INVQAL */
                ril = ilow >> 2;
                wd2 = qm4[ril];
                dlow = (encode_state_.band[0].det*wd2) >> 15;

                /* Block 3L, LOGSCL */
                il4 = rl42[ril];
                wd = (encode_state_.band[0].nb*127) >> 7;
                encode_state_.band[0].nb = wd + wl[il4];

                if (encode_state_.band[0].nb < 0)
                    encode_state_.band[0].nb = 0;
                else if (encode_state_.band[0].nb > 18432)
                    encode_state_.band[0].nb = 18432;

                /* Block 3L, SCALEL */
                wd1 = (encode_state_.band[0].nb >> 6) & 31;

                wd2 = 8 - (encode_state_.band[0].nb >> 11);

                wd3 = (wd2 < 0)  ? (ilb[wd1] << -wd2)  : (ilb[wd1] >> wd2);

                encode_state_.band[0].det = wd3 << 2;

                block4_encode(0, dlow);

                if (encode_state_.eight_k) {
                    /* Just leave the high bits as zero */
                    code = (0xC0 | ilow) >> (8 - encode_state_.bits_per_sample);
                } else {
                    /* Block 1H, SUBTRA */
                    eh = saturate(xhigh - encode_state_.band[1].s);

                    /* Block 1H, QUANTH */
                    wd = (eh >= 0)  ?  eh  :  - (eh + 1);
                    wd1 = (564*encode_state_.band[1].det) >> 12;
                    mih = (wd >= wd1)  ?  2  :  1;
                    ihigh = (eh < 0)  ?  ihn[mih]  :  ihp[mih];

                    /* Block 2H, INVQAH */
                    wd2 = qm2[ihigh];
                    dhigh = (encode_state_.band[1].det*wd2) >> 15;

                    /* Block 3H, LOGSCH */
                    ih2 = rh2[ihigh];
                    wd = (encode_state_.band[1].nb*127) >> 7;
                    encode_state_.band[1].nb = wd + wh[ih2];

                    if (encode_state_.band[1].nb < 0)
                        encode_state_.band[1].nb = 0;
                    else if (encode_state_.band[1].nb > 22528)
                        encode_state_.band[1].nb = 22528;

                    /* Block 3H, SCALEH */
                    wd1 = (encode_state_.band[1].nb >> 6) & 31;

                    wd2 = 10 - (encode_state_.band[1].nb >> 11);

                    wd3 = (wd2 < 0)  ? (ilb[wd1] << -wd2)  : (ilb[wd1] >> wd2);

                    encode_state_.band[1].det = wd3 << 2;

                    block4_encode(1, dhigh);

                    code = ((ihigh << 6) | ilow) >> (8 - encode_state_.bits_per_sample);
                }

                if (encode_state_.packed) {
                    /* Pack the code bits */
                    encode_state_.out_buffer |= (code << encode_state_.out_bits);
                    encode_state_.out_bits += encode_state_.bits_per_sample;

                    if (encode_state_.out_bits >= 8) {
                        g722_data[g722_bytes++] = (uint8_t)(encode_state_.out_buffer & 0xFF);
                        encode_state_.out_bits -= 8;
                        encode_state_.out_buffer >>= 8;
                    }
                } else {
                    g722_data[g722_bytes++] = (uint8_t) code;
                }
            }

            return g722_bytes;
        }

        g722_state_t decode_state_;
        g722_state_t encode_state_;
};

// the class factories
// cppcheck-suppress unusedFunction
extern "C" sfl::AudioCodec* AUDIO_CODEC_ENTRY()
{
    return new G722;
}

// cppcheck-suppress unusedFunction
extern "C" void destroy(sfl::AudioCodec* a)
{
    delete a;
}

