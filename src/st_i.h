#ifndef ST_I_H
#define ST_I_H
/*
 * Sound Tools Internal - October 11, 2001
 *
 *   This file is meant for libst internal use only
 *
 * Copyright 2001 Chris Bagwell
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Chris Bagwell And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "stconfig.h"
#include "st.h"

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

/* various gcc optimizations and portablity defines */
#define NORET __attribute__((noreturn))
#define REGPARM(n) __attribute__((regparm(n)))

/* declared in misc.c */
typedef struct {char const *text; int value;} enum_item;
#define ENUM_ITEM(prefix, item) {#item, prefix##item},
enum_item const * find_enum_text(
    char const * text, enum_item const * enum_items);

typedef enum {ST_SHORT, ST_INT, ST_FLOAT, ST_DOUBLE} st_data_t;
typedef enum {ST_WAVE_SINE, ST_WAVE_TRIANGLE} st_wave_t;
extern enum_item const st_wave_enum[];
void st_generate_wave_table(
    st_wave_t wave_type,
    st_data_t data_type,
    void *table,
    uint32_t table_size,
    double min,
    double max,
    double phase);

REGPARM(2) st_sample_t st_gcd(st_sample_t a, st_sample_t b);
REGPARM(2) st_sample_t st_lcm(st_sample_t a, st_sample_t b);

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(char const * s1, char const * s2, size_t n);
#endif

#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif

void st_initrand(void);

/* Read and write basic data types from "ft" stream.  Uses ft->swap for
 * possible byte swapping.
 */
/* declared in misc.c */
size_t st_readbuf(ft_t ft, void *buf, size_t size, st_size_t len);
size_t st_writebuf(ft_t ft, void const *buf, size_t size, st_size_t len);
int st_reads(ft_t ft, char *c, st_size_t len);
int st_writes(ft_t ft, char *c);
int st_readb(ft_t ft, uint8_t *ub);
int st_writeb(ft_t ft, uint8_t ub);
int st_readw(ft_t ft, uint16_t *uw);
int st_writew(ft_t ft, uint16_t uw);
int st_read3(ft_t ft, uint24_t *u3);
int st_write3(ft_t ft, uint24_t u3);
int st_readdw(ft_t ft, uint32_t *udw);
int st_writedw(ft_t ft, uint32_t udw);
int st_readf(ft_t ft, float *f);
int st_writef(ft_t ft, float f);
int st_readdf(ft_t ft, double *d);
int st_writedf(ft_t ft, double d);
int st_seeki(ft_t ft, st_size_t offset, int whence);
st_size_t st_filelength(ft_t ft);
int st_flush(ft_t ft);
st_size_t st_tell(ft_t ft);
int st_eof(ft_t ft);
int st_error(ft_t ft);
void st_rewind(ft_t ft);
void st_clearerr(ft_t ft);

/* Utilities to byte-swap values, use libc optimized macros if possible  */
#ifdef HAVE_BYTESWAP_H
#define st_swapw(x) bswap_16(x)
#define st_swapdw(x) bswap_32(x)
#else
uint16_t st_swapw(uint16_t uw);
uint32_t st_swapdw(uint32_t udw);
#endif
float st_swapf(float f);
uint32_t st_swap24(uint32_t udw);
double st_swapd(double d);

/* util.c */
struct st_output_message_s;
typedef struct st_output_message_s * st_output_message_t;
typedef void (* st_output_message_handler_t)(int level, st_output_message_t);
extern st_output_message_handler_t st_output_message_handler;
extern int st_output_verbosity_level;
void st_output_message(FILE * file, st_output_message_t);

void st_fail(const char *, ...);
void st_warn(const char *, ...);
void st_report(const char *, ...);
void st_debug(const char *, ...);
void st_debug_more(char const * fmt, ...);
void st_debug_most(char const * fmt, ...);

#define st_fail       st_message_filename=__FILE__,st_fail
#define st_warn       st_message_filename=__FILE__,st_warn
#define st_report     st_message_filename=__FILE__,st_report
#define st_debug      st_message_filename=__FILE__,st_debug
#define st_debug_more st_message_filename=__FILE__,st_debug_more
#define st_debug_most st_message_filename=__FILE__,st_debug_most

