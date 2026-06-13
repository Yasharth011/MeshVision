#include <batman.h>
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gtk/gtk.h>
#include <view.h>

GstElement *init_gtksink(GtkData *data) {
  GstElement *videosink, *gtkglsink;
  GtkWidget *extracted_widget = NULL;

  videosink = gst_element_factory_make("glsinkbin", "videosink");
  gtkglsink = gst_element_factory_make("gtkglsink", "gtkglsink");

  if (gtkglsink != NULL && videosink != NULL) {
    g_print("Successfully created GTK4 GL Sink\n");
    g_object_set(videosink, "sink", gtkglsink, NULL);
    g_object_get(gtkglsink, "widget", &extracted_widget, NULL);
    data->sink_widget = extracted_widget;
  } else {
    g_printerr(
        "Could not create gtkglsink, falling back to software gtksink\n");
    if (videosink)
      gst_object_unref(videosink);
    if (gtkglsink)
      gst_object_unref(gtkglsink);

    videosink = gst_element_factory_make("gtksink", "videosink");
    g_object_get(videosink, "widget", &extracted_widget, NULL);
    data->sink_widget = extracted_widget;
  }

  return videosink;
}

/* This function is called when the main window is closed */
static void delete_event_cb(GtkWidget *widget, GdkEvent *event, GtkData *data) {
  gst_element_set_state(data->pipeline, GST_STATE_NULL);
  gtk_main_quit();
}

void create_ui(GtkData *data) {
  GtkWidget *main_window;
  GtkWidget *main_box;
  GtkWidget *main_hbox, *controls;
  GtkWidget *button;

  main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(main_window), "delete-event",
                   G_CALLBACK(delete_event_cb), data);

  main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start(GTK_BOX(main_hbox), data->sink_widget, TRUE, TRUE, 0);

  controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  data->controls_widget = controls;
  gtk_widget_set_size_request(data->controls_widget, 180, 80);

  main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(main_box), main_hbox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), controls, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(main_window), main_box);
  gtk_window_set_default_size(GTK_WINDOW(main_window), 800, 480);

  gtk_widget_show_all(main_window);
}

static void on_button_click(GtkButton *button, GtkData *data) {
  // reqeuest vedio from node
  GError *error = NULL;

  GSocket *socket = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM,
                                 G_SOCKET_PROTOCOL_UDP, &error);

  const char *dest_ip =
      (const char *)g_object_get_data(G_OBJECT(button), "node-ip");

  GSocketAddress *dest_addr =
      g_inet_socket_address_new_from_string(dest_ip, 6000);

  // Transmit the command string instantly over the mesh link
  g_socket_send_to(socket, dest_addr, data->local_ip, strlen(data->local_ip),
                   NULL, &error);
  if (error != NULL) {
    g_printerr("[Socket Error]: %s\n", error->message);
    g_clear_error(&error);
  }

  g_object_unref(dest_addr);
  g_object_unref(socket);
}

void add_button(GtkData *data, char dest_ip[64]) {
  GtkWidget *button;
  button = gtk_button_new();

  gtk_button_set_label(GTK_BUTTON(button), dest_ip);

  g_object_set_data_full(G_OBJECT(button), "node-ip", g_strdup(dest_ip),
                         g_free);

  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_button_click),
                   data);

  gtk_box_pack_start(GTK_BOX(data->controls_widget), button, FALSE, FALSE, 2);

  gtk_widget_show(button);
}
