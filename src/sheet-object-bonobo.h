#ifndef GNUMERIC_SHEET_OBJECT_BONOBO_H
#define GNUMERIC_SHEET_OBJECT_BONOBO_H

#include "sheet-object.h"
#include <bonobo/gnome-client-site.h>

/*
 * SheetObjectBonobo:
 *
 * SheetObject abstract class for embedding Bonobo components.
 *    The sheet-object-container implements a window-based Gnome::View embedder
 *    The sheet-object-item implements a canvas-based Gnome::Canvas
 */
#define SHEET_OBJECT_BONOBO_TYPE     (sheet_object_bonobo_get_type ())
#define SHEET_OBJECT_BONOBO(obj)     (GTK_CHECK_CAST((obj), SHEET_OBJECT_BONOBO_TYPE, SheetObjectBonobo))
#define SHEET_OBJECT_BONOBO_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), SHEET_OBJECT_BONOBO_TYPE, SheetObjectBonoboClass))
#define IS_SHEET_OBJECT_BONOBO(o)    (GTK_CHECK_TYPE((o), SHEET_OBJECT_BONOBO_TYPE))

typedef struct {
	SheetObject     parent_object;

	/*
	 * The ClientSite for the bonobo object
	 *
	 * If this is NULL the object has not yet been
	 * activated/bound to this site
	 */
	GnomeClientSite *client_site;

	/*
	 * Points to the object server that implements this SheetObjectBonobo
	 */
	GnomeObjectClient *object_server;
} SheetObjectBonobo;

typedef struct {
	SheetObjectClass parent_class;
} SheetObjectBonoboClass;

/*
 * Bonobo Items.
 */
GtkType            sheet_object_bonobo_get_type (void);
SheetObjectBonobo *sheet_object_bonobo_construct (SheetObjectBonobo *sob, 
						  Sheet *sheet,
						  GnomeObjectClient *object_server,
						  double x1, double y1,
						  double x2, double y2);

gboolean sheet_object_bonobo_load_from_file      (SheetObjectBonobo *sob,
						  const char *fname);
gboolean sheet_object_bonobo_load                (SheetObjectBonobo *sob,
						  GnomeStream *stream);
void     sheet_object_bonobo_query_size          (SheetObjectBonobo *sob);


#endif /* GNUMERIC_SHEET_OBJECT_ITEM_H */