void st_fail_errno(ft_t, int, const char *, ...);

int st_is_bigendian(void);
int st_is_littleendian(void);

#ifdef WORDS_BIGENDIAN
#define ST_IS_BIGENDIAN 1
#define ST_IS_LITTLEENDIAN 0
#else
#define ST_IS_BIGENDIAN 0
#define ST_IS_LITTLEENDIAN 1
#endif

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2  1.57079632679489661923  /* pi/2 */
#endif

/* The following is used at times in libst when alloc()ing buffers
 * to perform file I/O.  It can be useful to pass in similar sized
 * data to get max performance.
 */
#define ST_BUFSIZ 8192

/*=============================================================================
 * File Handlers
 *=============================================================================
 */

/* Psion record header check, defined in prc.c */
int prc_checkheader(ft_t ft, char *head);

typedef const st_format_t *(*st_format_fn_t)(void);

extern st_format_fn_t st_format_fns[];

extern const st_format_t *st_aiff_format_fn(void);
extern const st_format_t *st_aifc_format_fn(void);
#ifdef HAVE_ALSA
extern const st_format_t *st_alsa_format_fn(void);
#endif
extern const st_format_t *st_au_format_fn(void);
extern const st_format_t *st_auto_format_fn(void);
extern const st_format_t *st_avr_format_fn(void);
extern const st_format_t *st_cdr_format_fn(void);
extern const st_format_t *st_cvsd_format_fn(void);
extern const st_format_t *st_dvms_format_fn(void);
extern const st_format_t *st_dat_format_fn(void);
#ifdef HAVE_LIBFLAC
extern const st_format_t *st_flac_format_fn(void);
#endif
extern const st_format_t *st_gsm_format_fn(void);
extern const st_format_t *st_hcom_format_fn(void);
extern const st_format_t *st_maud_format_fn(void);
extern const st_format_t *st_mp3_format_fn(void);
extern const st_format_t *st_nul_format_fn(void);
#ifdef HAVE_OSS
extern const st_format_t *st_ossdsp_format_fn(void);
#endif
extern const st_format_t *st_prc_format_fn(void);
extern const st_format_t *st_raw_format_fn(void);
extern const st_format_t *st_al_format_fn(void);
extern const st_format_t *st_la_format_fn(void);
extern const st_format_t *st_lu_format_fn(void);
extern const st_format_t *st_s3_format_fn(void);
extern const st_format_t *st_sb_format_fn(void);
extern const st_format_t *st_sl_format_fn(void);
extern const st_format_t *st_sw_format_fn(void);
extern const st_format_t *st_u3_format_fn(void);
extern const st_format_t *st_ub_format_fn(void);
extern const st_format_t *st_u4_format_fn(void);
extern const st_format_t *st_ul_format_fn(void);
extern const st_format_t *st_uw_format_fn(void);
extern const st_format_t *st_sf_format_fn(void);
extern const st_format_t *st_smp_format_fn(void);
extern const st_format_t *st_snd_format_fn(void);
extern const st_format_t *st_sphere_format_fn(void);
#ifdef HAVE_SUNAUDIO
extern const st_format_t *st_sun_format_fn(void);
#endif
extern const st_format_t *st_svx_format_fn(void);
extern const st_format_t *st_txw_format_fn(void);
extern const st_format_t *st_voc_format_fn(void);
#if defined HAVE_LIBVORBISENC && defined HAVE_LIBVORBISFILE
extern const st_format_t *st_vorbis_format_fn(void);
#endif
extern const st_format_t *st_vox_format_fn(void);
extern const st_format_t *st_wav_format_fn(void);
extern const st_format_t *st_wve_format_fn(void);
extern const st_format_t *st_xa_format_fn(void);

/* Raw I/O
 */
int st_rawstartread(ft_t ft);
st_size_t st_rawread(ft_t ft, st_sample_t *buf, st_size_t nsamp);
int st_rawstopread(ft_t ft);
int st_rawstartwrite(ft_t ft);
st_size_t st_rawwrite(ft_t ft, const st_sample_t *buf, st_size_t nsamp);
int st_rawstopwrite(ft_t ft);
int st_rawseek(ft_t ft, st_size_t offset);

