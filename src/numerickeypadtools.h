/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#ifndef __NUM_KPAD_TOOLS__
#define __NUM_KPAD_TOOLS__
#include <qapplication.h>

#define KEYPAD_ID_1 	0
#define KEYPAD_ID_2 	1
#define KEYPAD_ID_3		2
#define KEYPAD_ID_4 	3
#define KEYPAD_ID_5 	4	
#define KEYPAD_ID_6 	5	
#define KEYPAD_ID_7 	6
#define KEYPAD_ID_8 	7
#define KEYPAD_ID_9 	8
#define KEYPAD_ID_STAR 	9	
#define KEYPAD_ID_0 	10
#define KEYPAD_ID_HASH 	11

#ifdef __cplusplus
extern "C" {
#endif
	
// 26 letters mapped to keypad numbers.
const int keymapTable[] = {
			2, 2, 2,	// ABC
			3, 3, 3,	// DEF
			4, 4, 4,	// GHI
			5, 5, 5,	// JKL
			6, 6, 6,	// MNO
			7, 7, 7, 7,	// PQRS
			8, 8, 8,	// TUV
			9, 9, 9, 9	// WXYZ
		     };
#ifdef __cplusplus
}
#endif

class NumericKeypadTools {
public:
	NumericKeypadTools		();
	~NumericKeypadTools		();
	static int	keyToNumber	(int);

private:
};

#endif 	// __NUM_KPAD_TOOLS__

// EOF
