/*
 * August 7, 2000
 *
 * Copyright (C) 2000 Chris Bagwell (cbagwell@sprynet.com)
 *
 */

/*
 * NIST Sphere file format handler.
 */

#include <math.h>
#include <string.h>
#include <errno.h>
#include "st.h"

/* Private data for SKEL file */
typedef struct spherestuff {
	char	shorten_check[4];
	ULONG   numSamples;
} *sphere_t;

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *	Find out sampling rate, 
 *	size and encoding of samples, 
 *	mono/stereo/quad.
 */
int st_spherestartread(ft) 
ft_t ft;
{
	sphere_t sphere = (sphere_t) ft->priv;
	int rc;
	char buf[256];
	char fldname[64], fldtype[16], fldsval[128];
	int header_size, bytes_read;
	
	/* Needed for rawread() */
	rc = st_rawstartread(ft);
	if (rc)
	    return rc;

	/* Magic header */
	if (st_reads(ft, buf, 8) == ST_EOF || strncmp(buf, "NIST_1A", 7) != 0)
	{
	    st_fail_errno(ft,ST_EHDR,"Sphere header does not begin with magic mord 'NIST_1A'");
	    return(ST_EOF);
	}

	if (st_reads(ft, fldsval, 8) == ST_EOF)
	{
	    st_fail_errno(ft,ST_EHDR,"Error reading Sphere header %s",fldsval);
	    return(ST_EOF);
	}

	sscanf(fldsval, "%d", &header_size);

	/* Skip what we have read so far */
	header_size -= 16;

	if (st_reads(ft, buf, 255) == ST_EOF)
	{
	    st_fail_errno(ft,ST_EHDR,"Error reading Sphere header");
	    return(ST_EOF);
	}

	header_size -= (strlen(buf) + 1);

	while (strncmp(buf, "end_head", 8) != 0)
	{
	    if (strncmp(buf, "sample_n_bytes", 14) == 0 && ft->info.size == -1)
	    {
		sscanf(buf, "%s %s %d", fldname, fldtype, &ft->info.size);
	    }
	    if (strncmp(buf, "channel_count", 13) == 0 && 
		ft->info.channels == -1)
	    {
		sscanf(buf, "%s %s %d", fldname, fldtype, &ft->info.channels);
	    }
	    if (strncmp(buf, "sample_coding", 13) == 0)
	    {
		sscanf(buf, "%s %s %s", fldname, fldtype, fldsval);
		/* Only bother looking for ulaw flag.  All others
		 * should be caught below by default PCM check
		 */
		if (ft->info.encoding == -1 && 
		    strncmp(fldsval,"ulaw",4) == 0)
		{
		    ft->info.encoding = ST_ENCODING_ULAW;
		}
	    }
	    if (strncmp(buf, "sample_rate ", 12) == 0 &&
		ft->info.rate == 0)
	    {
#ifdef __alpha__
		sscanf(buf, "%s %s %d", fldname, fldtype, &ft->info.rate);
#else
		sscanf(buf, "%s %s %ld", fldname, fldtype, &ft->info.rate);
#endif
	    }
	    if (strncmp(buf, "sample_byte_format", 18) == 0)
	    {
		sscanf(buf, "%s %s %s", fldname, fldtype, fldsval);
		if (strncmp(fldsval,"01",2) == 0)
		{
		    /* Data is in little endian. */
		    if (ST_IS_BIGENDIAN)
		    {
			ft->swap = ft->swap ? 0 : 1;
		    }
		}
		else if (strncmp(fldsval,"10",2) == 0)
		{
		    /* Data is in big endian. */
		    if (ST_IS_LITTLEENDIAN)
		    {
			ft->swap = ft->swap ? 0 : 1;
		    }
		}
	    }

	    if (st_reads(ft, buf, 255) == ST_EOF)
	    {
	        st_fail_errno(ft,ST_EHDR,"Error reading Sphere header");
	        return(ST_EOF);
	    }

	    header_size -= (strlen(buf) + 1);
	}

	if (ft->info.size == -1)
	    ft->info.size = ST_SIZE_BYTE;

	/* sample_coding is optional and is PCM if missing.
	 * This means encoding is signed if size = word or
	 * unsigned if size = byte.
	 */
	if (ft->info.encoding == -1)
	{
	    if (ft->info.size == 1)
		ft->info.encoding = ST_ENCODING_UNSIGNED;
	    else
		ft->info.encoding = ST_ENCODING_SIGN2;
	}

	while (header_size)
	{
	    bytes_read = st_read(ft, buf, ST_SIZE_BYTE, (header_size > 256) ? 256 : header_size);
	    if (bytes_read == 0)
	    {
		return(ST_EOF);
	    }
	    header_size -= bytes_read;
	}

	sphere->shorten_check[0] = 0;

	/* TODO: Check first four bytes of data to see if its shorten
	 * compressed or not.  This data will need to be written to
	 * buffer during first st_sphereread().
	 */
#if 0
	st_reads(ft, sphere->shorten_check, 4);

	if (!strcmp(sphere->shorten_check,"ajkg"))
	{
	    st_fail_errno(ft,ST_EFMT,"File uses shorten compression, can not handle this.\n");
	    return(ST_EOF);
	}
#endif

	return (ST_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

LONG st_sphereread(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
    sphere_t sphere = (sphere_t) ft->priv;

    if (sphere->shorten_check[0])
    {
	/* TODO: put these 4 bytes into the buffer.  Requires
	 * knowing how to process ulaw and all version of PCM data size.
	 */
	sphere->shorten_check[0] = 0;
    }
    return st_rawread(ft, buf, len);
}

int st_spherestartwrite(ft) 
ft_t ft;
{
    int rc;
    int x;
    sphere_t sphere = (sphere_t) ft->priv;

    if (!ft->seekable)
    {
	st_fail_errno(ft,ST_EOF,"File must be seekable for sphere file output");
	return (ST_EOF);
    }

    switch (ft->info.encoding)
    {
	case ST_ENCODING_ULAW:
	case ST_ENCODING_SIGN2:
	case ST_ENCODING_UNSIGNED:
	    break;
	default:
	    st_fail_errno(ft,ST_EFMT,"SPHERE format only supports ulaw and PCM data.");
	    return(ST_EOF);
    }

    sphere->numSamples = 0;

    /* Needed for rawwrite */
    rc = st_rawstartwrite(ft);
    if (rc)
	return rc;

    for (x = 0; x < 1024; x++)
    {
	st_writeb(ft, ' ');
    }

    return(ST_SUCCESS);
	
}

LONG st_spherewrite(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
    sphere_t sphere = (sphere_t) ft->priv;

    sphere->numSamples += len; /* must later be divided by channels */
    return st_rawwrite(ft, buf, len);
}

int st_spherestopwrite(ft) 
ft_t ft;
{
    int rc;
    char buf[128];
    sphere_t sphere = (sphere_t) ft->priv;

    rc = st_rawstopwrite(ft);
    if (rc)
	return rc;

    if (fseek(ft->fp, 0L, 0) != 0)
    {
	st_fail_errno(ft,errno,"Could not rewird output file to rewrite sphere header.\n");
	return (ST_EOF);
    }

    st_writes(ft, "NIST_1A\n");
    st_writes(ft, "   1024\n");

#ifdef __alpha__
    sprintf(buf, "sample_count -i %d\n", sphere->numSamples/ft->info.channels);
#else
    sprintf(buf, "sample_count -i %ld\n", sphere->numSamples/ft->info.channels);
#endif
    st_writes(ft, buf);

    sprintf(buf, "sample_n_bytes -i %d\n", ft->info.size);
    st_writes(ft, buf);

    sprintf(buf, "channel_count -i %d\n", ft->info.channels);
    st_writes(ft, buf);

    if (ft->swap)
    {
	sprintf(buf, "sample_byte_format -s2 %s\n", ST_IS_BIGENDIAN ? "01" : "10");
    }
    else
    {
	sprintf(buf, "sample_byte_format -s2 %s\n", ST_IS_BIGENDIAN ? "10" : "01");
    }
    st_writes(ft, buf);

#ifdef __alpha__
    sprintf(buf, "sample_rate -i %d\n", ft->info.rate);
#else
    sprintf(buf, "sample_rate -i %ld\n", ft->info.rate);
#endif
    st_writes(ft, buf);

    if (ft->info.encoding == ST_ENCODING_ULAW)
	st_writes(ft, "sample_coding -s4 ulaw\n");
    else
	st_writes(ft, "sample_coding -s3 pcm\n");

    st_writes(ft, "end_head\n");

    return (ST_SUCCESS);
}
