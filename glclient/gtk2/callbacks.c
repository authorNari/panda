/*
 * PANDA -- a simple transaction monitor
 * Copyright (C) 1998-1999 Ogochan.
 * Copyright (C) 2000-2008 Ogochan & JMA (Japan Medical Association).
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

#include	<gnome.h>
#include	<gtkpanda/gtkpanda.h>
#include	<gdk/gdkkeysyms.h>

#include	"callbacks.h"
#include	"types.h"
#include	"libmondai.h"
#include	"comm.h"
#include	"glclient.h"
#include	"action.h"
#include	"protocol.h"
#include	"glterm.h"
#include	"debug.h"

extern	gboolean
select_all(
	GtkWidget	*widget,
	GdkEventFocus	*event,
	gpointer		user_data)
{
	GtkEditable *editable = GTK_EDITABLE (widget);

ENTER_FUNC;
	gtk_editable_select_region(editable, 0, -1);
	gtk_editable_set_position(editable, 0);
LEAVE_FUNC;
	return (FALSE);
}

extern	gboolean
unselect_all(
	GtkWidget	*widget,
	GdkEventFocus	*event,
	gpointer		user_data)
{
	GtkEditable *editable = GTK_EDITABLE (widget);

ENTER_FUNC;
	gtk_editable_select_region(editable, 0, 0);
LEAVE_FUNC;
	return (FALSE);
}

extern	gboolean
keypress_filter(
	GtkWidget	*widget,
	GdkEventKey	*event,
	char		*next)
{
	GtkWidget	*nextWidget;
	GtkWidget	*window;

ENTER_FUNC;
	if		(event->keyval == GDK_KP_Enter) {
		window = gtk_widget_get_toplevel(widget);
		nextWidget = GetWidgetByLongName(next);
		if (nextWidget != NULL) {
			gtk_widget_grab_focus (nextWidget);
		}
		g_signal_stop_emission_by_name (G_OBJECT(widget), 
			"key_press_event");
	}
LEAVE_FUNC;
	return	(FALSE);
}

extern	gboolean
press_filter(
	GtkWidget	*widget,
	GdkEventButton	*event,
	gpointer		user_data)
{
	gboolean	rc;
ENTER_FUNC;
	/* If WIDGET already has focus, do the default action */
	if (GTK_WIDGET_HAS_FOCUS (widget)) {
		rc = FALSE;
	} else {
		/* Otherwise, just grab focus */
		gtk_widget_grab_focus (widget);
		g_signal_stop_emission_by_name (G_OBJECT(widget), 
			"button_press_event");
		rc = TRUE;
	}
LEAVE_FUNC;
	return	(rc);
}

static GtkWidget*
find_widget_ancestor(
	GtkWidget	*w,
	GType		t)
{
	GtkWidget	*widget;
	GtkType		type;
	widget = w;
	while	(	widget	) {
		type = (long)GTK_WIDGET_TYPE(widget);
		if	(	type == t	){
			return widget;
		}
		widget = gtk_widget_get_parent(widget);
	}
	return NULL;
}

extern	void
send_event(
	GtkWidget	*widget,
	char		*event)
{
	GdkWindow	*pane;
	char		*wname;
	static Bool	ignore_event = FALSE;

ENTER_FUNC;
	if		(  !fInRecv &&  !ignore_event ) {
		pane = ShowBusyCursor(widget);

		StopTimer(GTK_WINDOW(gtk_widget_get_toplevel(widget)));

		wname = GetWindowName(widget);
		/* send event */
		if		(  event  !=  NULL  ) {
			SendEvent(FPCOMM(glSession),
					  wname,
					  (char *)gtk_widget_get_name(widget),
					  event);
		} else {
			SendEvent(FPCOMM(glSession),
					  wname,
					  (char *)gtk_widget_get_name(widget),
					  (char *)gtk_widget_get_name(widget));
		}
		SendWindowData();
		BlockChangedHandlers();
		GetScreenData(FPCOMM(glSession));
		UnblockChangedHandlers();
		if	( ! fKeyBuff  ) {
			ignore_event = TRUE;
			ClearKeyBuffer();
			ignore_event = FALSE;
		}
		HideBusyCursor(pane); 
	}
LEAVE_FUNC;
}

static gint
send_event_if_kana (gpointer widget)
{
	GtkWidget   *window;
	char *timeout_event;
	guchar *text = (guchar *)gtk_entry_get_text (GTK_ENTRY (widget));
	int len = strlen (text);
ENTER_FUNC;
	window = gtk_widget_get_toplevel(widget);
	if (len == 0 || text[len - 1] >= 0x80)
	{
		timeout_event = GetTimerEvent(GTK_WINDOW(window));
		entry_changed (widget, timeout_event);
		send_event (widget, timeout_event);
	}
LEAVE_FUNC;
	return FALSE;
}

