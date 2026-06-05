#include "batman.h"
#include "gst/gstclock.h"
#include "gst/gstelement.h"
#include "gst/gstobject.h"
#include <arpa/inet.h>
#include <batman.h>
#include <gio/gio.h>
#include <glib.h>
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <view.h>

GstElement *init_producer() {
  GstElement *pipeline, *source, *convert, *encoder, *rtp, *udpsink;

  // Create Pipeline Elements
  source = gst_element_factory_make("v4l2src", "source");
  convert = gst_element_factory_make("videoconvert", "convert");
  encoder = gst_element_factory_make("x264enc", "encoder");
  rtp = gst_element_factory_make("rtph264pay", "rtp");
  udpsink = gst_element_factory_make("dynudpsink", "udpsink");

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

GstElement *init_consumer(GstElement *sink) {
  GstElement *pipeline, *udpsrc, *convert, *parser, *decoder, *rtp;
  GstCaps *udp_caps;

  // Create pipeline elements
  convert = gst_element_factory_make("videoconvert", "convert");
  parser = gst_element_factory_make("h264parse", "parser");
  rtp = gst_element_factory_make("rtph264depay", "rtp");
  udpsrc = gst_element_factory_make("udpsrc", "udpsrc");
  decoder = gst_element_factory_make("decodebin", "decoder");

  // Create the emepty pipeline
  pipeline = gst_pipeline_new("test-pipeline");

  if (!pipeline || !udpsrc || !convert || !parser || !rtp || !decoder) {
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

static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer user_data) {
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

static gboolean on_video_request(GIOChannel *source, GIOCondition condition,
                                 gpointer data) {
  GstElement *sink = (GstElement *)data;
  int fd = g_io_channel_unix_get_fd(source);

  char target_ip[256];
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  // Read the incoming UDP packet bytes
  ssize_t bytes_read = recvfrom(fd, target_ip, sizeof(target_ip) - 1, 0,
                                (struct sockaddr *)&client_addr, &addr_len);
  if (bytes_read > 0) {
    target_ip[bytes_read] = '\0'; // Enforce safe string null-termination
    g_strstrip(target_ip);

    // Retrieve the active viewers hash table attached to this GstElement object
    GHashTable *active_viewers =
        (GHashTable *)g_object_get_data(G_OBJECT(sink), "active-viewers-table");

    if (active_viewers != NULL && strlen(target_ip) > 0) {
      if (!g_hash_table_contains(active_viewers, target_ip)) {
        g_signal_emit_by_name(sink, "add-client", target_ip, 5000, NULL);
        g_hash_table_add(active_viewers, g_strdup(target_ip));
      } else {
        g_signal_emit_by_name(sink, "remove-client", target_ip, 5000, NULL);
        g_hash_table_remove(active_viewers, target_ip);
      }
    }
  }
  return G_SOURCE_CONTINUE;
}

int main(int argc, char *argv[]) {

  GstElement *producer_pl, *consumer_pl, *consumer_sink;
  GstBus *producer_bus, *consumer_bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  GMainLoop *bus_loop;
  GtkData gtk_data;
  GError *error;
  GSocket *socket;
  GSocketAddress *sock_addr;
  GInetAddress *inet_addr;
  char *local_ip;

  // Initialize GStreamer & Gtk
  gst_init(&argc, &argv);
  gtk_init(&argc, &argv);

  // get local ip addr
  get_local_ip("bat0", local_ip);

  // set Gtk Data params
  memset(&gtk_data, 0, sizeof(gtk_data));
  gtk_data.bus_loop = bus_loop;
  gtk_data.pipeline = consumer_pl;
  consumer_sink = init_gtksink(&gtk_data);
  gtk_data.local_ip = local_ip;

  // Get the pipelines
  producer_pl = init_producer();
  consumer_pl = init_consumer(consumer_sink);

  // Create bus for producer and consumer
  producer_bus = gst_element_get_bus(producer_pl);
  consumer_bus = gst_element_get_bus(consumer_pl);

  // create the GUI
  create_ui(&gtk_data);

  // add gstreamer bus to producer and consumer
  gst_bus_add_watch(producer_bus, bus_callback, "Producer");
  gst_bus_add_watch(consumer_bus, bus_callback, "Consumer");

  // set-up video request listner
  socket = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM,
                        G_SOCKET_PROTOCOL_UDP, &error);
  if (error != NULL) {
    g_printerr("[Socket Error]: %s\n", error->message);
    g_clear_error(&error);
    return -1;
  }
  sock_addr = g_inet_socket_address_new_from_string("0.0.0.0", 6000);
  g_socket_bind(socket, sock_addr, TRUE, &error);
  if (error != NULL) {
    g_printerr("[Socket Error]: %s\n", error->message);
    g_clear_error(&error);
    return -1;
  }

  int fd = g_socket_get_fd(socket);
  GIOChannel *channel = g_io_channel_unix_new(fd);
  g_io_add_watch(channel, G_IO_IN, (GIOFunc)on_video_request, consumer_sink);

  // register function to check batman neighbours 
  g_timeout_add_seconds(5, (GSourceFunc)refresh_ui, &gtk_data);

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

  gtk_main();

  // Free resource
  gst_object_unref(producer_bus);
  gst_object_unref(consumer_bus);
  gst_object_unref(consumer_sink);
  gst_element_set_state(producer_pl, GST_STATE_NULL);
  gst_element_set_state(consumer_pl, GST_STATE_NULL);
  gst_object_unref(producer_pl);
  gst_object_unref(consumer_pl);
  gst_object_unref(bus_loop);

  return 0;
}
