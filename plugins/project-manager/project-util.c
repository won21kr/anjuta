/*  -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 * 
 * Copyright (C) 2003 Gustavo Giráldez
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA. 
 * 
 * Author: Gustavo Giráldez <gustavo.giraldez@gmx.net>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdarg.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "tree-data.h"
#include "project-view.h"
#include "project-model.h"
#include "project-util.h"
#include "pkg-config.h"

#define ICON_SIZE 16

#define GLADE_FILE PACKAGE_DATA_DIR  "/glade/pm_dialogs.ui"

enum {
    COLUMN_FILE,
    COLUMN_URI,
    N_COLUMNS
};

static GtkBuilder *
load_interface (const gchar *top_widget)
{
    GtkBuilder *xml = gtk_builder_new ();
    GError* error = NULL;

    if (!gtk_builder_add_from_file (xml, GLADE_FILE, &error))
    {
        g_warning ("Couldn't load builder file: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    return xml;
}

static gboolean
groups_filter_fn (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    GbfTreeData *data = NULL;
    gboolean retval = FALSE;

    gtk_tree_model_get (model, iter,
                        GBF_PROJECT_MODEL_COLUMN_DATA, &data, -1);
    retval = (data && data->type == GBF_TREE_NODE_GROUP);
    
    return retval;
}

static void 
setup_groups_treeview (AnjutaPmProject *project,
                       GtkWidget          *view,
                       GtkTreeIter        *select_group)
{
    GtkTreeModel *filter;
    GtkTreePath *path;
    GbfProjectModel *model;

    g_return_if_fail (project != NULL);
    g_return_if_fail (view != NULL && GBF_IS_PROJECT_VIEW (view));

    model = anjuta_pm_project_get_model (project);
    filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (model), NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                            groups_filter_fn, NULL, NULL);
    gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (filter));
    g_object_unref (filter);
    
    /* select default group */
    if (select_group) {
        GtkTreeIter iter_filter;

        gtk_tree_model_filter_convert_child_iter_to_iter (
            GTK_TREE_MODEL_FILTER (filter), &iter_filter, select_group);
        path = gtk_tree_model_get_path (filter, &iter_filter);
        gtk_tree_view_expand_to_path (GTK_TREE_VIEW (view), path);
    } else {
        GtkTreePath *root_path;

        gtk_tree_view_expand_all (GTK_TREE_VIEW (view));
        root_path = gbf_project_model_get_project_root (model);
        if (root_path) {
            path = gtk_tree_model_filter_convert_child_path_to_path (
                GTK_TREE_MODEL_FILTER (filter), root_path);
            gtk_tree_path_free (root_path);
        } else {
            path = gtk_tree_path_new_first ();
        }
    }
    
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, NULL, FALSE);
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (view), path, NULL,
                                  TRUE, 0.5, 0.0);
    gtk_tree_path_free (path);
}


static void
error_dialog (GtkWindow *parent, const gchar *summary, const gchar *msg, ...)
{
    va_list ap;
    gchar *tmp;
    GtkWidget *dialog;
    
    va_start (ap, msg);
    tmp = g_strdup_vprintf (msg, ap);
    va_end (ap);
    
    dialog = gtk_message_dialog_new_with_markup (parent,
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 "<b>%s</b>\n\n%s", summary, tmp);
    g_free (tmp);
    
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

static void
entry_changed_cb (GtkEditable *editable, gpointer user_data)
{
    GtkWidget *button = user_data;
    gchar *text;

    if (!button)
        return;

    text = gtk_editable_get_chars (editable, 0, -1);
    if (strlen (text) > 0) {
        gtk_widget_set_sensitive (button, TRUE);
        gtk_widget_grab_default (button);
    } else {
        gtk_widget_set_sensitive (button, FALSE);
    }
    g_free (text);
}

enum {
    TARGET_TYPE_TYPE = 0,
    TARGET_TYPE_NAME,
    TARGET_TYPE_PIXBUF,
    TARGET_TYPE_N_COLUMNS
};

/* create a tree model with the target types */
static GtkListStore *
build_types_store (AnjutaPmProject *project)
{
    GtkListStore *store;
    GtkTreeIter iter;
    GList *types;
    GList *node;

    types = anjuta_pm_project_get_node_info (project);
    store = gtk_list_store_new (TARGET_TYPE_N_COLUMNS,
                                G_TYPE_POINTER,
                                G_TYPE_STRING,
                                GDK_TYPE_PIXBUF);
    
    for (node = g_list_first (types); node != NULL; node = g_list_next (node)) {
        GdkPixbuf *pixbuf;
        const gchar *name;
        AnjutaProjectNodeType type;

        type = anjuta_project_node_info_type ((AnjutaProjectNodeInfo *)node->data);
        if (type & ANJUTA_PROJECT_TARGET)
        {
            name = anjuta_project_node_info_name ((AnjutaProjectNodeInfo *)node->data);
            pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default(),
                                            GTK_STOCK_CONVERT,
                                            ICON_SIZE,
                                            GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                            NULL);

            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                TARGET_TYPE_TYPE, type,
                                TARGET_TYPE_NAME, name,
                                TARGET_TYPE_PIXBUF, pixbuf,
                                -1);

            if (pixbuf)
                g_object_unref (pixbuf);
        }
    }

    g_list_free (types);

    return store;
}

