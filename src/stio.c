#include "st.h"
#include "st_i.h"
#include "stconfig.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <stdlib.h>

#include <sys/types.h> /* for fstat() */
#include <sys/stat.h> /* for fstat() */
#ifdef _MSC_VER
/*
 * __STDC__ is defined, so these symbols aren't created.
 */
#define S_IFMT   _S_IFMT
#define S_IFREG  _S_IFREG
#define fstat _fstat
#endif

static int is_seekable(ft_t ft)
{
        struct stat st;

        fstat(fileno(ft->fp), &st);

        return ((st.st_mode & S_IFMT) == S_IFREG);
}

/* check that all settings have been given */
static int st_checkformat(ft_t ft)
{

        ft->st_errno = ST_SUCCESS;

        if (ft->info.rate == 0)
        {
                st_fail_errno(ft,ST_EFMT,"sampling rate was not specified");
                return ST_EOF;
        }

        if (ft->info.size == -1)
        {
                st_fail_errno(ft,ST_EFMT,"data size was not specified");
                return ST_EOF;
        }

        if (ft->info.encoding == -1)
        {
                st_fail_errno(ft,ST_EFMT,"data encoding was not specified");
                return ST_EOF;
        }

        if ((ft->info.size <= 0) || (ft->info.size > ST_INFO_SIZE_MAX))
        {
                st_fail_errno(ft,ST_EFMT,"data size %i is invalid");
                return ST_EOF;
        }

        /* anyway to check length on st_encoding_str[] ? */
        if (ft->info.encoding <= 0  || ft->info.encoding > ST_ENCODING_MAX)
        {
                st_fail_errno(ft,ST_EFMT,"data encoding %i is invalid");
                return ST_EOF;
        }

        return ST_SUCCESS;
}

ft_t st_open_input(const char *path, const st_signalinfo_t *si,
                   const char *filetype, const char swap)
{
    ft_t ft;

    ft = (ft_t)calloc(sizeof(struct st_soundstream), 1);

    if (!ft )
        return NULL;

    ft->filename = strdup(path);

    /* Let auto effect do the work if user is not overriding. */
    if (!filetype)
        ft->filetype = strdup("auto");
    else
        ft->filetype = strdup(filetype);

    if (!ft->filename || !ft->filetype)
        goto input_error;

    if (st_gettype(ft) != ST_SUCCESS)
    {
        /* FIXME */
        st_fail("Unknown input file format for '%s':  %s", 
                ft->filename, 
                ft->st_errstr);
        goto input_error;
    }

    ft->info.size = -1;
    ft->info.encoding = -1;
    ft->info.channels = -1;
    ft->info = *si;
    ft->swap = swap;
    ft->mode = 'r';

    if (!(ft->h->flags & ST_FILE_NOSTDIO))
    {
        /* Open file handler based on input name.  Used stdin file handler
         * if the filename is "-"
         */
        if (!strcmp(ft->filename, "-"))
            ft->fp = stdin;
        else if ((ft->fp = fopen(ft->filename, "rb")) == NULL)
        {
            /* FIXME */
            st_fail("Can't open input file '%s': %s", ft->filename,
                    strerror(errno));
            goto input_error;
        }

        /* See if this file is seekable or not */
        ft->seekable = is_seekable(ft);
    }

    /* Read and write starters can change their formats. */
    if ((*ft->h->startread)(ft) != ST_SUCCESS)
    {
        /* FIXME */
        st_fail("Failed reading %s: %s", ft->filename, ft->st_errstr);
        goto input_error;
    }

    /* Go a head and assume 1 channel audio if nothing is detected.
     * This is because libst usually doesn't set this for mono file
     * formats (for historical reasons).
     */
    if (ft->info.channels == -1)
        ft->info.channels = 1;

    if (st_checkformat(ft) )
    {
        st_fail("bad input format for file %s: %s", ft->filename,
                ft->st_errstr);
        goto input_error;
    }

    return ft;

input_error:

    if (ft->filename)
        free(ft->filename);
    if (ft->filetype)
        free(ft->filetype);
    free(ft);
    return NULL;
}

#if defined(DOS) || defined(WIN32)
#define LASTCHAR '\\'
#else
#define LASTCHAR '/'
#endif

