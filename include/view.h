#pragma once
#include <glib.h>
#include <gst/gst.h>
#include <gtk/gtk.h>

typedef struct _GtkData {
  GtkWidget *sink_widget;
  GtkWidget *controls_widget;
  GstElement *pipeline;
  const gchar *local_ip;
} GtkData;

GstElement *init_gtksink(GtkData *data);

void create_ui(GtkData *data);

void add_button(GtkData *data, char dest_ip[64]);
