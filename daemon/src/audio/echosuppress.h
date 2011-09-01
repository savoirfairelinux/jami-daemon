/*
 * EchoSuppress.h
 *
 *  Created on: 2011-05-18
 *      Author: asavard
 */

#ifndef ECHOSUPPRESS_H_
#define ECHOSUPPRESS_H_

#include "pjmedia/echo.h"
#include "global.h"

class EchoSuppress {
    public:
        EchoSuppress(pj_pool_t *pool);

        ~EchoSuppress();

        /**
         * Add speaker data into internal buffer
         * \param inputData containing far-end voice data to be sent to speakers
         */
        void putData (SFLDataFormat *, int);

        int getData(SFLDataFormat *);

    private:

        /**
         * The internal state of the echo canceller
         */
        pjmedia_echo_state *echoState;
};

#endif /* ECHOSUPPRESS_H_ */
