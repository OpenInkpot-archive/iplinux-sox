/*
 * libSoX libsndfile formats.
 *
 * Copyright 2007 Reuben Thomas <rrt@sc3d.org>
 * Copyright 1999-2005 Erik de Castro Lopo <eridk@mega-nerd.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.  */

#include "sox_i.h"

#ifdef HAVE_SNDFILE_H

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sndfile.h>

#define LOG_MAX 2048 /* As per the SFC_GET_LOG_INFO example */

#ifndef HACKED_LSF
#define sf_stop(x)
#endif

/* Private data for sndfile files */
typedef struct sndfile
{
  SNDFILE *sf_file;
  SF_INFO *sf_info;
  char * log_buffer;
  char const * log_buffer_ptr;
} *sndfile_t;

assert_static(sizeof(struct sndfile) <= SOX_MAX_FILE_PRIVSIZE, 
              /* else */ sndfile_PRIVSIZE_too_big);

/*
 * Drain LSF's wonderful log buffer
 */
static void drain_log_buffer(sox_format_t * ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;

  sf_command(sf->sf_file, SFC_GET_LOG_INFO, sf->log_buffer, LOG_MAX);
  while (*sf->log_buffer_ptr) {
    static char const warning_prefix[] = "*** Warning : ";
    char const * end = strchr(sf->log_buffer_ptr, '\n');
    if (!end)
      end = strchr(sf->log_buffer_ptr, '\0');
    if (!strncmp(sf->log_buffer_ptr, warning_prefix, strlen(warning_prefix))) {
      sf->log_buffer_ptr += strlen(warning_prefix);
      sox_warn("`%s': %.*s",
          ft->filename, end - sf->log_buffer_ptr, sf->log_buffer_ptr);
    } else
      sox_debug("`%s': %.*s",
          ft->filename, end - sf->log_buffer_ptr, sf->log_buffer_ptr);
    sf->log_buffer_ptr = end;
    if (*sf->log_buffer_ptr == '\n')
      ++sf->log_buffer_ptr;
  }
}

/* Get sample encoding and size from libsndfile subtype; return value
   is encoding if conversion was made, or SOX_ENCODING_UNKNOWN for
   invalid input. If the libsndfile subtype can't be represented in
   SoX types, use 16-bit signed. */
static sox_encoding_t sox_encoding_and_size(unsigned format, unsigned * size)
{
  *size = 0;                   /* Default */
  format &= SF_FORMAT_SUBMASK;
  
  switch (format) {
  case SF_FORMAT_PCM_S8:
    *size = 8;
    return SOX_ENCODING_SIGN2;
  case SF_FORMAT_PCM_16:
    *size = 16;
    return SOX_ENCODING_SIGN2;
  case SF_FORMAT_PCM_24:
    *size = 24;
    return SOX_ENCODING_SIGN2;
  case SF_FORMAT_PCM_32:
    *size = 32;
    return SOX_ENCODING_SIGN2;
  case SF_FORMAT_PCM_U8:
    *size = 8;
    return SOX_ENCODING_UNSIGNED;
  case SF_FORMAT_FLOAT:
    *size = 32;
    return SOX_ENCODING_FLOAT;
  case SF_FORMAT_DOUBLE:
    *size = 64;
    return SOX_ENCODING_FLOAT;
  case SF_FORMAT_ULAW:
    *size = 8;
    return SOX_ENCODING_ULAW;
  case SF_FORMAT_ALAW:
    *size = 8;
    return SOX_ENCODING_ALAW;
  case SF_FORMAT_IMA_ADPCM:
    *size = 16;
    return SOX_ENCODING_IMA_ADPCM;
  case SF_FORMAT_MS_ADPCM:
    *size = 16;
    return SOX_ENCODING_MS_ADPCM;
  case SF_FORMAT_GSM610:
    *size = 16;
    return SOX_ENCODING_GSM;
  case SF_FORMAT_VOX_ADPCM:
    *size = 16;
    return SOX_ENCODING_OKI_ADPCM;

  /* For encodings we can't represent, have a sensible default */
  case SF_FORMAT_G721_32:
  case SF_FORMAT_G723_24:
  case SF_FORMAT_G723_40:
  case SF_FORMAT_DWVW_12:
  case SF_FORMAT_DWVW_16:
  case SF_FORMAT_DWVW_24:
  case SF_FORMAT_DWVW_N:
  case SF_FORMAT_DPCM_8:
  case SF_FORMAT_DPCM_16:
    return SOX_ENCODING_SIGN2;

  default: /* Invalid libsndfile subtype */
    return SOX_ENCODING_UNKNOWN;
  }

  assert(0); /* Should never reach here */
  return SOX_ENCODING_UNKNOWN;
}

