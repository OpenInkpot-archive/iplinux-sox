#ifndef ST_H
#define ST_H
/*
 * Sound Tools Library - October 11, 1999
 *
 * Copyright 1999 Chris Bagwell
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Chris Bagwell And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include <stdio.h>
#include <stdlib.h>
#include "ststdint.h"

/* Avoid warnings about unused parameters. */
#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#endif

/* C language enhancements: */

/* Boolean type, compatible with C++ */
typedef enum {false, true} bool;

/* Compile-time ("static") assertion */
/*   e.g. assert_static(sizeof(int) >= 4, int_type_too_small)    */
#define assert_static(e,f) enum {assert_static__##f = 1/(e)}

#ifdef min
#undef min
#endif
#define min(a, b) ((a) <= (b) ? (a) : (b))

#ifdef max
#undef max
#endif
#define max(a, b) ((a) >= (b) ? (a) : (b))

/* Array-length operator */
#define array_length(a) (sizeof(a)/sizeof(a[0]))

typedef int32_t int24_t;     /* But beware of the extra byte. */
typedef uint32_t uint24_t;   /* ditto */

#define ST_INT_MIN(bits) (1L <<((bits)-1))
#define ST_INT_MAX(bits) (-1UL>>(33-(bits)))
#define ST_UINT_MAX(bits) (ST_INT_MIN(bits)|ST_INT_MAX(bits))

#define ST_INT8_MAX  ST_INT_MAX(8)
#define ST_INT16_MAX ST_INT_MAX(16)
#define ST_INT24_MAX ST_INT_MAX(24)
#define ST_INT32_MAX ST_INT_MAX(32)
#define ST_INT64_MAX 0x7fffffffffffffffLL /* Not in use yet */

typedef int32_t st_sample_t;
typedef uint32_t st_usample_t; /* FIXME: this naming is different from
                                  other types */
/* Minimum and maximum values a sample can hold. */
#define ST_SAMPLE_MAX (st_sample_t)ST_INT_MAX(32)
#define ST_SAMPLE_MIN (st_sample_t)ST_INT_MIN(32)



/*                Conversions: Linear PCM <--> st_sample_t
 *
 *   I/O       I/O     st_sample_t  Clips?    I/O     st_sample_t  Clips? 
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
static st_sample_t st_macro_temp_sample UNUSED;
static double st_macro_temp_double UNUSED;

#define ST_SAMPLE_NEG ST_INT_MIN(32)
#define ST_SAMPLE_TO_UNSIGNED(bits,d,clips) \
  (uint##bits##_t)( \
    st_macro_temp_sample=d, \
    st_macro_temp_sample>(st_sample_t)(ST_SAMPLE_MAX-(1UL<<(31-bits)))? \
      ++(clips),ST_UINT_MAX(bits): \
      ((uint32_t)(st_macro_temp_sample^ST_SAMPLE_NEG)+(1UL<<(31-bits)))>>(32-bits))
#define ST_SAMPLE_TO_SIGNED(bits,d,clips) \
  (int##bits##_t)(ST_SAMPLE_TO_UNSIGNED(bits,d,clips)^ST_INT_MIN(bits))
#define ST_SIGNED_TO_SAMPLE(bits,d)((st_sample_t)(d)<<(32-bits))
#define ST_UNSIGNED_TO_SAMPLE(bits,d)(ST_SIGNED_TO_SAMPLE(bits,d)^ST_SAMPLE_NEG)

#define ST_UNSIGNED_BYTE_TO_SAMPLE(d,clips) ST_UNSIGNED_TO_SAMPLE(8,d)
#define ST_SIGNED_BYTE_TO_SAMPLE(d,clips) ST_SIGNED_TO_SAMPLE(8,d)
#define ST_UNSIGNED_WORD_TO_SAMPLE(d,clips) ST_UNSIGNED_TO_SAMPLE(16,d)
#define ST_SIGNED_WORD_TO_SAMPLE(d,clips) ST_SIGNED_TO_SAMPLE(16,d)
#define ST_UNSIGNED_24BIT_TO_SAMPLE(d,clips) ST_UNSIGNED_TO_SAMPLE(24,d)
#define ST_SIGNED_24BIT_TO_SAMPLE(d,clips) ST_SIGNED_TO_SAMPLE(24,d)
#define ST_UNSIGNED_DWORD_TO_SAMPLE(d,clips) (st_sample_t)((d)^ST_SAMPLE_NEG)
#define ST_SIGNED_DWORD_TO_SAMPLE(d,clips) (st_sample_t)(d)
#define ST_FLOAT_DWORD_TO_SAMPLE ST_FLOAT_DDWORD_TO_SAMPLE
#define ST_FLOAT_DDWORD_TO_SAMPLE(d,clips) (st_macro_temp_double=d,st_macro_temp_double<-1?++(clips),(-ST_SAMPLE_MAX):st_macro_temp_double>1?++(clips),ST_SAMPLE_MAX:(st_sample_t)((uint32_t)((double)(st_macro_temp_double)*ST_SAMPLE_MAX+(ST_SAMPLE_MAX+.5))-ST_SAMPLE_MAX))
#define ST_SAMPLE_TO_UNSIGNED_BYTE(d,clips) ST_SAMPLE_TO_UNSIGNED(8,d,clips)
#define ST_SAMPLE_TO_SIGNED_BYTE(d,clips) ST_SAMPLE_TO_SIGNED(8,d,clips)
#define ST_SAMPLE_TO_UNSIGNED_WORD(d,clips) ST_SAMPLE_TO_UNSIGNED(16,d,clips)
#define ST_SAMPLE_TO_SIGNED_WORD(d,clips) ST_SAMPLE_TO_SIGNED(16,d,clips)
#define ST_SAMPLE_TO_UNSIGNED_24BIT(d,clips) ST_SAMPLE_TO_UNSIGNED(24,d,clips)
#define ST_SAMPLE_TO_SIGNED_24BIT(d,clips) ST_SAMPLE_TO_SIGNED(24,d,clips)
#define ST_SAMPLE_TO_UNSIGNED_DWORD(d,clips) (uint32_t)((d)^ST_SAMPLE_NEG)
#define ST_SAMPLE_TO_SIGNED_DWORD(d,clips) (int32_t)(d)
#define ST_SAMPLE_TO_FLOAT_DWORD ST_SAMPLE_TO_FLOAT_DDWORD
#define ST_SAMPLE_TO_FLOAT_DDWORD(d,clips) (st_macro_temp_sample=d,st_macro_temp_sample==ST_SAMPLE_MIN?++(clips),-1.0:((double)(st_macro_temp_sample)*(1.0/ST_SAMPLE_MAX)))



/* MACRO to clip a data type that is greater then st_sample_t to
 * st_sample_t's limits and increment a counter if clipping occurs..
 */
