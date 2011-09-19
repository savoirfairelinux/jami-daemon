/*
 * EchoSuppress.cpp
 *
 *  Created on: 2011-05-18
 *      Author: asavard
 */

#include <cassert>
#include <stdexcept>

#include "echosuppress.h"
#include "pj/pool.h"
#include "pj/os.h"

#define ECHO_CANCEL_MEM_SIZE 1000
#define SAMPLES_PER_FRAME 160

EchoSuppress::EchoSuppress(pj_pool_t *pool)
{
    if (pjmedia_echo_create(pool, 8000, SAMPLES_PER_FRAME, 250, 0, PJMEDIA_ECHO_SIMPLE, &echoState) != PJ_SUCCESS)
    	throw std::runtime_error("EchoCancel: Error: Could not create echo canceller");
}

EchoSuppress::~EchoSuppress()
{
	pjmedia_echo_destroy(echoState);
}

void EchoSuppress::putData (SFLDataFormat *inputData, int samples)
{
	assert(samples = SAMPLES_PER_FRAME);
	if (pjmedia_echo_playback(echoState, reinterpret_cast<pj_int16_t *>(inputData)) != PJ_SUCCESS)
        _warn("EchoCancel: Warning: Problem while putting input data");
}

void EchoSuppress::getData(SFLDataFormat *outputData)
{
    if (pjmedia_echo_capture(echoState, reinterpret_cast<pj_int16_t *>(outputData), 0) != PJ_SUCCESS)
        _warn("EchoCancel: Warning: Problem while getting output data");
}
