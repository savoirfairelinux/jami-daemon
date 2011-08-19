/*
 * EchoSuppress.h
 *
 *  Created on: 2011-05-18
 *      Author: asavard
 */

#ifndef ECHOSUPPRESS_H_
#define ECHOSUPPRESS_H_

#include "pjmedia/echo.h"
#include "pj/pool.h"

#include "audioprocessing.h"
#include "audio/algorithm.h"

class EchoSuppress : public Algorithm {
    public:
        EchoSuppress(pj_pool_t *pool);

        virtual ~EchoSuppress();

        virtual void reset (void);

        /**
         * Add speaker data into internal buffer
         * \param inputData containing far-end voice data to be sent to speakers
         */
        virtual void putData (SFLDataFormat *, int);

        virtual int getData(SFLDataFormat *);

        /**
         * Unused
         */
        virtual void process (SFLDataFormat *, int);

        /**
         * Perform echo cancellation using internal buffers
         * \param inputData containing mixed echo and voice data
         * \param outputData containing
         */
        virtual int process (SFLDataFormat *, SFLDataFormat *, int);
    private:

        /**
         * Memory pool for echo cancellation
         */
        pj_pool_t *echoCancelPool;

        /**
         * The internal state of the echo canceller
         */
        pjmedia_echo_state *echoState;
};

#endif /* ECHOSUPPRESS_H_ */
