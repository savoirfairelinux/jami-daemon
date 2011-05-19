/*
 * EchoSuppress.cpp
 *
 *  Created on: 2011-05-18
 *      Author: asavard
 */

#include "echosuppress.h"
#include "pj/os.h"

#define ECHO_CANCEL_MEM_SIZE 1000

EchoSuppress::EchoSuppress(pj_pool_t *pool)
{
    pj_status_t status;

    _debug("********************************************************************** Init echo suppress");

    // pj_thread_register

    pj_thread_desc aPJThreadDesc;
    pj_thread_t *pjThread;
    _debug("EchoCancel: Thread not yet registered to pjsip, registering");
    status = pj_thread_register("EchoCanceller", aPJThreadDesc, &pjThread);
    if (status != PJ_SUCCESS) {
    	_error("EchoCancel: Error: Could not register new thread");
    }
//    if (!pj_thread_is_registered()) {
//    }

    if (!pj_thread_is_registered()) {
    	_warn("EchoCancel: Thread not registered...");
    }

    // pj_thread_register();

//    status = pj_init();
//    if(status != PJ_SUCCESS) {
//        _error("EchoCancel: Error: could not init pj");
//    }
//
//    pj_caching_pool_init(&echoCachingPool, &pj_pool_factory_default_policy, ECHO_CANCEL_MEM_SIZE);

    // echoCancelPool = pj_pool_create_int(&echoCachingPool.factory, "EchoCancelPool", 1000, 1000, NULL);
    echoCancelPool = pool;

    status = pjmedia_echo_create(echoCancelPool, 8000, 160, 250, 0, PJMEDIA_ECHO_SIMPLE, &echoState);
    if(status != PJ_SUCCESS) {
    	_error("EchoCancel: Error: Could not create echo canceller");
    }
}

EchoSuppress::~EchoSuppress()
{
//	pjmedia_echo_destroy(echoState);

	// pj_pool_destroy(echoCancelPool);

	//pj_caching_pool_destroy(&echoCachingPool);
}

void EchoSuppress::reset()
{

}

void EchoSuppress::putData (SFLDataFormat *inputData, int nbBytes)
{
	pj_status_t status;

	status = pjmedia_echo_playback(echoState, reinterpret_cast<pj_int16_t *>(inputData));

	if(status != PJ_SUCCESS) {
		_warn("EchoCancel: Warning: Problem while putting input data");
	}
}

int EchoSuppress::getData(SFLDataFormat *outputData)
{
	pj_status_t status;

	status = pjmedia_echo_capture(echoState, reinterpret_cast<pj_int16_t *>(outputData), 0);
	if(status != PJ_SUCCESS) {
		_warn("EchoCancel: Warning: Problem while getting output data");
	}

	return 0;
}

void EchoSuppress::process (SFLDataFormat *data UNUSED, int nbBytes UNUSED) {}

int EchoSuppress::process (SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes) { return 0; }

void EchoSuppress::process (SFLDataFormat *micData UNUSED, SFLDataFormat *spkrData UNUSED, SFLDataFormat *outputData UNUSED, int nbBytes UNUSED) {}
