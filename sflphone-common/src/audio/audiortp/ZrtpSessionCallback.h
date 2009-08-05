/*
 *  Copyright (C) 2004-2009 Savoir-Faire Linux inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
#ifndef __ZRTP_CALLBACK_H__
#define __ZRTP_CALLBACK_H__

#include <libzrtpcpp/zrtpccrtp.h>
#include <libzrtpcpp/ZrtpQueue.h>
#include <libzrtpcpp/ZrtpUserCallback.h>

#include "../../sipcall.h"

namespace sfl {
    class ZrtpSessionCallback: public ZrtpUserCallback {
    public:
        ZrtpSessionCallback(SIPCall *sipcall);

        void secureOn(std::string cipher);
        void secureOff(void);
        void showSAS(std::string sas, bool verified);
        void zrtpNotSuppOther(void);
        void showMessage(GnuZrtpCodes::MessageSeverity sev, int32_t subCode); 
        void zrtpNegotiationFailed(GnuZrtpCodes::MessageSeverity severity, int subCode);
        void confirmGoClear();
    private:
        SIPCall* _sipcall;
        static std::map<int32, std::string*> _infoMap;
        static std::map<int32, std::string*> _warningMap;
        static std::map<int32, std::string*> _severeMap;
        static std::map<int32, std::string*> _zrtpMap;
        static bool _mapInitialized;
    };
}
#endif