#define ST_SAMPLE_CLIP_COUNT(samp, clips) \
  do { \
    if (samp > ST_SAMPLE_MAX) \
      { samp = ST_SAMPLE_MAX; clips++; } \
    else if (samp < ST_SAMPLE_MIN) \
      { samp = ST_SAMPLE_MIN; clips++; } \
  } while (0)

/* Rvalue MACRO to round and clip a double to a st_sample_t,
 * and increment a counter if clipping occurs.
 */
#define ST_ROUND_CLIP_COUNT(d, clips) \
  ((d) < 0? (d) <= ST_SAMPLE_MIN - 0.5? ++(clips), ST_SAMPLE_MIN: (d) - 0.5 \
        : (d) >= ST_SAMPLE_MAX + 0.5? ++(clips), ST_SAMPLE_MAX: (d) + 0.5)

/* Rvalue MACRO to clip a st_sample_t to 24 bits,
 * and increment a counter if clipping occurs.
 */
#define ST_24BIT_CLIP_COUNT(l, clips) \
  ((l) >= ((st_sample_t)1 << 23)? ++(clips), ((st_sample_t)1 << 23) - 1 : \
   (l) <=-((st_sample_t)1 << 23)? ++(clips),-((st_sample_t)1 << 23) + 1 : (l))



typedef uint32_t st_size_t;
/* Maximum value size type can hold. (Minimum is 0). */
#define ST_SIZE_MAX 0xffffffffL

typedef int32_t st_ssize_t;
/* Minimum and maximum value signed size type can hold. */
#define ST_SSIZE_MAX 0x7fffffffL
#define ST_SSIZE_MIN (-ST_SSIZE_MAX - 1L)

typedef uint32_t st_rate_t;
/* Warning, this is a MAX value used in the library.  Each format and
 * effect may have its own limitations of rate.
 */
#define ST_MAXRATE      (50UL * 1024) /* maximum sample rate in library */

typedef enum {
  ST_ENCODING_UNKNOWN   ,
  ST_ENCODING_UNSIGNED  , /* unsigned linear: Sound Blaster */
  ST_ENCODING_SIGN2     , /* signed linear 2's comp: Mac */
  ST_ENCODING_ULAW      , /* u-law signed logs: US telephony, SPARC */
  ST_ENCODING_ALAW      , /* A-law signed logs: non-US telephony */
  ST_ENCODING_FLOAT     , /* 32-bit float */
  ST_ENCODING_ADPCM     , /* Compressed PCM */
  ST_ENCODING_IMA_ADPCM , /* Compressed PCM */
  ST_ENCODING_GSM       , /* GSM 6.10 33byte frame lossy compression */
  ST_ENCODING_INV_ULAW  , /* Inversed bit-order u-law */
  ST_ENCODING_INV_ALAW  , /* Inversed bit-order A-law */
  ST_ENCODING_MP3       , /* MP3 compression */
  ST_ENCODING_VORBIS    , /* Vorbis compression */
  ST_ENCODING_FLAC      , /* FLAC compression */
  ST_ENCODING_OKI_ADPCM , /* Compressed PCM */

  ST_ENCODINGS            /* End of list marker */
} st_encoding_t;

