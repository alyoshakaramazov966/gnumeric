/*
 * sheet-object-container.c:
 *   SheetObject for containers (Bonobo, Graphics)
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *   Michael Meeks (mmeeks@gnu.org)
 */
#include <config.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>
#include "gnumeric.h"
#include "gnumeric-util.h"
#include "gnumeric-sheet.h"
#include "sheet-object-container.h"
#include <bonobo/gnome-container.h>
#include <bonobo/gnome-view-frame.h>
#include <bonobo/gnome-client-site.h>
#include <bonobo/gnome-embeddable.h>

static SheetObject *sheet_object_container_parent_class;

static GnomeCanvasItem *
make_container_item (SheetObject *so, SheetView *sheet_view, GtkWidget *w)
{
	GnomeCanvasItem *item;
	double x1, y1, x2, y2;

	sheet_object_get_bounds (so, &x1, &y1, &x2, &y2);
	item = gnome_canvas_item_new (
		sheet_view->object_group,
		gnome_canvas_widget_get_type (),
		"widget", w,
		"x",      x1,
		"y",      y1,
		"width",  x2 - x1,
		"height", y2 - y1,
		"size_pixels", FALSE,
		NULL);
	sheet_object_widget_handle (so, w, item);
	gtk_widget_show (w);

	return item;
}

static gint
user_activation_request_cb (GnomeViewFrame *view_frame, SheetObject *so)
{
	Sheet *sheet = so->sheet;

	printf ("user activation request\n");
	if (sheet->active_object_frame){
		gnome_view_frame_view_deactivate (sheet->active_object_frame);
		if (sheet->active_object_frame != NULL)
                        gnome_view_frame_set_covered (sheet->active_object_frame, TRUE);
		sheet->active_object_frame = NULL;
	}

	gnome_view_frame_view_activate (view_frame);
	sheet_object_make_current (so);
	
	return FALSE;
}

static gint
view_activated_cb (GnomeViewFrame *view_frame, gboolean activated, SheetObject *so)
{
	Sheet *sheet = so->sheet;
	
        if (activated) {
                if (sheet->active_object_frame != NULL) {
                        g_warning ("View requested to be activated but there is already "
                                   "an active View!\n");
                        return FALSE;
                }

                /*
                 * Otherwise, uncover it so that it can receive
                 * events, and set it as the active View.
                 */
                gnome_view_frame_set_covered (view_frame, FALSE);
                sheet->active_object_frame = view_frame;
        } else {
                /*
                 * If the View is asking to be deactivated, always
                 * oblige.  We may have already deactivated it (see
                 * user_activation_request_cb), but there's no harm in
                 * doing it again.  There is always the possibility
                 * that a View will ask to be deactivated when we have
                 * not told it to deactivate itself, and that is
                 * why we cover the view here.
                 */
                gnome_view_frame_set_covered (view_frame, TRUE);

                if (view_frame == sheet->active_object_frame)
			sheet->active_object_frame = NULL;
	}
	return FALSE;
}

/*
 * Invoked when an item has been destroyed
 */
static void
item_destroyed (GnomeCanvasItem *item, GnomeViewFrame *view_frame)
{
	gnome_object_destroy (GNOME_OBJECT (view_frame));
}

static GnomeCanvasItem *
sheet_object_container_realize (SheetObject *so, SheetView *sheet_view)
{
	SheetObjectContainer *soc;
	GnomeCanvasItem *i;
	GnomeViewFrame *view_frame;
	GtkWidget *view_widget;

	soc = SHEET_OBJECT_CONTAINER (so);
	
	view_frame = gnome_client_site_new_view (SHEET_OBJECT_BONOBO (so)->client_site);
	gnome_view_frame_set_ui_handler (view_frame, so->sheet->workbook->uih);
		
	gtk_signal_connect (GTK_OBJECT (view_frame), "user_activate",
			    GTK_SIGNAL_FUNC (user_activation_request_cb), so);
	gtk_signal_connect (GTK_OBJECT (view_frame), "view_activated",
			    GTK_SIGNAL_FUNC (view_activated_cb), so);
	/*
	 * We need somehow to grab events from the wrapper in order to be able to
	 * move the component around easily.
	 *
	 * gtk_signal_connect (GTK_OBJECT (gnome_view_frame_get_wrapper (view_frame)),
	 *                    "event",
	 *                    GTK_SIGNAL_FUNC (sheet_object_event), so);
	 */
	
	view_widget = gnome_view_frame_get_wrapper (view_frame);
	i = make_container_item (so, sheet_view, view_widget);

	gtk_signal_connect (GTK_OBJECT (i), "destroy",
			    GTK_SIGNAL_FUNC (item_destroyed), view_frame);
	return i;
}

/*
 * This implemenation moves the widget rather than
 * destroying/updating/creating the views
 */
static void
sheet_object_container_update_bounds (SheetObject *so)
{
	GList *l;
	double x1, y1, x2, y2;

	sheet_object_get_bounds (so, &x1, &y1, &x2, &y2);

	for (l = so->realized_list; l; l = l->next){
		GnomeCanvasItem *item = l->data;

		gnome_canvas_item_set (
			item,
			"x",      x1,
			"y",      y1,
			"width",  x2 - x1,
			"height", y2 - y1,
			NULL);
	}
}

static void
sheet_object_container_class_init (GtkObjectClass *object_class)
{
	SheetObjectClass *sheet_object_class = SHEET_OBJECT_CLASS (object_class);

	sheet_object_container_parent_class = gtk_type_class (sheet_object_get_type ());

	/* SheetObject class method overrides */
	sheet_object_class->realize = sheet_object_container_realize;
	sheet_object_class->update_bounds = sheet_object_container_update_bounds;
}

GtkType
sheet_object_container_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"SheetObjectContainer",
			sizeof (SheetObjectContainer),
			sizeof (SheetObjectContainerClass),
			(GtkClassInitFunc) sheet_object_container_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (sheet_object_bonobo_get_type (), &info);
	}

	return type;
}

SheetObject *
sheet_object_container_new (Sheet *sheet,
			    double x1, double y1,
			    double x2, double y2,
			    const char *goadid)
{
	GnomeObjectClient *object_server;
	SheetObjectContainer *c;
	
	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	object_server = gnome_object_activate_with_goad_id (NULL, goadid, 0, NULL);
	if (!object_server)
		return NULL;

	c = gtk_type_new (sheet_object_container_get_type ());
	
	if (!sheet_object_bonobo_construct (
		SHEET_OBJECT_BONOBO (c), sheet,
		object_server, x1, y1, x2, y2)){
		gtk_object_destroy (GTK_OBJECT (c));
		return NULL;
	}

	return SHEET_OBJECT (c);
}
			  