AnjutaProjectNode* 
gbf_project_util_new_target (AnjutaPmProject *project,
                             GtkWindow       *parent,
                             GtkTreeIter     *default_group,
                             const gchar     *default_target_name_to_add)
{
    GtkBuilder *gui;
    GtkWidget *dialog, *target_name_entry, *ok_button;
    GtkWidget *target_type_combo, *groups_view;
    GtkListStore *types_store;
    GtkCellRenderer *renderer;
    gint response;
    gboolean finished = FALSE;
    AnjutaProjectNode *new_target = NULL;
    
    g_return_val_if_fail (project != NULL, NULL);
    
    gui = load_interface ("new_target_dialog");
    g_return_val_if_fail (gui != NULL, NULL);
    
    /* get all needed widgets */
    dialog = GTK_WIDGET (gtk_builder_get_object (gui, "new_target_dialog"));
    groups_view = GTK_WIDGET (gtk_builder_get_object (gui, "target_groups_view"));
    target_name_entry = GTK_WIDGET (gtk_builder_get_object (gui, "target_name_entry"));
    target_type_combo = GTK_WIDGET (gtk_builder_get_object (gui, "target_type_combo"));
    ok_button = GTK_WIDGET (gtk_builder_get_object (gui, "ok_target_button"));
    
    /* set up dialog */
    if (default_target_name_to_add)
        gtk_entry_set_text (GTK_ENTRY (target_name_entry),
                            default_target_name_to_add);
    g_signal_connect (target_name_entry, "changed",
                      (GCallback) entry_changed_cb, ok_button);
    if (default_target_name_to_add)
        gtk_widget_set_sensitive (ok_button, TRUE);
    else
        gtk_widget_set_sensitive (ok_button, FALSE);
    
    setup_groups_treeview (project, groups_view, default_group);
    gtk_widget_show (groups_view);

    /* setup target types combo box */
    types_store = build_types_store (project);
    gtk_combo_box_set_model (GTK_COMBO_BOX (target_type_combo), 
                             GTK_TREE_MODEL (types_store));

    /* create cell renderers */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (target_type_combo),
                                renderer, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (target_type_combo),
                                    renderer,
                                    "pixbuf", TARGET_TYPE_PIXBUF,
                                    NULL);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (target_type_combo),
                                renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (target_type_combo),
                                    renderer,
                                    "text", TARGET_TYPE_NAME,
                                    NULL);
    gtk_widget_show (target_type_combo);

    /* preselect */
    gtk_combo_box_set_active (GTK_COMBO_BOX (target_type_combo), 0);

    if (parent) {
        gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
    }
    
    /* execute dialog */
    while (!finished) {
        response = gtk_dialog_run (GTK_DIALOG (dialog));

        switch (response) {
            case GTK_RESPONSE_OK: 
            {
                GError *err = NULL;
                AnjutaProjectNode *group;
                GtkTreeIter iter;
                gchar *name;
                AnjutaProjectNodeType type;
                
                name = gtk_editable_get_chars (
                    GTK_EDITABLE (target_name_entry), 0, -1);
                group = gbf_project_view_find_selected (GBF_PROJECT_VIEW (groups_view),
                                                         ANJUTA_PROJECT_GROUP);

                /* retrieve target type */
                if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (target_type_combo), &iter)) {
                    gtk_tree_model_get (GTK_TREE_MODEL (types_store), &iter, 
                                        TARGET_TYPE_TYPE, &type,
                                        -1);
                }
                
                if (group && type) {
                    new_target = anjuta_pm_project_add_target (project, group, NULL, name, type, &err);
                    if (err) {
                        error_dialog (parent, _("Cannot add target"), "%s",
                                      err->message);
                        g_error_free (err);
                    } else {
			finished = TRUE;
		    }
                } else {
                    error_dialog (parent, _("Cannot add target"), "%s",
				  _("No group selected"));
                }
            
                g_free (name);

                break;
            }
            default:
                finished = TRUE;
                break;
        }
    }
    
    /* destroy stuff */
    g_object_unref (types_store);
    gtk_widget_destroy (dialog);
    g_object_unref (gui);
    return new_target;
}