/* Global parameters */

typedef struct  st_globalinfo
{
    bool octave_plot_effect;/* To help user choose effect & options */
    double speed;         /* Gather up all speed changes here, then resample */
} st_globalinfo_t;

/* Signal parameters */

typedef struct  st_signalinfo
{
    st_rate_t rate;       /* sampling rate */
    signed char size;     /* word length of data */
    st_encoding_t encoding; /* format of sample numbers */
    unsigned channels;    /* number of sound channels */
    char swap;            /* do byte- or word-swap */
    double compression;   /* compression factor (where applicable) */
} st_signalinfo_t;

/* Loop parameters */

typedef struct  st_loopinfo
{
    st_size_t    start;          /* first sample */
    st_size_t    length;         /* length */
    unsigned int count;          /* number of repeats, 0=forever */
    signed char  type;           /* 0=no, 1=forward, 2=forward/back */
} st_loopinfo_t;

/* Instrument parameters */

/* vague attempt at generic information for sampler-specific info */

typedef struct  st_instrinfo
{
    char MIDInote;       /* for unity pitch playback */
    char MIDIlow, MIDIhi;/* MIDI pitch-bend range */
    char loopmode;       /* semantics of loop data */
    signed char nloops;  /* number of active loops (max ST_MAX_NLOOPS) */
} st_instrinfo_t;

/* Loop modes, upper 4 bits mask the loop blass, lower 4 bits describe */
/* the loop behaviour, ie. single shot, bidirectional etc. */
#define ST_LOOP_NONE          0
#define ST_LOOP_8             32 /* 8 loops: don't know ?? */
#define ST_LOOP_SUSTAIN_DECAY 64 /* AIFF style: one sustain & one decay loop */

/*
 * File buffer info.  Holds info so that data can be read in blocks.
 */

typedef struct st_fileinfo
{
    char          *buf;                 /* Pointer to data buffer */
    size_t        size;                 /* Size of buffer */
    size_t        count;                /* Count read in to buffer */
    size_t        pos;                  /* Position in buffer */
    unsigned char eof;                  /* Marker that EOF has been reached */
} st_fileinfo_t;


/*
 *  Format information for input and output files.
 */

#define ST_MAX_FILE_PRIVSIZE    1000
#define ST_MAX_EFFECT_PRIVSIZE 1000

#define ST_MAX_NLOOPS           8

/*
 * Handler structure for each format.
 */

typedef struct st_soundstream *ft_t;

typedef struct st_format {
    const char   * const *names;
    const char   *usage;
    unsigned int flags;
    int          (*startread)(ft_t ft);
    st_size_t    (*read)(ft_t ft, st_sample_t *buf, st_size_t len);
    int          (*stopread)(ft_t ft);
    int          (*startwrite)(ft_t ft);
    st_size_t    (*write)(ft_t ft, const st_sample_t *buf, st_size_t len);
    int          (*stopwrite)(ft_t ft);
    int          (*seek)(ft_t ft, st_size_t offset);
} st_format_t;

struct st_soundstream {
    st_signalinfo_t info;                 /* signal specifications */
    st_instrinfo_t  instr;                /* instrument specification */
    st_loopinfo_t   loops[ST_MAX_NLOOPS]; /* Looping specification */
    char            seekable;             /* can seek on this file */
    char            mode;                 /* read or write mode */
    /* Total samples per channel of file.  Zero if unknown. */
    st_size_t       length;    
    st_size_t       clippedCount;         /* increment if clipping occurs */
    char            *filename;            /* file name */
    char            *filetype;            /* type of file */
    char            *comment;             /* comment string */
    FILE            *fp;                  /* File stream pointer */
    unsigned char   eof;                  /* Marker that EOF has been reached */
    int             st_errno;             /* Failure error codes */
    char            st_errstr[256];       /* Extend Failure text */
    const st_format_t *h;                 /* format struct for this file */
    /* The following is a portable trick to align this variable on
     * an 8-byte bounder.  Once this is done, the buffer alloced
     * after it should be align on an 8-byte boundery as well.
     * This lets you cast any structure over the private area
     * without concerns of alignment.
     */
    double priv1;
    char   priv[ST_MAX_FILE_PRIVSIZE]; /* format's private data area */
};

