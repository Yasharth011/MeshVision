#include "glib.h"
#include "gst/gstbus.h"
#include "gst/gstelement.h"
#include "gst/gstutils.h"
#include <gst/gst.h>

GstElement *init_producer() {
  GstElement *pipeline, *source, *convert, *encoder, *rtp, *udpsink;

  // Create Pipeline Elements
  source = gst_element_factory_make("v4l2src", "source");
  convert = gst_element_factory_make("videoconvert", "convert");
  encoder = gst_element_factory_make("x264enc", "encoder");
  rtp = gst_element_factory_make("rtph264pay", "rtp");
  udpsink = gst_element_factory_make("udpsink", "udpsink");

  // Create the empty pipeline
  pipeline = gst_pipeline_new("producer-pipeline");

  if (!pipeline || !source || !udpsink || !convert || !encoder || !rtp) {
    g_printerr("Not all elements could be created. \n");
    return NULL;
  }

  // Build the pipeline
  gst_bin_add_many(GST_BIN(pipeline), source, udpsink, convert, encoder, rtp,
                   NULL);
  if (!gst_element_link_many(source, convert, encoder, rtp, udpsink, NULL)) {
    g_printerr("Elements could not be linked");
    return NULL;
  }

  // set props
  g_object_set(encoder, "bitrate", 500, "tune", 4, NULL);
  g_object_set(udpsink, "host", "127.0.1.255", "port", 5000, NULL);

  return pipeline;
}

static void pad_added_handler(GstElement *element, GstPad *pad, gpointer data) {
  GstElement *convert = (GstElement *)data;
  GstPad *sinkpad;

  sinkpad = gst_element_get_static_pad(convert, "sink");
  if (!gst_pad_is_linked(sinkpad)) {
    if (gst_pad_link(pad, sinkpad) == GST_PAD_LINK_OK)
      g_print("Linked decodebin to videoconvert");
    else
      g_printerr("Failed to link decodebin");
  }
  gst_object_unref(sinkpad);
}

GstElement *init_consumer() {
  GstElement *pipeline, *udpsrc, *sink, *convert, *parser, *decoder, *rtp;
  GstCaps *udp_caps;

  // Create pipeline elements
  sink = gst_element_factory_make("autovideosink", "sink");
  convert = gst_element_factory_make("videoconvert", "convert");
  parser = gst_element_factory_make("h264parse", "parser");
  rtp = gst_element_factory_make("rtph264depay", "rtp");
  udpsrc = gst_element_factory_make("udpsrc", "udpsrc");
  decoder = gst_element_factory_make("decodebin", "decoder");

  // Create the emepty pipeline
  pipeline = gst_pipeline_new("test-pipeline");

  if (!pipeline || !udpsrc || !sink || !convert || !parser || !rtp ||
      !decoder) {
    g_printerr("Not all elements could be created. \n");
    return NULL;
  }

  // set caps & props
  udp_caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING,
                                 "video", "clock-rate", G_TYPE_INT, 90000,
                                 "encoding-name", G_TYPE_STRING, "H264",
                                 "payload", G_TYPE_INT, 96, NULL);
  g_object_set(udpsrc, "port", 5000, "caps", udp_caps, NULL);
  gst_caps_unref(udp_caps);

  // build the pipeline
  gst_bin_add_many(GST_BIN(pipeline), sink, udpsrc, convert, parser, rtp,
                   decoder, NULL);
  if (!gst_element_link_many(udpsrc, rtp, parser, decoder, NULL) ||
      !gst_element_link(convert, sink)) {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline);
    return NULL;
  }
  g_signal_connect(decoder, "pad-added", G_CALLBACK(pad_added_handler),
                   convert);

  return pipeline;
}

static gboolean bus_callback(GstBus *bus, GstMessage *msg,
                                gpointer user_data) {
  const gchar *pipeline_name = (const gchar *)user_data;

  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_ERROR: {
    GError *err;
    gchar *debug;
    gst_message_parse_error(msg, &err, &debug);
    g_printerr("[%s Error] From element %s: %s\n", pipeline_name,
               GST_OBJECT_NAME(msg->src), err->message);
    g_error_free(err);
    g_free(debug);
    break;
  }
  case GST_MESSAGE_EOS:
    g_print("[%s] End of stream reached.\n", pipeline_name);
    break;
  default:
    break;
  }
  return TRUE;
}

int main(int argc, char *argv[]) {

  GstElement *producer_pl, *consumer_pl;
  GstBus *producer_bus, *consumer_bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  GMainLoop *bus_loop;

  // Initialize GStreamer
  gst_init(&argc, &argv);

  // Get the pipelines
  producer_pl = init_producer();
  consumer_pl = init_consumer();

  // Start Playing 
  ret = gst_element_set_state(producer_pl, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state");
    gst_object_unref(producer_pl);
    return -1;
  }
  ret = gst_element_set_state(consumer_pl, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state");
    gst_object_unref(consumer_pl);
    return -1;
  }

  // Create bus for producer and consumer
  producer_bus = gst_element_get_bus(producer_pl);
  consumer_bus = gst_element_get_bus(consumer_pl);

  gst_bus_add_watch(producer_bus, bus_callback, "Producer");
  gst_bus_add_watch(consumer_bus, bus_callback, "Consumer");

  // error event manager loop
  bus_loop = g_main_loop_new(NULL, false);
  g_main_loop_run(bus_loop);

  // Free resource
  gst_object_unref(producer_bus);
  gst_object_unref(consumer_bus);
  gst_element_set_state(producer_pl, GST_STATE_NULL);
  gst_element_set_state(consumer_pl, GST_STATE_NULL);
  gst_object_unref(producer_pl);
  gst_object_unref(consumer_pl);

  return 0;
}