static gboolean
targets_filter_fn (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    GbfTreeData *data = NULL;
    gboolean retval = FALSE;

    gtk_tree_model_get (model, iter,
                        GBF_PROJECT_MODEL_COLUMN_DATA, &data, -1);
    retval = (data && !data->is_shortcut &&
              (data->type == GBF_TREE_NODE_GROUP ||
               data->type == GBF_TREE_NODE_TARGET));
    
    return retval;
}

static void 
setup_targets_treeview (AnjutaPmProject *project,
                        GtkWidget           *view,
                        GtkTreeIter         *select_target)
{
    GtkTreeModel *filter;
    GtkTreeIter iter_filter;
    GtkTreePath *path = NULL;
    GbfProjectModel *model;
    
    g_return_if_fail (project != NULL);
    g_return_if_fail (view != NULL && GBF_IS_PROJECT_VIEW (view));

    model = anjuta_pm_project_get_model (project);
    filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (model), NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                            targets_filter_fn, NULL, NULL);
    gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (filter));
    g_object_unref (filter);

    /* select default target */
    if (select_target) {
        if (gtk_tree_model_filter_convert_child_iter_to_iter (
                GTK_TREE_MODEL_FILTER (filter), &iter_filter, select_target))
        {
            path = gtk_tree_model_get_path (filter, &iter_filter);
        }
    }
    if (path)
    {
        gtk_tree_view_expand_to_path (GTK_TREE_VIEW (view), path);
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, NULL, FALSE);
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (view), path, NULL,
                                      TRUE, 0.5, 0.0);
        gtk_tree_path_free (path);
    } else {
        gtk_tree_view_expand_all (GTK_TREE_VIEW (view));
    }
}

static gboolean
modules_filter_fn (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    GbfTreeData *data = NULL;
    gboolean retval = FALSE;
    GtkTreeIter root;

    gtk_tree_model_get (model, iter,
                        GBF_PROJECT_MODEL_COLUMN_DATA, &data, -1);
    retval = (data && !data->is_shortcut &&
              (!gtk_tree_model_iter_parent (model, &root, iter) ||
               data->type == GBF_TREE_NODE_MODULE ||
               data->type == GBF_TREE_NODE_PACKAGE));
    
    return retval;
}

