/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
 *
 *  Author: Ezra Pierce <ezra.pierce@savoirfairelinux.com>
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

#include "jami.h"
#include "videomanager_interface.h"
#include "libav_deps.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "libav_utils.h"

#include "../test_runner.h"

namespace jami {
namespace test {

class RingbufferTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "ringbuffer"; }

    void setUp();
    void tearDown();

private:
    void testCreateRingbuffer();
    void testBindRingbuffers();
    void testUnbindRingbuffers();
    void testBindHalfDuplexRingbuffers();
    void testUnbindHalfDuplexRingbuffers();

    CPPUNIT_TEST_SUITE(RingbufferTest);
    CPPUNIT_TEST(testCreateRingbuffer);
    CPPUNIT_TEST(testBindRingbuffers);
    CPPUNIT_TEST(testUnbindRingbuffers);
    CPPUNIT_TEST(testBindHalfDuplexRingbuffers);
    CPPUNIT_TEST(testUnbindHalfDuplexRingbuffers);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<RingBufferPool> rbPool_;
    const std::string rbuf1Name_ = "rbuf1";
    const std::string rbuf2Name_ = "rbuf2";
    std::shared_ptr<RingBuffer> rbuf1_;
    std::shared_ptr<RingBuffer> rbuf2_;
    std::shared_ptr<RingBuffer> rbuf3_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RingbufferTest, RingbufferTest::name());

void
RingbufferTest::setUp()
{
    rbPool_.reset(new RingBufferPool);
    rbuf1_ = rbPool_->createRingBuffer(rbuf1Name_);
    rbuf2_ = rbPool_->createRingBuffer(rbuf2Name_);
    rbuf3_ = rbPool_->getRingBuffer(RingBufferPool::AUDIO_LAYER_ID);
}

void
RingbufferTest::tearDown()
{}

void
RingbufferTest::testCreateRingbuffer()
{
    CPPUNIT_ASSERT(rbPool_->getRingBuffer(RingBufferPool::AUDIO_LAYER_ID));
    CPPUNIT_ASSERT(rbPool_->getRingBuffer(RingBufferPool::RECORDER_ID) == 0);
    std::shared_ptr<RingBuffer> rbuf = rbPool_->createRingBuffer(RingBufferPool::RECORDER_ID);
    CPPUNIT_ASSERT(rbPool_->getRingBuffer(RingBufferPool::RECORDER_ID));
}

void
RingbufferTest::testBindRingbuffers()
{
    rbPool_->bindRingbuffers(rbuf1Name_, RingBufferPool::AUDIO_LAYER_ID);
    rbPool_->bindHalfDuplexOut(rbuf2Name_, RingBufferPool::AUDIO_LAYER_ID);
    CPPUNIT_ASSERT(rbuf1_->readOffsetCount() == 1);
    CPPUNIT_ASSERT(rbuf2_->readOffsetCount() == 0);
    CPPUNIT_ASSERT(rbuf3_->readOffsetCount() == 2);
    rbPool_->bindRingbuffers(rbuf1Name_, rbuf2Name_);
    CPPUNIT_ASSERT(rbuf1_->readOffsetCount() == 2);
    CPPUNIT_ASSERT(rbuf2_->readOffsetCount() == 1);
    CPPUNIT_ASSERT(rbuf3_->readOffsetCount() == 2);
}

void
RingbufferTest::testUnbindRingbuffers()
{
    rbPool_->bindRingbuffers(rbuf1Name_, RingBufferPool::AUDIO_LAYER_ID);
    rbPool_->unBindRingbuffers(rbuf1Name_, RingBufferPool::AUDIO_LAYER_ID);
    CPPUNIT_ASSERT(rbuf1_->readOffsetCount() == 0);
    CPPUNIT_ASSERT(rbuf3_->readOffsetCount() == 0);
    rbPool_->bindRingbuffers(rbuf1Name_, rbuf2Name_);
    rbPool_->bindRingbuffers(rbuf2Name_, RingBufferPool::AUDIO_LAYER_ID);
    rbPool_->bindRingbuffers(rbuf1Name_, RingBufferPool::AUDIO_LAYER_ID);
    rbPool_->unBindAll(rbuf1Name_);
    CPPUNIT_ASSERT(rbuf1_->readOffsetCount() == 0);
    CPPUNIT_ASSERT(rbuf2_->readOffsetCount() == 1);
    CPPUNIT_ASSERT(rbuf3_->readOffsetCount() == 1);
    rbPool_->unBindRingbuffers(rbuf2Name_, RingBufferPool::AUDIO_LAYER_ID);
    CPPUNIT_ASSERT(rbuf1_->readOffsetCount() == 0);
    CPPUNIT_ASSERT(rbuf2_->readOffsetCount() == 0);
    CPPUNIT_ASSERT(rbuf3_->readOffsetCount() == 0);
}

void
RingbufferTest::testBindHalfDuplexRingbuffers()
{
    rbPool_->bindHalfDuplexOut(rbuf1Name_, RingBufferPool::AUDIO_LAYER_ID);
    rbPool_->bindHalfDuplexOut(rbuf2Name_, RingBufferPool::AUDIO_LAYER_ID);
    CPPUNIT_ASSERT(rbuf1_->readOffsetCount() == 0);
    CPPUNIT_ASSERT(rbuf2_->readOffsetCount() == 0);
    CPPUNIT_ASSERT(rbuf3_->readOffsetCount() == 2);
}

void
RingbufferTest::testUnbindHalfDuplexRingbuffers()
{
    rbPool_->bindRingbuffers(rbuf1Name_, rbuf2Name_);
    rbPool_->bindRingbuffers(rbuf2Name_, RingBufferPool::AUDIO_LAYER_ID);
    rbPool_->bindRingbuffers(rbuf1Name_, RingBufferPool::AUDIO_LAYER_ID);
    rbPool_->unBindAllHalfDuplexOut(rbuf1Name_);
    rbPool_->unBindHalfDuplexOut(RingBufferPool::AUDIO_LAYER_ID, rbuf2Name_);
    CPPUNIT_ASSERT(rbuf1_->readOffsetCount() == 0);
    CPPUNIT_ASSERT(rbuf2_->readOffsetCount() == 1);
    CPPUNIT_ASSERT(rbuf3_->readOffsetCount() == 2);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::RingbufferTest::name());