/* The following functions can be used to simply return success if
 * a file handler or effect doesn't need to do anything special
 */
int st_format_nothing(ft_t ft);
st_size_t st_format_nothing_read_io(ft_t ft, st_sample_t *buf, st_size_t len);
st_size_t st_format_nothing_write_io(ft_t ft, const st_sample_t *buf, st_size_t len);
int st_format_nothing_seek(ft_t ft, st_size_t offset);
int st_effect_nothing(eff_t effp);
int st_effect_nothing_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_effect_nothing_getopts(eff_t effp, int n, char **argv UNUSED);

#define st_rawstopread st_format_nothing
int st_rawstart(ft_t ft);
#define st_rawstartread st_rawstart
#define st_rawstartwrite st_rawstart

/*=============================================================================
 * Effects
 *=============================================================================
 */

typedef const st_effect_t *(*st_effect_fn_t)(void);

extern st_effect_fn_t st_effect_fns[];

extern const st_effect_t *st_avg_effect_fn(void);
extern const st_effect_t *st_pick_effect_fn(void);
extern const st_effect_t *st_band_effect_fn(void);
extern const st_effect_t *st_bass_effect_fn(void);
extern const st_effect_t *st_bandpass_effect_fn(void);
extern const st_effect_t *st_bandreject_effect_fn(void);
extern const st_effect_t *st_chorus_effect_fn(void);
extern const st_effect_t *st_compand_effect_fn(void);
extern const st_effect_t *st_copy_effect_fn(void);
extern const st_effect_t *st_dcshift_effect_fn(void);
extern const st_effect_t *st_deemph_effect_fn(void);
extern const st_effect_t *st_earwax_effect_fn(void);
extern const st_effect_t *st_echo_effect_fn(void);
extern const st_effect_t *st_echos_effect_fn(void);
extern const st_effect_t *st_equalizer_effect_fn(void);
extern const st_effect_t *st_fade_effect_fn(void);
extern const st_effect_t *st_filter_effect_fn(void);
extern const st_effect_t *st_flanger_effect_fn(void);
extern const st_effect_t *st_highp_effect_fn(void);
extern const st_effect_t *st_highpass_effect_fn(void);
extern const st_effect_t *st_lowp_effect_fn(void);
extern const st_effect_t *st_lowpass_effect_fn(void);
extern const st_effect_t *st_mask_effect_fn(void);
extern const st_effect_t *st_mcompand_effect_fn(void);
extern const st_effect_t *st_noiseprof_effect_fn(void);
extern const st_effect_t *st_noisered_effect_fn(void);
extern const st_effect_t *st_pan_effect_fn(void);
extern const st_effect_t *st_phaser_effect_fn(void);
extern const st_effect_t *st_pitch_effect_fn(void);
extern const st_effect_t *st_polyphase_effect_fn(void);
#ifdef HAVE_SAMPLERATE
extern const st_effect_t *st_rabbit_effect_fn(void);
#endif
extern const st_effect_t *st_rate_effect_fn(void);
extern const st_effect_t *st_repeat_effect_fn(void);
extern const st_effect_t *st_resample_effect_fn(void);
extern const st_effect_t *st_reverb_effect_fn(void);
extern const st_effect_t *st_reverse_effect_fn(void);
extern const st_effect_t *st_silence_effect_fn(void);
extern const st_effect_t *st_speed_effect_fn(void);
extern const st_effect_t *st_stat_effect_fn(void);
extern const st_effect_t *st_stretch_effect_fn(void);
extern const st_effect_t *st_swap_effect_fn(void);
extern const st_effect_t *st_synth_effect_fn(void);
extern const st_effect_t *st_treble_effect_fn(void);
extern const st_effect_t *st_trim_effect_fn(void);
extern const st_effect_t *st_vibro_effect_fn(void);
extern const st_effect_t *st_vol_effect_fn(void);

/* Needed in sox.c */
st_size_t st_trim_get_start(eff_t effp);
void st_trim_clear_start(eff_t effp);

/* Needed in rate.c */
int st_resample_start(eff_t effp);
int st_resample_getopts(eff_t effp, int n, char **argv);
int st_resample_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, st_size_t *isamp, st_size_t *osamp);
int st_resample_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_resample_stop(eff_t effp);

#endif