static struct {
  const char *ext;
  int format;
} format_map[] =
{
  { "aif",	SF_FORMAT_AIFF },
  { "aiff",	SF_FORMAT_AIFF },
  { "wav",	SF_FORMAT_WAV },
  { "au",	SF_FORMAT_AU },
  { "snd",	SF_FORMAT_AU },
#ifdef HAVE_SNDFILE_1_0_12
  { "caf",	SF_FORMAT_CAF },
  { "flac",	SF_FORMAT_FLAC },
#endif
#ifdef HAVE_SNDFILE_1_0_18
  { "wve",	SF_FORMAT_WVE },
  { "ogg",	SF_FORMAT_OGG },
#endif
  { "svx",	SF_FORMAT_SVX },
  { "8svx",     SF_FORMAT_SVX },
  { "paf",	SF_ENDIAN_BIG | SF_FORMAT_PAF },
  { "fap",	SF_ENDIAN_LITTLE | SF_FORMAT_PAF },
  { "gsm",	SF_FORMAT_RAW | SF_FORMAT_GSM610 },
  { "nist", 	SF_FORMAT_NIST },
  { "sph",      SF_FORMAT_NIST },
  { "ircam",	SF_FORMAT_IRCAM },
  { "sf",	SF_FORMAT_IRCAM },
  { "voc",	SF_FORMAT_VOC },
  { "w64", 	SF_FORMAT_W64 },
  { "raw",	SF_FORMAT_RAW },
  { "mat4", 	SF_FORMAT_MAT4 },
  { "mat5", 	SF_FORMAT_MAT5 },
  { "mat",	SF_FORMAT_MAT4 },
  { "pvf",	SF_FORMAT_PVF },
  { "sds",	SF_FORMAT_SDS },
  { "sd2",	SF_FORMAT_SD2 },
  { "vox",	SF_FORMAT_RAW | SF_FORMAT_VOX_ADPCM },
  { "xi",	SF_FORMAT_XI }
};

/* Convert file name or type to libsndfile format */
static int name_to_format(const char *name)
{
  int k;
#define FILE_TYPE_BUFLEN 15
  char buffer[FILE_TYPE_BUFLEN + 1], *cptr;

  if ((cptr = strrchr(name, '.')) != NULL) {
    strncpy(buffer, cptr + 1, FILE_TYPE_BUFLEN);
    buffer[FILE_TYPE_BUFLEN] = '\0';
  
    for (k = 0; buffer[k]; k++)
      buffer[k] = tolower((buffer[k]));
  } else
    strncpy(buffer, name, FILE_TYPE_BUFLEN);
  
  for (k = 0; k < (int)(sizeof(format_map) / sizeof(format_map [0])); k++) {
    if (strcmp(buffer, format_map[k].ext) == 0)
      return format_map[k].format;
  }

  return 0;
}