static void 
setup_modules_treeview (AnjutaPmProject *project,
                        GtkWidget           *view,
                        GtkTreeIter         *select_module)
{
    GtkTreeModel *filter;
    GtkTreeIter iter_filter;
    GtkTreePath *path = NULL;
    GbfProjectModel *model;
    
    g_return_if_fail (view != NULL && GBF_IS_PROJECT_VIEW (view));

    model = anjuta_pm_project_get_model (project);
    filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (model), NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                            modules_filter_fn, NULL, NULL);
    gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (filter));
    g_object_unref (filter);

    /* select default module */
    if (select_module) {
        if (gtk_tree_model_filter_convert_child_iter_to_iter (
                GTK_TREE_MODEL_FILTER (filter), &iter_filter, select_module))
        {
            path = gtk_tree_model_get_path (filter, &iter_filter);
        }
    }
    if (path)
    {
        gtk_tree_view_expand_to_path (GTK_TREE_VIEW (view), path);
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, NULL, FALSE);
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (view), path, NULL,
                                      TRUE, 0.5, 0.0);
    } else {
        path = gtk_tree_path_new_first ();
        gtk_tree_view_expand_to_path (GTK_TREE_VIEW (view), path);
    }
    gtk_tree_path_free (path);
}

GList *
gbf_project_util_all_child (AnjutaProjectNode *parent, AnjutaProjectNodeType type)
{
    AnjutaProjectNode *node;
    GList *list = NULL;
 
    for (node = anjuta_project_node_first_child (parent); node != NULL; node = anjuta_project_node_next_sibling (node))
    {
        if ((type == ANJUTA_PROJECT_UNKNOWN) || (anjuta_project_node_get_type (node) == type))
        {
            list = g_list_prepend (list, node);
        }
    }
 
    list = g_list_reverse (list);
 
    return list;
}
 
GList *
gbf_project_util_node_all (AnjutaProjectNode *parent, AnjutaProjectNodeType type)
{
    AnjutaProjectNode *node;
    GList *list = NULL;
    gint type_id;
    gint type_flag;
    gint type_type;

    type_type = type & ANJUTA_PROJECT_TYPE_MASK;
    type_flag = type & ANJUTA_PROJECT_FLAG_MASK;
    type_id = type & ANJUTA_PROJECT_ID_MASK;
    
    for (node = anjuta_project_node_first_child (parent); node != NULL; node = anjuta_project_node_next_sibling (node))
    {
        GList *child_list;
        
        if (anjuta_project_node_get_type (node) == type_type)
        {
            gint type;
        
            type = anjuta_project_node_get_full_type (node);
            if (((type_id == 0) || (type_id == (type & ANJUTA_PROJECT_ID_MASK))) && 
                ((type_flag == 0) || ((type & type_flag) != 0)))
            {
                list = g_list_prepend (list, node);
            }
        }

        child_list = gbf_project_util_node_all (node, type);
        child_list = g_list_reverse (child_list);
        list = g_list_concat (child_list, list);
    }
 
    list = g_list_reverse (list);
 
    return list;
}

GList *
gbf_project_util_replace_by_file (GList* list)
{
        GList* link;
    
	for (link = g_list_first (list); link != NULL; link = g_list_next (link))
	{
                AnjutaProjectNode *node = (AnjutaProjectNode *)link->data;

                link->data = g_object_ref (anjuta_project_node_get_file (node));
	}

        return list;
}

static void
on_cursor_changed(GtkTreeView* view, gpointer data)
{
    GtkWidget* button = GTK_WIDGET(data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);

    if (gtk_tree_selection_count_selected_rows (selection) > 0)
        gtk_widget_set_sensitive(button, TRUE);
    else
        gtk_widget_set_sensitive(button, FALSE);
}

