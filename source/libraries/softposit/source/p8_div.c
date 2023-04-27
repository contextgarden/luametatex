
/*============================================================================

This C source file is part of the SoftPosit Posit Arithmetic Package
by S. H. Leong (Cerlane).

Copyright 2017, 2018 A*STAR.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions, and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include <stdlib.h>

#include "platform.h"
#include "internals.h"

posit8_t p8_div( posit8_t pA, posit8_t pB ) {
	union ui8_p8 uA, uB, uZ;
	uint_fast8_t uiA, uiB, fracA, fracB, regA, regime, tmp;
	bool signA, signB, signZ, regSA, regSB, bitNPlusOne=0, bitsMore=0, rcarry;
	int_fast8_t kA=0;
	uint_fast16_t frac16A, frac16Z, rem;
	div_t divresult;

	uA.p = pA;
	uiA = uA.ui;
	uB.p = pB;
	uiB = uB.ui;

	//Zero or infinity
	if ( uiA==0x80 || uiB==0x80 || uiB==0){
#ifdef SOFTPOSIT_EXACT
		uZ.ui.v = 0x80;
		uZ.ui.exact = 0;
#else
		uZ.ui = 0x80;
#endif
		return uZ.p;
	}
	else if (uiA==0){
#ifdef SOFTPOSIT_EXACT
		uZ.ui.v = 0;
		if ( (uiA==0 && uiA.ui.exact) || (uiB==0 && uiB.ui.exact) )
			uZ.ui.exact = 1;
		else
			uZ.ui.exact = 0;
#else
		uZ.ui = 0;
#endif
		return uZ.p;
	}

	signA = signP8UI( uiA );
	signB = signP8UI( uiB );
	signZ = signA ^ signB;
	if(signA) uiA = (-uiA & 0xFF);
	if(signB) uiB = (-uiB & 0xFF);
	regSA = signregP8UI(uiA);
	regSB = signregP8UI(uiB);

	tmp = (uiA<<2) & 0xFF;
	if (regSA){
		while (tmp>>7){
			kA++;
			tmp= (tmp<<1) & 0xFF;
		}
	}
	else{
		kA=-1;
		while (!(tmp>>7)){
			kA--;
			tmp= (tmp<<1) & 0xFF;
		}
		tmp&=0x7F;
	}
	fracA = (0x80 | tmp);
	frac16A = fracA<<7; //hidden bit 2nd bit

	tmp = (uiB<<2) & 0xFF;
	if (regSB){
		while (tmp>>7){
			kA--;
			tmp= (tmp<<1) & 0xFF;
		}
		fracB = (0x80 | tmp);
	}
	else{
		kA++;
		while (!(tmp>>7)){
			kA++;
			tmp= (tmp<<1) & 0xFF;
		}
		tmp&=0x7F;
		fracB = (0x80 | (0x7F & tmp));
	}

	divresult = div (frac16A,fracB);
	frac16Z = divresult.quot;
	rem = divresult.rem;

	if (frac16Z!=0){
		rcarry = frac16Z >> 7; // this is the hidden bit (7th bit) , extreme right bit is bit 0
		if (!rcarry){
			kA --;
			frac16Z<<=1;
		}
	}

	if(kA<0){
		regA = (-kA & 0xFF);
		regSA = 0;
		regime = 0x40>>regA;
	}
	else{
		regA = kA+1;
		regSA=1;
		regime = 0x7F-(0x7F>>regA);
	}
	if(regA>6){
		//max or min pos. exp and frac does not matter.
		(regSA) ? (uZ.ui= 0x7F): (uZ.ui=0x1);
	}
	else{
		//remove carry and rcarry bits and shift to correct position
		frac16Z &=0x7F;
		fracA = (uint_fast16_t)frac16Z >> (regA+1);

		bitNPlusOne = (0x1 & (frac16Z>> regA)) ;
		uZ.ui = packToP8UI(regime, fracA);

		//uZ.ui = (uint16_t) (regime) + ((uint16_t) (expA)<< (13-regA)) + ((uint16_t)(fracA));
		if (bitNPlusOne){
			(((1<<regA)-1) & frac16Z) ? (bitsMore=1) : (bitsMore=0);
			if (rem) bitsMore =1;
			 //n+1 frac bit is 1. Need to check if another bit is 1 too if not round to even
			uZ.ui += (uZ.ui&1) | bitsMore;
		}
	}
	if (signZ) uZ.ui = -uZ.ui & 0xFF;

	return uZ.p;
}

