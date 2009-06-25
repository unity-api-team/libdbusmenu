/*
A library to take the object model made consistent by libdbusmenu-glib
and visualize it in GTK.

Copyright 2009 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of either or both of the following licenses:

1) the GNU Lesser General Public License version 3, as published by the 
Free Software Foundation; and/or
2) the GNU Lesser General Public License version 2.1, as published by 
the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR 
PURPOSE.  See the applicable version of the GNU Lesser General Public 
License for more details.

You should have received a copy of both the GNU Lesser General Public 
License version 3 and version 2.1 along with this program.  If not, see 
<http://www.gnu.org/licenses/>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include "menu.h"
#include "libdbusmenu-glib/client.h"
#include "client.h"

/* Properties */
enum {
	PROP_0,
	PROP_DBUSOBJECT,
	PROP_DBUSNAME
};

/* Private */
typedef struct _DbusmenuGtkMenuPrivate DbusmenuGtkMenuPrivate;
struct _DbusmenuGtkMenuPrivate {
	DbusmenuGtkClient * client;

	gchar * dbus_object;
	gchar * dbus_name;
};

#define DBUSMENU_GTKMENU_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), DBUSMENU_GTKMENU_TYPE, DbusmenuGtkMenuPrivate))

/* Prototypes */
static void dbusmenu_gtkmenu_class_init (DbusmenuGtkMenuClass *klass);
static void dbusmenu_gtkmenu_init       (DbusmenuGtkMenu *self);
static void dbusmenu_gtkmenu_dispose    (GObject *object);
static void dbusmenu_gtkmenu_finalize   (GObject *object);
static void set_property (GObject * obj, guint id, const GValue * value, GParamSpec * pspec);
static void get_property (GObject * obj, guint id, GValue * value, GParamSpec * pspec);
/* Internal */
static void build_client (DbusmenuGtkMenu * self);

/* GObject Stuff */
G_DEFINE_TYPE (DbusmenuGtkMenu, dbusmenu_gtkmenu, GTK_TYPE_MENU);

