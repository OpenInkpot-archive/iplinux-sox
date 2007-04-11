/*
 * libSoX Library Public Interface
 *
 * Copyright 1999-2007 Chris Bagwell and SoX Contributors.
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Chris Bagwell And SoX Contributors are not responsible for
 * the consequences of using this software.
 */

#ifndef SOX_H
#define SOX_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "soxstdint.h"

/* The following is the API version of libSoX.  It is not meant
 * to follow the version number of SoX but it has historically.
 * Please do not count of these numbers being in sync.
 * The following is at 13.0.0
 */
#define SOX_LIB_VERSION_CODE 0x0d0000
#define SOX_LIB_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

/* Avoid warnings about unused parameters. */
#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#endif

/* Boolean type, assignment (but not necessarily binary) compatible with
 * C++ bool */
typedef enum {sox_false, sox_true} sox_bool;

typedef int32_t int24_t;     /* But beware of the extra byte. */
typedef uint32_t uint24_t;   /* ditto */

#define SOX_INT_MIN(bits) (1 <<((bits)-1))
#define SOX_INT_MAX(bits) (-1U>>(33-(bits)))
#define SOX_UINT_MAX(bits) (SOX_INT_MIN(bits)|SOX_INT_MAX(bits))

#define SOX_INT8_MAX  SOX_INT_MAX(8)
#define SOX_INT16_MAX SOX_INT_MAX(16)
#define SOX_INT24_MAX SOX_INT_MAX(24)
#define SOX_INT32_MAX SOX_INT_MAX(32)
#define SOX_INT64_MAX 0x7fffffffffffffffLL /* Not in use yet */

typedef int32_t sox_sample_t;
typedef uint32_t sox_usample_t; /* FIXME: this naming is different from
                                  other types */
/* Minimum and maximum values a sample can hold. */
#define SOX_SAMPLE_MAX (sox_sample_t)SOX_INT_MAX(32)
#define SOX_SAMPLE_MIN (sox_sample_t)SOX_INT_MIN(32)



/*                Conversions: Linear PCM <--> sox_sample_t
 *
 *   I/O       I/O     sox_sample_t  Clips?    I/O     sox_sample_t  Clips? 
 *  Format   Minimum     Minimum     I O    Maximum     Maximum     I O      
 *  ------  ---------  ------------ -- --   --------  ------------ -- --  
 *  Float      -1     -1.00000000047 y y       1           1        y n         
 *  Byte      -128        -128       n n      127     127.9999999   n y   
 *  Word     -32768      -32768      n n     32767    32767.99998   n y   
 *  24bit   -8388608    -8388608     n n    8388607   8388607.996   n y   
 *  Dword  -2147483648 -2147483648   n n   2147483647 2147483647    n n   
 *
 * Conversions are as accurate as possible (with rounding).
 *
 * Rounding: halves toward +inf, all others to nearest integer.
 *
 * Clips? shows whether on not there is the possibility of a conversion
 * clipping to the minimum or maximum value when inputing from or outputing 
 * to a given type.
 *
 * Unsigned integers are converted to and from signed integers by flipping
 * the upper-most bit then treating them as signed integers.
 */

/* Temporary variables to prevent multiple evaluation of macro arguments: */
static sox_sample_t sox_macro_temp_sample UNUSED;
static double sox_macro_temp_double UNUSED;

#define SOX_SAMPLE_NEG SOX_INT_MIN(32)
#define SOX_SAMPLE_TO_UNSIGNED(bits,d,clips) \
  (uint##bits##_t)( \
    sox_macro_temp_sample=(d), \
    sox_macro_temp_sample>(sox_sample_t)(SOX_SAMPLE_MAX-(1U<<(31-bits)))? \
      ++(clips),SOX_UINT_MAX(bits): \
      ((uint32_t)(sox_macro_temp_sample^SOX_SAMPLE_NEG)+(1U<<(31-bits)))>>(32-bits))
