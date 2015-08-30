/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
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

#include <stdbool.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
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
static int samples;

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

static GstFlowReturn new_sample_callback(GstAppSink *appsink, gpointer user_data) {
    samples++;
    return GST_FLOW_OK;
}

static GstAppSinkCallbacks gst_callbacks = {
    .eos = NULL,
    .new_preroll = NULL,
    .new_sample = new_sample_callback,
};

bool gstreamer_init(bool native) {
    /* Initialize GStreamer */
    gst_init(0, NULL);

    /* Create the elements */
    data.source = gst_element_factory_make("appsrc", "video_source");
    data.decoder = gst_element_factory_make("decodebin", "video_decoder");
    data.sink = gst_element_factory_make(native?"autovideosink":"appsink", "video_sink");

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
    
    if (!native)
        gst_app_sink_set_callbacks (GST_APP_SINK(data.sink), &gst_callbacks, NULL, NULL);

    return true;
}

void gstreamer_setup(int width, int height, int redrawRate) {

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

void gstreamer_destroy() {
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
}

int gstreamer_decode(unsigned char* indata, int inlen) {
    GstBuffer *buffer;
    GstFlowReturn flowReturn;
    GstMapInfo info;
    gsize buffer_idx = 0;

    /* Create an empty buffer */
    buffer = gst_buffer_new_and_alloc((gsize) inlen);
    buffer = gst_buffer_make_writable(buffer);
    gst_buffer_fill(buffer, buffer_idx, indata, (gsize)inlen);
    g_signal_emit_by_name(data.source, "push-buffer", buffer, &flowReturn);
    gst_buffer_unref(buffer);
    if (flowReturn != GST_FLOW_OK) {
        //TODO solve problems
        return flowReturn;
    }
    return 1;
}

GstSample* gstreamer_get_frame() {
    if (samples > 0) {
        samples--;
        return gst_app_sink_pull_sample(GST_APP_SINK(data.sink));
    } else
        return NULL;
}