extern void
send_event_when_idle(
	GtkWidget	*widget,
	char		*event)
{
	static int registed = 0;
	static int timeout = -1;
	static int openchanged = 0;

ENTER_FUNC;
	StopTimer(GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	if (!registed) {
		RegisterChangedHandler(G_OBJECT(widget), 
			G_CALLBACK(send_event_when_idle), event);
		registed = 1;
	}
	if (timeout == -1) {
		if (fTimer)
			timeout = atoi (TimerPeriod);
		else
			timeout = 0;
	}

	if (timeout > 0) {
		if ( openchanged == 0 ) {
			openchanged += 1;
		} else {
			StartTimer(event, timeout, send_event_if_kana, widget);
		}
	} else {
		entry_changed (widget, event);
	}
LEAVE_FUNC;
}

extern void
clist_send_event(
	GtkWidget	*widget,
	gint		row,
	gint		column,
	char		*event)
{
	send_event(widget, "SELECT");
}

extern	void
changed(
	GtkWidget	*entry,
	gpointer	user_data)
{
	AddChangedWidget((GtkWidget *)entry);
}

extern	void
entry_changed(
	GtkWidget	*entry,
	gpointer	user_data)
{
	AddChangedWidget((GtkWidget *)entry);
}

extern	void
text_changed(
	GtkWidget	*entry,
	gpointer	user_data)
{
	AddChangedWidget((GtkWidget *)entry);
}

extern	void
button_toggled(
	GtkWidget	*button,
	gpointer	user_data)
{
	AddChangedWidget((GtkWidget *)button);
}

extern	void
selection_changed(
	GtkWidget	*widget,
	gpointer	user_data)
{
	AddChangedWidget(widget);
}

extern void
fileentry_changed(
	GtkWidget	*widget,
	gpointer	user_data)
{
	GtkWidget	*fileentry;

    fileentry = find_widget_ancestor(widget, GNOME_TYPE_FILE_ENTRY);
    if (fileentry != NULL) {
	  AddChangedWidget(fileentry);
	  AddChangedWidget(widget);
    }
}

extern	void
click_column(
	GtkWidget	*button,
	gpointer	user_data)
{
	AddChangedWidget((GtkWidget *)button);
}

extern	void
map_event(
	GtkWidget	*widget,
	gpointer	user_data)
{
	//fprintf(stderr,"map_event\n");
}

extern	void
set_focus(
	GtkWidget	*widget,
	gpointer	user_data)
{
ENTER_FUNC;
LEAVE_FUNC;
}

extern	void
day_selected(
	GtkCalendar	*widget,
	gpointer	user_data)
{
	AddChangedWidget((GtkWidget *)widget);
#ifdef	DEBUG
	printf("%d\n",(int)user_data);
	printf("year = %d\n",widget->year);
	printf("month = %d\n",widget->month+1);
	printf("day = %d\n",widget->selected_day);
#endif
}

extern	gboolean
switch_page(
	GtkNotebook	*widget,
	GtkNotebookPage *page,
	gint			page_num,
	char			*user_data)
{
	int			recv_page;
	gboolean	rc;
	gpointer *object;

	object = GetObjectData(GTK_WIDGET(widget), "recv_page");
	recv_page = (int )(*object);
	SetObjectData((GtkWidget *)widget, "page", (void *)&page_num);
	AddChangedWidget((GtkWidget *)widget);
	if ((user_data != NULL ) &&
		(recv_page != page_num)){
		g_signal_stop_emission_by_name (G_OBJECT (widget), "switch_page");
		rc = TRUE;	
	} else {
		rc = FALSE;
	}
	return rc;
}

extern	void
window_close(
	GtkWidget	*widget,
	gpointer	user_data)
{
	GtkWidget	*window;
	char		*name;

ENTER_FUNC;
	name = (char *)gtk_widget_get_name(widget);
	window = GetWidgetByLongName(name);
	if (window != NULL) {
		gtk_widget_hide(GTK_WIDGET(window));
	}
LEAVE_FUNC;
}

extern	void
window_destroy(
	GtkWidget	*widget,
	gpointer	user_data)
{
	fInRecv = TRUE;
	gtk_main_quit();
}

#ifdef	GTK_PANDA_TYPE_HTML
extern	void
open_browser(
	GtkPandaHTML	*html,
	void *data1,
	void *data2)
{
ENTER_FUNC;
	// do nothing
LEAVE_FUNC;
}
#endif