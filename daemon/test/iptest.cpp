/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include <string>
#include "iptest.h"
#include "ip_utils.h"
#include "logger.h"
#include "test_utils.h"

namespace ring { namespace test {

void IpTest::testIpAddr()
{
	TITLE();

	IpAddr ip = {"8.8.8.8"};
	CPPUNIT_ASSERT(ip);
	CPPUNIT_ASSERT(ip.toString() == "8.8.8.8");
	CPPUNIT_ASSERT(ip.getPort() == 0);
	CPPUNIT_ASSERT(ip.isIpv4());
	CPPUNIT_ASSERT(not ip.isIpv6());
	CPPUNIT_ASSERT(not ip.isLoopback());
	CPPUNIT_ASSERT(not ip.isPrivate());
	CPPUNIT_ASSERT(not ip.isUnspecified());

	IpAddr ip_2 = ip.toString();
	CPPUNIT_ASSERT(ip_2 == ip);

	ip = IpAddr();
	CPPUNIT_ASSERT(not ip);
	CPPUNIT_ASSERT(ip.isUnspecified());
	CPPUNIT_ASSERT(not ip.isIpv4());
	CPPUNIT_ASSERT(not ip.isIpv6());
	CPPUNIT_ASSERT(ip.getPort() == 0);

	ip = IpAddr("8.8.8.8:42");
	CPPUNIT_ASSERT(ip);
	CPPUNIT_ASSERT(ip.toString() == "8.8.8.8");
	CPPUNIT_ASSERT(ip.getPort() == 42);

	pj_sockaddr_set_port(ip.pjPtr(), 5042);
	CPPUNIT_ASSERT(ip.getPort() == 5042);

#if HAVE_IPV6
	const in6_addr native_ip = {{
		0x3f, 0xfe, 0x05, 0x01,
		0x00, 0x08, 0x00, 0x00,
		0x02, 0x60, 0x97, 0xff,
		0xfe, 0x40, 0xef, 0xab
	}};
	ip = IpAddr(native_ip);
	CPPUNIT_ASSERT(ip);
	CPPUNIT_ASSERT(ip == IpAddr("3ffe:0501:0008:0000:0260:97ff:fe40:efab"));
	CPPUNIT_ASSERT(not ip.isIpv4());
	CPPUNIT_ASSERT(ip.isIpv6());

	CPPUNIT_ASSERT(IpAddr::isValid("3ffe:0501:0008:0000:0260:97ff:fe40:efab"));
	CPPUNIT_ASSERT(IpAddr::isValid("[3ffe:0501:0008:0000:0260:97ff:fe40:efab]"));
	CPPUNIT_ASSERT(IpAddr::isValid("[3ffe:0501:0008:0000:0260:97ff:fe40:efab]:4242"));
	CPPUNIT_ASSERT(IpAddr::isValid("[3ffe:501:8::260:97ff:fe40:efab]:4242"));
#endif
}

IpTest::IpTest() : CppUnit::TestCase("IP Tests") {}

}} // namespace ring::test
