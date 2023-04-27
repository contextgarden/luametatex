
/*============================================================================

This C source file is part of the SoftPosit Posit Arithmetic Package
by S. H. Leong (Cerlane).

Copyright 2017 A*STAR.  All rights reserved.

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


#include "platform.h"
#include "internals.h"

posit8_t p8_sub( posit8_t a, posit8_t b ){

    union ui8_p8 uA, uB;
    uint_fast8_t uiA, uiB;
    union ui8_p8 uZ;

    uA.p = a;
	uiA = uA.ui;
	uB.p = b;
	uiB = uB.ui;



#ifdef SOFTPOSIT_EXACT
		uZ.ui.exact = (uiA.ui.exact & uiB.ui.exact);
#endif

    //infinity
	if ( uiA==0x80 || uiB==0x80 ){
#ifdef SOFTPOSIT_EXACT
		uZ.ui.v = 0x80;
		uZ.ui.exact = 0;
#else
		uZ.ui = 0x80;
#endif
		return uZ.p;
	}
    //Zero
	else if ( uiA==0 || uiB==0 ){
#ifdef SOFTPOSIT_EXACT
		uZ.ui.v = (uiA | -uiB);
		uZ.ui.exact = 0;
#else
		uZ.ui = (uiA | -uiB);
#endif
		return uZ.p;
	}

	//different signs
	if (signP8UI(uiA^uiB))
		return softposit_addMagsP8(uiA, (-uiB & 0xFF));
	else
		return softposit_subMagsP8(uiA, (-uiB & 0xFF));




}

