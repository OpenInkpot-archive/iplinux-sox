
/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools masking noise effect file.
 */

#include <stdlib.h>
#include <math.h>
#include "st.h"

#define HALFABIT 1.44			/* square root of 2 */

/*
 * Problems:
 * 	1) doesn't allow specification of noise depth
 *	2) does triangular noise, could do local shaping
 *	3) can run over 32 bits.
 */

/*
 * Process options
 */
int st_mask_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	if (n)
	{
		st_fail("Mask effect takes no options.");
		return (ST_EOF);
	}
	/* should take # of bits */

	st_initrand();
	return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

int st_mask_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
	int len, done;
	
	LONG l;
	LONG tri16;	/* 16 signed bits of triangular noise */

	len = ((*isamp > *osamp) ? *osamp : *isamp);
	switch (effp->outinfo.encoding) {
		case ST_ENCODING_ULAW:
		case ST_ENCODING_ALAW:
			for(done = 0; done < len; done++) {
				tri16 = (rand() + rand()) - 32767;

				l = *ibuf++ + tri16*16*HALFABIT;  /* 2^4.5 */
				*obuf++ = l;
			}
			break;
		default:
		switch (effp->outinfo.size) {
			case ST_SIZE_BYTE:
			for(done = 0; done < len; done++) {
				tri16 = (rand() + rand()) - 32767;

				l = *ibuf++ + tri16*256*HALFABIT;  /* 2^8.5 */
				*obuf++ = l;
			}
			break;
			case ST_SIZE_WORD:
			for(done = 0; done < len; done++) {
				tri16 = (rand() + rand()) - 32767;

				l = *ibuf++ + tri16*HALFABIT;  /* 2^.5 */
				*obuf++ = l;
			}
			break;
			default:
			for(done = 0; done < len; done++) {
				*obuf++ = *ibuf++;
			}
			break;
		}
	}

	*isamp = done;
	*osamp = done;
	return (ST_SUCCESS);
}
