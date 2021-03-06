/*
 * PANDA -- a simple transaction monitor
 * Copyright (C) 2004-2008 Kouji TAKAO & JMA (Japan Medical Association).
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*
#define	DEBUG
#define	TRACE
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<sys/time.h>
#include	<glib.h>
#include	<gtk/gtk.h>
#include	<gtkpanda/gtkpanda.h>
#include	<errno.h>

#include	"glclient.h"
#include	"gettext.h"
#include	"action.h"
#include	"dialogs.h"
#include	"desktop.h"
#include	"print.h"
#include	"widgetcache.h"
#include	"utils.h"
#include	"message.h"
#include	"debug.h"

void
ShowPrintDialog(
	char	*title,
	char	*fname,
	size_t	size)
{
	GtkWindow *parent;
	GtkWidget *dialog;
	GtkWidget *content;
	GtkWidget *pandapdf;
	gchar *_title;

	pandapdf = gtk_panda_pdf_new();
	if (!gtk_panda_pdf_load(GTK_PANDA_PDF(pandapdf),fname)) {
		gtk_widget_destroy(pandapdf);
		return;
	}
	
	parent = (GtkWindow *)g_list_nth_data(DialogStack,
		g_list_length(DialogStack)-1);
	if (parent == NULL) {
		parent = GTK_WINDOW(TopWindow);
	}

	_title = g_strdup_printf(_("client printing - %s"),title);

	dialog = gtk_dialog_new_with_buttons(_("Preview"),parent,
		GTK_DIALOG_MODAL,
		GTK_STOCK_CLOSE,
		GTK_RESPONSE_NONE,NULL);
	gtk_window_set_title(GTK_WINDOW(dialog),_title);
	gtk_window_set_modal(GTK_WINDOW(dialog),TRUE);
	pandapdf = gtk_panda_pdf_new();
	gtk_panda_pdf_load(GTK_PANDA_PDF(pandapdf),fname);
	gtk_widget_set_size_request(pandapdf,800,600);
	content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_add(GTK_CONTAINER(content),pandapdf);
	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);
	g_free(_title);
}

void
PrintWithDefaultPrinter(
	char	*fname)
{
	GtkWidget *pandapdf;

	pandapdf = gtk_panda_pdf_new();
	if (!gtk_panda_pdf_load(GTK_PANDA_PDF(pandapdf),fname)) {
		gtk_widget_destroy(pandapdf);
		return;
	}
	gtk_panda_pdf_print(GTK_PANDA_PDF(pandapdf),FALSE);
	gtk_widget_destroy(pandapdf);
}