GList*
gbf_project_util_add_module (AnjutaPmProject *project,
                             GtkWindow          *parent,
                             GtkTreeIter        *default_target,
                             const gchar        *default_module)
{
    GtkBuilder *gui;
    GtkWidget *dialog;
    GtkWidget *ok_button, *new_button;
    GtkWidget *targets_view;
    GtkWidget *modules_view;
    gint response;
    gboolean finished = FALSE;
    GList* new_modules = NULL;
    GtkTreeSelection *module_selection;
    
    g_return_val_if_fail (project != NULL, NULL);
    
    gui = load_interface ("add_module_dialog");
    g_return_val_if_fail (gui != NULL, NULL);
    
    /* get all needed widgets */
    dialog = GTK_WIDGET (gtk_builder_get_object (gui, "add_module_dialog"));
    targets_view = GTK_WIDGET (gtk_builder_get_object (gui, "module_targets_view"));
    modules_view = GTK_WIDGET (gtk_builder_get_object (gui, "modules_view"));
    new_button = GTK_WIDGET (gtk_builder_get_object (gui, "new_package_button"));
    ok_button = GTK_WIDGET (gtk_builder_get_object (gui, "ok_module_button"));

    setup_targets_treeview (project, targets_view, default_target);
    gtk_widget_show (targets_view);
    setup_modules_treeview (project, modules_view, NULL);
    gtk_widget_show (modules_view);
    module_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (modules_view));
    gtk_tree_selection_set_mode (module_selection, GTK_SELECTION_MULTIPLE);

    if (gbf_project_view_find_selected (GBF_PROJECT_VIEW (modules_view), ANJUTA_PROJECT_MODULE))
    {
        gtk_widget_set_sensitive (ok_button, TRUE);
    }
    else
    {
        gtk_widget_set_sensitive (ok_button, FALSE);
    }
    g_signal_connect (G_OBJECT(modules_view), "cursor-changed",
        G_CALLBACK(on_cursor_changed), ok_button);

    
    if (parent) {
        gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
    }

    if (default_module)
        gtk_widget_grab_focus (modules_view);
    else
        gtk_widget_grab_focus (targets_view);
    
    /* execute dialog */
    while (!finished) {
        response = gtk_dialog_run (GTK_DIALOG (dialog));

        switch (response) {
            case 1:
            {
                gbf_project_util_add_package (project, parent, NULL, NULL);

                break;
            }
            case GTK_RESPONSE_OK: 
            {
                AnjutaProjectNode *target;
            
                target = gbf_project_view_find_selected (GBF_PROJECT_VIEW (targets_view),
                                                         ANJUTA_PROJECT_TARGET);
                if (target) {
                    GString *err_mesg = g_string_new (NULL);
                    GList *list;
                    GList *node;
                    
                    list = gbf_project_view_get_all_selected (GBF_PROJECT_VIEW (modules_view));
                    for (node = g_list_first (list); node != NULL; node = g_list_next (node))
                    {
                        GError *err = NULL;
                        AnjutaProjectNode* selected_module = (AnjutaProjectNode *)node->data;
                        AnjutaProjectNode* new_module;
                        gchar* uri = NULL;
                        GFile* module_file;

                        /*module_file = gbf_tree_data_get_file (selected_module);
                        new_module = ianjuta_project_add_module (project,
                                                            target,
                                                            module_file,
                                                            &err);
                        g_object_unref (module_file);*/
                        new_module = NULL;
                        if (err) {
                            gchar *str = g_strdup_printf ("%s: %s\n",
                                                            uri,
                                                            err->message);
                            g_string_append (err_mesg, str);
                            g_error_free (err);
                            g_free (str);
                        }
                        else
                            new_modules = g_list_append (new_modules,
                                                        new_module);

                        g_free (uri);
                    }
                    g_list_free (list);
                    
                    if (err_mesg->str && strlen (err_mesg->str) > 0) {
                        error_dialog (parent, _("Cannot add modules"),
                                        "%s", err_mesg->str);
                    } else {
                        finished = TRUE;
                    }
                    g_string_free (err_mesg, TRUE);
                } else {
                    error_dialog (parent, _("Cannot add modules"),
                            "%s", _("No target has been selected"));
                }
                
                break;
            }
            default:
                finished = TRUE;
                break;
        }
    }
    
    /* destroy stuff */
    gtk_widget_destroy (dialog);
    g_object_unref (gui);
    
    return new_modules;
}

static void on_changed_disconnect (GtkEditable* entry, gpointer data);

