/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "tray.h"

static const char kIcon[] = "_MINTRAYR_ICON";
static const char kWatch[] = "_MINTRAYR_WATCH";

static int gWatchMode = 0;

typedef struct _minimize_callback_data_t {
  void *handle;
  minimize_callback_t callback;
} minimize_callback_data_t;

typedef struct _icon_data_t {
  void *handle;
  int clickCount;
  mouseevent_callback_t callback;
  GtkStatusIcon *statusIcon;
} icon_data_t;

typedef struct _find_data_t {
  char *title;
  void *rv;
} find_data_t;

#define XATOM(atom) const Atom atom = XInternAtom(xev->xany.display, #atom, 0)

/**
 * Helper: Gdk filter function to "watch" the window
 */
static
GdkFilterReturn filter_windows(XEvent *xev, GdkEvent* event, minimize_callback_data_t *data)
{
  XATOM(WM_DELETE_WINDOW);

  if (!xev) {
    return GDK_FILTER_CONTINUE;
  }

  switch (xev->type) {
    case MapNotify:
      data->callback(data->handle, 1);
      break;

    case UnmapNotify:
      if (gWatchMode & (1<<0)) {
        data->callback(data->handle, 0);
        return GDK_FILTER_REMOVE;
      }
      break;

    case ClientMessage:
      if (xev->xclient.data.l
          && (Atom)(xev->xclient.data.l[0]) == WM_DELETE_WINDOW
          && (gWatchMode & (1<<1))
      ) {
        data->callback(data->handle, 0);
        return GDK_FILTER_REMOVE;
      }
      break;

    default:
      break;
  }
  return GDK_FILTER_CONTINUE;
}

static void button_event(GtkStatusIcon* icon, GdkEventButton *event, icon_data_t *data)
{
  mouseevent_t mevent;

  mevent.button = event->button - 1;

  switch (event->type) {
    case GDK_BUTTON_RELEASE: // use release, so that we don't duplicate events
      mevent.clickCount = data->clickCount;
      data->clickCount = 1;
      break;
    case GDK_2BUTTON_PRESS:
      data->clickCount = 2;
      return;
    case GDK_3BUTTON_PRESS:
      data->clickCount = 3;
      return;
    default:
      return;
  }

  mevent.x = event->x + event->x_root;
  mevent.y = event->y + event->y_root;

  mevent.keys = 0;
#define HASSTATE(x,y) (event->state & x ? (1<<y) : 0)
  mevent.keys |= HASSTATE(GDK_CONTROL_MASK, 0);
  mevent.keys |= HASSTATE(GDK_MOD1_MASK, 1);
  mevent.keys |= HASSTATE(GDK_SHIFT_MASK, 2);
#undef HASSTATE
  data->callback(data->handle, &mevent);
}

static void find_window(GdkWindow *window, find_data_t *data)
{
  const char *title;
  GtkWidget *widget = NULL;
  gdk_window_get_user_data(window, (gpointer*)&widget);
  if (!widget) {
    return;
  }
  widget = gtk_widget_get_toplevel(widget);
  if (!GTK_IS_WINDOW(widget)) {
    return;
  }
  title = gtk_window_get_title((GtkWindow*)widget);
  if (title && !strcmp(title, data->title)) {
    data->rv = (void*)window;
  }
}

void mintrayr_Init()
{
}

void mintrayr_Destroy()
{
}

BOOL mintrayr_WatchWindow(void *handle, minimize_callback_t callback)
{
  minimize_callback_data_t *data;
  GdkWindow* window = (GdkWindow*)handle;

  if (!window) {
    return 0;
  }
  if (g_object_get_data(G_OBJECT(window), kWatch)) {
    // already watched!
    return 0;
  }

  data = (minimize_callback_data_t*)malloc(sizeof(minimize_callback_data_t));
  if (!data) {
    return 0;
  }
  data->handle = handle;
  data->callback = callback;

  gdk_window_add_filter(
    window,
    (GdkFilterFunc)filter_windows,
    data
    );

  return 1;
}
BOOL mintrayr_UnwatchWindow(void *handle)
{
  minimize_callback_data_t *data;
  GdkWindow* window = (GdkWindow*)handle;

  if (!window) {
    return 0;
  }
  data = (minimize_callback_data_t*)g_object_get_data(G_OBJECT(window), kWatch);
  if (!data) {
    // not watched!
    return 0;
  }
  g_object_set_data(G_OBJECT(window), kWatch, NULL);
  gdk_window_remove_filter(
    window,
    (GdkFilterFunc)filter_windows,
    data
    );
  free(data);

  return 1;
}

