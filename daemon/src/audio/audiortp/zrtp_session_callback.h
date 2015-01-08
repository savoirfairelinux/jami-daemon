/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#ifndef ZRTP_SESSION_CALLBACK_H_
#define ZRTP_SESSION_CALLBACK_H_
#include <cstddef>

using std::ptrdiff_t;

#include <libzrtpcpp/zrtpccrtp.h>
#include <libzrtpcpp/ZrtpQueue.h>
#include <libzrtpcpp/ZrtpUserCallback.h>
#include <string>
#include <map>

class SIPCall;

namespace ring {

class ZrtpSessionCallback: public ZrtpUserCallback {
    public:
        ZrtpSessionCallback(SIPCall &call);

        void secureOn(std::string cipher);
        void secureOff();
        void showSAS(std::string sas, bool verified);
        void zrtpNotSuppOther();
        void showMessage(GnuZrtpCodes::MessageSeverity sev, int32_t subCode);
        void zrtpNegotiationFailed(GnuZrtpCodes::MessageSeverity severity, int subCode);
        void confirmGoClear();

    private:
        SIPCall &call_;
        static std::map<int32, std::string> infoMap_;
        static std::map<int32, std::string> warningMap_;
        static std::map<int32, std::string> severeMap_;
        static std::map<int32, std::string> zrtpMap_;
};
}
#endif // ZRTP_SESSION_CALLBACK_H_
