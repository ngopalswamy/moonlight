/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
  *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gstreamer.h"

#include "limelight-common/Limelight.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DECODER_BUFFER_SIZE 92*1024

static char* decode_buffer;

static void decoder_renderer_setup(int width, int height, int redrawRate, void* context, int drFlags) {
  decode_buffer = malloc(DECODER_BUFFER_SIZE);
  if (decode_buffer == NULL) {
    fprintf(stderr, "Not enough memory\n");
    exit(1);
  }

  gstreamer_setup(width, height, redrawRate);
}

static int decoder_renderer_submit_decode_unit(PDECODE_UNIT decodeUnit) {
  if (decodeUnit->fullLength < DECODER_BUFFER_SIZE) {
    PLENTRY entry = decodeUnit->bufferList;
    int length = 0;
    while (entry != NULL) {
      memcpy(decode_buffer+length, entry->data, entry->length);
      length += entry->length;
      entry = entry->next;
    }

    gstreamer_decode(decode_buffer, length);
  } else {
    fprintf(stderr, "Video decode buffer too small");
    exit(1);
  }

  return DR_OK;
}

static void decoder_renderer_cleanup() {
  gstreamer_destroy();
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_gst = {
  .setup = decoder_renderer_setup,
  .cleanup = decoder_renderer_cleanup,
  .submitDecodeUnit = decoder_renderer_submit_decode_unit,
};
