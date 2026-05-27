#include <gst/gst.h>

int main(int argc, char *argv[]) {

  GstElement *pipeline, *source, *udpsink, *convert, *enc, *rtp;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;

  // Initialize GStreamer
  gst_init(&argc, &argv);

  // Create Pipeline Elements
  source = gst_element_factory_make("v4l2src", "source");
  convert = gst_element_factory_make("videoconvert", "convert");
  enc = gst_element_factory_make("x264enc", "encoder");
  rtp = gst_element_factory_make("rtph264pay", "rtp");
  udpsink = gst_element_factory_make("udpsink", "udpsink");

  // Create the emepty pipeline
  pipeline = gst_pipeline_new("test-pipeline");

  if (!pipeline || !source || !udpsink || !convert || !enc || !rtp) {
    g_printerr("Not all elements could be created. \n");
    return -1;
  }

  // Build the pipeline
  gst_bin_add_many(GST_BIN(pipeline), source, udpsink, convert, enc, rtp, NULL);
  if (gst_element_link(source, convert) != TRUE ||
      gst_element_link(convert, enc) != TRUE ||
      gst_element_link(enc, rtp) != TRUE ||
      gst_element_link(rtp, udpsink) != TRUE) {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  // set properties for elements
  g_object_set(enc, "bitrate", 500, "tune", 4, NULL);
  g_object_set(udpsink, "host", "127.0.1.255","port", 5000, "tune", 4, NULL);

  /* Start Playing */
  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state");
    gst_object_unref(pipeline);
    return -1;
  }

  /* Wait until error or EOS */
  bus = gst_element_get_bus(pipeline);
  msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                   GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  // Parse Message
  if (msg != NULL) {
    GError *err;
    gchar *debug_info;
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &err, &debug_info);
      g_printerr("Error received from element %s: %s\n",
                 GST_OBJECT_NAME(msg->src), err->message);
      g_printerr("Debugging information: %s\n",
                 debug_info ? debug_info : "none");
      g_clear_error(&err);
      g_free(debug_info);
      break;
    case GST_MESSAGE_EOS:
      g_print("End-Of-Stream reached.\n");
      break;
    default:
      /* We should not reach here because we only asked for ERRORs and EOS */
      g_printerr("Unexpected message received.\n");
      break;
    }
    gst_message_unref(msg);
  }

  // Free resource
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}