static void st_copyformat(ft_t ft, const st_signalinfo_t *info,
                          const char *comment, const st_loopinfo_t *loops,
                          const st_instrinfo_t *instr)
{
    int i;
    double factor;

    if (ft->info.rate == 0)
        ft->info.rate = info->rate;
    if (ft->info.size == -1)
        ft->info.size = info->size;
    if (ft->info.encoding == -1)
        ft->info.encoding = info->encoding;
    if (ft->info.channels == -1)
        ft->info.channels = info->channels;

    if (ft->comment == NULL && comment != NULL)
        ft->comment = strdup(comment);
    else
        ft->comment = strdup("Processed by SoX");

    /*
     * copy loop info, resizing appropriately
     * it's in samples, so # channels don't matter
     */
    factor = (double) ft->info.rate / (double) info->rate;
    for(i = 0; i < ST_MAX_NLOOPS; i++) {
        ft->loops[i].start = loops[i].start * factor;
        ft->loops[i].length = loops[i].length * factor;
        ft->loops[i].count = loops[i].count;
        ft->loops[i].type = loops[i].type;
    }
    /* leave SMPTE # alone since it's absolute */
    ft->instr = *instr;
}

ft_t st_open_output(const char *path, const st_signalinfo_t *info,
                    const st_signalinfo_t *input_info,
                    const char *comment, const st_loopinfo_t *loops,
                    const st_instrinfo_t *instr,
                    const char *filetype, const char swap)
{
    ft_t ft;
    ft = (ft_t)calloc(sizeof(struct st_soundstream), 1);

    if (!ft )
        return NULL;

    ft->filename = strdup(path);

    /* Let auto effect do the work if user is not overriding. */
    if (!filetype)
    {
        char *chop;
        int len;

        len = strlen(ft->filename);

        /* Use filename extension to determine audio type. */
        chop = ft->filename + len;
        while (chop > ft->filename && *chop != LASTCHAR)
            chop--;

        while (chop < ft->filename+len && *chop != '.')
            chop++;

        if (*chop == '.')
        {
            chop++;
            ft->filetype = strdup(chop);
        }
    }
    else
        ft->filetype = strdup(filetype);

    if (!ft->filename || !ft->filetype)
        goto output_error;

    if (st_gettype(ft) != ST_SUCCESS)
    {
        /* FIXME */
        st_fail("Unknown output file format for '%s':  %s", 
                ft->filename, 
                ft->st_errstr);
        goto output_error;
    }

    ft->info.size = -1;
    ft->info.encoding = -1;
    ft->info.channels = -1;
    ft->info = *info;
    ft->swap = swap;
    ft->mode = 'w';

    if (!(ft->h->flags & ST_FILE_NOSTDIO))
    {
        /* Open file handler based on input name.  Used stdin file handler
         * if the filename is "-"
         */
        if (!strcmp(ft->filename, "-"))
        {
            ft->fp = stdout;

        }
        else if ((ft->fp = fopen(ft->filename, "wb")) == NULL)
        {
            /* FIXME */
            st_fail("Can't open output file '%s': %s", ft->filename,
                    strerror(errno));
            goto output_error;
        }

        /* stdout tends to be line-buffered.  Override this */
        /* to be Full Buffering. */
        /* FIXME: Use buffer size from ft structure */
        if (setvbuf (ft->fp, NULL, _IOFBF, sizeof(char)*ST_BUFSIZ))
        {
            /* FIXME */
            st_fail("Can't set write buffer");
            goto output_error;
        }

        /* See if this file is seekable or not */
        ft->seekable = is_seekable(ft);
    }

    st_copyformat(ft, input_info, comment, loops, instr);

    /* Read and write starters can change their formats. */
    if ((*ft->h->startwrite)(ft) != ST_SUCCESS)
    {
        /* FIXME */
        st_fail("Failed writing %s: %s", ft->filename, ft->st_errstr);
        goto output_error;
    }

    if (st_checkformat(ft) )
    {
        st_fail("bad output format for file %s: %s", ft->filename,
                ft->st_errstr);
        goto output_error;
    }

    return ft;

output_error:

    if (ft->filename)
        free(ft->filename);
    if (ft->filetype)
        free(ft->filetype);
    free(ft);
    return NULL;
}

int st_close(ft_t ft)
{
    int rc;

    if (ft->mode == 'r')
        rc = (*ft->h->stopread)(ft);
    else
        rc = (*ft->h->stopwrite)(ft);

    if (!(ft->h->flags & ST_FILE_NOSTDIO))
    {
        fclose(ft->fp);
    }
    if (ft->filename)
        free(ft->filename);
    if (ft->filetype)
        free(ft->filetype);
    /* Currently, since startread() mallocs comments, stopread
     * is expected to also free it.
     */
    if (ft->mode == 'w' && ft->comment)
        free(ft->comment);

    return rc;
}