static void
dbusmenu_gtkmenu_class_init (DbusmenuGtkMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DbusmenuGtkMenuPrivate));

	object_class->dispose = dbusmenu_gtkmenu_dispose;
	object_class->finalize = dbusmenu_gtkmenu_finalize;
	object_class->set_property = set_property;
	object_class->get_property = get_property;

	g_object_class_install_property (object_class, PROP_DBUSOBJECT,
	                                 g_param_spec_string(DBUSMENU_CLIENT_PROP_DBUS_OBJECT, "DBus Object we represent",
	                                              "The Object on the client that we're getting our data from.",
	                                              NULL,
	                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class, PROP_DBUSNAME,
	                                 g_param_spec_string(DBUSMENU_CLIENT_PROP_DBUS_NAME, "DBus Client we connect to",
	                                              "Name of the DBus client we're connecting to.",
	                                              NULL,
	                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	return;
}

static void
dbusmenu_gtkmenu_init (DbusmenuGtkMenu *self)
{
	DbusmenuGtkMenuPrivate * priv = DBUSMENU_GTKMENU_GET_PRIVATE(self);

	priv->client = NULL;

	priv->dbus_object = NULL;
	priv->dbus_name = NULL;

	return;
}

static void
dbusmenu_gtkmenu_dispose (GObject *object)
{
	DbusmenuGtkMenuPrivate * priv = DBUSMENU_GTKMENU_GET_PRIVATE(object);

	if (priv->client != NULL) {
		g_object_unref(G_OBJECT(priv->client));
		priv->client = NULL;
	}

	G_OBJECT_CLASS (dbusmenu_gtkmenu_parent_class)->dispose (object);
	return;
}

static void
dbusmenu_gtkmenu_finalize (GObject *object)
{
	DbusmenuGtkMenuPrivate * priv = DBUSMENU_GTKMENU_GET_PRIVATE(object);

	g_free(priv->dbus_object);
	priv->dbus_object = NULL;

	g_free(priv->dbus_name);
	priv->dbus_name = NULL;

	G_OBJECT_CLASS (dbusmenu_gtkmenu_parent_class)->finalize (object);
	return;
}

static void
set_property (GObject * obj, guint id, const GValue * value, GParamSpec * pspec)
{
	DbusmenuGtkMenuPrivate * priv = DBUSMENU_GTKMENU_GET_PRIVATE(obj);

	switch (id) {
	case PROP_DBUSNAME:
		priv->dbus_name = g_value_dup_string(value);
		if (priv->dbus_name != NULL && priv->dbus_object != NULL) {
			build_client(DBUSMENU_GTKMENU(obj));
		}
		break;
	case PROP_DBUSOBJECT:
		priv->dbus_object = g_value_dup_string(value);
		if (priv->dbus_name != NULL && priv->dbus_object != NULL) {
			build_client(DBUSMENU_GTKMENU(obj));
		}
		break;
	default:
		g_warning("Unknown property %d.", id);
		return;
	}

	return;
}

static void
get_property (GObject * obj, guint id, GValue * value, GParamSpec * pspec)
{
	DbusmenuGtkMenuPrivate * priv = DBUSMENU_GTKMENU_GET_PRIVATE(obj);

	switch (id) {
	case PROP_DBUSNAME:
		g_value_set_string(value, priv->dbus_name);
		break;
	case PROP_DBUSOBJECT:
		g_value_set_string(value, priv->dbus_object);
		break;
	default:
		g_warning("Unknown property %d.", id);
		return;
	}

	return;
}

/* Internal Functions */

static void
root_child_added (DbusmenuMenuitem * root, DbusmenuMenuitem * child, guint position, DbusmenuGtkMenu * menu)
{
	g_debug("Root new child");
	DbusmenuGtkMenuPrivate * priv = DBUSMENU_GTKMENU_GET_PRIVATE(menu);
	gtk_menu_shell_insert(GTK_MENU_SHELL(menu), GTK_WIDGET(dbusmenu_gtkclient_menuitem_get(priv->client, child)), position);
	gtk_widget_show(GTK_WIDGET(menu));
	return;
}

static void
root_child_moved (DbusmenuMenuitem * root, DbusmenuMenuitem * child, guint newposition, guint oldposition, DbusmenuGtkMenu * menu)
{
	g_debug("Root child moved");
	DbusmenuGtkMenuPrivate * priv = DBUSMENU_GTKMENU_GET_PRIVATE(menu);
	gtk_menu_reorder_child(GTK_MENU(menu), GTK_WIDGET(dbusmenu_gtkclient_menuitem_get(priv->client, child)), newposition);
	return;
}

static void
root_child_delete (DbusmenuMenuitem * root, DbusmenuMenuitem * child, DbusmenuGtkMenu * menu)
{
	g_debug("Root child deleted");
	if (g_list_length(dbusmenu_menuitem_get_children(root)) == 0) {
		gtk_widget_hide(GTK_WIDGET(menu));
	}
	return;
}

static void
root_changed (DbusmenuGtkClient * client, DbusmenuMenuitem * newroot, DbusmenuGtkMenu * menu) {
	g_debug("Root changed");
	if (newroot == NULL) {
		gtk_widget_hide(GTK_WIDGET(menu));
		return;
	}

	g_signal_connect(G_OBJECT(newroot), DBUSMENU_MENUITEM_SIGNAL_CHILD_ADDED,   G_CALLBACK(root_child_added),  menu);
	g_signal_connect(G_OBJECT(newroot), DBUSMENU_MENUITEM_SIGNAL_CHILD_MOVED,   G_CALLBACK(root_child_moved),  menu);
	g_signal_connect(G_OBJECT(newroot), DBUSMENU_MENUITEM_SIGNAL_CHILD_REMOVED, G_CALLBACK(root_child_delete), menu);

	GList * child = NULL;
	guint count = 0;
	for (child = dbusmenu_menuitem_get_children(newroot); child != NULL; child = g_list_next(child)) {
		gtk_menu_append(menu, GTK_WIDGET(dbusmenu_gtkclient_menuitem_get(client, child->data)));
		count++;
	}

	if (count > 0) {
		gtk_widget_show(GTK_WIDGET(menu));
	} else {
		gtk_widget_hide(GTK_WIDGET(menu));
	}

	return;
}

/* Builds the client and connects all of the signals
   up for it so that it's happy-happy */
static void
build_client (DbusmenuGtkMenu * self)
{
	DbusmenuGtkMenuPrivate * priv = DBUSMENU_GTKMENU_GET_PRIVATE(self);

	if (priv->client == NULL) {
		priv->client = dbusmenu_gtkclient_new(priv->dbus_name, priv->dbus_object);

		/* Register for layout changes, this should come after the
		   creation of the client pulls it from DBus */
		g_signal_connect(G_OBJECT(priv->client), DBUSMENU_GTKCLIENT_SIGNAL_ROOT_CHANGED, G_CALLBACK(root_changed), self);

		root_changed(priv->client, dbusmenu_client_get_root(DBUSMENU_CLIENT(priv->client)), self);
	}

	return;
}

/* Public API */

/**
	dbusmenu_gtkmenu_new:
	@dbus_name: Name of the #DbusmenuServer on DBus
	@dbus_name: Name of the object on the #DbusmenuServer

	Creates a new #DbusmenuGtkMenu object and creates a #DbusmenuClient
	that connects across DBus to a #DbusmenuServer.

	Return value: A new #DbusmenuGtkMenu sync'd with a server
*/
DbusmenuGtkMenu *
dbusmenu_gtkmenu_new (gchar * dbus_name, gchar * dbus_object)
{
	return g_object_new(DBUSMENU_GTKMENU_TYPE,
	                    DBUSMENU_CLIENT_PROP_DBUS_OBJECT, dbus_object,
	                    DBUSMENU_CLIENT_PROP_DBUS_NAME, dbus_name,
	                    NULL);
}

