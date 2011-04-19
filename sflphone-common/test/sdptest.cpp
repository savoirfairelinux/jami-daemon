/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include "sdptest.h"

#include <iostream>


void SDPTest::setUp()
{
	pj_caching_pool_init (&_poolCache, &pj_pool_factory_default_policy, 0);

	_testPool = pj_pool_create (&_poolCache.factory, "sdptest", 4000, 4000, NULL);

	_session = new Sdp(_testPool);
}

void SDPTest::tearDown()
{
	delete _session;
	_session = NULL;

        pj_pool_release(_testPool);
}


void SDPTest::testInitialOffer ()
{
	std::cout << "-------------------- SDPTest::testInitialOffer --------------------" << std::endl;

	CodecOrder codecSelection;

	codecSelection.push_back(PAYLOAD_CODEC_ULAW);
	codecSelection.push_back(PAYLOAD_CODEC_ALAW);
	codecSelection.push_back(PAYLOAD_CODEC_G722);

        _session->createInitialOffer(codecSelection);
}
