/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "configurationtest.h"
#include "fileutils.h"
#include "config/yamlparser.h"

void ConfigurationTest::testNodeParse()
{
    std::stringstream ss;
    ss << "[{a: 0, b: 1, c: 2}, {a: 0, b: 1, c: 2}]";
    YAML::Parser parser(ss);
    YAML::Node node;
    parser.GetNextDocument(node);

    auto result = yaml_utils::parseVectorMap(node, {"a", "b", "c"});
    CPPUNIT_ASSERT(result[1]["b"] == "1");
}

void ConfigurationTest::test_expand_path(void){
  const std::string pattern_1 = "~";
  const std::string pattern_2 = "~/x";
  const std::string pattern_3 = "~foo/x"; // deliberately broken,
                                          // tilde should not be expanded
  std::string home = fileutils::get_home_dir();

  CPPUNIT_ASSERT(fileutils::expand_path(pattern_1) == home);
  CPPUNIT_ASSERT(fileutils::expand_path(pattern_2) == home.append("/x"));
  CPPUNIT_ASSERT(fileutils::expand_path(pattern_3) == "~foo/x");
}
