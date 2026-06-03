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
  gst_element_set_state(data->pipeline, GST_STATE_READY);
  g_main_loop_quit(data->bus_loop);
}

void create_ui(GtkData *data) {
  GtkWidget *main_window;
  GtkWidget *main_box;

  main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(main_window), "delete-event",
                   G_CALLBACK(delete_event_cb), data);

  main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(main_box), data->sink_widget, TRUE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(main_window), main_box);
  gtk_window_set_default_size(GTK_WINDOW(main_window), 640, 480);

  gtk_widget_show_all(main_window);
}
