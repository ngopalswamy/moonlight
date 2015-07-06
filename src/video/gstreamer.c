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

#include <gst/gst.h>
#include <string.h>
#include <gst/gstmemory.h>

typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *source;
    GstElement *convert;
    GstElement *sink;
    guint64 frames_added;
    int framerate;
} CustomData;

static CustomData data;

bool video_gstreamer_init() {
    /* Create the shared data */
    memset(&data, 0, sizeof(data));

    /* Initialize GStreamer */
    gst_init(0, NULL);

    /* Create the pipeline elements */
    data.source = gst_element_factory_make("appsrc", "video_source");
    data.convert = gst_element_factory_make("ffmpegcolorspace", "video_csp");
    data.sink = gst_element_factory_make("autovideosink", "video_sink");

    /* Create the pipeline */
    data.pipeline = gst_pipeline_new("moonlight-embedded");

    /* check that all elements have been created */
    if(!data.pipeline || !data.source || !data.convert || !data.sink) {
        g_printerr("Error creating gstreamer elements.\n");
        return FALSE;
    }
    return TRUE;
}

void decoder_renderer_setup(int width, int height, int redrawRate, void* context, int drFlags) {
    data.framerate = redrawRate;
    /* Configure the appsrc */
    GstCaps *video_caps = gst_caps_new_simple("video/x-raw",
                            "format", G_TYPE_STRING, "h264",
                            "widht", G_TYPE_INT, width,
                            "height", G_TYPE_INT, height,
                            "framerate", GST_TYPE_FRACTION, redrawRate, 1,
                            NULL);
    g_object_set(data.source, "caps", video_caps, NULL);
    gst_caps_unref(video_caps);

    /* Link all elements */
    gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.convert, data.sink, NULL);
    if(!gst_element_link_many(data.source, data.convert, data.sink)) {
        g_printerr("Error linking GStreamer elements\n");
        gst_object_unref(data.pipeline);
        return;
    }

    /* Start the pipeline */
    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
}

void decoder_renderer_cleanup() {
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    gst_object_unref(data.source);
    gst_object_unref(data.convert);
    gst_object_unref(data.sink);
}

int decoder_renderer_submit_decode_unit(PDECODE_UNIT decodeUnit) {
    GstBuffer *buffer;
    GstFlowReturn ret;
    GstMapInfo info;

    PLENTRY entry = decodeUnit->bufferList;
    /* Create an empty buffer */
    buffer = gst_buffer_new_and_alloc(entry->length);

    while (entry != NULL) {
        GST_BUFFER_TIMESTAMP(buffer) = gst_util_uint64_scale(data.frames_added, GST_SECOND, data.framerate);
        GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(entry->length, GST_SECOND, data.framerate);
        gst_buffer_map(buffer, &info, GST_MAP_WRITE);
        for(int i=0; i < entry->length; i++) {
            info.data[i] = entry->data[i];
        }
        gst_buffer_unmap(buffer, &info);
        g_signal_emit_by_name(data.source, "push-buffer", buffer, &ret);
        if(ret != GST_FLOW_OK) {
            goto exit;
        }
        data.frames_added += entry->length;
        entry = entry->next;
    }
    gst_buffer_unref(buffer);
    return DR_OK;
    exit:
        gst_buffer_unref(buffer);
        return -1;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_gstreamer = {
    .setup = decoder_renderer_setup,
    .cleanup = decoder_renderer_cleanup,
    .submitDecodeUnit = decoder_renderer_submit_decode_unit,
};
