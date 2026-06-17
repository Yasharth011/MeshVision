#include "batman.h"
#include "gst/gstcaps.h"
#include "gst/gstelement.h"
#include "gst/gstobject.h"
#include "gst/gstvalue.h"
#include "netlink/socket.h"
#include <arpa/inet.h>
#include <batman.h>
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>
#include <sys/socket.h>
#include <view.h>

GstElement *init_producer() {
  GstElement *pipeline, *source, *convert, *encoder, *rtp, *udpsink,
      *capsfilter;

  // Create Pipeline Elements
  source = gst_element_factory_make("v4l2src", "source");
  convert = gst_element_factory_make("videoconvert", "convert");
  encoder = gst_element_factory_make("x264enc", "encoder");
  rtp = gst_element_factory_make("rtph264pay", "rtp");
  udpsink = gst_element_factory_make("multiudpsink", "udpsink");
  capsfilter = gst_element_factory_make("capsfilter", "capsfilter");

  // Create the empty pipeline
  pipeline = gst_pipeline_new("producer-pipeline");

  if (!pipeline || !source || !capsfilter || !udpsink || !convert || !encoder ||
      !rtp) {
    g_printerr("Not all elements could be created. \n");
    return NULL;
  }

  // Build the pipeline
  gst_bin_add_many(GST_BIN(pipeline), source, capsfilter, udpsink, convert,
                   encoder, rtp, NULL);
  if (!gst_element_link_many(source, capsfilter, convert, encoder, rtp, udpsink,
                             NULL)) {
    g_printerr("Elements could not be linked");
    return NULL;
  }

  // set props
  g_object_set(encoder, "bitrate", 500, "tune", 4, NULL);
  GstCaps *caps = gst_caps_new_simple(
      "video/x-raw", "fromat", G_TYPE_STRING, "YUY2", "width", G_TYPE_INT, 640,
      "height", G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
  g_object_set(capsfilter, "caps", caps, NULL);

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

static gboolean on_video_request(GSocket *source, GIOCondition condition,
                                 gpointer data) {
  GstElement *sink = (GstElement *)data;
  GError *error = NULL;
  char target_ip[256] = {0};
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  // Read the incoming UDP packet bytes
  gssize bytes_read =
      g_socket_receive(source, target_ip, sizeof(target_ip) - 1, NULL, &error);
  if (error != NULL) {
    g_printerr("[Socket] Error: %s\n", error->message);
    g_clear_error(&error);
    return G_SOURCE_CONTINUE;
  }

  if (bytes_read > 0) {
    target_ip[bytes_read] = '\0'; // Enforce safe string null-termination
    g_strstrip(target_ip);

    // Validate the payload for ip format
    size_t ip_len = strlen(target_ip);
    if (ip_len < 7 || ip_len > 15 || strchr(target_ip, '.') == NULL) {
      g_printerr("[Warning]: Discarded malformed string data: '%s'\n",
                 target_ip);
      return G_SOURCE_CONTINUE;
    }

    // Retrieve the active viewers hash table attached to this GstElement object
    GHashTable *active_viewers =
        (GHashTable *)g_object_get_data(G_OBJECT(sink), "active-viewers");

    if (active_viewers != NULL) {
      if (!g_hash_table_contains(active_viewers, target_ip)) {
        g_signal_emit_by_name(sink, "add", target_ip, 5000, NULL);
        g_hash_table_add(active_viewers, g_strdup(target_ip));
      } else {
        g_signal_emit_by_name(sink, "remove", target_ip, 5000, NULL);
        g_hash_table_remove(active_viewers, target_ip);
      }
    }
  }
  return G_SOURCE_CONTINUE;
}

static gboolean glib_netlink_handler(GIOChannel *source, GIOCondition condition,
                                     gpointer user_data) {
  struct nl_sock *nl_socket = (struct nl_sock *)user_data;

  if (condition & G_IO_IN) {
    int bytes_processed = nl_recvmsgs_default(nl_socket);
    if (bytes_processed < 0) {
      g_printerr(
          "[Netlink Error] Failed to process incoming kernel messages.\n");
    }
  }
  return G_SOURCE_CONTINUE;
}

int main(int argc, char *argv[]) {

  GstElement *producer_pl, *consumer_pl, *consumer_sink, *udp_sink;
  GstBus *producer_bus, *consumer_bus;
  GHashTable *active_viewers;
  GstStateChangeReturn ret;
  GtkData gtk_data;
  GError *error = NULL;
  GSocket *socket;
  GSource *socket_source;
  GSocketAddress *socket_addr;
  struct nl_sock *bat_socket;
  GIOChannel *bat_channel;
  char local_ip[64] = {0};
  int neighbor_count = 0;
  MeshNeighbor neighbors[MESH_MAX_NEIGHBORS];

  // Initialize GStreamer & Gtk
  gst_init(&argc, &argv);
  gtk_init(&argc, &argv);

  // get local ip addr
  if (get_local_ip("bat0:avahi", local_ip)) {
    g_printerr("Local IP of interface bat0:avahi : %s",local_ip);
  } else if(get_local_ip("bat0", local_ip)) {
    g_printerr("Local IP of interface bat0 : %s",local_ip);
  } else {
    g_printerr("Error: Could not get local ip\n");
    return -1;
  }

  // set Gtk Data params
  memset(&gtk_data, 0, sizeof(gtk_data));

  gtk_data.local_ip = local_ip;

  // Get the pipelines
  producer_pl = init_producer();

  consumer_sink = init_gtksink(&gtk_data);
  consumer_pl = init_consumer(consumer_sink);

  // Get producer udp sink
  udp_sink = gst_bin_get_by_name(GST_BIN(producer_pl), "udpsink");
  active_viewers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  g_object_set_data_full(G_OBJECT(udp_sink), "active-viewers", active_viewers,
                         (GDestroyNotify)g_hash_table_destroy);

  gtk_data.pipeline = consumer_pl;

  // Create bus for producer and consumer
  producer_bus = gst_element_get_bus(producer_pl);
  consumer_bus = gst_element_get_bus(consumer_pl);

  // create the GUI
  create_ui(&gtk_data);

  // initialize the GUI buttons
  neighbor_count = fetch_mesh_neighbors(neighbors);
  for (int i = 0; i < neighbor_count; i++) {
    add_button(&gtk_data, neighbors[i].ip);
  }

  // add gstreamer bus to producer and consumer
  gst_bus_add_watch(producer_bus, bus_callback, "Producer");
  gst_bus_add_watch(consumer_bus, bus_callback, "Consumer");

  /* set-up video request listner */
  socket = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM,
                        G_SOCKET_PROTOCOL_UDP, &error);
  if (error != NULL) {
    g_printerr("[Socket Error]: %s\n", error->message);
    g_clear_error(&error);
    return -1;
  }
  socket_addr = g_inet_socket_address_new_from_string("0.0.0.0", 6000); 
  g_socket_bind(socket, socket_addr, TRUE, &error);
  if(error!=NULL){
	  g_printerr("[Socket Error] %s\n",error->message);
	  g_clear_error(&error);
	  return -1;
  }
  socket_source = g_socket_create_source(socket, G_IO_IN, NULL);
  g_source_set_callback(socket_source, (GSourceFunc)on_video_request, udp_sink,
                        NULL);
  g_source_attach(socket_source, g_main_context_default());

  /* set-up new batman nodes listener */
  bat_socket = nl_socket_alloc();
  genl_connect(bat_socket);
  int family_id = genl_ctrl_resolve(bat_socket, "batadv");
  int group_id = genl_ctrl_resolve_grp(bat_socket, "batadv", "tq_changes");
  nl_socket_add_membership(bat_socket, group_id);
  nl_socket_modify_cb(bat_socket, NL_CB_VALID, NL_CB_CUSTOM, on_new_bat_node,
                      &gtk_data);
  int bat_fd = nl_socket_get_fd(bat_socket);
  bat_channel = g_io_channel_unix_new(bat_fd);
  g_io_add_watch(bat_channel, G_IO_IN, (GIOFunc)glib_netlink_handler,
                 bat_socket);

  /* Start Playing */
  ret = gst_element_set_state(producer_pl, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Error: Unable to set the pipeline to the playing state");
    gst_object_unref(producer_pl);
    return -1;
  }
  ret = gst_element_set_state(consumer_pl, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Error: Unable to set the pipeline to the playing state");
    gst_object_unref(consumer_pl);
    return -1;
  }

  gtk_main();

  /* Free resource */
  gst_object_unref(producer_bus);
  gst_object_unref(consumer_bus);
  gst_element_set_state(producer_pl, GST_STATE_NULL);
  gst_element_set_state(consumer_pl, GST_STATE_NULL);
  gst_object_unref(producer_pl);
  gst_object_unref(consumer_pl);
  g_object_unref(socket);
  g_source_unref(socket_source);
  g_object_unref(socket_addr);
  nl_socket_free(bat_socket);
  g_io_channel_unref(bat_channel);

  return 0;
}
