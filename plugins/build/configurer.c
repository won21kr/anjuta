/*
 * configurer.c Copyright (C) 2000  Kh. Naba Kumar Singh
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59 
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA 
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#include <gnome.h>

#include <libanjuta/resources.h>

#include "anjuta.h"
#include "message-manager.h"
#include "text_editor.h"
#include "utilities.h"
#include "launcher.h"
#include "configurer.h"
#include "build_file.h"

static GtkWidget* create_configurer_dialog (Configurer* c);
static void on_configurer_response (GtkDialog *dialog, gint res, gpointer data);
static void on_configurer_entry_changed (GtkEditable *editable, gpointer data);
/*
static void on_configurer_environment_changed (GtkEditable * editable,
											   gpointer user_data);
*/

Configurer *
configurer_new (PropsID props)
{
	Configurer *c = g_new0 (Configurer, 1);
	c->props = props;
	return c;
}

void
configurer_destroy (Configurer * c)
{
	g_return_if_fail (c != NULL);
	g_free (c);
}

void
configurer_show (Configurer * c)
{
	gtk_widget_show (create_configurer_dialog (c));
}

static GtkWidget *
create_configurer_dialog (Configurer * c)
{
	GtkWidget *dialog;
	GtkWidget *entry;
	gchar *options;
	
	GladeXML *gxml;
	gxml = glade_xml_new (GLADE_FILE_ANJUTA, "configurer_dialog", NULL);
	dialog = glade_xml_get_widget (gxml, "configurer_dialog");
	g_signal_connect (G_OBJECT (dialog), "response",
				G_CALLBACK (on_configurer_response), c);
	gtk_widget_show (dialog);
	
	entry = glade_xml_get_widget (gxml, "configurer_entry");
	options = prop_get (c->props, "project.configure.options");
	if (options)
	{
		gtk_entry_set_text (GTK_ENTRY (entry), options);
		g_free (options);
	}
	g_signal_connect (G_OBJECT (entry), "changed",
			    G_CALLBACK (on_configurer_entry_changed), c);
	
	/* Not implemented yet
	
	entry = glade_xml_get_widget (gxml, "configurer_environment_entry");
	options = prop_get (c->props, "project.configure.environment");
	if (options)
	{
		gtk_entry_set_text (GTK_ENTRY (entry), options);
		g_free (options);
	}
	g_signal_connect (G_OBJECT (entry), "changed",
			    G_CALLBACK (on_configurer_environment_changed), c); */

	g_object_unref (G_OBJECT (gxml));
	
	/* gtk_accel_group_attach (app->accel_group, GTK_OBJECT (dialog)); */
	return dialog;
}

static void
on_configurer_entry_changed (GtkEditable * editable, gpointer user_data)
{
	Configurer *c = (Configurer *) user_data;
	const gchar *options;
	options = gtk_entry_get_text (GTK_ENTRY (editable));
	if (options)
		prop_set_with_key (c->props, "project.configure.options",
				   options);
	else
		prop_set_with_key (c->props, "project.configure.options", "");
}

/* FIXME: ... */
#if 0
static void
on_configurer_environment_changed (GtkEditable * editable, gpointer user_data)
{
	Configurer *c = (Configurer *) user_data;
	const gchar *options;
	options = gtk_entry_get_text (GTK_ENTRY (editable));
	if (options)
		prop_set_with_key (c->props, "project.configure.environment",
				   options);
	else
		prop_set_with_key (c->props, "project.configure.environment", "");
}
#endif

static void
on_configurer_response (GtkDialog* dialog, gint res, gpointer user_data)
{
	Configurer *cof;
	gchar *tmp, *options;
	cof = user_data;

	g_return_if_fail (cof != NULL);
	if (res == GTK_RESPONSE_OK)
	{
		g_return_if_fail (app->project_dbase->project_is_open);
		
		chdir (app->project_dbase->top_proj_dir);
		if (file_is_executable ("./configure") == FALSE)
		{
			anjuta_error (_
					  ("Project does not have an executable configure script.\n"
					   "Auto generate the Project first."));
			return;
		}
		options = prop_get (cof->props, "project.configure.options");
		if (options)
		{
			tmp = g_strconcat ("./configure ", options, NULL);
			g_free (options);
		}
		else
		{
			tmp = g_strdup ("./configure ");
		}
		options = prop_get(cof->props, "project.configure.environment");
		if (options)
		{
			gchar *tmp1 = g_strdup_printf("sh -c '%s %s'", options, tmp);
			g_free(tmp);
			tmp = tmp1;
		}
	#ifdef DEBUG
		g_message("Executing '%s'\n", tmp);
	#endif
		if (build_execute_command (tmp) == FALSE)
		{
			anjuta_error ("Project configuration failed.");
			g_free (tmp);
			return;
		}
		g_free (tmp);
		anjuta_update_app_status (TRUE, _("Configure"));
		an_message_manager_clear (app->messages, MESSAGE_BUILD);
		an_message_manager_append (app->messages, _("Configuring the Project...\n"), MESSAGE_BUILD);
		an_message_manager_show (app->messages, MESSAGE_BUILD);
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}