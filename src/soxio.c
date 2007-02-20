#include "sox_i.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h> /* for fstat() */
#include <sys/stat.h> /* for fstat() */

#ifdef _MSC_VER
/* __STDC__ is defined, so these symbols aren't created. */
#define S_IFMT   _S_IFMT
#define S_IFREG  _S_IFREG
#define fstat _fstat
#endif

/* Based on zlib's minigzip: */
#if defined(WIN32) || defined(__NT__)
#include <fcntl.h>
#include <io.h>
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif
#define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#define SET_BINARY_MODE(file)
#endif

void set_endianness_if_not_already_set(ft_t ft)
{
  if (ft->signal.reverse_bytes == SOX_OPTION_DEFAULT) {
    if (ft->h->flags & SOX_FILE_ENDIAN)
      ft->signal.reverse_bytes = SOX_IS_LITTLEENDIAN != !(ft->h->flags & SOX_FILE_ENDBIG);
    else
      ft->signal.reverse_bytes = SOX_OPTION_NO;
  }
  if (ft->signal.reverse_nibbles == SOX_OPTION_DEFAULT)
    ft->signal.reverse_nibbles = SOX_OPTION_NO;
  if (ft->signal.reverse_bits == SOX_OPTION_DEFAULT)
    ft->signal.reverse_bits = SOX_OPTION_NO;
}

static int is_seekable(ft_t ft)
{
        struct stat st;

        fstat(fileno(ft->fp), &st);

        return ((st.st_mode & S_IFMT) == S_IFREG);
}

/* check that all settings have been given */
static int sox_checkformat(ft_t ft)
{

        ft->sox_errno = SOX_SUCCESS;

        if (ft->signal.rate == 0)
        {
                sox_fail_errno(ft,SOX_EFMT,"sampling rate was not specified");
                return SOX_EOF;
        }

        if (ft->signal.size == -1)
        {
                sox_fail_errno(ft,SOX_EFMT,"data size was not specified");
                return SOX_EOF;
        }

        if (ft->signal.encoding == SOX_ENCODING_UNKNOWN)
        {
                sox_fail_errno(ft,SOX_EFMT,"data encoding was not specified");
                return SOX_EOF;
        }

        if ((ft->signal.size <= 0) || (ft->signal.size > SOX_INFO_SIZE_MAX))
        {
                sox_fail_errno(ft,SOX_EFMT,"data size %d is invalid", ft->signal.size);
                return SOX_EOF;
        }

        if (ft->signal.encoding <= 0  || ft->signal.encoding >= SOX_ENCODINGS)
        {
                sox_fail_errno(ft,SOX_EFMT,"data encoding %d is invalid", ft->signal.encoding);
                return SOX_EOF;
        }

        return SOX_SUCCESS;
}

ft_t sox_open_read(const char *path, const sox_signalinfo_t *info,
                  const char *filetype)
{
    ft_t ft = (ft_t)xcalloc(sizeof(struct sox_soundstream), 1);

    ft->filename = xstrdup(path);

    /* Let auto type do the work if user is not overriding. */
    if (!filetype)
        ft->filetype = xstrdup("auto");
    else
        ft->filetype = xstrdup(filetype);

    if (sox_gettype(ft, sox_false) != SOX_SUCCESS) {
        sox_warn("Unknown input file format for `%s':  %s",
                ft->filename,
                ft->sox_errstr);
        goto input_error;
    }

    ft->signal.size = -1;
    ft->signal.encoding = SOX_ENCODING_UNKNOWN;
    ft->signal.channels = 0;
    if (info)
        ft->signal = *info;
    ft->mode = 'r';

    if (!(ft->h->flags & SOX_FILE_NOSTDIO))
    {
        /* Open file handler based on input name.  Used stdin file handler
         * if the filename is "-"
         */
        if (!strcmp(ft->filename, "-"))
        {
            SET_BINARY_MODE(stdin);
            ft->fp = stdin;
        }
        else if ((ft->fp = fopen(ft->filename, "rb")) == NULL)
        {
            sox_warn("Can't open input file `%s': %s", ft->filename,
                    strerror(errno));
            goto input_error;
        }

        /* See if this file is seekable or not */
        ft->seekable = is_seekable(ft);
    }

    if (filetype)
      set_endianness_if_not_already_set(ft);

    /* Read and write starters can change their formats. */
    if ((*ft->h->startread)(ft) != SOX_SUCCESS)
    {
        sox_warn("Failed reading `%s': %s", ft->filename, ft->sox_errstr);
        goto input_error;
    }

    /* Go a head and assume 1 channel audio if nothing is detected.
     * This is because libst usually doesn't set this for mono file
     * formats (for historical reasons).
     */
    if (ft->signal.channels == 0)
        ft->signal.channels = 1;

    if (sox_checkformat(ft) )
    {
        sox_warn("bad input format for file %s: %s", ft->filename,
                ft->sox_errstr);
        goto input_error;
    }
    return ft;

input_error:

    free(ft->filename);
    free(ft->filetype);
    free(ft);
    return NULL;
}

#if defined(DOS) || defined(WIN32)
#define LASTCHAR '\\'
#else
#define LASTCHAR '/'
#endif

