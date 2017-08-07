/*
    This file is part of darktable,
    copyright (c) 2011 johannes hanika.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

// writes buffers as digital negative (dng) raw images

// #include "common/darktable.h"
// #include "common/exif.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define II 1
#define MM 2
#define BYTE 1
#define ASCII 2
#define SHORT 3
#define LONG 4
#define RATIONAL 5
#define SRATIONAL 10

static inline void dt_imageio_dng_write_buf(uint8_t *buf, int adr, int val)
{
  buf[adr + 3] = val & 0xff;
  buf[adr + 2] = (val >> 8) & 0xff;
  buf[adr + 1] = (val >> 16) & 0xff;
  buf[adr] = val >> 24;
}

static inline uint8_t *dt_imageio_dng_make_tag(
    uint16_t tag, uint16_t type, uint32_t lng, uint32_t fld,
    uint8_t *b, uint8_t *cnt)
{
  dt_imageio_dng_write_buf(b, 0, (tag << 16) | type);
  dt_imageio_dng_write_buf(b, 4, lng);
  dt_imageio_dng_write_buf(b, 8, fld);
  *cnt = *cnt + 1;
  return b + 12;
}

static inline void dt_imageio_dng_convert_rational(float f, int32_t *num, int32_t *den)
{
  int32_t sign = 1;
  if(f < 0)
  {
    sign = -1;
    f = -f;
  }
  float mult = 1.0f;
  while(f * mult - (int)(f * mult + 0.00005f) > 0.0001f) mult++;
  *den = mult;
  *num = (int)(*den * f);
  *num *= sign;
}

static inline void dt_imageio_dng_write_tiff_header(
    FILE *fp, uint32_t xs, uint32_t ys, float Tv, float Av,
    float f, float iso, uint32_t filter,
    const uint8_t xtrans[6][6],
    const uint16_t black,
    const uint16_t white)
{
  const uint32_t channels = 1;
  uint8_t *b /*, *offs1, *offs2*/;
  // uint32_t exif_offs;
  uint8_t buf[1024];
  uint8_t cnt = 0;

  memset(buf, 0, sizeof(buf));
  /* TIFF file header.  */
  buf[0] = 0x4d;
  buf[1] = 0x4d;
  buf[3] = 42;
  buf[7] = 10;

  b = buf + 12;
  b = dt_imageio_dng_make_tag(254, LONG, 1, 0, b, &cnt);           /* New subfile type.  */
  b = dt_imageio_dng_make_tag(256, SHORT, 1, (xs << 16), b, &cnt); /* Image width.  */
  b = dt_imageio_dng_make_tag(257, SHORT, 1, (ys << 16), b, &cnt); /* Image length.  */
  // b = dt_imageio_dng_make_tag(  258, SHORT, channels, 506, b, &cnt ); /* Bits per sample.  */
  // b = dt_imageio_dng_make_tag(258, SHORT, 1, 32 << 16, b, &cnt); /* Bits per sample.  */
  b = dt_imageio_dng_make_tag(258, SHORT, 1, 16 << 16, b, &cnt); /* Bits per sample.  */
  // bits per sample: 32-bit float / 16-bit uint
  // buf[507] = buf[509] = buf[511] = 32;
  b = dt_imageio_dng_make_tag(259, SHORT, 1, (1 << 16), b, &cnt); /* Compression.  */
  b = dt_imageio_dng_make_tag(262, SHORT, 1, 32803 << 16, b, &cnt);
      /* cfa */ // 34892, b, &cnt ); // linear raw /* Photo interp.  */
  // b = dt_imageio_dng_make_tag(  271, ASCII, 8, 494, b, &cnt); // maker, needed for dcraw
  // b = dt_imageio_dng_make_tag(  272, ASCII, 9, 484, b, &cnt); // model
  //   offs2 = b + 8;
  b = dt_imageio_dng_make_tag(273, LONG, 1, 584, b, &cnt);             /* Strip offset.  */
  b = dt_imageio_dng_make_tag(274, SHORT, 1, 1 << 16, b, &cnt);        /* Orientation. */
  b = dt_imageio_dng_make_tag(277, SHORT, 1, channels << 16, b, &cnt); /* Samples per pixel.  */
  b = dt_imageio_dng_make_tag(278, SHORT, 1, (ys << 16), b, &cnt);     /* Rows per strip.  */
  // b = dt_imageio_dng_make_tag(279, LONG, 1, (ys * xs * channels * 4), b,
  b = dt_imageio_dng_make_tag(279, LONG, 1, (ys * xs * channels * 2), b,
                              &cnt);                              // 32 bits/channel /* Strip byte count.  */
  b = dt_imageio_dng_make_tag(284, SHORT, 1, (1 << 16), b, &cnt); /* Planar configuration.  */
  b = dt_imageio_dng_make_tag(339, SHORT, 1, (1 << 16), b,
                              &cnt); /* SampleFormat = 3 => ieee floating point, 1 => unsigned int */

  if(filter == 9u) // xtrans
    b = dt_imageio_dng_make_tag(33421, SHORT, 2, (6 << 16) | 6, b, &cnt); /* CFAREPEATEDPATTERNDIM */
  else
    b = dt_imageio_dng_make_tag(33421, SHORT, 2, (2 << 16) | 2, b, &cnt); /* CFAREPEATEDPATTERNDIM */

  uint32_t cfapattern = 0;
  switch(filter)
  {
    case 0x94949494:
      cfapattern = (0 << 24) | (1 << 16) | (1 << 8) | 2; // rggb
      break;
    case 0x49494949:
      cfapattern = (1 << 24) | (2 << 16) | (0 << 8) | 1; // gbrg
      break;
    case 0x61616161:
      cfapattern = (1 << 24) | (0 << 16) | (2 << 8) | 1; // grbg
      break;
    default:                                             // case 0x16161616:
      cfapattern = (2 << 24) | (1 << 16) | (1 << 8) | 0; // bggr
      break;
  }
  if(filter == 9u) // xtrans
    b = dt_imageio_dng_make_tag(33422, BYTE, 36, 340, b, &cnt); /* CFAPATTERN */
  else // bayer
    b = dt_imageio_dng_make_tag(33422, BYTE, 4, cfapattern, b, &cnt); /* CFAPATTERN */

  // b = dt_imageio_dng_make_tag(  306, ASCII, 20, 428, b, &cnt ); // DateTime
  //   offs1 = b + 8;// + 3;
  // b = dt_imageio_dng_make_tag(34665, LONG, 1, 264, b, &cnt); // exif ifd
  b = dt_imageio_dng_make_tag(50706, BYTE, 4, (1 << 24) | (2 << 16), b, &cnt); // DNG Version/backward version
  b = dt_imageio_dng_make_tag(50707, BYTE, 4, (1 << 24) | (1 << 16), b, &cnt);
  // uint32_t whitei = *(uint32_t *)&whitelevel;
  // b = dt_imageio_dng_make_tag(50717, LONG, 1, whitei, b, &cnt); // WhiteLevel in float, actually.
  b = dt_imageio_dng_make_tag(50713, SHORT, 2, (1<<16)|1, b, &cnt); // BlackLevelRepeatDim
  b = dt_imageio_dng_make_tag(50714, SHORT, 1, black<<16, b, &cnt); // BlackLevel
  b = dt_imageio_dng_make_tag(50717, SHORT, 1, white<<16, b, &cnt); // WhiteLevel
  // b = dt_imageio_dng_make_tag(50708, ASCII, 9, 484, b, &cnt); // unique camera model
  // b = dt_imageio_dng_make_tag(50721, SRATIONAL, 9, 328, b, &cnt); // ColorMatrix1 (XYZ->native cam)
  // b = dt_imageio_dng_make_tag(50728, RATIONAL, 3, 512, b, &cnt); // AsShotNeutral
  // b = dt_imageio_dng_make_tag(50729, RATIONAL, 2, 512, b, &cnt); // AsShotWhiteXY
  b = dt_imageio_dng_make_tag(0, 0, 0, 0, b, &cnt); /* Next IFD.  */
  buf[11] = cnt - 1; // write number of directory entries of this ifd

  // exif is written later, by exiv2:
  // printf("offset: %d\n", b - buf); // find out where we're writing data
  // apparently this doesn't need byteswap:
  memcpy(buf+340, xtrans, sizeof(uint8_t)*36);

  // dt_imageio_dng_write_buf(buf, offs2-buf, 584);
  int written = fwrite(buf, 1, 584, fp);
  if(written != 584) fprintf(stderr, "[dng_write_header] failed to write image header!\n");
}