void mintrayr_MinimizeWindow(void *handle)
{
  if (!handle) {
    return;
  }
  gdk_window_hide((GdkWindow*)handle);
}
void  mintrayr_RestoreWindow(void *handle)
{
  if (!handle) {
    return;
  }
  gdk_window_show((GdkWindow*)handle);
}

BOOL mintrayr_CreateIcon(void *handle, mouseevent_callback_t callback)
{
  GdkWindow *window = (GdkWindow*)handle;
  GtkWidget *widget = NULL;
  GtkWindow *gtkWindow;
  GdkPixbuf *buf;
  const gchar *iconname;

  icon_data_t *data = NULL;

  if (!handle) {
    return 0;
  }
  if (g_object_get_data(G_OBJECT(window), kIcon)) {
    // already watched!
    return 0;
  }
  gdk_window_get_user_data(window, (gpointer*)&widget);
  if (!widget) {
    return 0;
  }
  widget = gtk_widget_get_toplevel(widget);
  gtkWindow = (GtkWindow*)widget;

  data = (icon_data_t*)malloc(sizeof(icon_data_t));
  if (!data) {
    return 0;
  }
  data->clickCount = 1;
  data->handle = handle;
  data->callback = callback;

  data->statusIcon = gtk_status_icon_new();
  if (!data->statusIcon) {
    goto error_cleanup;
  }
  gtk_status_icon_set_tooltip_text(data->statusIcon, gtk_window_get_title(gtkWindow));

  buf = gtk_window_get_icon(gtkWindow);
  if (buf) {
    gtk_status_icon_set_from_pixbuf(data->statusIcon, buf);
  }
  else if ((iconname = gtk_window_get_icon_name(gtkWindow)) != NULL) {
    gtk_status_icon_set_from_icon_name(data->statusIcon, iconname);
  }
  else {
    goto error_cleanup;
  }
  g_object_set_data(G_OBJECT(window), kIcon, data);

  g_signal_connect(G_OBJECT(data->statusIcon), "button-press-event", G_CALLBACK(button_event), data);
  g_signal_connect(G_OBJECT(data->statusIcon), "button-release-event", G_CALLBACK(button_event), data);

  gtk_status_icon_set_visible(data->statusIcon, 1);

  return 1;

error_cleanup:
  free(data);
  return 0;
}
BOOL mintrayr_DestroyIcon(void *handle)
{
  GdkWindow *window = (GdkWindow*)handle;
  icon_data_t *data;
  if (!window) {
    return 0;
  }
  data = (icon_data_t*)g_object_get_data(G_OBJECT(window), kIcon);
  if (!data) {
    return 0;
  }
  g_object_set_data(G_OBJECT(window), kIcon, NULL);

  mintrayr_RestoreWindow(handle);

  gtk_status_icon_set_visible(data->statusIcon, 0);
  g_object_unref(data->statusIcon);

  free(data);
  return 0;

}

void* mintrayr_GetBaseWindow(char *title)
{
  find_data_t data = {title, NULL};
  GdkScreen *screen = gdk_screen_get_default();
  GList *windows, *i;

  if (!screen) {
    return data.rv;
  }
  windows = gdk_screen_get_toplevel_windows(screen);
  g_list_foreach(windows, (GFunc)find_window, &data);
  g_list_free(windows);

  return data.rv;
}

void mintrayr_SetWatchMode(int mode) {
  gWatchMode = mode;
}
