/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
#include "manager.h"
#include "global.h" // for _debug

IAXCall::IAXCall (const CallID& id, Call::CallType type) : Call (id, type), _session (NULL)
{
}

IAXCall::~IAXCall()
{
	_session = NULL; // just to be sure to don't have unknown pointer, do not delete it!
}

	void
IAXCall::setFormat (int format)
{
	_format = format;

	_info ("IAX set supported format: ");

	switch (format) {

		case AST_FORMAT_ULAW:
			_info("PCMU");
			setAudioCodec (PAYLOAD_CODEC_ULAW);
			break;

		case AST_FORMAT_GSM:
			_info ("GSM");
			setAudioCodec (PAYLOAD_CODEC_GSM);
			break;

		case AST_FORMAT_ALAW:
			_info ("ALAW");
			setAudioCodec (PAYLOAD_CODEC_ALAW);
			break;

		case AST_FORMAT_ILBC:
			_info ("ILBC");
			setAudioCodec (PAYLOAD_CODEC_ILBC_20);
			break;

		case AST_FORMAT_SPEEX:
			_info ("SPEEX");
			setAudioCodec (PAYLOAD_CODEC_SPEEX_8000);
			break;

		default:
			_info ("Error audio codec type %i not supported!", format);
			setAudioCodec ( (AudioCodecType) -1);
			break;
	}
}


	int
IAXCall::getSupportedFormat (std::string accountID)
{
	CodecOrder map;
	int format = 0;
	unsigned int iter;
	Account *account;

	_info ("IAX get supported format: ");

	account = Manager::instance().getAccount (accountID);
	if (account != NULL) {
		map = account->getActiveCodecs();
	}
	else {
		_error ("No IAx account could be found");
	}

	for (iter=0 ; iter < map.size() ; iter++) {
		switch (map[iter]) {

			case PAYLOAD_CODEC_ULAW:
				_info ("PCMU ");
				format |= AST_FORMAT_ULAW;
				break;

			case PAYLOAD_CODEC_GSM:
				_info ("GSM ");
				format |= AST_FORMAT_GSM;
				break;

			case PAYLOAD_CODEC_ALAW:
				_info ("PCMA ");
				format |= AST_FORMAT_ALAW;
				break;

			case PAYLOAD_CODEC_ILBC_20:
				_info ("ILBC ");
				format |= AST_FORMAT_ILBC;
				break;

			case PAYLOAD_CODEC_SPEEX_8000:
				_info ("SPEEX ");
				format |= AST_FORMAT_SPEEX;
				break;

			default:
				break;
		}
	}

	return format;

}

int IAXCall::getFirstMatchingFormat (int needles, std::string accountID) {

	Account *account;
	CodecOrder map;
	int format = 0;
	unsigned int iter;

	_debug ("IAX get first matching codec: ");

	account = Manager::instance().getAccount (accountID);
	if (account != NULL) {
		map = account->getActiveCodecs();
	}
	else {
		_error ("No IAx account could be found");
	}

	for (iter=0 ; iter < map.size() ; iter++) {
		switch (map[iter]) {

			case PAYLOAD_CODEC_ULAW:
				_debug ("PCMU");
				format = AST_FORMAT_ULAW;
				break;

			case PAYLOAD_CODEC_GSM:
				_debug ("GSM");
				format = AST_FORMAT_GSM;
				break;

			case PAYLOAD_CODEC_ALAW:
				_debug ("PCMA");
				format = AST_FORMAT_ALAW;
				break;

			case PAYLOAD_CODEC_ILBC_20:
				_debug ("ILBC");
				format = AST_FORMAT_ILBC;
				break;

			case PAYLOAD_CODEC_SPEEX_8000:
				_debug ("SPEEX");
				format = AST_FORMAT_SPEEX;
				break;

			default:
				break;
		}

		// Return the first that matches
		if (format & needles)
			return format;

	}

	return 0;
}

CodecDescriptor& IAXCall::getCodecMap()
{
	return _codecMap;
}

AudioCodecType IAXCall::getAudioCodec()
{
	return _audioCodec;
}



