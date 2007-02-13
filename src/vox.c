/*
 * SOX file format handler for Dialogic/Oki ADPCM VOX files.
 *
 * Copyright 1991-2004 Tony Seebregts And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for any
 * purpose.  This copyright notice must be maintained.
 *
 * Tony Seebregts And Sundry Contributors are not responsible for the
 * consequences of using this software.
 */

#include "st_i.h"
#include "adpcms.h"

/* .vox doesn't need any private state over and above adpcm_io_t, so
   just have simple wrappers that pass it on directly. */

static int vox_start(ft_t ft)
{
  return st_adpcm_oki_start(ft, (adpcm_io_t)ft->priv);
}

static int ima_start(ft_t ft)
{
  return st_adpcm_ima_start(ft, (adpcm_io_t)ft->priv);
}

static st_size_t read(ft_t ft, st_sample_t *buffer, st_size_t len)
{
  return st_adpcm_read(ft, (adpcm_io_t)ft->priv, buffer, len);
}

static int stopread(ft_t ft)
{
  return st_adpcm_stopread(ft, (adpcm_io_t)ft->priv);
}

static st_size_t write(ft_t ft, const st_sample_t *buffer, st_size_t length)
{
  return st_adpcm_write(ft, (adpcm_io_t)ft->priv, buffer, length);
}

static int stopwrite(ft_t ft)
{
  return st_adpcm_stopwrite(ft, (adpcm_io_t)ft->priv);
}

const st_format_t *st_vox_format_fn(void)
{
  static char const * names[] = {"vox", NULL};
  static st_format_t driver = {
    names, NULL, 0,
    vox_start,
    read,
    stopread,
    vox_start,
    write,
    stopwrite,
    st_format_nothing_seek
  };
  return &driver;
}

const st_format_t *st_ima_format_fn(void)
{
  static char const * names[] = {"ima", NULL};
  static st_format_t driver = {
    names, NULL, 0,
    ima_start,
    read,
    stopread,
    ima_start,
    write,
    stopwrite,
    st_format_nothing_seek
  };
  return &driver;
}
