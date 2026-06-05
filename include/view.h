#pragma once
#include <glib.h>
#include <gst/gst.h>
#include <gtk/gtk.h>

typedef struct _GtkData {
  GtkWidget *sink_widget;
  GtkWidget *controls_widget;
  GstElement *pipeline;
  GMainLoop *bus_loop;
  const gchar *local_ip;
} GtkData;

GstElement *init_gtksink(GtkData *data);

void create_ui(GtkData *data);

gboolean refresh_ui(GtkData *data);