/* file flags field */
#define ST_FILE_STEREO  1  /* does file format support stereo? */
#define ST_FILE_LOOPS   2  /* does file format support loops? */
#define ST_FILE_INSTR   4  /* does file format support instrument specs? */
#define ST_FILE_SEEK    8  /* does file format support seeking? */
#define ST_FILE_NOSTDIO 16 /* does not use stdio routines */
#define ST_FILE_NOFEXT  32 /* does not use file extensions */

/* Size field */
#define ST_SIZE_BYTE    1
#define ST_SIZE_8BIT    1
#define ST_SIZE_WORD    2
#define ST_SIZE_16BIT   2
#define ST_SIZE_24BIT   3
#define ST_SIZE_DWORD   4
#define ST_SIZE_32BIT   4
#define ST_SIZE_DDWORD  8
#define ST_SIZE_64BIT   8
#define ST_INFO_SIZE_MAX     8

/* declared in misc.c */
extern const char * const st_sizes_str[];
extern const char * const st_size_bits_str[];
extern const char * const st_encodings_str[];

#define ST_EFF_CHAN     1               /* Effect can mix channels up/down */
#define ST_EFF_RATE     2               /* Effect can alter data rate */
#define ST_EFF_MCHAN    4               /* Effect can handle multi-channel */
#define ST_EFF_REPORT   8               /* Effect does not affect the audio */
#define ST_EFF_NULL    16               /* Effect does nothing */

/*
 * Handler structure for each effect.
 */

typedef struct st_effect *eff_t;

typedef struct
{
    char    const * name;           /* effect name */
    char    const * usage;
    unsigned int flags;

    int (*getopts)(eff_t effp, int argc, char *argv[]);
    int (*start)(eff_t effp);
    int (*flow)(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf,
                st_size_t *isamp, st_size_t *osamp);
    int (*drain)(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
    int (*stop)(eff_t effp);
} st_effect_t;

struct st_effect
{
    char    const * name;           /* effect name */
    struct st_globalinfo * globalinfo;/* global ST parameters */
    struct st_signalinfo ininfo;    /* input signal specifications */
    struct st_signalinfo outinfo;   /* output signal specifications */
    const st_effect_t *h;           /* effects driver */
    st_sample_t     *obuf;          /* output buffer */
    st_size_t       odone, olen;    /* consumed, total length */
    st_size_t       clippedCount;   /* increment if clipping occurs */
    /* The following is a portable trick to align this variable on
     * an 8-byte boundary.  Once this is done, the buffer alloced
     * after it should be align on an 8-byte boundery as well.
     * This lets you cast any structure over the private area
     * without concerns of alignment.
     */
    double priv1;
    char priv[ST_MAX_EFFECT_PRIVSIZE]; /* private area for effect */
};

extern ft_t st_open_read(const char *path, const st_signalinfo_t *info, 
                         const char *filetype);
extern ft_t st_open_write(const char *path, const st_signalinfo_t *info,
                          const char *filetype, const char *comment);
extern ft_t st_open_write_instr(const char *path, const st_signalinfo_t *info,
                                const char *filetype, const char *comment, 
                                const st_instrinfo_t *instr,
                                const st_loopinfo_t *loops);
extern st_size_t st_read(ft_t ft, st_sample_t *buf, st_size_t len);
extern st_size_t st_write(ft_t ft, const st_sample_t *buf, st_size_t len);
extern int st_close(ft_t ft);

#define ST_SEEK_SET 0
extern int st_seek(ft_t ft, st_size_t offset, int whence);

int st_geteffect_opt(eff_t, int, char **);
int st_geteffect(eff_t, const char *);
bool is_effect_name(char const * text);
int st_updateeffect(eff_t, const st_signalinfo_t *in, const st_signalinfo_t *out, int);
int st_gettype(ft_t, bool);
ft_t st_initformat(void);
int st_parsesamples(st_rate_t rate, const char *str, st_size_t *samples, char def);

extern char const * st_message_filename;

#define ST_EOF (-1)
#define ST_SUCCESS (0)

const char *st_version(void);                   /* return version number */

/* ST specific error codes.  The rest directly map from errno. */
#define ST_EHDR 2000            /* Invalid Audio Header */
#define ST_EFMT 2001            /* Unsupported data format */
#define ST_ERATE 2002           /* Unsupported rate for format */
#define ST_ENOMEM 2003          /* Can't alloc memory */
#define ST_EPERM 2004           /* Operation not permitted */
#define ST_ENOTSUP 2005         /* Operation not supported */
#define ST_EINVAL 2006          /* Invalid argument */
#define ST_EFFMT 2007           /* Unsupported file format */

/* Define fseeko and ftello for platforms lacking them */
#ifndef HAVE_FSEEKO
#define fseeko fseek
#define ftello ftell
#define off_t long
#endif

#endif
