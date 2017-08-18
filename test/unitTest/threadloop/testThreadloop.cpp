/*
 *  Copyright (C) 2017 Savoir-Faire Linux Inc.
 *  Author: Olivier Gregoire <olivier.gregoire@savoirfairelinux.com>
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
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string>

#include "../../test_runner.h"
#include "threadloop.h"

namespace ring {


    class ThreadloopTest : public CppUnit::TestFixture {
        public:
            static std::string name() { return "threadloop"; }

            void setUp();
            void tearDown();
        private:
            void setupTest();
            CPPUNIT_TEST_SUITE(ThreadloopTest);
            CPPUNIT_TEST(setupTest);
            CPPUNIT_TEST_SUITE_END();
            void process();
            bool setup();
            void cleanup();
            int multithreadVar_=0;
            std::unique_ptr<ring::ThreadLoop> loop_;
            const int PROCESS_TIME_SLEEP_MS = 500;
    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ThreadloopTest, ThreadloopTest::name());

    void
    ThreadloopTest::setUp()
    {

    }

    void
    ThreadloopTest::tearDown()
    {

    }

    void
    ThreadloopTest::process()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds {PROCESS_TIME_SLEEP_MS});
        multithreadVar_++;
    }

    bool
    ThreadloopTest::setup()
    {
        multithreadVar_=1;
        return true;
    }

    void
    ThreadloopTest::cleanup()
    {
        multithreadVar_=-1;
    }

    void
    ThreadloopTest::setupTest()
    {
        loop_.reset(new ThreadLoop(std::bind(&ThreadloopTest::setup, this),
                std::bind(&ThreadloopTest::process, this),
                []{}));

        CPPUNIT_ASSERT(!loop_->isRunning());
        loop_->start();
        CPPUNIT_ASSERT(!loop_->isStopping());
        CPPUNIT_ASSERT(loop_->isRunning());

        //do 3 cycle of process
        std::this_thread::sleep_for(std::chrono::milliseconds {3*PROCESS_TIME_SLEEP_MS+1});
        CPPUNIT_ASSERT(multithreadVar_==4);
    }

} // namespace ring_test

RING_TEST_RUNNER(ring::ThreadloopTest::name())
