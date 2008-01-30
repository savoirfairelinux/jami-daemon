/*
 *  Copyright (C) 2004-2005-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author:  Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __GSM_H__
#define __GSM_H__

#include <gsm.h>

#include "audiocodec.h"
/*extern "C" {
#include "gsm/gsm.h"
}*/

/**
 * GSM audio codec C++ class (over gsm/gsm.h)
 */
class Gsm : public AudioCodec {
public:
  // _payload should be 3
  Gsm (int payload=3);
  ~Gsm (void);

  int	codecDecode	(short *, unsigned char *, unsigned int);
  int	codecEncode	(unsigned char *, short *, unsigned int);
  void test();

private:
  gsm _decode_gsmhandle;
  gsm _encode_gsmhandle;
};

#endif // __ULAW_H__
