/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/CompilerOutputter.h>

#include "jami.h"

#include <stdexcept>

void init_daemon()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    libjami::start("test/unitTest/jami-sample.yml");
}

int main()
{
    init_daemon();

    CppUnit::TextUi::TestRunner runner;

    // Register all tests
    auto& registry = CppUnit::TestFactoryRegistry::getRegistry();
    runner.addTest(registry.makeTest());

    // Use a compiler error format outputter for results and output into stderr
    runner.setOutputter(new CppUnit::CompilerOutputter(&runner.result(), std::cerr ));

    bool ret;

    try {
        // Run tests
        ret = !runner.run("", false);
    } catch (const std::exception& e) {
        std::cerr << "Exception catched during tests: " << e.what() << '\n';
        ret = 1;
    }

    libjami::fini();

    return ret;
}
