/*
 * EchoSuppress.cpp
 *
 *  Created on: 2011-05-18
 *      Author: asavard
 */

#include <cassert>
#include <stdexcept>
#include "logger.h"
#include "echosuppress.h"
#include "pjmedia/echo.h"
#include "pj/pool.h"
#include "pj/os.h"

#define SAMPLES_PER_FRAME 160

EchoSuppress::EchoSuppress(pj_pool_t *pool) : echoState_(0)
{
    if (pjmedia_echo_create(pool, 8000, SAMPLES_PER_FRAME, 250, 0, PJMEDIA_ECHO_SIMPLE | PJMEDIA_ECHO_NO_LOCK, &echoState_) != PJ_SUCCESS)
        throw std::runtime_error("EchoCancel: Could not create echo canceller");
}

EchoSuppress::~EchoSuppress()
{
    pjmedia_echo_destroy(echoState_);
}

void EchoSuppress::putData(SFLDataFormat *inputData, int samples)
{
    assert(samples == SAMPLES_PER_FRAME);
    assert(sizeof(SFLDataFormat) == sizeof(pj_int16_t));

    if (pjmedia_echo_playback(echoState_, reinterpret_cast<pj_int16_t *>(inputData)) != PJ_SUCCESS)
        WARN("EchoCancel: Problem while putting input data");
}

void EchoSuppress::getData(SFLDataFormat *outputData)
{
    assert(sizeof(SFLDataFormat) == sizeof(pj_int16_t));

    if (pjmedia_echo_capture(echoState_, reinterpret_cast<pj_int16_t *>(outputData), 0) != PJ_SUCCESS)
        WARN("EchoCancel: Problem while getting output data");
}
