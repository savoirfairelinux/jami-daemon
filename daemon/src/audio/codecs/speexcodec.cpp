/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
#include "speexcodec.h"
#include "ring_plugin.h"

// cppcheck-suppress unusedFunction
RING_PLUGIN_EXIT(pluginExit) {}

// cppcheck-suppress unusedFunction
RING_PLUGIN_INIT_DYNAMIC(pluginAPI)
{
    std::unique_ptr<Speex> codec_nb(new Speex(110, 8000, 160, 24, true, &speex_nb_mode));
    std::unique_ptr<Speex> codec_wb(new Speex(111, 16000, 320, 42, true, &speex_wb_mode));
    std::unique_ptr<Speex> codec_ub(new Speex(112, 32000, 640, 0, true, &speex_uwb_mode));

    if (!pluginAPI->invokeService(pluginAPI, "registerAudioCodec",
                                  reinterpret_cast<void*>(codec_nb.get()))) {
        codec_nb.release();
    }

    if (!pluginAPI->invokeService(pluginAPI, "registerAudioCodec",
                                  reinterpret_cast<void*>(codec_wb.get()))) {
        codec_wb.release();
    }

    if (!pluginAPI->invokeService(pluginAPI, "registerAudioCodec",
                                  reinterpret_cast<void*>(codec_ub.get()))) {
        codec_ub.release();
    }

    return pluginExit;
}
