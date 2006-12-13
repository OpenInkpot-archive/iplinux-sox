/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "gsm.h"
#include "private.h"

gsm gsm_create ()
{
	gsm = (gsm)calloc(sizeof(struct gsm_state));

        if (r)
          r->nrp = 40;

	return r;
}
