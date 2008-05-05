/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2001 Ximian, Inc.
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gconf/gconf-client.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-bg.h>
#include <X11/Xatom.h>

#include "gnome-settings-profile.h"
#include "gsd-background-manager.h"

#include "preferences.h"

#define GSD_BACKGROUND_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_BACKGROUND_MANAGER, GsdBackgroundManagerPrivate))

struct GsdBackgroundManagerPrivate
{
        BGPreferences *prefs;
        GnomeBG       *bg;
        guint          timeout_id;
};

static void     gsd_background_manager_class_init  (GsdBackgroundManagerClass *klass);
static void     gsd_background_manager_init        (GsdBackgroundManager      *background_manager);
static void     gsd_background_manager_finalize    (GObject             *object);

G_DEFINE_TYPE (GsdBackgroundManager, gsd_background_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static gboolean
nautilus_is_running (void)
{
       Atom           window_id_atom;
       Window         nautilus_xid;
       Atom           actual_type;
       int            actual_format;
       unsigned long  nitems;
       unsigned long  bytes_after;
       unsigned char *data;
       int            retval;
       Atom           wmclass_atom;
       gboolean       running;
       gint           error;

       window_id_atom = XInternAtom (GDK_DISPLAY (),
                                     "NAUTILUS_DESKTOP_WINDOW_ID", True);

       if (window_id_atom == None) {
               return FALSE;
       }

       retval = XGetWindowProperty (GDK_DISPLAY (),
                                    GDK_ROOT_WINDOW (),
                                    window_id_atom,
                                    0,
                                    1,
                                    False,
                                    XA_WINDOW,
                                    &actual_type,
                                    &actual_format,
                                    &nitems,
                                    &bytes_after,
                                    &data);

       if (data != NULL) {
               nautilus_xid = *(Window *) data;
               XFree (data);
       } else {
               return FALSE;
       }

       if (actual_type != XA_WINDOW) {
               return FALSE;
       }
       if (actual_format != 32) {
               return FALSE;
       }

       wmclass_atom = XInternAtom (GDK_DISPLAY (), "WM_CLASS", False);

       gdk_error_trap_push ();

       retval = XGetWindowProperty (GDK_DISPLAY (),
                                    nautilus_xid,
                                    wmclass_atom,
                                    0,
                                    24,
                                    False,
                                    XA_STRING,
                                    &actual_type,
                                    &actual_format,
                                    &nitems,
                                    &bytes_after,
                                    &data);

       error = gdk_error_trap_pop ();

       if (error == BadWindow) {
               return FALSE;
       }

       if (actual_type == XA_STRING &&
           nitems == 24 &&
           bytes_after == 0 &&
           actual_format == 8 &&
           data != NULL &&
           !strcmp ((char *)data, "desktop_window") &&
           !strcmp ((char *)data + strlen ((char *)data) + 1, "Nautilus")) {
               running = TRUE;
       } else {
               running = FALSE;
       }

       if (data != NULL) {
               XFree (data);
       }

       return running;
}

static gboolean
apply_prefs (GsdBackgroundManager *manager)
{
        gnome_settings_profile_start (NULL);

        if (! nautilus_is_running ()) {
                GdkDisplay      *display;
                int              n_screens;
                int              i;
                GnomeBGPlacement placement;
                GnomeBGColorType color;
                const char      *uri;

                display = gdk_display_get_default ();
                n_screens = gdk_display_get_n_screens (display);

                uri = manager->priv->prefs->wallpaper_filename;

                placement = GNOME_BG_PLACEMENT_TILED;

                switch (manager->priv->prefs->wallpaper_type) {
                case WPTYPE_TILED:
                        placement = GNOME_BG_PLACEMENT_TILED;
                        break;
                case WPTYPE_CENTERED:
                        placement = GNOME_BG_PLACEMENT_CENTERED;
                        break;
                case WPTYPE_SCALED:
                        placement = GNOME_BG_PLACEMENT_SCALED;
                        break;
                case WPTYPE_STRETCHED:
                        placement = GNOME_BG_PLACEMENT_FILL_SCREEN;
                        break;
                case WPTYPE_ZOOM:
                        placement = GNOME_BG_PLACEMENT_ZOOMED;
                        break;
                case WPTYPE_NONE:
                case WPTYPE_UNSET:
                        uri = NULL;
                        break;
                }

                switch (manager->priv->prefs->orientation) {
                case ORIENTATION_SOLID:
                        color = GNOME_BG_COLOR_SOLID;
                        break;
                case ORIENTATION_HORIZ:
                        color = GNOME_BG_COLOR_H_GRADIENT;
                        break;
                case ORIENTATION_VERT:
                        color = GNOME_BG_COLOR_V_GRADIENT;
                        break;
                default:
                        color = GNOME_BG_COLOR_SOLID;
                        break;
                }

                gnome_bg_set_uri (manager->priv->bg, uri);
                gnome_bg_set_placement (manager->priv->bg, placement);
                gnome_bg_set_color (manager->priv->bg,
                                    color,
                                    manager->priv->prefs->color1,
                                    manager->priv->prefs->color2);

                for (i = 0; i < n_screens; ++i) {
                        GdkScreen *screen;
                        GdkWindow *root_window;
                        GdkPixmap *pixmap;

                        screen = gdk_display_get_screen (display, i);

                        root_window = gdk_screen_get_root_window (screen);

                        pixmap = gnome_bg_create_pixmap (manager->priv->bg,
                                                         root_window,
                                                         gdk_screen_get_width (screen),
                                                         gdk_screen_get_height (screen),
                                                         TRUE);

                        gnome_bg_set_pixmap_as_root (screen, pixmap);

                        g_object_unref (pixmap);
                }
        }

        gnome_settings_profile_end (NULL);

        return FALSE;
}

static void
queue_apply (GsdBackgroundManager *manager)
{
        if (manager->priv->timeout_id) {
                g_source_remove (manager->priv->timeout_id);
        }

        manager->priv->timeout_id = g_timeout_add (100, (GSourceFunc)apply_prefs, manager);
}

static void
background_callback (GConfClient          *client,
                     guint                 cnxn_id,
                     GConfEntry           *entry,
                     GsdBackgroundManager *manager)
{
        bg_preferences_merge_entry (manager->priv->prefs, entry);

        queue_apply (manager);
}

static void
on_bg_changed (GnomeBG              *bg,
               GsdBackgroundManager *manager)
{
        queue_apply (manager);
}

gboolean
gsd_background_manager_start (GsdBackgroundManager *manager,
                              GError              **error)
{
        GConfClient *client;
        gboolean nautilus_show_desktop;

        g_debug ("Starting background manager");
        gnome_settings_profile_start (NULL);

        manager->priv->prefs = BG_PREFERENCES (bg_preferences_new ());
        manager->priv->bg = gnome_bg_new ();

        g_signal_connect (manager->priv->bg,
                          "changed",
                          G_CALLBACK (on_bg_changed),
                          manager);
        bg_preferences_load (manager->priv->prefs);

        client = gconf_client_get_default ();
        gconf_client_add_dir (client, "/desktop/gnome/background",
                              GCONF_CLIENT_PRELOAD_NONE, NULL);
        gconf_client_notify_add (client,
                                 "/desktop/gnome/background",
                                 (GConfClientNotifyFunc) background_callback,
                                 manager,
                                 NULL,
                                 NULL);

        /* If this is set, nautilus will draw the background and is
	 * almost definitely in our session.  however, it may not be
	 * running yet (so is_nautilus_running() will fail).  so, on
	 * startup, just don't do anything if this key is set so we
	 * don't waste time setting the background only to have
	 * nautilus overwrite it.
	 */
        nautilus_show_desktop = gconf_client_get_bool (client,
                                                       "/apps/nautilus/preferences/show_desktop",
                                                       NULL);
        g_object_unref (client);

        if (!nautilus_show_desktop) {
                apply_prefs (manager);
        } else {
                g_timeout_add_seconds (8, apply_prefs, manager);
        }

        gnome_settings_profile_end (NULL);

        return TRUE;
}

void
gsd_background_manager_stop (GsdBackgroundManager *manager)
{
        g_debug ("Stopping background manager");

        if (manager->priv->prefs != NULL) {
                g_object_unref (manager->priv->prefs);
                manager->priv->prefs = NULL;
        }

        if (manager->priv->bg != NULL) {
                g_object_unref (manager->priv->bg);
                manager->priv->bg = NULL;
        }
}

static void
gsd_background_manager_set_property (GObject        *object,
                                     guint           prop_id,
                                     const GValue   *value,
                                     GParamSpec     *pspec)
{
        GsdBackgroundManager *self;

        self = GSD_BACKGROUND_MANAGER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_background_manager_get_property (GObject        *object,
                                     guint           prop_id,
                                     GValue         *value,
                                     GParamSpec     *pspec)
{
        GsdBackgroundManager *self;

        self = GSD_BACKGROUND_MANAGER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
gsd_background_manager_constructor (GType                  type,
                                    guint                  n_construct_properties,
                                    GObjectConstructParam *construct_properties)
{
        GsdBackgroundManager      *background_manager;
        GsdBackgroundManagerClass *klass;

        klass = GSD_BACKGROUND_MANAGER_CLASS (g_type_class_peek (GSD_TYPE_BACKGROUND_MANAGER));

        background_manager = GSD_BACKGROUND_MANAGER (G_OBJECT_CLASS (gsd_background_manager_parent_class)->constructor (type,
                                                                                                                        n_construct_properties,
                                                                                                                        construct_properties));

        return G_OBJECT (background_manager);
}

static void
gsd_background_manager_dispose (GObject *object)
{
        GsdBackgroundManager *background_manager;

        background_manager = GSD_BACKGROUND_MANAGER (object);

        G_OBJECT_CLASS (gsd_background_manager_parent_class)->dispose (object);
}

static void
gsd_background_manager_class_init (GsdBackgroundManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = gsd_background_manager_get_property;
        object_class->set_property = gsd_background_manager_set_property;
        object_class->constructor = gsd_background_manager_constructor;
        object_class->dispose = gsd_background_manager_dispose;
        object_class->finalize = gsd_background_manager_finalize;

        g_type_class_add_private (klass, sizeof (GsdBackgroundManagerPrivate));
}

static void
gsd_background_manager_init (GsdBackgroundManager *manager)
{
        manager->priv = GSD_BACKGROUND_MANAGER_GET_PRIVATE (manager);
}

static void
gsd_background_manager_finalize (GObject *object)
{
        GsdBackgroundManager *background_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_BACKGROUND_MANAGER (object));

        background_manager = GSD_BACKGROUND_MANAGER (object);

        g_return_if_fail (background_manager->priv != NULL);

        G_OBJECT_CLASS (gsd_background_manager_parent_class)->finalize (object);
}

GsdBackgroundManager *
gsd_background_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (GSD_TYPE_BACKGROUND_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return GSD_BACKGROUND_MANAGER (manager_object);
}