static inline void dt_imageio_write_dng(
    const char *filename, const uint16_t *const pixel, const int wd,
    const int ht, void *exif, const int exif_len, const uint32_t filter,
    const uint8_t xtrans[6][6],
    const uint16_t blacklevel,
    const uint16_t whitelevel)
{
  FILE *f = fopen(filename, "wb");
  if(f)
  {
    dt_imageio_dng_write_tiff_header(f, wd, ht, 1.0f / 100.0f, 1.0f / 4.0f, 50.0f, 100.0f, filter, xtrans, blacklevel, whitelevel);
    int k = fwrite(pixel, sizeof(uint16_t), (size_t)wd*ht, f);
    if(k != wd*ht) fprintf(stderr, "[dng_write] error writing image data to %s\n", filename);
    fclose(f);
  }
}

// adapter for cr2hdr:
int save_dng(char *filename, struct raw_info *info)
{
  const uint8_t xtrans[6][6] = {{0}};
  dt_imageio_write_dng(filename, info->buffer, info->width,
      info->height, 0, 0, info->cfa_pattern, xtrans, info->black_level, info->white_level);
  return 0;
}
// linkage for dummy api:
void dng_set_framerate(int fpsx1000){}
void dng_set_thumbnail_size(int width, int height){}

void dng_set_framerate_rational(int nom, int denom){}
void dng_set_shutter(int nom, int denom){}
void dng_set_aperture(int nom, int denom){}
void dng_set_camname(char *str){}
void dng_set_camserial(char *str){}
void dng_set_description(char *str){}
void dng_set_lensmodel(char *str){}
void dng_set_focal(int nom, int denom){}
void dng_set_iso(int value){}
void dng_set_wbgain(int gain_r_n, int gain_r_d, int gain_g_n, int gain_g_d, int gain_b_n, int gain_b_d){}
void dng_set_datetime(char *datetime, char *subsectime){}


#undef II
#undef MM
#undef BYTE
#undef ASCII
#undef SHORT
#undef LONG
#undef RATIONAL
#undef SRATIONAL


// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.sh
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-spaces modified;
