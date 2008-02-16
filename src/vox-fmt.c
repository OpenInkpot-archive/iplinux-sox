/*
 * SOX file format handler for Dialogic/Oki ADPCM VOX files.
 *
 * Copyright 1991-2007 Tony Seebregts And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for any
 * purpose.  This copyright notice must be maintained.
 *
 * Tony Seebregts And Sundry Contributors are not responsible for the
 * consequences of using this software.
 */

#include "sox_i.h"
#include "vox.h"
 
const sox_format_handler_t *sox_vox_format_fn(void);

const sox_format_handler_t *sox_vox_format_fn(void)
{
  static char const * names[] = {"vox", NULL};
  static sox_format_handler_t handler = {
    names, 0,
    sox_vox_start,
    sox_vox_read,
    sox_vox_stopread,
    sox_vox_start,
    sox_vox_write,
    sox_vox_stopwrite,
    NULL
  };
  return &handler;
}
