/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion>@savoirfairelinux.com>
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
 */

%{

#include <vector>
#include <cstdint>

#include "lib/sip-fmt.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=malloc"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wstrict-overflow"
%}

 /* Definitions */
%option 8bit
%option warn
%option always-interactive
%option pointer
%option reentrant
%option noyywrap
%option noyylineno
%option nounput
%option nodefault
%option noinput
%option debug
%option prefix="sip_yy"
%option extra-type="SIPFmt*"

%x RESPONSE_STATUS
%x RESPONSE_MSG
%x REQUEST_URI
%x REQUEST_VERSION
%x FIELD_SEQUENCE
%x BODY

CRLF                    "\r\n"
SIP_METHOD              REGISTER|INVITE|ACK|CANCEL|BYE|OPTIONS|MESSAGE|INFO
SIP_VERSION             "SIP/2.0"
SIP_STATUS              [0-9]{3}
SIP_MSG                 [^\r\n]*
SIP_URI                 ("sip"|"sips")":"[^\r\n ]+
FIELD_NAME              ([a-zA-Z0-9]|[-_ ])+
FIELD_VALUE             [^\r\n]+

 /* Rules */
%%

 /* Reponse line */
<INITIAL>^{SIP_VERSION} {
        BEGIN(RESPONSE_STATUS);
        yyextra->setAsResponse();
        yyextra->setVersion(yytext);
}

<INITIAL>^{SIP_METHOD} {
        BEGIN(REQUEST_URI);
        yyextra->setAsRequest();
        yyextra->setMethod(yytext);
 }

<REQUEST_URI>{SIP_URI} {
        BEGIN(REQUEST_VERSION);
        yyextra->setURI(yytext);
}

<REQUEST_VERSION>{SIP_VERSION} {
        yyextra->setVersion(yytext);
}

<RESPONSE_STATUS>{SIP_STATUS} {
        BEGIN(RESPONSE_MSG);
        yyextra->setStatus(yytext);
}

<RESPONSE_MSG>{SIP_MSG} {
        yyextra->setMsg(yytext);
}

<REQUEST_VERSION,RESPONSE_MSG>{CRLF} {
        BEGIN(FIELD_SEQUENCE);
}


 /*
  * Field sequence
  *
  * We don't support multi-line field value.
  */
<FIELD_SEQUENCE>{

{FIELD_NAME}":"{FIELD_VALUE}{CRLF} {
        yytext[yyleng - 2] = '\0';
        yyextra->setField(yytext);
        yytext[yyleng - 2] = '\r';
}

{CRLF} {
        BEGIN(BODY);
}

}

<BODY>{

.+|\n {
     yyextra->pushBody(yytext, yyleng);
}

}

 /* Don't care about spaces */
<INITIAL,RESPONSE_MSG,RESPONSE_STATUS,REQUEST_VERSION,REQUEST_URI>[ \t]+  { }

<INITIAL,BODY><<EOF>> { return 0; }
<RESPONSE_STATUS,RESPONSE_MSG,FIELD_SEQUENCE><<EOF>> { return -1; }

 /* Default rule */
<*>.|\n { return -1; }

%%

/* END LEX */

bool
SIPFmt::parse(const std::vector<uint8_t>& blob)
{
        yyscan_t scanner = NULL;
        int err;

        YY_BUFFER_STATE state;

        isValid_ = false;

        if (sip_yylex_init_extra(this, &scanner)) {
                return isValid_;
        }

        // sip_yyset_debug(1, scanner);

        state = sip_yy_scan_bytes((const char*)blob.data(),
                                  blob.size(),
                                  scanner);

        sip_yy_switch_to_buffer(state, scanner);

        err = sip_yylex(scanner);

        sip_yy_delete_buffer(state, scanner);
        sip_yylex_destroy(scanner);

        if (err >= 0) {
                isValid_ = true;
        }

        return isValid_;
}
