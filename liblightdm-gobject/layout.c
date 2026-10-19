/* -*- Mode: C; indent-tabs-mode:nil; tab-width:4 -*-
 *
 * Copyright (C) 2010 Robert Ancell.
 * Author: Robert Ancell <robert.ancell@canonical.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 or version 3 of the License.
 * See http://www.gnu.org/copyleft/lgpl.html the full text of the license.
 */

#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbregistry.h>

#include "lightdm/layout.h"

/**
 * SECTION:layout
 * @short_description: Control the keyboard layout
 * @include: lightdm.h
 *
 * #LightDMLayout is an object that describes a keyboard that is available on the system.
 */

/**
 * LightDMLayout:
 *
 * #LightDMLayout is an opaque data structure and can only be accessed
 * using the provided functions.
 */

/**
 * LightDMLayoutClass:
 *
 * Class structure for #LightDMLayout.
 */

enum {
    PROP_NAME = 1,
    PROP_SHORT_DESCRIPTION,
    PROP_DESCRIPTION,
    PROP_VARIANT
};

typedef struct
{
    gchar *name;
    gchar *short_description;
    gchar *description;
    gchar *variant;
} LightDMLayoutPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (LightDMLayout, lightdm_layout, G_TYPE_OBJECT)

static gboolean have_layouts = FALSE;
static Display *display = NULL;
static struct rxkb_context *rxkb_context = NULL;
static struct xkb_keymap* xkb_keymap = NULL;
static struct xkb_context* xkb_context = NULL;
static struct rxkb_layout* rxkb_layout = NULL;
static struct xkb_state* xkb_state = NULL;
static GList *layouts = NULL;
static LightDMLayout *default_layout = NULL;

static void
create_layout (struct rxkb_layout *item)
{
    LightDMLayout *layout = g_object_new (LIGHTDM_TYPE_LAYOUT, "name", rxkb_layout_get_name(item), "short-description", rxkb_layout_get_brief(item), "description", rxkb_layout_get_description(item), "variant", rxkb_layout_get_variant(item), NULL);
    layouts = g_list_append (layouts, layout);
}

/**
 * lightdm_get_layouts:
 *
 * Get a list of keyboard layouts to present to the user.
 *
 * Return value: (element-type LightDMLayout) (transfer none): A list of #LightDMLayout that should be presented to the user.
 **/
