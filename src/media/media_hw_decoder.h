/*
 *  Copyright (C) 2015-2015 Savoir-Faire Linux Inc.
 *  Author: Eloi Bail <Eloi.Bail@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
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


#pragma once

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "libav_deps.h" // MUST BE INCLUDED FIRST

namespace ring {

struct HwDecoderCtx {
    AVCodecContext *avctx;
    AVFrame *pic;
    struct vd_lavc_hwdec *hwdec;
    enum AVPixelFormat pix_fmt;
    int best_csp;
    enum AVDiscard skip_frame;
    const char *software_fallback_decoder;
    bool hwdec_failed;
    bool hwdec_notified;

    // From VO
    //struct mp_hwdec_info *hwdec_info;

    // For free use by hwdec implementation
    void *hwdec_priv;

    int hwdec_fmt;
    int hwdec_w;
    int hwdec_h;
    int hwdec_profile;

    bool hwdec_request_reinit;
};


class MediaHwDecoder {
    public:
        // pure virtual class methods
        virtual void init(HwDecoderCtx ctx) = 0;
        virtual void uninit(HwDecoderCtx ctx) = 0;
        virtual void init_decoder(HwDecoderCtx ctx ,int w, int h) = 0;
        virtual void lock(HwDecoderCtx ctx) = 0;
        virtual void unlock(HwDecoderCtx ctx) = 0;
};
