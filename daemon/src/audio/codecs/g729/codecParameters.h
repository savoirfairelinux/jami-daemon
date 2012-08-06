/*
 codecParameters.h

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
#ifndef CODECPARAMETERS_H
#define CODECPARAMETERS_H

#define  L_FRAME      80      /* Frame size.                                */
#define  L_SUBFRAME   40      /* subFrame size.                             */

#define  L_LP_ANALYSIS_WINDOW 240 /* Size of the window used in the LP Analysis */
/******************************************************************************/
/***                         LSP coefficients                               ***/
/******************************************************************************/
/* define each coefficient bit number and range */
#define L0_LENGTH	1
#define L1_LENGTH	7
#define L2_LENGTH	5
#define L3_LENGTH	5
#define L0_RANGE	(1<<L0_LENGTH)
#define L1_RANGE	(1<<L1_LENGTH)
#define L2_RANGE	(1<<L2_LENGTH)
#define L3_RANGE	(1<<L3_LENGTH)

/* MA Prediction history length: maximum number of previous LSP used */
#define MA_MAX_K 4

/* Linear Prediction filters order: 10th order filters gives 10 (quantized) LP coefficients */
/* NB_LSP_COEFF is the number of LSP coefficient */
#define NB_LSP_COEFF 10

/* Maximum value of integer part of pitch delay */
#define MAXIMUM_INT_PITCH_DELAY 143
/* past excitation vector length: Maximum Pitch Delay (143 + 1(fractionnal part)) + Interpolation Windows Length (10) */
#define L_PAST_EXCITATION 154

/* rearrange coefficient gap in Q13 */
/* GAP1 is 0.0012, GAP2 is 0.0006 */
#define GAP1 10
#define GAP2 5

/* qLSF stability in Q13*/
/* Limits for quantized LSF */
/* in Q2.13, Max is 3.135 and Min is 0.005 */
#define qLSF_MIN  40
#define qLSF_MAX  25681
/* min distance between 2 consecutive qLSF is 0.0391 */
#define MIN_qLSF_DISTANCE 321

/* pitch gain boundaries in Q14 */
#define BOUNDED_PITCH_GAIN_MIN 3277
#define BOUNDED_PITCH_GAIN_MAX 13107

/* post filters values defined in 4.2.2 in Q15 pow 1 to 10 */
#define GAMMA_N1 18022
#define GAMMA_N2 9912
#define GAMMA_N3 5452
#define GAMMA_N4 2998
#define GAMMA_N5 1649
#define GAMMA_N6 907
#define GAMMA_N7 499
#define GAMMA_N8 274
#define GAMMA_N9 151
#define GAMMA_N10 83
#define GAMMA_D1 22938
#define GAMMA_D2 16056
#define GAMMA_D3 11239
#define GAMMA_D4 7868
#define GAMMA_D5 5507
#define GAMMA_D6 3855
#define GAMMA_D7 2699
#define GAMMA_D8 1889
#define GAMMA_D9 1322
#define GAMMA_D10 926

/* post filter value GAMMA_T 0.8 in Q15 (spec A.4.2.3)*/
#define GAMMA_T 26214

/* weighted speech for open-loop pitch delay (spec A3.3.3) in Q15 0.75^(1..10)*/
#define GAMMA_E1 24756
#define GAMMA_E2 18432
#define GAMMA_E3 13824
#define GAMMA_E4 10368
#define GAMMA_E5 7776
#define GAMMA_E6 5832
#define GAMMA_E7 4374
#define GAMMA_E8 3280
#define GAMMA_E9 2460
#define GAMMA_E10 1845

/*** Number of parameters in the encoded signal ***/
#define NB_PARAMETERS 15

/*** LP to LSP conversion ***/
#define NB_COMPUTED_VALUES_CHEBYSHEV_POLYNOMIAL 51

#endif /* ifndef CODECPARAMETERS_H */
