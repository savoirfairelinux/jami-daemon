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


namespace ring { namespace test {
    class ImplementThreadloop
    {
        public:
            //ImplementThreadloop();
            ImplementThreadloop();
            ~ImplementThreadloop();
            void start();
            void stop();
            int getMultiThreadVar();
            void reset();
        private:
            void process();
            bool setup();
            ThreadLoop loop_;
            int multithreadVar_=0;
    };

    /*ImplementThreadloop::ImplementThreadloop()
    : loop_([this] { return true; }, [this] { process(); }, [] {})
    {}*/

    ImplementThreadloop::ImplementThreadloop()
    : loop_(std::bind(&ImplementThreadloop::setup, this),
            std::bind(&ImplementThreadloop::process, this),
            []{})
    {}


    void
    ImplementThreadloop::process()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds {500});
        multithreadVar_++;
    }

    bool
    ImplementThreadloop::setup()
    {
        multithreadVar_=1;
        return true;
    }

    void
    ImplementThreadloop::start()
    {
        loop_.start();
    }

    void
    ImplementThreadloop::stop()
    {
        loop_.stop();
    }

    int
    ImplementThreadloop::getMultiThreadVar()
    {
        return multithreadVar_;
    }

    void
    ImplementThreadloop::reset()
    {
        loop_.stop();
        multithreadVar_ = 0;
    }
////////////////////////////////////////////////////////////////////////////////


    class ThreadloopTest : public CppUnit::TestFixture {
        public:
            static std::string name() { return "threadloop"; }

            void setUp();
            void tearDown();
        private:
            void threadloopTest();
            CPPUNIT_TEST_SUITE(ThreadloopTest);
            CPPUNIT_TEST(threadloopTest);
            CPPUNIT_TEST_SUITE_END();
            ImplementThreadloop *implementThreadloop;
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
    ThreadloopTest::threadloopTest()
    {
        implementThreadloop = new ImplementThreadloop();
        implementThreadloop->start();
        std::this_thread::sleep_for(std::chrono::milliseconds {1600});
        implementThreadloop->stop();
        CPPUNIT_ASSERT(implementThreadloop->getMultiThreadVar()==4);

        /*std::string str = "setup";
        implementThreadloop = new ImplementThreadloop(str);
        implementThreadloop->start();
        std::this_thread::sleep_for(std::chrono::milliseconds {1600});
        implementThreadloop->stop();
        CPPUNIT_ASSERT(implementThreadloop->getMultiThreadVar()==4);*/


    }

}} // namespace ring_test

RING_TEST_RUNNER(ring::test::ThreadloopTest::name())
