#ifndef GNUMERIC_SHEET_OBJECT_CONTAINER_H
#define GNUMERIC_SHEET_OBJECT_CONTAINER_H

#include "sheet-object-bonobo.h"
#include <bonobo/gnome-client-site.h>

/*
 * SheetObjectContainer:
 *
 * Container for Bonobo objects
 */
#define SHEET_OBJECT_CONTAINER_TYPE     (sheet_object_container_get_type ())
#define SHEET_OBJECT_CONTAINER(obj)     (GTK_CHECK_CAST((obj), SHEET_OBJECT_CONTAINER_TYPE, SheetObjectContainer))
#define SHEET_OBJECT_CONTAINER_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), SHEET_OBJECT_CONTAINER_TYPE, SheetObjectContainerClass))
#define IS_SHEET_CONTAINER_OBJECT(o)    (GTK_CHECK_TYPE((o), SHEET_OBJECT_CONTAINER_TYPE))

typedef struct {
	SheetObjectBonobo     parent_object;
} SheetObjectContainer;

typedef struct {
	SheetObjectClass parent_class;
} SheetObjectContainerClass;

/*
 * GNOME::View Bonobo containers.
 */
GtkType      sheet_object_container_get_type (void);
SheetObject *sheet_object_container_new      (Sheet *sheet,
					      double x1, double y1,
					      double x2, double y2,
					      const char *object_name);

SheetObject *sheet_object_graphic_new        (Sheet *sheet,
					      double x1, double y1,
					      double x2, double y2);

#endif /* GNUMERIC_SHEET_OBJECT_CONTAINER_H */