ft_t sox_open_write(
    sox_bool (*overwrite_permitted)(const char *filename),
    const char *path,
    const sox_signalinfo_t *info,
    const char *filetype,
    const char *comment,
    const sox_instrinfo_t *instr,
    const sox_loopinfo_t *loops)
{
    ft_t ft = (ft_t)xcalloc(sizeof(struct sox_soundstream), 1);
    int i;
    sox_bool no_filetype_given = filetype == NULL;

    ft->filename = xstrdup(path);

    /* Let auto effect do the work if user is not overriding. */
    if (!filetype) {
        char *chop;
        int len;

        len = strlen(ft->filename);

        /* Use filename extension to determine audio type.
         * Search for the last '.' appearing in the filename, same
         * as for input files.
         */
        chop = ft->filename + len;
        while (chop > ft->filename && *chop != LASTCHAR && *chop != '.')
            chop--;

        if (*chop == '.') {
            chop++;
            ft->filetype = xstrdup(chop);
        }
    } else
        ft->filetype = xstrdup(filetype);

    if (!ft->filetype || sox_gettype(ft, no_filetype_given) != SOX_SUCCESS)
    {
        sox_fail("Unknown output file format for '%s':  %s",
                ft->filename,
                ft->sox_errstr);
        goto output_error;
    }

    ft->signal.size = -1;
    ft->signal.encoding = SOX_ENCODING_UNKNOWN;
    ft->signal.channels = 0;
    if (info)
        ft->signal = *info;
    ft->mode = 'w';

    if (!(ft->h->flags & SOX_FILE_NOSTDIO))
    {
        /* Open file handler based on output name.  Used stdout file handler
         * if the filename is "-"
         */
        if (!strcmp(ft->filename, "-"))
        {
            SET_BINARY_MODE(stdout);
            ft->fp = stdout;
        }
        else {
          struct stat st;
          if (!stat(ft->filename, &st) && (st.st_mode & S_IFMT) == S_IFREG &&
              !overwrite_permitted(ft->filename)) {
            sox_fail("Permission to overwrite '%s' denied", ft->filename);
            goto output_error;
          }
          if ((ft->fp = fopen(ft->filename, "wb")) == NULL) {
            sox_fail("Can't open output file '%s': %s", ft->filename,
                    strerror(errno));
            goto output_error;
          }
        }

        /* stdout tends to be line-buffered.  Override this */
        /* to be Full Buffering. */
        if (setvbuf (ft->fp, NULL, _IOFBF, sizeof(char)*SOX_BUFSIZ))
        {
            sox_fail("Can't set write buffer");
            goto output_error;
        }

        /* See if this file is seekable or not */
        ft->seekable = is_seekable(ft);
    }

    ft->comment = xstrdup(comment);

    if (loops)
        for (i = 0; i < SOX_MAX_NLOOPS; i++)
            ft->loops[i] = loops[i];

    /* leave SMPTE # alone since it's absolute */
    if (instr)
        ft->instr = *instr;

    set_endianness_if_not_already_set(ft);

    /* Read and write starters can change their formats. */
    if ((*ft->h->startwrite)(ft) != SOX_SUCCESS)
    {
        sox_fail("Failed writing %s: %s", ft->filename, ft->sox_errstr);
        goto output_error;
    }

    if (sox_checkformat(ft) )
    {
        sox_fail("bad output format for file %s: %s", ft->filename,
                ft->sox_errstr);
        goto output_error;
    }

    return ft;

output_error:

    free(ft->filename);
    free(ft->filetype);
    free(ft);
    return NULL;
}

sox_size_t sox_read(ft_t f, sox_sample_t * buf, sox_size_t len)
{
  sox_size_t actual = (*f->h->read)(f, buf, len);
  return (actual > len? 0 : actual);
}

sox_size_t sox_write(ft_t ft, const sox_sample_t *buf, sox_size_t len)
{
    return (*ft->h->write)(ft, buf, len);
}

/* N.B. The file (if any) may already have been deleted. */
int sox_close(ft_t ft)
{
    int rc;

    if (ft->mode == 'r')
        rc = (*ft->h->stopread)(ft);
    else
        rc = (*ft->h->stopwrite)(ft);

    if (!(ft->h->flags & SOX_FILE_NOSTDIO))
        fclose(ft->fp);
    free(ft->filename);
    free(ft->filetype);
    /* Currently, since startread() mallocs comments, stopread
     * is expected to also free it. */
    if (ft->mode == 'w')
        free(ft->comment);

    return rc;
}

int sox_seek(ft_t ft, sox_size_t offset, int whence)       
{       
    /* FIXME: Implement SOX_SEEK_CUR and SOX_SEEK_END. */         
    if (whence != SOX_SEEK_SET)          
        return SOX_EOF; /* FIXME: return SOX_EINVAL */    

    /* If file is a seekable file and this handler supports seeking,    
     * the invoke handlers function.    
     */         
    if (ft->seekable  && (ft->h->flags & SOX_FILE_SEEK))         
        return (*ft->h->seek)(ft, offset);      
    else        
        return SOX_EOF; /* FIXME: return SOX_EBADF */     
}
