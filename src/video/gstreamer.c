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
#include <gst/video/gstvideodecoder.h>

typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *source;
    GstElement *parser;
    GstElement *decoder;
    GstElement *deinterlace;
    GstElement *sink;
} CustomData;

static CustomData data;

static void pad_added(GstElement *src, GstPad *pad, CustomData *data) {
    GstPad *sink_pad = gst_element_get_static_pad(data->sink, "sink");
    GstCaps *pad_caps;
    GstStructure *caps_struct;
    GstPadLinkReturn ret;
    const gchar *pad_type;

    pad_caps = gst_pad_query_caps(pad, NULL);
    caps_struct = gst_caps_get_structure(pad_caps, 0);
    pad_type = gst_structure_get_name(caps_struct);
    if (g_str_has_prefix (pad_type, "video/x-raw")) {
        ret = gst_pad_link (pad, sink_pad);
        if (GST_PAD_LINK_FAILED (ret)) {
            g_print ("\n\t[GStreamer] Linking of sink pad failed!\n");
        }
    }
    /* Unreference the new pad's caps, if we got them */
    if (pad_caps != NULL)
        gst_caps_unref (pad_caps);
    /* Unreference the sink pad */
    gst_object_unref (sink_pad);
}

bool gstreamer_init() {

    /* Initialize GStreamer */
    gst_init(0, NULL);

    /* Create the elements */
    data.source = gst_element_factory_make("appsrc", "video_source");
    data.decoder = gst_element_factory_make("decodebin", "video_decoder");
    data.sink = gst_element_factory_make("autovideosink", "video_sink");

    /* Create the pipeline */
    data.pipeline = gst_pipeline_new("moonlight-embedded");

    if(!data.source || !data.decoder || !data.sink || !data.pipeline) {
        g_printerr("\n\t[GStreamer] Error creating elements!\n");
        return FALSE;
    }
    gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.decoder, data.sink, NULL);

    /* Link the source*/
    if(!gst_element_link(data.source, data.decoder)) {
        g_printerr("\n\t[GStreamer] Error linking source -> decoder\n");
        gst_object_unref(data.pipeline);
        return FALSE;
    }

    /* Register callback on pad-added signal */
    g_signal_connect(data.decoder, "pad-added", G_CALLBACK(pad_added), &data);
    return true;
}

static void decoder_renderer_setup(int width, int height, int redrawRate, void* context, int drFlags) {
    /* Configure the appsrc */
    GstCaps *video_caps = gst_caps_new_simple("video/x-h264",
                                              "stream-format", G_TYPE_STRING, "byte-stream",
                                              "widht", G_TYPE_INT, width,
                                              "height", G_TYPE_INT, height,
                                              "framerate", GST_TYPE_FRACTION, redrawRate, 1,
                                              NULL);
    g_object_set(data.source, "caps", video_caps, NULL);
    gst_caps_unref(video_caps);

    /* Start the pipeline */
    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
}

static void decoder_renderer_cleanup() {
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
}

static int decoder_renderer_submit_decode_unit(PDECODE_UNIT decodeUnit) {
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
    gst_buffer_unref(buffer);
    if (flowReturn != GST_FLOW_OK) {
        //TODO solve problems
        return flowReturn;
    }
    return DR_OK;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_gstreamer = {
    .setup = decoder_renderer_setup,
    .cleanup = decoder_renderer_cleanup,
    .submitDecodeUnit = decoder_renderer_submit_decode_unit,
};