/* Make libsndfile subtype from sample encoding and size */
static int sndfile_format(sox_encoding_t encoding, unsigned size)
{
  size = (size + 7) & ~7u;
  switch (encoding) {
    case SOX_ENCODING_ULAW:
      return SF_FORMAT_ULAW;
    case SOX_ENCODING_ALAW:
      return SF_FORMAT_ALAW;
    case SOX_ENCODING_MS_ADPCM:
      return SF_FORMAT_MS_ADPCM;
    case SOX_ENCODING_IMA_ADPCM:
      return SF_FORMAT_IMA_ADPCM;
    case SOX_ENCODING_OKI_ADPCM:
      return SF_FORMAT_VOX_ADPCM;
    case SOX_ENCODING_UNSIGNED:
      if (size == 8)
        return SF_FORMAT_PCM_U8;
      else
        return 0;
    case SOX_ENCODING_SIGN2:
    case SOX_ENCODING_MP3:
    case SOX_ENCODING_VORBIS:
#ifdef HAVE_SNDFILE_1_0_12
    case SOX_ENCODING_FLAC:
      switch (size) {
      case 8:
        return SF_FORMAT_PCM_S8;
      case 16:
        return SF_FORMAT_PCM_16;
      case 24:
        return SF_FORMAT_PCM_24;
      case 32:
        return SF_FORMAT_PCM_32;
      default: /* invalid size */
        return 0;
      }
      break;
#endif
    case SOX_ENCODING_FLOAT:
      return SF_FORMAT_FLOAT;
    case SOX_ENCODING_GSM:
      return SF_FORMAT_GSM610;
    default: /* Bad encoding */
      return 0;
  }
}

static void start(sox_format_t * ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;
  int subtype = sndfile_format(ft->encoding.encoding, ft->encoding.bits_per_sample? ft->encoding.bits_per_sample : ft->signal.precision);
  sf->log_buffer_ptr = sf->log_buffer = lsx_malloc(LOG_MAX);
  sf->sf_info = (SF_INFO *)lsx_calloc(1, sizeof(SF_INFO));

  /* Copy format info */
  if (subtype) {
    if (strcmp(ft->filetype, "sndfile") == 0)
      sf->sf_info->format = name_to_format(ft->filename) | subtype;
    else
      sf->sf_info->format = name_to_format(ft->filetype) | subtype;
  }
  sf->sf_info->samplerate = ft->signal.rate;
  sf->sf_info->channels = ft->signal.channels;
  if (ft->signal.channels)
    sf->sf_info->frames = ft->length / ft->signal.channels;
}

/*
 * Open file in sndfile.
 */
static int startread(sox_format_t * ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;

  start(ft);

  /* We'd like to use sf_open_fd, but auto file typing has already
     invoked stdio buffering. */
  sf->sf_file = sf_open(ft->filename, SFM_READ, sf->sf_info);
  drain_log_buffer(ft);

  if (sf->sf_file == NULL) {
    memset(ft->sox_errstr, 0, sizeof(ft->sox_errstr));
    strncpy(ft->sox_errstr, sf_strerror(sf->sf_file), sizeof(ft->sox_errstr)-1);
    free(sf->sf_file);
    return SOX_EOF;
  }

  /* Copy format info */
  ft->encoding.encoding = sox_encoding_and_size((unsigned)sf->sf_info->format, &ft->encoding.bits_per_sample);
  ft->signal.channels = sf->sf_info->channels;
  ft->length = sf->sf_info->frames * sf->sf_info->channels;

  /* FIXME: it would be better if LSF were able to do this */
  if ((sf->sf_info->format & SF_FORMAT_TYPEMASK) == SF_FORMAT_RAW) {
    if (ft->signal.rate == 0) {
      sox_warn("'%s': sample rate not specified; trying 8kHz", ft->filename);
      ft->signal.rate = 8000;
    }
  }
  else ft->signal.rate = sf->sf_info->samplerate;

  return SOX_SUCCESS;
}

/*
 * Read up to len samples of type sox_sample_t from file into buf[].
 * Return number of samples read.
 */
static sox_size_t read_samples(sox_format_t * ft, sox_sample_t *buf, sox_size_t len)
{
  sndfile_t sf = (sndfile_t)ft->priv;

  /* FIXME: We assume int == sox_sample_t here */
  return (sox_size_t)sf_read_int(sf->sf_file, (int *)buf, (sf_count_t)len);
}

/*
 * Close file for libsndfile (this doesn't close the file handle)
 */
static int stopread(sox_format_t * ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;
  sf_stop(sf->sf_file);
  drain_log_buffer(ft);
  sf_close(sf->sf_file);
  return SOX_SUCCESS;
}