static void
on_cursor_changed_set_entry(GtkTreeView* view, gpointer data)
{
    GtkWidget* entry = GTK_WIDGET(data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);

    if (gtk_tree_selection_count_selected_rows (selection) == 1)
    {
        GtkTreeModel *model;
        GList *list;
        GtkTreeIter iter;

        list = gtk_tree_selection_get_selected_rows (selection, &model);
        if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath *)(list->data)))
        {
            gchar *name;
            gchar *ptr;
            
            gtk_tree_model_get (model, &iter, COL_PKG_PACKAGE, &name, -1);

            /* Remove numeric suffix */
            ptr = name + strlen(name) - 1;
            while (g_ascii_isdigit (*ptr))
            {
                while (g_ascii_isdigit (*ptr)) ptr--;
                if ((*ptr != '_') && (*ptr != '-') && (*ptr != '.')) break;
                *ptr = '\0';
                ptr--;
            }

            /* Convert to upper case and remove invalid characters */
            for (ptr = name; *ptr != '\0'; ptr++)
            {
                if (g_ascii_isalnum (*ptr))
                {
                    *ptr = g_ascii_toupper (*ptr);
                }
                else
                {
                    *ptr = '_';
                }
            }

            g_signal_handlers_block_by_func (G_OBJECT (entry), on_changed_disconnect, view);
            gtk_entry_set_text (GTK_ENTRY (entry), name);
            g_signal_handlers_unblock_by_func (G_OBJECT (entry), on_changed_disconnect, view);
            g_free (name);
        }
        
        g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
        g_list_free (list);
    }
}

static void
on_changed_disconnect (GtkEditable* entry, gpointer data)
{
    g_signal_handlers_block_by_func (G_OBJECT (data), on_cursor_changed_set_entry, entry);
}