#define SOX_SAMPLE_TO_SIGNED(bits,d,clips) \
  (int##bits##_t)(SOX_SAMPLE_TO_UNSIGNED(bits,d,clips)^SOX_INT_MIN(bits))
#define SOX_SIGNED_TO_SAMPLE(bits,d)((sox_sample_t)(d)<<(32-bits))
#define SOX_UNSIGNED_TO_SAMPLE(bits,d)(SOX_SIGNED_TO_SAMPLE(bits,d)^SOX_SAMPLE_NEG)

#define SOX_UNSIGNED_BYTE_TO_SAMPLE(d,clips) SOX_UNSIGNED_TO_SAMPLE(8,d)
#define SOX_SIGNED_BYTE_TO_SAMPLE(d,clips) SOX_SIGNED_TO_SAMPLE(8,d)
#define SOX_UNSIGNED_WORD_TO_SAMPLE(d,clips) SOX_UNSIGNED_TO_SAMPLE(16,d)
#define SOX_SIGNED_WORD_TO_SAMPLE(d,clips) SOX_SIGNED_TO_SAMPLE(16,d)
#define SOX_UNSIGNED_24BIT_TO_SAMPLE(d,clips) SOX_UNSIGNED_TO_SAMPLE(24,d)
#define SOX_SIGNED_24BIT_TO_SAMPLE(d,clips) SOX_SIGNED_TO_SAMPLE(24,d)
#define SOX_UNSIGNED_DWORD_TO_SAMPLE(d,clips) (sox_sample_t)((d)^SOX_SAMPLE_NEG)
#define SOX_SIGNED_DWORD_TO_SAMPLE(d,clips) (sox_sample_t)(d)
#define SOX_FLOAT_DWORD_TO_SAMPLE SOX_FLOAT_DDWORD_TO_SAMPLE
#define SOX_FLOAT_DDWORD_TO_SAMPLE(d,clips) (sox_macro_temp_double=(d),sox_macro_temp_double<-1?++(clips),(-SOX_SAMPLE_MAX):sox_macro_temp_double>1?++(clips),SOX_SAMPLE_MAX:(sox_sample_t)((uint32_t)((double)(sox_macro_temp_double)*SOX_SAMPLE_MAX+(SOX_SAMPLE_MAX+.5))-SOX_SAMPLE_MAX))
#define SOX_SAMPLE_TO_UNSIGNED_BYTE(d,clips) SOX_SAMPLE_TO_UNSIGNED(8,d,clips)
#define SOX_SAMPLE_TO_SIGNED_BYTE(d,clips) SOX_SAMPLE_TO_SIGNED(8,d,clips)
#define SOX_SAMPLE_TO_UNSIGNED_WORD(d,clips) SOX_SAMPLE_TO_UNSIGNED(16,d,clips)
#define SOX_SAMPLE_TO_SIGNED_WORD(d,clips) SOX_SAMPLE_TO_SIGNED(16,d,clips)
#define SOX_SAMPLE_TO_UNSIGNED_24BIT(d,clips) SOX_SAMPLE_TO_UNSIGNED(24,d,clips)
#define SOX_SAMPLE_TO_SIGNED_24BIT(d,clips) SOX_SAMPLE_TO_SIGNED(24,d,clips)
#define SOX_SAMPLE_TO_UNSIGNED_DWORD(d,clips) (uint32_t)((d)^SOX_SAMPLE_NEG)
#define SOX_SAMPLE_TO_SIGNED_DWORD(d,clips) (int32_t)(d)
#define SOX_SAMPLE_TO_FLOAT_DWORD SOX_SAMPLE_TO_FLOAT_DDWORD
#define SOX_SAMPLE_TO_FLOAT_DDWORD(d,clips) (sox_macro_temp_sample=(d),sox_macro_temp_sample==SOX_SAMPLE_MIN?++(clips),-1.0:((double)(sox_macro_temp_sample)*(1.0/SOX_SAMPLE_MAX)))



/* MACRO to clip a data type that is greater then sox_sample_t to
 * sox_sample_t's limits and increment a counter if clipping occurs..
 */
#define SOX_SAMPLE_CLIP_COUNT(samp, clips) \
  do { \
    if (samp > SOX_SAMPLE_MAX) \
      { samp = SOX_SAMPLE_MAX; clips++; } \
    else if (samp < SOX_SAMPLE_MIN) \
      { samp = SOX_SAMPLE_MIN; clips++; } \
  } while (0)

/* Rvalue MACRO to round and clip a double to a sox_sample_t,
 * and increment a counter if clipping occurs.
 */
#define SOX_ROUND_CLIP_COUNT(d, clips) \
  ((d) < 0? (d) <= SOX_SAMPLE_MIN - 0.5? ++(clips), SOX_SAMPLE_MIN: (d) - 0.5 \
        : (d) >= SOX_SAMPLE_MAX + 0.5? ++(clips), SOX_SAMPLE_MAX: (d) + 0.5)

/* Rvalue MACRO to clip a sox_sample_t to 24 bits,
 * and increment a counter if clipping occurs.
 */
#define SOX_24BIT_CLIP_COUNT(l, clips) \
  ((l) >= ((sox_sample_t)1 << 23)? ++(clips), ((sox_sample_t)1 << 23) - 1 : \
   (l) <=-((sox_sample_t)1 << 23)? ++(clips),-((sox_sample_t)1 << 23) + 1 : (l))



typedef uint32_t sox_size_t;
/* Maximum value size type can hold. (Minimum is 0). */
#define SOX_SIZE_MAX 0xffffffff
#define SOX_SAMPLE_BITS (sizeof(sox_size_t) * CHAR_BIT)

typedef int32_t sox_ssize_t;
/* Minimum and maximum value signed size type can hold. */
#define SOX_SSIZE_MAX 0x7fffffff
#define SOX_SSIZE_MIN (-SOX_SSIZE_MAX - 1)

typedef unsigned sox_rate_t;
/* Warning, this is a MAX value used in the library.  Each format and
 * effect may have its own limitations of rate.
 */
#define SOX_MAXRATE      (50U * 1024) /* maximum sample rate in library */

typedef enum {
  SOX_ENCODING_UNKNOWN   ,

  SOX_ENCODING_ULAW      , /* u-law signed logs: US telephony, SPARC */
  SOX_ENCODING_ALAW      , /* A-law signed logs: non-US telephony */
  SOX_ENCODING_ADPCM     , /* G72x Compressed PCM */
  SOX_ENCODING_MS_ADPCM  , /* Microsoft Compressed PCM */
  SOX_ENCODING_IMA_ADPCM , /* IMA Compressed PCM */
  SOX_ENCODING_OKI_ADPCM , /* Dialogic/OKI Compressed PCM */

  SOX_ENCODING_SIZE_IS_WORD, /* FIXME: marks raw types (above) that mis-report size. sox_signalinfo_t really needs a precision_in_bits item */

  SOX_ENCODING_UNSIGNED  , /* unsigned linear: Sound Blaster */
  SOX_ENCODING_SIGN2     , /* signed linear 2's comp: Mac */
  SOX_ENCODING_FLOAT     , /* 32-bit float */
  SOX_ENCODING_GSM       , /* GSM 6.10 33byte frame lossy compression */
  SOX_ENCODING_MP3       , /* MP3 compression */
  SOX_ENCODING_VORBIS    , /* Vorbis compression */
  SOX_ENCODING_FLAC      , /* FLAC compression */
  SOX_ENCODING_AMR_WB    , /* AMR-WB compression */

  SOX_ENCODINGS            /* End of list marker */
} sox_encoding_t;

/* Global parameters */

typedef struct  sox_globalinfo
{
    sox_bool octave_plot_effect;/* To help user choose effect & options */
    double speed;         /* Gather up all speed changes here, then resample */
} sox_globalinfo_t;

typedef enum {SOX_OPTION_NO, SOX_OPTION_YES, SOX_OPTION_DEFAULT} sox_option_t;

/* Signal parameters */

typedef struct sox_signalinfo
{
    sox_rate_t rate;       /* sampling rate */
    int size;             /* compressed or uncompressed datum size */
    sox_encoding_t encoding; /* format of sample numbers */
    unsigned channels;    /* number of sound channels */
    double compression;   /* compression factor (where applicable) */

    /* There is a delineation between these vars being tri-state and
     * effectively boolean.  Logically the line falls between setting
     * them up (could be done in libSoX, or by the libSoX client) and
     * using them (in libSoX).  libSoX's logic to set them up includes
     * knowledge of the machine default and the format default.  (The
     * sox client logic adds to this a layer of overridability via user
     * options.)  The physical delineation is in the somewhat
     * snappily-named libSoX function `set_endianness_if_not_already_set'
     * which is called at the right times (as files are openned) by the
     * libSoX core, not by the file drivers themselves.  The file drivers
     * indicate to the libSoX core if they have a preference using
     * SOX_FILE_xxx flags.
     */
    sox_option_t reverse_bytes;    /* endiannesses... */
    sox_option_t reverse_nibbles;
    sox_option_t reverse_bits;
} sox_signalinfo_t;

/* Loop parameters */

typedef struct  sox_loopinfo
{
    sox_size_t    start;          /* first sample */
    sox_size_t    length;         /* length */
    unsigned int count;          /* number of repeats, 0=forever */
    signed char  type;           /* 0=no, 1=forward, 2=forward/back */
} sox_loopinfo_t;

/* Instrument parameters */

/* vague attempt at generic information for sampler-specific info */

typedef struct  sox_instrinfo
{
    char MIDInote;       /* for unity pitch playback */
    char MIDIlow, MIDIhi;/* MIDI pitch-bend range */
    char loopmode;       /* semantics of loop data */
    signed char nloops;  /* number of active loops (max SOX_MAX_NLOOPS) */
} sox_instrinfo_t;

/* Loop modes, upper 4 bits mask the loop blass, lower 4 bits describe */
/* the loop behaviour, ie. single shot, bidirectional etc. */
#define SOX_LOOP_NONE          0
#define SOX_LOOP_8             32 /* 8 loops: don't know ?? */
#define SOX_LOOP_SUSTAIN_DECAY 64 /* AIFF style: one sustain & one decay loop */

/*
 * File buffer info.  Holds info so that data can be read in blocks.
 */

typedef struct sox_fileinfo
{
    char          *buf;                 /* Pointer to data buffer */
    size_t        size;                 /* Size of buffer */
    size_t        count;                /* Count read in to buffer */
    size_t        pos;                  /* Position in buffer */
} sox_fileinfo_t;


/*
 *  Format information for input and output files.
 */

#define SOX_MAX_FILE_PRIVSIZE    1000
#define SOX_MAX_EFFECT_PRIVSIZE 1000

#define SOX_MAX_NLOOPS           8

/*
 * Handler structure for each format.
 */

typedef struct sox_soundstream *ft_t;

typedef struct sox_format {
    const char   * const *names;
    const char   *usage;
    unsigned int flags;
    int          (*startread)(ft_t ft);
    sox_size_t    (*read)(ft_t ft, sox_sample_t *buf, sox_size_t len);
    int          (*stopread)(ft_t ft);
    int          (*startwrite)(ft_t ft);
    sox_size_t    (*write)(ft_t ft, const sox_sample_t *buf, sox_size_t len);
    int          (*stopwrite)(ft_t ft);
    int          (*seek)(ft_t ft, sox_size_t offset);
} sox_format_t;

struct sox_soundstream {
    sox_signalinfo_t signal;               /* signal specifications */
    sox_instrinfo_t  instr;                /* instrument specification */
    sox_loopinfo_t   loops[SOX_MAX_NLOOPS]; /* Looping specification */
    sox_bool         seekable;             /* can seek on this file */
    char             mode;                 /* read or write mode */
    sox_size_t       length;               /* frames in file, or 0 if unknown. */
    sox_size_t       clips;                /* increment if clipping occurs */
    char             *filename;            /* file name */
    char             *filetype;            /* type of file */
    char             *comment;             /* comment string */
    FILE             *fp;                  /* File stream pointer */
    int              sox_errno;            /* Failure error codes */
    char             sox_errstr[256];      /* Extend Failure text */
    const sox_format_t *h;                 /* format struct for this file */
    /* The following is a portable trick to align this variable on
     * an 8-byte boundery.  Once this is done, the buffer alloced
     * after it should be align on an 8-byte boundery as well.
     * This lets you cast any structure over the private area
     * without concerns of alignment.
     */
    double priv1;
    char   priv[SOX_MAX_FILE_PRIVSIZE]; /* format's private data area */
};

/* file flags field */
#define SOX_FILE_LOOPS   1  /* does file format support loops? */
#define SOX_FILE_INSTR   2  /* does file format support instrument specs? */
#define SOX_FILE_SEEK    4  /* does file format support seeking? */
#define SOX_FILE_NOSTDIO 8  /* does not use stdio routines */
#define SOX_FILE_DEVICE  16 /* file is an audio device */
#define SOX_FILE_PHONY   32 /* phony file/device */
/* These two for use by the libSoX core or libSoX clients: */
#define SOX_FILE_ENDIAN  64 /* is file format endian? */
#define SOX_FILE_ENDBIG  128/* if so, is it big endian? */
/* These two for use by libSoX drivers: */
#define SOX_FILE_LIT_END  (0   + 64)
#define SOX_FILE_BIG_END  (128 + 64)

/* Size field */
#define SOX_SIZE_BYTE    1
#define SOX_SIZE_8BIT    1
#define SOX_SIZE_16BIT   2
#define SOX_SIZE_24BIT   3
#define SOX_SIZE_32BIT   4
#define SOX_SIZE_64BIT   8
#define SOX_INFO_SIZE_MAX     8

/* declared in misc.c */
extern const char * const sox_sizes_str[];
extern const char * const sox_size_bits_str[];
extern const char * const sox_encodings_str[];

#define SOX_EFF_CHAN     1           /* Effect can mix channels up/down */
#define SOX_EFF_RATE     2           /* Effect can alter data rate */
#define SOX_EFF_MCHAN    4           /* Effect can handle multi-channel */
#define SOX_EFF_REPORT   8           /* Effect does not affect the audio */
#define SOX_EFF_DEPRECATED 16        /* Effect is living on borrowed time */
#define SOX_EFF_NULL     32          /* Effect does nothing */

/*
 * Handler structure for each effect.
 */

typedef struct sox_effect *eff_t;

typedef struct
{
    char const *name;               /* effect name */
    char const *usage;
    unsigned int flags;

    int (*getopts)(eff_t effp, int argc, char *argv[]);
    int (*start)(eff_t effp);
    int (*flow)(eff_t effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                sox_size_t *isamp, sox_size_t *osamp);
    int (*drain)(eff_t effp, sox_sample_t *obuf, sox_size_t *osamp);
    int (*stop)(eff_t effp);
    int (*kill)(eff_t effp);
} sox_effect_t;

struct sox_effect
{
    char const *name;               /* effect name */
    struct sox_globalinfo * globalinfo;/* global parameters */
    struct sox_signalinfo ininfo;    /* input signal specifications */
    struct sox_signalinfo outinfo;   /* output signal specifications */
    const sox_effect_t *h;           /* effects driver */
    sox_sample_t     *obuf;          /* output buffer */
    sox_size_t       odone, olen;    /* consumed, total length */
    sox_size_t       clips;   /* increment if clipping occurs */
    /* The following is a portable trick to align this variable on
     * an 8-byte boundary.  Once this is done, the buffer alloced
     * after it should be align on an 8-byte boundery as well.
     * This lets you cast any structure over the private area
     * without concerns of alignment.
     */
    double priv1;
    char priv[SOX_MAX_EFFECT_PRIVSIZE]; /* private area for effect */
};

void set_endianness_if_not_already_set(ft_t ft);
extern ft_t sox_open_read(const char *path, const sox_signalinfo_t *info, 
                         const char *filetype);
ft_t sox_open_write(
    sox_bool (*overwrite_permitted)(const char *filename),
    const char *path,
    const sox_signalinfo_t *info,
    const char *filetype,
    const char *comment,
    const sox_instrinfo_t *instr,
    const sox_loopinfo_t *loops);
extern sox_size_t sox_read(ft_t ft, sox_sample_t *buf, sox_size_t len);
extern sox_size_t sox_write(ft_t ft, const sox_sample_t *buf, sox_size_t len);
extern int sox_close(ft_t ft);

#define SOX_SEEK_SET 0
extern int sox_seek(ft_t ft, sox_size_t offset, int whence);

int sox_geteffect_opt(eff_t, int, char **);
int sox_geteffect(eff_t, const char *);
sox_bool is_effect_name(char const * text);
int sox_updateeffect(eff_t, const sox_signalinfo_t *in, const sox_signalinfo_t *out, int);
int sox_gettype(ft_t, sox_bool);
ft_t sox_initformat(void);
char const * sox_parsesamples(sox_rate_t rate, const char *str, sox_size_t *samples, int def);

/* The following routines are unique to the trim effect.
 * sox_trim_get_start can be used to find what is the start
 * of the trim operation as specified by the user.
 * sox_trim_clear_start will reset what ever the user specified
 * back to 0.
 * These two can be used together to find out what the user
 * wants to trim and use a sox_seek() operation instead.  After
 * sox_seek()'ing, you should set the trim option to 0.
 */
sox_size_t sox_trim_get_start(eff_t effp);
void sox_trim_clear_start(eff_t effp);

extern char const * sox_message_filename;

#define SOX_EOF (-1)
#define SOX_SUCCESS (0)

const char *sox_version(void);                   /* return version number */

/* libSoX specific error codes.  The rest directly map from errno. */
#define SOX_EHDR 2000            /* Invalid Audio Header */
#define SOX_EFMT 2001            /* Unsupported data format */
#define SOX_ERATE 2002           /* Unsupported rate for format */
#define SOX_ENOMEM 2003          /* Can't alloc memory */
#define SOX_EPERM 2004           /* Operation not permitted */
#define SOX_ENOTSUP 2005         /* Operation not supported */
#define SOX_EINVAL 2006          /* Invalid argument */
#define SOX_EFFMT 2007           /* Unsupported file format */

#endif