static int startwrite(sox_format_t * ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;

  start(ft);
  /* If output format is invalid, try to find a sensible default */
  if (!sf_format_check(sf->sf_info)) {
    SF_FORMAT_INFO format_info;
    int i, count;

    sf_command(sf->sf_file, SFC_GET_SIMPLE_FORMAT_COUNT, &count, sizeof(int));
    for (i = 0; i < count; i++) {
      format_info.format = i;
      sf_command(sf->sf_file, SFC_GET_SIMPLE_FORMAT, &format_info, sizeof(format_info));
      if ((format_info.format & SF_FORMAT_TYPEMASK) == (sf->sf_info->format & SF_FORMAT_TYPEMASK)) {
        sf->sf_info->format = format_info.format;
        /* FIXME: Print out exactly what we chose, needs sndfile ->
           sox encoding conversion functions */
        break;
      }
    }

    if (!sf_format_check(sf->sf_info)) {
      sox_fail("cannot find a usable output encoding");
      return SOX_EOF;
    }
    if ((sf->sf_info->format & SF_FORMAT_TYPEMASK) != SF_FORMAT_RAW)
      sox_warn("cannot use desired output encoding, choosing default");
  }

  sf->sf_file = sf_open(ft->filename, SFM_WRITE, sf->sf_info);
  drain_log_buffer(ft);

  if (sf->sf_file == NULL) {
    memset(ft->sox_errstr, 0, sizeof(ft->sox_errstr));
    strncpy(ft->sox_errstr, sf_strerror(sf->sf_file), sizeof(ft->sox_errstr)-1);
    free(sf->sf_file);
    return SOX_EOF;
  }

  return SOX_SUCCESS;
}

/*
 * Write len samples of type sox_sample_t from buf[] to file.
 * Return number of samples written.
 */
static sox_size_t write_samples(sox_format_t * ft, const sox_sample_t *buf, sox_size_t len)
{
  sndfile_t sf = (sndfile_t)ft->priv;

  /* FIXME: We assume int == sox_sample_t here */
  return (sox_size_t)sf_write_int(sf->sf_file, (int *)buf, (sf_count_t)len);
}

/*
 * Close file for libsndfile (this doesn't close the file handle)
 */
static int stopwrite(sox_format_t * ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;
  sf_stop(sf->sf_file);
  drain_log_buffer(ft);
  sf_close(sf->sf_file);
  return SOX_SUCCESS;
}

static int seek(sox_format_t * ft, sox_size_t offset)
{
  sndfile_t sf = (sndfile_t)ft->priv;
  sf_seek(sf->sf_file, (sf_count_t)(offset / ft->signal.channels), SEEK_CUR);
  return SOX_SUCCESS;
}

SOX_FORMAT_HANDLER(sndfile)
{
  /* Format file suffixes */
  /* For now, comment out formats built in to SoX */
  static char const * const names[] = {
    "sndfile", /* special type to force use of sndfile */
    /* "aif", */
    /* "wav", */
    /* "au", */
#ifdef HAVE_SNDFILE_1_0_12
    "caf",
#endif
    /* "flac", */
    /* "snd", */
    /* "svx", */
    "paf",
    "fap",
    /* "gsm", */
    /* "nist", */
    /* "ircam", */
    /* "sf", */
    /* "voc", */
    "w64",
    /* "raw", */
    "mat4",
    "mat5",
    "mat",
    "pvf",
    "sds",
    "sd2",
    /* "vox", */
    "xi",
    NULL
  };

  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 16, 24, 32, 8, 0,
    SOX_ENCODING_UNSIGNED, 8, 0,
    SOX_ENCODING_FLOAT, 32, 64, 0,
    SOX_ENCODING_ALAW, 8, 0,
    SOX_ENCODING_ULAW, 8, 0,
    SOX_ENCODING_IMA_ADPCM, 4, 0,
    SOX_ENCODING_MS_ADPCM, 4, 0,
    SOX_ENCODING_OKI_ADPCM, 4, 0,
    SOX_ENCODING_GSM, 0,
    0};

  static sox_format_handler_t const format = {
    SOX_LIB_VERSION_CODE,
    "Pseudo format to use libsndfile",
    names, SOX_FILE_NOSTDIO,
    startread, read_samples, stopread,
    startwrite, write_samples, stopwrite,
    seek, write_encodings, NULL
  };

  return &format;
}

#endif
