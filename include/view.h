#pragma once
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <glib.h>

typedef struct _GtkData {
  GtkWidget *sink_widget;
  GstElement *pipeline;
  GMainLoop *bus_loop;
  gint64 duration;
} GtkData;

GstElement *init_gtksink(GtkData *data);

void create_ui(GtkData* data);