GList* 
gbf_project_util_add_package (AnjutaPmProject *project,
                              GtkWindow        *parent,
                              GtkTreeIter      *default_module,
                              GList            *packages_to_add)
{
    GtkBuilder *gui;
    GtkWidget *dialog;
    GtkWidget *ok_button;
    GtkWidget *module_entry;
    GtkWidget *packages_view;
    GtkCellRenderer* renderer;
    GtkListStore *store;
    GList *packages = NULL;
    GtkTreeViewColumn *col;
    gint response;
    gboolean finished = FALSE;
    GtkTreeSelection *package_selection;
    GtkTreeIter root;
    gboolean valid;
    gint default_pos = -1;
    GbfProjectModel *model;
    
    g_return_val_if_fail (project != NULL, NULL);
    
    gui = load_interface ("add_package_dialog");
    g_return_val_if_fail (gui != NULL, NULL);
    
    /* get all needed widgets */
    dialog = GTK_WIDGET (gtk_builder_get_object (gui, "add_package_dialog"));
    module_entry = GTK_WIDGET (gtk_builder_get_object (gui, "module_entry"));
    packages_view = GTK_WIDGET (gtk_builder_get_object (gui, "packages_view"));
    ok_button = GTK_WIDGET (gtk_builder_get_object (gui, "ok_package_button"));

    /* Fill combo box with modules */
    store = gtk_list_store_new(1, G_TYPE_STRING);
    gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (module_entry), 0);

    model = anjuta_pm_project_get_model(project);
    for (valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &root); valid != FALSE; valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &root))
    {
        GbfTreeData *data;
        
        gtk_tree_model_get (GTK_TREE_MODEL (model), &root, GBF_PROJECT_MODEL_COLUMN_DATA, &data, -1);
        if (data && (data->type == GBF_TREE_NODE_GROUP)) break;
    }
    
    if (valid)
    {
        GtkTreeIter iter;
        gint pos = 0;
        GbfTreeData *data;
        GtkTreePath *default_path = default_module != NULL ? gtk_tree_model_get_path(GTK_TREE_MODEL (model), default_module) : NULL;

        gtk_tree_model_get (GTK_TREE_MODEL (model), &root, GBF_PROJECT_MODEL_COLUMN_DATA, &data, -1);
        
        for (valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (model), &iter, &root); valid != FALSE; valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter))
        {
            GbfTreeData *data;
            
            gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, GBF_PROJECT_MODEL_COLUMN_DATA, &data, -1);

            if (data && (data->type == GBF_TREE_NODE_MODULE))
            {
                GtkTreeIter list_iter;
                GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL (model), &iter);

                gtk_list_store_append (store, &list_iter);
                gtk_list_store_set (store, &list_iter, 0, data->name, -1);
                if ((default_path != NULL) && (gtk_tree_path_compare (path, default_path) == 0))
                {
                    default_pos = pos;
                }
                gtk_tree_path_free (path);
                pos++;
            }
        }
        if (default_path != NULL) gtk_tree_path_free (default_path);
    }
    gtk_combo_box_set_model (GTK_COMBO_BOX(module_entry), GTK_TREE_MODEL(store));
    gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (module_entry), 0);
    g_object_unref (store);
    if (default_pos >= 0)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (module_entry), default_pos);
    }
    else
    {
        /* Create automatically a module name from the package name when missing */
        GtkWidget *entry = gtk_bin_get_child (GTK_BIN (module_entry));
        
        g_signal_connect (G_OBJECT(packages_view), "cursor-changed",
            G_CALLBACK(on_cursor_changed_set_entry), entry);
        g_signal_connect (G_OBJECT(entry), "changed",
            G_CALLBACK(on_changed_disconnect), packages_view);
    }
    
    /* Fill package list */
    renderer = gtk_cell_renderer_text_new ();
    col = gtk_tree_view_column_new_with_attributes (_("Package"),
                                                    renderer,
                                                    "text", COL_PKG_PACKAGE,
                                                    NULL);
    gtk_tree_view_column_set_sort_column_id (col, COL_PKG_PACKAGE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (packages_view), col);
    renderer = gtk_cell_renderer_text_new ();
    col = gtk_tree_view_column_new_with_attributes (_("Description"),
                                                    renderer,
                                                    "text",
                                                    COL_PKG_DESCRIPTION,
                                                    NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (packages_view), col);
    store = packages_get_pkgconfig_list ();
    gtk_tree_view_set_model (GTK_TREE_VIEW (packages_view),
                            GTK_TREE_MODEL (store));

    on_cursor_changed (GTK_TREE_VIEW (packages_view), ok_button);
    g_signal_connect (G_OBJECT(packages_view), "cursor-changed",
        G_CALLBACK(on_cursor_changed), ok_button);
    package_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (packages_view));
    gtk_tree_selection_set_mode (package_selection, GTK_SELECTION_MULTIPLE);

    if (parent) {
        gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
    }
    
    /* execute dialog */
    while (!finished) {
        response = gtk_dialog_run (GTK_DIALOG (dialog));

        switch (response) {
            case GTK_RESPONSE_OK: 
            {
                gchar *module;

                module = gtk_combo_box_get_active_text (GTK_COMBO_BOX (module_entry));
                if (module)
                {
                    GString *err_mesg = g_string_new (NULL);
                    GList *list;
                    GList *node;
                    GtkTreeModel *model;
                    GtkTreeIter iter;
                    
                    list = gtk_tree_selection_get_selected_rows (package_selection, &model);
                    for (node = list; node != NULL; node = g_list_next (node))
                    {
                        if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath *)(node->data)))
                        {
                            gchar *name;
                            AnjutaProjectNode* new_package;
                            GError *err = NULL;
                            
                            gtk_tree_model_get (model, &iter, COL_PKG_PACKAGE, &name, -1);

                            /*new_package = ianjuta_project_add_package (project,
                                                            module,
                                                            name,
                                                            &err);*/
                            new_package = NULL;
                            if (err)
                            {
                                gchar *str = g_strdup_printf ("%s: %s\n",
                                                                name,
                                                                err->message);
                                g_string_append (err_mesg, str);
                                g_error_free (err);
                                g_free (str);
                            }
                            else
                            {
                                packages = g_list_append (packages,
                                                                new_package);
                            }
                            g_free (name);
                        }
                    }
                    g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
                    g_list_free (list);
                    
                    if (err_mesg->str && strlen (err_mesg->str) > 0)
                    {
                        error_dialog (parent, _("Cannot add packages"),
                                        "%s", err_mesg->str);
                    }
                    else
                    {
                        finished = TRUE;
                    }
                    g_string_free (err_mesg, TRUE);
                }
                else
                {
                    error_dialog (parent, _("Cannot add packages"),
                            "%s", _("No module has been selected"));
                }
                break;
            }
            default:
                finished = TRUE;
                break;
        }
    }
    
    /* destroy stuff */
    gtk_widget_destroy (dialog);
    g_object_unref (gui);
    
    return packages;
}