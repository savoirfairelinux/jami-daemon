/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

#ifndef ACCOUNTTEST_H_
#define ACCOUNTTEST_H_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class AccountTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE( AccountTest );
  CPPUNIT_TEST( TestAddRemove );
  CPPUNIT_TEST_SUITE_END();

 public:
  void TestAddRemove(void);
};
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AccountTest, "AccountTest");
CPPUNIT_TEST_SUITE_REGISTRATION( AccountTest );

#endif /* ACCOUNTTEST_H_ */
