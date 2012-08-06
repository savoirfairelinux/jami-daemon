/*
 decoder.h

 Copyright (C) 2011 Belledonne Communications, Grenoble, France
 Author : Johan Pascal

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef DECODER_H
#define DECODER_H
typedef struct bcg729DecoderChannelContextStruct_struct bcg729DecoderChannelContextStruct;
#include <stdint.h>

/*****************************************************************************/
/* initBcg729DecoderChannel : create context structure and initialise it     */
/*    return value :                                                         */
/*      - the decoder channel context data                                   */
/*                                                                           */
/*****************************************************************************/
__attribute__ ((visibility ("default"))) bcg729DecoderChannelContextStruct *initBcg729DecoderChannel();

/*****************************************************************************/
/* closeBcg729DecoderChannel : free memory of context structure              */
/*    parameters:                                                            */
/*      -(i) decoderChannelContext : the channel context data                */
/*                                                                           */
/*****************************************************************************/
__attribute__ ((visibility ("default"))) void closeBcg729DecoderChannel(bcg729DecoderChannelContextStruct *decoderChannelContext);

/*****************************************************************************/
/* bcg729Decoder :                                                           */
/*    parameters:                                                            */
/*      -(i) decoderChannelContext : the channel context data                */
/*      -(i) bitStream : 15 parameters on 80 bits                            */
/*      -(i) frameErased: flag: true, frame has been erased                  */
/*      -(o) signal : a decoded frame 80 samples (16 bits PCM)               */
/*                                                                           */
/*****************************************************************************/
__attribute__ ((visibility ("default"))) void bcg729Decoder(bcg729DecoderChannelContextStruct *decoderChannelContext, uint8_t bitStream[], uint8_t frameErasureFlag, int16_t signal[]);
#endif /* ifndef DECODER_H */
