/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Torben Hansing
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

#include "limelight-common/Limelight.h"

#include <stdbool.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/gstmemory.h>

typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *source;
    GstElement *parser;
    GstElement *decoder;
    GstElement *sink;
} CustomData;

static CustomData data;

bool gstreamer_init() {

    /* Initialize GStreamer */
    gst_init(0, NULL);

    /* Create the elements */
    data.source = gst_element_factory_make("appsrc", "video_source");
    data.parser = gst_element_factory_make("h264parse", "video_parser");
    data.decoder = gst_element_factory_make("avdec_h264", "video_decoder"); //TODO: How to select decoder dynamically?!
    data.sink = gst_element_factory_make("glimagesink", "video_sink");

    /* Create the pipeline */
    data.pipeline = gst_pipeline_new("moonlight-embedded");

    if(!data.source || !data.parser || !data.decoder || !data.sink || !data.pipeline) {
        g_printerr("Error creating Gstreamer elements");
        return FALSE;
    }
    gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.parser, data.decoder, data.sink, NULL);
    /* Link all elements except for the source*/
    if(!gst_element_link_many(data.parser, data.decoder, data.sink, NULL)) {
        g_printerr("Error linking GStreamer elements\n");
        gst_object_unref(data.pipeline);
        return FALSE;
    }
    return true;
}

void decoder_renderer_setup(int width, int height, int redrawRate, void* context, int drFlags) {
    /* Configure the appsrc */
    GstCaps *video_caps = gst_caps_new_simple("video/x-h264",
                                              "stream-format", G_TYPE_STRING, "(string)byte-stream",
                                              "widht", G_TYPE_INT, width,
                                              "height", G_TYPE_INT, height,
                                              "framerate", GST_TYPE_FRACTION, redrawRate, 1,
                                              NULL);
    g_object_set(data.source, "caps", video_caps, NULL);
    gst_caps_unref(video_caps);

    /* Link the source*/
    if(!gst_element_link(data.source, data.parser)) {
        g_printerr("Error linking GStreamer source\n");
        gst_object_unref(data.pipeline);
        return;
    }

    /* Start the pipeline */
    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
}

void decoder_renderer_cleanup() {
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
}

int decoder_renderer_submit_decode_unit(PDECODE_UNIT decodeUnit) {
    GstBuffer *buffer;
    GstFlowReturn flowReturn;
    GstMapInfo info;
    gsize buffer_idx = 0;

    /* Create an empty buffer */
    buffer = gst_buffer_new_and_alloc((gsize) decodeUnit->fullLength);
    buffer = gst_buffer_make_writable(buffer);
    PLENTRY entry = decodeUnit->bufferList;
    while (entry != NULL) {
        gst_buffer_fill(buffer, buffer_idx, entry->data, (gsize)entry->length);
        buffer_idx += entry->length;
        entry = entry->next;
    }
    g_signal_emit_by_name(data.source, "push-buffer", buffer, &flowReturn);
    gst_buffer_unmap(buffer, &info);
    if (flowReturn != GST_FLOW_OK) {
        gst_buffer_unref(buffer);
        return flowReturn;
    }
    gst_buffer_unref(buffer);
    return DR_OK;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_gstreamer = {
    .setup = decoder_renderer_setup,
    .cleanup = decoder_renderer_cleanup,
    .submitDecodeUnit = decoder_renderer_submit_decode_unit,
};
