#include "glib.h"
#include "gst/gstcaps.h"
#include "gst/gstelement.h"
#include "gst/gstobject.h"
#include "gst/gstpad.h"
#include <gst/gst.h>

static void pad_added_handler(GstElement *element, GstPad* pad, gpointer data){ 
	GstElement *convert = (GstElement *)data; 
	GstPad *sinkpad; 

	sinkpad = gst_element_get_static_pad(convert, "sink"); 
	if(!gst_pad_is_linked(sinkpad)){
		if(gst_pad_link(pad, sinkpad) == GST_PAD_LINK_OK)
			g_print("Linked decodebin to videoconvert");
		else
		 	g_printerr("Failed to link decodebin");
	}
	gst_object_unref(sinkpad);
}

int main(int argc, char *argv[]) {

  GstElement *pipeline, *udpsrc, *sink, *convert, *parser, *decoder, *rtp;
  GstBus *bus;
  GstMessage *msg;
  GstCaps *udp_caps;
  GstStateChangeReturn ret;

  // Initialize GStreamer
  gst_init(&argc, &argv);

  // Create Pipeline Elements
  sink = gst_element_factory_make("autovideosink", "sink");
  convert = gst_element_factory_make("videoconvert", "convert");
  parser = gst_element_factory_make("h264parse", "parser");
  rtp = gst_element_factory_make("rtph264depay", "rtp");
  udpsrc = gst_element_factory_make("udpsrc", "udpsrc");
  decoder = gst_element_factory_make("decodebin", "decoder");

  // Create the emepty pipeline
  pipeline = gst_pipeline_new("test-pipeline");

  if (!pipeline || !udpsrc || !sink || !convert || !parser || !rtp || !decoder) {
    g_printerr("Not all elements could be created. \n");
    return -1;
  }

  // Build empty pipeline

  // set udp caps
  udp_caps = gst_caps_new_simple("application/x-rtp",
                                 "media", G_TYPE_STRING, "video",
                                 "clock-rate", G_TYPE_INT, 90000,
                                 "encoding-name", G_TYPE_STRING, "H264",
                                 "payload", G_TYPE_INT, 96,
                                 NULL);
  // set properties for elements
  g_object_set(udpsrc, "port", 5000, "caps", udp_caps, NULL);
  gst_caps_unref(udp_caps);

  // build pipeline bin and link elements
  gst_bin_add_many(GST_BIN(pipeline), sink, udpsrc, convert, parser, rtp, decoder, NULL);
  if (gst_element_link(udpsrc, rtp) != TRUE ||
      gst_element_link(rtp, parser) != TRUE ||
      gst_element_link(parser, decoder) != TRUE ||
      gst_element_link(convert, sink) != TRUE) {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }
  g_signal_connect(decoder, "pad-added", G_CALLBACK(pad_added_handler), convert);

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