GList *
lightdm_get_layouts (void)
{
    if (have_layouts)
        return layouts;

    display = XOpenDisplay (NULL);
    if (display == NULL)
        return NULL;

    xcb_connection_t* xcb_connection = xcb_connect (NULL, NULL);
    int err = xcb_connection_has_error(xcb_connection);
    if (err)
        return NULL;

    xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if(!xkb_context)
        return NULL;

    err = xkb_x11_setup_xkb_extension(xcb_connection, XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION, XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, NULL, NULL, NULL, NULL);
    if(err == 0)  // returns 1 on success
        return NULL;

    int32_t device_id = xkb_x11_get_core_keyboard_device_id(xcb_connection);
    if(device_id < 0)
        return NULL;

    xkb_keymap = xkb_x11_keymap_new_from_device(xkb_context, xcb_connection, device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if(!xkb_keymap)
        return NULL;

    xkb_state = xkb_x11_state_new_from_device(xkb_keymap, xcb_connection, device_id);
    if(!xkb_state)
        return NULL;

    rxkb_context = rxkb_context_new (RXKB_CONTEXT_NO_FLAGS);
    if (!rxkb_context_parse_default_ruleset(rxkb_context))
        return NULL;

    rxkb_layout = rxkb_layout_first(rxkb_context);
    while(rxkb_layout != NULL)
    {
        create_layout(rxkb_layout);
        rxkb_layout = rxkb_layout_next(rxkb_layout);
    }

    have_layouts = TRUE;

    return layouts;
}

/**
 * lightdm_set_layout:
 * @layout: The layout to use
 *
 * Set the layout for this session.
 **/
void
lightdm_set_layout (LightDMLayout *dmlayout)
{
    g_return_if_fail (dmlayout != NULL);
    lightdm_get_layouts();

    g_debug ("Setting keyboard layout to '%s'", lightdm_layout_get_name (dmlayout));

    g_autofree gchar *layout = g_strdup(lightdm_layout_get_name (dmlayout));
    g_autofree gchar *variant = g_strdup(lightdm_layout_get_variant (dmlayout));

    if (layouts && xkb_keymap)
    {
        default_layout = dmlayout;
    }

    // We used to use libxklavier, which called xmodmap - we can use setxkbmap.
    char cmd[1024];
    g_snprintf(cmd, sizeof(cmd), "setxkbmap %s %s", layout, variant ? variant : "");
    if (system(cmd) != 0)
        g_warning("Error executing setxkbmap command.");
}

/**
 * lightdm_get_layout:
 *
 * Get the current keyboard layout.
 *
 * Return value: (transfer none): The currently active layout for this user.
 **/
LightDMLayout *
lightdm_get_layout (void)
{
    lightdm_get_layouts ();

    if (layouts && xkb_keymap && !default_layout)
    {
        xkb_layout_index_t idx = xkb_state_serialize_layout(xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
        g_autofree gchar *full_name = g_strdup(xkb_keymap_layout_get_name(xkb_keymap, idx));

        for (GList *item = layouts; item; item = item->next)
        {
            // xkb_keymap_layout_get_name returns the 'description' of the registry
            LightDMLayout *iter_layout = (LightDMLayout *) item->data;
            if (g_strcmp0 (lightdm_layout_get_description (iter_layout), full_name) == 0)
            {
                default_layout = iter_layout;
                break;
            }
        }
    }

    return default_layout;
}

/**
 * lightdm_layout_get_name:
 * @layout: A #LightDMLayout
 *
 * Get the name of a layout.
 *
 * Return value: The name of the layout
 **/
const gchar *
lightdm_layout_get_name (LightDMLayout *layout)
{
    g_return_val_if_fail (LIGHTDM_IS_LAYOUT (layout), NULL);

    LightDMLayoutPrivate *priv = lightdm_layout_get_instance_private (layout);
    return priv->name;
}

/**
 * lightdm_layout_get_short_description:
 * @layout: A #LightDMLayout
 *
 * Get the short description of a layout.
 *
 * Return value: A short description of the layout
 **/
const gchar *
lightdm_layout_get_short_description (LightDMLayout *layout)
{
    g_return_val_if_fail (LIGHTDM_IS_LAYOUT (layout), NULL);

    LightDMLayoutPrivate *priv = lightdm_layout_get_instance_private (layout);
    return priv->short_description;
}

/**
 * lightdm_layout_get_description:
 * @layout: A #LightDMLayout
 *
 * Get the long description of a layout.
 *
 * Return value: A long description of the layout
 **/
const gchar *
lightdm_layout_get_description (LightDMLayout *layout)
{
    g_return_val_if_fail (LIGHTDM_IS_LAYOUT (layout), NULL);

    LightDMLayoutPrivate *priv = lightdm_layout_get_instance_private (layout);
    return priv->description;
}

/**
 * lightdm_layout_get_variant:
 * @layout: A #LightDMLayout
 *
 * Get the variant of a layout.
 *
 * Return value: The variant string of the layout
 **/
const gchar *
lightdm_layout_get_variant (LightDMLayout *layout)
{
    g_return_val_if_fail (LIGHTDM_IS_LAYOUT (layout), NULL);

    LightDMLayoutPrivate *priv = lightdm_layout_get_instance_private (layout);
    return priv->variant;
}

static void
lightdm_layout_init (LightDMLayout *layout)
{
}

static void
lightdm_layout_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    LightDMLayout *self = LIGHTDM_LAYOUT (object);
    LightDMLayoutPrivate *priv = lightdm_layout_get_instance_private (self);

    switch (prop_id) {
    case PROP_NAME:
        g_free (priv->name);
        priv->name = g_strdup (g_value_get_string (value));
        break;
    case PROP_SHORT_DESCRIPTION:
        g_free (priv->short_description);
        priv->short_description = g_strdup (g_value_get_string (value));
        break;
    case PROP_DESCRIPTION:
        g_free (priv->description);
        priv->description = g_strdup (g_value_get_string (value));
        break;
    case PROP_VARIANT:
        g_free (priv->variant);
        priv->variant = g_strdup (g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
lightdm_layout_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    LightDMLayout *self = LIGHTDM_LAYOUT (object);

    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string (value, lightdm_layout_get_name (self));
        break;
    case PROP_SHORT_DESCRIPTION:
        g_value_set_string (value, lightdm_layout_get_short_description (self));
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, lightdm_layout_get_description (self));
        break;
    case PROP_VARIANT:
        g_value_set_string (value, lightdm_layout_get_variant (self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
lightdm_layout_finalize (GObject *object)
{
    LightDMLayout *self = LIGHTDM_LAYOUT (object);
    LightDMLayoutPrivate *priv = lightdm_layout_get_instance_private (self);

    g_free (priv->name);
    g_free (priv->short_description);
    g_free (priv->description);
    g_free (priv->variant);
}

static void
lightdm_layout_class_init (LightDMLayoutClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = lightdm_layout_set_property;
    object_class->get_property = lightdm_layout_get_property;
    object_class->finalize = lightdm_layout_finalize;

    g_object_class_install_property (object_class,
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          "name",
                                                          "Name of the layout",
                                                          NULL,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (object_class,
                                     PROP_SHORT_DESCRIPTION,
                                     g_param_spec_string ("short-description",
                                                          "short-description",
                                                          "Short description of the layout",
                                                          NULL,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (object_class,
                                     PROP_DESCRIPTION,
                                     g_param_spec_string ("description",
                                                          "description",
                                                          "Long description of the layout",
                                                          NULL,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (object_class,
                                     PROP_VARIANT,
                                     g_param_spec_string ("variant",
                                                          "variant",
                                                          "Variant of the layout",
                                                          NULL,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}
