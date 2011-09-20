/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#include "iaxcall.h"
#include "iax2/frame.h"
#include "account.h"
#include "manager.h"

namespace {
    int codecToASTFormat(int c)
    {
        static std::map<int, int> mapping;
        if (mapping.empty()) {
            mapping[PAYLOAD_CODEC_ULAW] = AST_FORMAT_ULAW;
            mapping[PAYLOAD_CODEC_GSM] = AST_FORMAT_GSM;
            mapping[PAYLOAD_CODEC_ALAW] = AST_FORMAT_ALAW;
            mapping[PAYLOAD_CODEC_ILBC_20] = AST_FORMAT_ILBC;
            mapping[PAYLOAD_CODEC_SPEEX_8000] = AST_FORMAT_SPEEX;
        }
        if (mapping.find(c) == mapping.end())
        {
            _error("Format not supported!");
            return -1;
        }
        else
            return mapping[c];
    }
    int ASTFormatToCodec(int format)
    {
        static std::map<int, int> mapping;
        if (mapping.empty()) {
            mapping[AST_FORMAT_ULAW] = PAYLOAD_CODEC_ULAW;
            mapping[AST_FORMAT_GSM] = PAYLOAD_CODEC_GSM;
            mapping[AST_FORMAT_ALAW] = PAYLOAD_CODEC_ALAW;
            mapping[AST_FORMAT_ILBC] = PAYLOAD_CODEC_ILBC_20;
            mapping[AST_FORMAT_SPEEX] = PAYLOAD_CODEC_SPEEX_8000;
        }
        if (mapping.find(format) == mapping.end()) {
            _error("Format not supported!");
            return static_cast<int>(-1);
        }
        else
            return mapping[format];
    }
}

IAXCall::IAXCall (const std::string& id, Call::CallType type) : Call (id, type), session (NULL)
{
}

int
IAXCall::getSupportedFormat (const std::string &accountID) const
{
    Account *account = Manager::instance().getAccount (accountID);

    int format = 0;
    if (account) {
        CodecOrder map(account->getActiveCodecs());
        for (CodecOrder::const_iterator iter = map.begin(); iter != map.end(); ++iter)
                format |= codecToASTFormat(*iter);
    }
    else
        _error ("No IAx account could be found");

    return format;
}

int IAXCall::getFirstMatchingFormat (int needles, const std::string &accountID) const
{
    Account *account = Manager::instance().getAccount (accountID);

    if (account != NULL) {
        CodecOrder map(account->getActiveCodecs());
        for (CodecOrder::const_iterator iter = map.begin(); iter != map.end(); ++iter) {
            int format = codecToASTFormat(*iter);
            // Return the first that matches
            if (format & needles)
                return format;
        }
    } else
        _error ("No IAx account could be found");

    return 0;
}

int IAXCall::getAudioCodec(void)
{
    return ASTFormatToCodec(format);
}
