/*
 * xml-io.c: save/read gnumeric Sheets using an XML encoding.
 *
 * Authors:
 *   Daniel Veillard <Daniel.Veillard@w3.org>
 *   Miguel de Icaza <miguel@gnu.org>
 *
 * $Id$
 */

#include <config.h>
#include <stdio.h>
#include <gnome.h>
#include <locale.h>
#include <math.h>
#include "gnumeric.h"
#include "gnome-xml/tree.h"
#include "gnome-xml/parser.h"
#include "color.h"
#include "border.h"
#include "sheet-object.h"
#include "sheet-object-graphic.h"
#include "print-info.h"
#include "xml-io.h"
#include "file.h"

/*
 * A parsing context.
 */
typedef struct {
	xmlDocPtr doc;		/* Xml document */
	xmlNsPtr ns;		/* Main name space */
	xmlNodePtr parent;	/* used only for g_hash_table_foreach callbacks */
	Sheet *sheet;		/* the associated sheet */
	Workbook *wb;		/* the associated sheet */
	GHashTable *style_table;/* old style styles compatibility */
} parse_xml_context_t;

/*
 * Internal stuff: xml helper functions.
 */

/*
 * Get a value for a node either carried as an attibute or as
 * the content of a child.
 */
static char *
xml_value_get (xmlNodePtr node, const char *name)
{
	char *ret;
	xmlNodePtr child;

	ret = (char *) xmlGetProp (node, name);
	if (ret != NULL)
		return ret;
	child = node->childs;

	while (child != NULL) {
		if (!strcmp (child->name, name)) {
		        /*
			 * !!! Inefficient, but ...
			 */
			ret = xmlNodeGetContent(child);
			if (ret != NULL)
			    return (ret);
		}
		child = child->next;
	}

	return NULL;
}

#if 0
/*
 * Get a String value for a node either carried as an attibute or as
 * the content of a child.
 */
static String *
xml_get_value_string (xmlNodePtr node, const char *name)
{
	char *val;
	String *ret;

	val = xml_value_get(node, name);
	if (val == NULL) return(NULL);
        ret = string_get(val);
	free(val);
	return(ret);
}
#endif

/*
 * Get an integer value for a node either carried as an attibute or as
 * the content of a child.
 */
static int
xml_get_value_int (xmlNodePtr node, const char *name, int *val)
{
	char *ret;
	int i;
	int res;

	ret = xml_value_get (node, name);
	if (ret == NULL) return(0);
	res = sscanf (ret, "%d", &i);
	free(ret);
	
	if (res == 1) {
	        *val = i;
		return 1;
	}
	return 0;
}

#if 0
/*
 * Get a float value for a node either carried as an attibute or as
 * the content of a child.
 */
static int
xml_get_value_float (xmlNodePtr node, const char *name, float *val)
{
	int res;
	char *ret;
	float f;

	ret = xml_value_get (node, name);
	if (ret == NULL) return(0);
	res = sscanf (ret, "%f", &f);
	free(ret);
	
	if (res == 1) {
	        *val = f;
		return 1;
	}
	return 0;
}
#endif

/*
 * Get a double value for a node either carried as an attibute or as
 * the content of a child.
 */
static int
xml_get_value_double (xmlNodePtr node, const char *name, double *val)
{
	int res;
	char *ret;

	ret = xml_value_get (node, name);
	if (ret == NULL) return(0);
	res = sscanf (ret, "%lf", val);
	free(ret);

	return (res == 1);
}

#if 0
/*
 * Get a set of coodinates for a node, carried as the content of a child.
 */
static int
xml_get_coordinate (xmlNodePtr node, const char *name, double *x, double *y)
{
	int res;
	char *ret;
	float X, Y;

	ret = xml_value_get (node, name);
	if (ret == NULL) return(0);
	res = sscanf (ret, "(%lf %lf)", x, y)
	free(ret);

	return (res == 2)
}
#endif

/*
 * Get a pair of coodinates for a node, carried as the content of a child.
 */

static int
xml_get_coordinates (xmlNodePtr node, const char *name,
		   double *x1, double *y1, double *x2, double *y2)
{
	int res;
	char *ret;

	ret = xml_value_get (node, name);
	if (ret == NULL) return(0);
	res = sscanf (ret, "(%lf %lf)(%lf %lf)", x1, y1, x2, y2);
	free(ret);
	
	if (res == 4) 
		return 1;

	return 0;
}

#if 0
/*
 * Get a GnomeCanvasPoints for a node, carried as the content of a child.
 */
static GnomeCanvasPoints *
xml_get_gnome_canvas_points (xmlNodePtr node, const char *name)
{
	char *val;
	GnomeCanvasPoints *ret = NULL;
	int res;
	const char *ptr;
	int index = 0, i;
	float coord[20];	/* TODO: must be dynamic !!!! */

	val = xml_value_get (node, name);
	if (val == NULL) return(NULL);
	ptr = val;
	do {
		while ((*ptr) && (*ptr != '('))
			ptr++;
		if (*ptr == 0)
			break;
		res = sscanf (ptr, "(%lf %lf)", &coord[index], &coord[index + 1]);
		if (res != 2)
			break;
		index += 2;
		ptr++;
	} while (res > 0);
	free(val);

	if (index >= 2)
		ret = gnome_canvas_points_new (index / 2);
	if (ret == NULL)
		return NULL;
	for (i = 0; i < index; i++)
		ret->coords[i] = coord[i];
	return ret;
}
#endif

/*
 * Set a string value for a node either carried as an attibute or as
 * the content of a child.
 */
static void
xml_set_gnome_canvas_points (xmlNodePtr node, const char *name,
			     GnomeCanvasPoints *val)
{
	xmlNodePtr child;
	char *str, *base;
	xmlChar *tstr;
	int i;

	if (val == NULL)
		return;
	if ((val->num_points < 0) || (val->num_points > 5000))
		return;
	base = str = g_malloc (val->num_points * 30 * sizeof (char) + 1);
	if (str == NULL)
		return;
	for (i = 0; i < val->num_points; i++){
		sprintf (str, "(%f %f)", val->coords[2 * i],
			 val->coords[2 * i + 1]);
		str += strlen (str);
	}
	*str = 0;

	child = node->childs;
	while (child != NULL){
		if (!strcmp (child->name, name)){
			xmlNodeSetContent (child, base);
			free (base);
			return;
		}
		child = child->next;
	}
	xmlNewChild (node, NULL, name,
		     (tstr = xmlEncodeEntitiesReentrant (node->doc, base)));
	if (tstr)
		free (tstr);
	g_free (base);
}

/*
 * Set a string value for a node either carried as an attibute or as
 * the content of a child.
 */
static void
xml_set_value (xmlNodePtr node, const char *name, const char *val)
{
	const char *ret;
	xmlNodePtr child;

	ret = xmlGetProp (node, name);
	if (ret != NULL){
		xmlSetProp (node, name, val);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!strcmp (child->name, name)){
			xmlNodeSetContent (child, val);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, name, val);
}

/*
 * Set a String value for a node either carried as an attibute or as
 * the content of a child.
 */
static void
xml_set_value_string (xmlNodePtr node, const char *name, String *val)
{
	const char *ret;
	xmlNodePtr child;

	ret = xmlGetProp (node, name);
	if (ret != NULL){
		xmlSetProp (node, name, val->str);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!strcmp (child->name, name)){
			xmlNodeSetContent (child, val->str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, name, val->str);
}

/*
 * Set an integer value for a node either carried as an attibute or as
 * the content of a child.
 */
static void
xml_set_value_int (xmlNodePtr node, const char *name, int val)
{
	const char *ret;
	xmlNodePtr child;
	char str[101];

	snprintf (str, 100, "%d", val);
	ret = xmlGetProp (node, name);
	if (ret != NULL){
		xmlSetProp (node, name, str);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!strcmp (child->name, name)){
			xmlNodeSetContent (child, str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, name, str);
}

#if 0
/*
 * Set a float value for a node either carried as an attibute or as
 * the content of a child.
 */
static void
xml_set_value_float (xmlNodePtr node, const char *name, float val)
{
	const char *ret;
	xmlNodePtr child;
	char str[101];

	snprintf (str, 100, "%f", val);
	ret = xmlGetProp (node, name);
	if (ret != NULL){
		xmlSetProp (node, name, str);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!strcmp (child->name, name)){
			xmlNodeSetContent (child, str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, name, str);
}
#endif

/*
 * Set a double value for a node either carried as an attibute or as
 * the content of a child.
 */
static void
xml_set_value_double (xmlNodePtr node, const char *name, double val)
{
	const char *ret;
	xmlNodePtr child;
	char str[101 + DBL_DIG];

	if (fabs (val) < 1e9 && fabs (val) > 1e-5)
		snprintf (str, 100 + DBL_DIG, "%.*g", DBL_DIG, val);
	else
		snprintf (str, 100 + DBL_DIG, "%f", val);
	ret = xmlGetProp (node, name);
	if (ret != NULL){
		xmlSetProp (node, name, str);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!strcmp (child->name, name)){
			xmlNodeSetContent (child, str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, name, str);
}

static void
xml_set_print_unit (xmlNodePtr node, const char *name,
		    const PrintUnit * const pu)
{
	xmlNodePtr  child;
	char       *txt = "points";
	xmlChar    *tstr;

	if (pu == NULL || name == NULL)
		return;

	switch (pu->desired_display) {
	case UNIT_POINTS:
		txt = "points";
		break;
	case UNIT_MILLIMETER:
		txt = "mm";
		break;
	case UNIT_CENTIMETER:
		txt = "cm";
		break;
	case UNIT_INCH:
		txt = "in";
		break;
	}

	child = xmlNewChild (node, NULL, "PrintUnit",
			     (tstr = xmlEncodeEntitiesReentrant (node->doc, name)));
	if (tstr)
		free (tstr);
	xml_set_value_double (child, "Points", pu->points);
	xml_set_value (child, "PrefUnit",
		       (tstr = xmlEncodeEntitiesReentrant (node->doc, txt)));
	if (tstr)
		free (tstr);
}

static void
xml_get_print_unit (xmlNodePtr node, PrintUnit * const pu)
{
	char       *txt;
	
	g_return_if_fail (pu != NULL);
	g_return_if_fail (node != NULL);
	g_return_if_fail (node->childs != NULL);

	xml_get_value_double (node, "Points", &pu->points);
	txt = xml_value_get  (node, "PrefUnit");
	if (txt) {
		if (!g_strcasecmp (txt, "points"))
			pu->desired_display = UNIT_POINTS;
		else if (!g_strcasecmp (txt, "mm"))
			pu->desired_display = UNIT_MILLIMETER;
		else if (!g_strcasecmp (txt, "cm"))
			pu->desired_display = UNIT_CENTIMETER;
		else if (!g_strcasecmp (txt, "in"))
			pu->desired_display = UNIT_INCH;
	}
}

/*
 * Search a child by name, if needed go down the tree to find it. 
 */
static xmlNodePtr
xml_search_child (xmlNodePtr node, const char *name)
{
	xmlNodePtr ret;
	xmlNodePtr child;

	child = node->childs;
	while (child != NULL){
		if (!strcmp (child->name, name))
			return child;
		child = child->next;
	}
	child = node->childs;
	while (child != NULL){
		ret = xml_search_child (child, name);
		if (ret != NULL)
			return ret;
		child = child->next;
	}
	return NULL;
}

/*
 * Get a color value for a node either carried as an attibute or as
 * the content of a child.
 *
 * TODO PBM: at parse time one doesn't have yet a widget, so we have
 *           to retrieve the default colormap, but this may be a bad
 *           option ...
 */
static int
xml_get_color_value (xmlNodePtr node, const char *name, StyleColor **color)
{
	char *ret;
	int red, green, blue;

	ret = xml_value_get (node, name);
	if (ret == NULL) return(0);
	if (sscanf (ret, "%X:%X:%X", &red, &green, &blue) == 3){
		*color = style_color_new (red, green, blue);
		free(ret);
		return 1;
	}
	return 0;
}

/*
 * Set a color value for a node either carried as an attibute or as
 * the content of a child.
 */
static void
xml_set_color_value (xmlNodePtr node, const char *name, StyleColor *val)
{
	const char *ret;
	xmlNodePtr child;
	char str[101];

	snprintf (str, 100, "%X:%X:%X", val->color.red, val->color.green, val->color.blue);
	ret = xmlGetProp (node, name);
	if (ret != NULL){
		xmlSetProp (node, name, str);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!strcmp (child->name, name)){
			xmlNodeSetContent (child, str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, name, str);
}

/**
 **
 ** Private functions : mapping between in-memory structure and XML tree
 **
 **/

static int
style_is_default_fore (StyleColor *color)
{
	if (!color)
		return TRUE;

	if (color->color.red == 0 && color->color.green == 0 && color->color.blue == 0)
		return TRUE;
	else
		return FALSE;
}

static int
style_is_default_back (StyleColor *color)
{
	if (!color)
		return TRUE;

	if (color->color.red == 0xffff && color->color.green == 0xffff && color->color.blue == 0xffff)
		return TRUE;
	else
		return FALSE;
}

/*
 * Create an XML subtree of doc equivalent to the given StyleBorder.
 */

static char *StyleSideNames[6] =
{
 	"Top",
 	"Bottom",
 	"Left",
 	"Right",
	"Diagonal",
	"Rev-Diagonal"
};

static xmlNodePtr
xml_write_style_border (parse_xml_context_t *ctxt,
			const MStyle *style)
{
	xmlNodePtr cur;
	xmlNodePtr side;
	int        i;
       
	for (i = MSTYLE_BORDER_TOP; i <= MSTYLE_BORDER_REV_DIAGONAL; i++) {
		MStyleBorder const *border;
		if (mstyle_is_element_set (style, i) &&
		    NULL != (border = mstyle_get_border (style, i)) &&
		    border->line_type != STYLE_BORDER_NONE) {
			break;
		}
	}
	if (i > MSTYLE_BORDER_REV_DIAGONAL)
		return NULL;

	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "StyleBorder", NULL);
	
	for (i = MSTYLE_BORDER_TOP; i <= MSTYLE_BORDER_REV_DIAGONAL; i++) {
		MStyleBorder const *border;
		if (mstyle_is_element_set (style, i) &&
		    NULL != (border = mstyle_get_border (style, i)) &&
		    border->line_type != STYLE_BORDER_NONE) {
			StyleBorderType t = border->line_type;
			StyleColor *col   = border->color;
 			side = xmlNewChild (cur, ctxt->ns,
					    StyleSideNames [i - MSTYLE_BORDER_TOP],
 					    NULL);
 			xml_set_color_value (side, "Color", col);
			xml_set_value_int (side, "Style", t);
 		}
	}
	return cur;
}

/*
 * Create a StyleBorder equivalent to the XML subtree of doc.
 */
static void
xml_read_style_border (parse_xml_context_t *ctxt, xmlNodePtr tree, MStyle *mstyle)
{
	xmlNodePtr side;
	int        i;

	if (strcmp (tree->name, "StyleBorder")){
		fprintf (stderr,
			 "xml_read_style_border: invalid element type %s, 'StyleBorder' expected`\n",
			 tree->name);
	}

	for (i = MSTYLE_BORDER_TOP; i <= MSTYLE_BORDER_REV_DIAGONAL; i++) {
 		if ((side = xml_search_child (tree,
					      StyleSideNames [i - MSTYLE_BORDER_TOP])) != NULL) {
			int		 t;
			StyleColor      *color = NULL;
			MStyleBorder    *border;
			xml_get_value_int (side, "Style", &t);
			xml_get_color_value (side, "Color", &color);
			border = style_border_fetch ((StyleBorderType)t, color, 
						     style_border_get_orientation (i));
			if (border)
				mstyle_set_border (mstyle, i, border);
 		}
	}
}

/*
 * Create an XML subtree of doc equivalent to the given Style.
 */
static xmlNodePtr
xml_write_style (parse_xml_context_t *ctxt,
		 MStyle *style)
{
	xmlNodePtr  cur, child;
	xmlChar    *tstr;

	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Style", NULL);
	
	if (mstyle_is_element_set (style, MSTYLE_ALIGN_H))
		xml_set_value_int (cur, "HAlign", mstyle_get_align_h (style));
	if (mstyle_is_element_set (style, MSTYLE_ALIGN_V))
		xml_set_value_int (cur, "VAlign", mstyle_get_align_v (style));
	if (mstyle_is_element_set (style, MSTYLE_FIT_IN_CELL))
		xml_set_value_int (cur, "Fit", mstyle_get_fit_in_cell (style));
	if (mstyle_is_element_set (style, MSTYLE_ORIENTATION))
		xml_set_value_int (cur, "Orient", mstyle_get_orientation (style));
	if (mstyle_is_element_set (style, MSTYLE_PATTERN))
		xml_set_value_int (cur, "Shade", mstyle_get_pattern (style));

	if (mstyle_is_element_set (style, MSTYLE_COLOR_FORE)) {
		if (!style_is_default_fore (mstyle_get_color (style, MSTYLE_COLOR_FORE)))
			xml_set_color_value (cur, "Fore", mstyle_get_color (style, MSTYLE_COLOR_FORE));
	}
	if (mstyle_is_element_set (style, MSTYLE_COLOR_BACK)) {
		if (!style_is_default_back (mstyle_get_color (style, MSTYLE_COLOR_BACK)))
			xml_set_color_value (cur, "Back", mstyle_get_color (style, MSTYLE_COLOR_BACK));
	}
	if (mstyle_is_element_set (style, MSTYLE_COLOR_PATTERN)) {
		if (!style_is_default_back (mstyle_get_color (style, MSTYLE_COLOR_PATTERN)))
			xml_set_color_value (cur, "PatternColor", mstyle_get_color (style, MSTYLE_COLOR_PATTERN));
	}
	if (mstyle_is_element_set (style, MSTYLE_FORMAT))
		xml_set_value (cur, "Format", mstyle_get_format (style)->format);

	if (mstyle_is_element_set (style, MSTYLE_FONT_NAME) ||
	    mstyle_is_element_set (style, MSTYLE_FONT_BOLD) ||
	    mstyle_is_element_set (style, MSTYLE_FONT_ITALIC) ||
	    mstyle_is_element_set (style, MSTYLE_FONT_SIZE)) {
		const char *fontname;

		if (mstyle_is_element_set (style, MSTYLE_FONT_NAME))
			fontname = mstyle_get_font_name (style);
		else /* backwards compatibility */
			fontname = "Helvetica";

		child = xmlNewChild (cur, ctxt->ns, "Font", 
				     (tstr = xmlEncodeEntitiesReentrant (ctxt->doc, fontname)));
		if (tstr)
			free (tstr);
		if (mstyle_is_element_set (style, MSTYLE_FONT_SIZE))
			xml_set_value_double (child, "Unit",
					      mstyle_get_font_size (style));
		if (mstyle_is_element_set (style, MSTYLE_FONT_BOLD))
			xml_set_value_int (child, "Bold",
					   mstyle_get_font_bold (style));
		if (mstyle_is_element_set (style, MSTYLE_FONT_ITALIC))
			xml_set_value_int (child, "Italic",
					   mstyle_get_font_italic (style));
	}

	child = xml_write_style_border (ctxt, style);
	if (child)
		xmlAddChild (cur, child);

	return cur;
}

static xmlNodePtr
xml_write_names (parse_xml_context_t *ctxt, GList *names)
{
	xmlNodePtr  cur;
	xmlChar    *tstr;

	if (!names)
		return NULL;

	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Names", NULL);

	while (names) {
		xmlNodePtr   tmp;
		ExprName    *expr_name = names->data;
		char        *text;

		g_return_val_if_fail (expr_name != NULL, NULL);

		tmp = xmlNewDocNode (ctxt->doc, ctxt->ns, "Name", NULL);
		xmlNewChild (tmp, ctxt->ns, "name",
			     (tstr = xmlEncodeEntitiesReentrant (ctxt->doc, expr_name->name->str)));
		if (tstr)
			free (tstr);

		text = expr_name_value (expr_name);
		xmlNewChild (tmp, ctxt->ns, "value",
			     (tstr = xmlEncodeEntitiesReentrant (ctxt->doc, text)));
		if (tstr)
			free (tstr);
		g_free (text);

		xmlAddChild (cur, tmp);
		names = g_list_next (names);
	}

	return cur;
}

static void
xml_read_names (parse_xml_context_t *ctxt, xmlNodePtr tree, Workbook *wb,
		Sheet *sheet)
{
	xmlNodePtr child;

	g_return_if_fail (ctxt != NULL);
	g_return_if_fail (tree != NULL);

	child = tree->childs;
	while (child) {
		char *name  = NULL;
		if (child->name && !strcmp (child->name, "Name")) {
			xmlNodePtr bits;

			bits = child->childs;
			while (bits) {
				
				if (!strcmp (bits->name, "name")) {
					name = xmlNodeGetContent (bits);
				} else {
					char     *txt;
					char     *error;
					g_return_if_fail (name != NULL);

					txt = xmlNodeGetContent (bits);
					g_return_if_fail (txt != NULL);
					g_return_if_fail (!strcmp (bits->name, "value"));

					if (!expr_name_create (wb, sheet, name, txt, &error))
						g_warning (error);

					g_free (txt);
				}
				bits = bits->next;
			}
		}
		child = child->next;
	}
}

static xmlNodePtr
xml_write_summary (parse_xml_context_t *ctxt, SummaryInfo *summary_info)
{
	GList *items, *m;
	xmlChar   *tstr;
	xmlNodePtr cur;

	if (!summary_info)
		return NULL;

	m = items = summary_info_as_list (summary_info);

	if (!items)
		return NULL;

	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Summary", NULL);

	while (items) {
		xmlNodePtr   tmp;
		SummaryItem *sit = items->data;
		if (sit) {
			char *text;

			tmp = xmlNewDocNode (ctxt->doc, ctxt->ns, "Item", NULL);
			xmlNewChild (tmp, ctxt->ns, "name",
				     (tstr = xmlEncodeEntitiesReentrant (ctxt->doc, sit->name)));
			if (tstr)
				free (tstr);

			if (sit->type == SUMMARY_INT) {

				text = g_strdup_printf ("%d", sit->v.i);
				xmlNewChild (tmp, ctxt->ns, "val-int",
					     (tstr = xmlEncodeEntitiesReentrant (ctxt->doc, text)));
				if (tstr)
					free (tstr);

			} else {

				text = summary_item_as_text (sit);
				xmlNewChild (tmp, ctxt->ns, "val-string",
					     (tstr = xmlEncodeEntitiesReentrant (ctxt->doc, text)));
				if (tstr)
					free (tstr);

			}
			g_free (text);
			xmlAddChild (cur, tmp);
		}
		items = g_list_next (items);
	}
	g_list_free (m);
	return cur;
}

static void
xml_read_summary (parse_xml_context_t *ctxt, xmlNodePtr tree, SummaryInfo *summary_info)
{
	xmlNodePtr child;

	g_return_if_fail (ctxt != NULL);
	g_return_if_fail (tree != NULL);
	g_return_if_fail (summary_info != NULL);

	child = tree->childs;
	while (child) {
		char *name = NULL;

		if (child->name && !strcmp (child->name, "Item")) {
			xmlNodePtr bits;

			bits = child->childs;
			while (bits) {
				SummaryItem *sit = NULL;
				
				if (!strcmp (bits->name, "name")) {
					name = xmlNodeGetContent (bits);
				} else {
					char *txt;
					g_return_if_fail (name);

					txt = xmlNodeGetContent (bits);
					if (txt != NULL){
						if (!strcmp (bits->name, "val-string"))
							sit = summary_item_new_string (name, txt);
						else if (!strcmp (bits->name, "val-int"))
							sit = summary_item_new_int (name, atoi (txt));
						
						if (sit)
							summary_info_add (summary_info, sit);
						g_free (txt);
					}
				}
				bits = bits->next;
			}
		}
		if (name){
			free (name);
			name = NULL;
		}
		child = child->next;
	}
}

static void
xml_set_print_hf (xmlNodePtr node, const char *name,
		  const PrintHF * const hf)
{
	xmlNodePtr  child;
	xmlChar    *tstr;

	if (hf == NULL || name == NULL)
		return;

	child = xmlNewChild (node, NULL, name, NULL);
	xml_set_value (child, "Left",
		       (tstr = xmlEncodeEntitiesReentrant (node->doc, hf->left_format)));
	if (tstr)
		free (tstr);
	xml_set_value (child, "Middle",
		       (tstr = xmlEncodeEntitiesReentrant (node->doc, hf->middle_format)));
	if (tstr)
		free (tstr);
	xml_set_value (child, "Right",
		       (tstr = xmlEncodeEntitiesReentrant (node->doc, hf->right_format)));
	if (tstr)
		free (tstr);
}

static void
xml_get_print_hf (xmlNodePtr node, PrintHF *const hf)
{
	char *txt;
	
	g_return_if_fail (hf != NULL);
	g_return_if_fail (node != NULL);

	txt = xml_value_get (node, "Left");
	if (txt) {
		if (hf->left_format)
			g_free (hf->left_format);
		
		hf->left_format = g_strdup (txt);
		free (txt);
	}
	
	txt = xml_value_get (node, "Middle");
	if (txt) {
		if (hf->middle_format)
			g_free (hf->middle_format);
		
		hf->middle_format = g_strdup (txt);
		free (txt);
	}

	txt = xml_value_get (node, "Right");
	if (txt) {
		if (hf->right_format)
			g_free (hf->right_format);
		
		hf->right_format = g_strdup (txt);
		free (txt);
	}
}

static xmlNodePtr
xml_write_print_info (parse_xml_context_t *ctxt, PrintInformation *pi)
{
	xmlNodePtr cur, child;

	g_return_val_if_fail (pi != NULL, NULL);

	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "PrintInformation", NULL);

	xml_set_print_unit (cur, "top",    &pi->margins.top);
	xml_set_print_unit (cur, "bottom", &pi->margins.bottom);
	xml_set_print_unit (cur, "left",   &pi->margins.left);
	xml_set_print_unit (cur, "right",  &pi->margins.right);
	xml_set_print_unit (cur, "header", &pi->margins.header);
	xml_set_print_unit (cur, "footer", &pi->margins.footer);


	child = xmlNewDocNode (ctxt->doc, ctxt->ns, "vcenter", NULL);
	xml_set_value_int  (child, "value", pi->center_vertically);
	xmlAddChild (cur, child);
	child = xmlNewDocNode (ctxt->doc, ctxt->ns, "hcenter", NULL);
	xml_set_value_int  (child, "value", pi->center_horizontally);
	xmlAddChild (cur, child);

	child = xmlNewDocNode (ctxt->doc, ctxt->ns, "grid", NULL);
	xml_set_value_int  (child, "value",    pi->print_line_divisions);
	xmlAddChild (cur, child);
	child = xmlNewDocNode (ctxt->doc, ctxt->ns, "monochrome", NULL);
	xml_set_value_int  (child, "value",    pi->print_black_and_white);
	xmlAddChild (cur, child);
	child = xmlNewDocNode (ctxt->doc, ctxt->ns, "draft", NULL);
	xml_set_value_int  (child, "value",    pi->print_as_draft);
	xmlAddChild (cur, child);
	child = xmlNewDocNode (ctxt->doc, ctxt->ns, "titles", NULL);
	xml_set_value_int  (child, "value",    pi->print_titles);
	xmlAddChild (cur, child);

	if (pi->print_order == PRINT_ORDER_DOWN_THEN_RIGHT)
		child = xmlNewDocNode (ctxt->doc, ctxt->ns, "order", "d_then_r");
	else
		child = xmlNewDocNode (ctxt->doc, ctxt->ns, "order", "r_then_d");
	xmlAddChild (cur, child);

	if (pi->orientation == PRINT_ORIENT_VERTICAL)
		child = xmlNewDocNode (ctxt->doc, ctxt->ns, "orientation", "portrait");
	else
		child = xmlNewDocNode (ctxt->doc, ctxt->ns, "orientation", "landscape");
	xmlAddChild (cur, child);

	xml_set_print_hf (cur, "Header", pi->header);
	xml_set_print_hf (cur, "Footer", pi->footer);

	child = xmlNewDocNode (ctxt->doc, ctxt->ns, "paper", gnome_paper_name (pi->paper));
	xmlAddChild (cur, child);
	
	return cur;
}

static void
xml_read_print_info (parse_xml_context_t *ctxt, xmlNodePtr tree)
{
	xmlNodePtr child;
	PrintInformation *pi;
	int b;

	g_return_if_fail (ctxt != NULL);
	g_return_if_fail (tree != NULL);
	g_return_if_fail (ctxt->sheet != NULL);

	pi = ctxt->sheet->print_info;
	
	g_return_if_fail (pi != NULL);

	if ((child = xml_search_child (tree, "top")))
		xml_get_print_unit (child, &pi->margins.top);
	if ((child = xml_search_child (tree, "bottom")))
		xml_get_print_unit (child, &pi->margins.bottom);
	if ((child = xml_search_child (tree, "left")))
		xml_get_print_unit (child, &pi->margins.left);
	if ((child = xml_search_child (tree, "right")))
		xml_get_print_unit (child, &pi->margins.right);
	if ((child = xml_search_child (tree, "header")))
		xml_get_print_unit (child, &pi->margins.header);
	if ((child = xml_search_child (tree, "footer")))
		xml_get_print_unit (child, &pi->margins.footer);

	if ((child = xml_search_child (tree, "vcenter"))) {
		xml_get_value_int  (child, "value", &b);
		pi->center_vertically   = (b == 1);
	}
	if ((child = xml_search_child (tree, "hcenter"))) {
		xml_get_value_int  (child, "value", &b);
		pi->center_horizontally = (b == 1);
	}

	if ((child = xml_search_child (tree, "grid"))) {
		xml_get_value_int  (child, "value",    &b);
		pi->print_line_divisions  = (b == 1);
	}
	if ((child = xml_search_child (tree, "monochrome"))) {
		xml_get_value_int  (child, "value", &b);
		pi->print_black_and_white = (b == 1);
	}
	if ((child = xml_search_child (tree, "draft"))) {
		xml_get_value_int  (child, "value",   &b);
		pi->print_as_draft        = (b == 1);
	}
	if ((child = xml_search_child (tree, "titles"))) {
		xml_get_value_int  (child, "value",  &b);
		pi->print_titles          = (b == 1);
	}
	
	if ((child = xml_search_child (tree, "order"))) {
		if (!strcmp (xmlNodeGetContent(child), "d_then_r"))
			pi->print_order = PRINT_ORDER_DOWN_THEN_RIGHT;
		else
			pi->print_order = PRINT_ORDER_RIGHT_THEN_DOWN;
	}

	if ((child = xml_search_child (tree, "orientation"))) {
		if (!strcmp (xmlNodeGetContent(child), "portrait"))
			pi->orientation = PRINT_ORIENT_VERTICAL;
		else
			pi->orientation = PRINT_ORIENT_HORIZONTAL;
	}
	
	if ((child = xml_search_child (tree, "Header")))
		xml_get_print_hf (child, pi->header);
	if ((child = xml_search_child (tree, "Footer")))
		xml_get_print_hf (child, pi->header);

	if ((child = xml_search_child (tree, "paper")))
		pi->paper = gnome_paper_with_name (xmlNodeGetContent (child));
}

static const char *
font_component (const char *fontname, int idx)
{
	int i = 0;
	const char *p = fontname;
      
	for (; *p && i < idx; p++){
		if (*p == '-')
			i++;
	}
	if (*p == '-')
		p++;

	return p;
}

/**
 * style_font_read_from_x11:
 * @mstyle: the style to setup to this font.
 * @fontname: an X11-like font name.
 *
 * Tries to guess the fontname, the weight and italization parameters
 * and setup mstyle
 *
 * Returns: A valid style font.
 */
static void
style_font_read_from_x11 (MStyle *mstyle, const char *fontname)
{
	const char *c;
	
	/*
	 * FIXME: we should do something about the typeface instead
	 * of hardcoding it to helvetica.
	 */
      
	c = font_component (fontname, 2);
	if (strncmp (c, "bold", 4) == 0)
		mstyle_set_font_bold (mstyle, TRUE);

	c = font_component (fontname, 3);
	if (strncmp (c, "o", 1) == 0)
		mstyle_set_font_italic (mstyle, TRUE);

	if (strncmp (c, "i", 1) == 0)
		mstyle_set_font_italic (mstyle, TRUE);
}

/*
 * Create a Style equivalent to the XML subtree of doc.
 */
static MStyle *
xml_read_style (parse_xml_context_t *ctxt, xmlNodePtr tree)
{
	xmlNodePtr child;
	char *prop;
	int val;
	StyleColor *c;
	MStyle     *mstyle;
	
	mstyle = mstyle_new ();
	
	if (strcmp (tree->name, "Style")) {
		fprintf (stderr,
			 "xml_read_style: invalid element type %s, 'Style' expected\n",
			 tree->name);
	}

	if (xml_get_value_int (tree, "HAlign", &val))
		mstyle_set_align_h (mstyle, val);

	if (xml_get_value_int (tree, "Fit", &val))
		mstyle_set_fit_in_cell (mstyle, val);

	if (xml_get_value_int (tree, "VAlign", &val))
		mstyle_set_align_v (mstyle, val);

	if (xml_get_value_int (tree, "Orient", &val))
		mstyle_set_orientation (mstyle, val);

	if (xml_get_value_int (tree, "Shade", &val))
		mstyle_set_pattern (mstyle, val);

	if (xml_get_color_value (tree, "Fore", &c))
		mstyle_set_color (mstyle, MSTYLE_COLOR_FORE, c);

	if (xml_get_color_value (tree, "Back", &c))
		mstyle_set_color (mstyle, MSTYLE_COLOR_BACK, c);

	if (xml_get_color_value (tree, "PatternColor", &c))
		mstyle_set_color (mstyle, MSTYLE_COLOR_PATTERN, c);

	prop = xml_value_get (tree, "Format");
	if (prop != NULL) {
		mstyle_set_format (mstyle, prop);
		free (prop);
	}

	child = tree->childs;
	while (child != NULL) {
		if (!strcmp (child->name, "Font")) {
			char *font;
			double units = 14;
			int t;
				
			xml_get_value_double (child, "Unit", &units);

			if (xml_get_value_int (child, "Bold", &t))
				mstyle_set_font_bold (mstyle, t);

			if (xml_get_value_int (child, "Italic", &t))
				mstyle_set_font_italic (mstyle, t);
			
			font = xmlNodeGetContent (child);
			if (font) {
				if (*font == '-')
					style_font_read_from_x11 (mstyle, font);
				else
					mstyle_set_font_name (mstyle, font);
			}
			
		} else if (!strcmp (child->name, "StyleBorder")) {
			xml_read_style_border (ctxt, child, mstyle);
		} else {
			fprintf (stderr, "xml_read_style: unknown type '%s'\n",
				 child->name);
		}
		child = child->next;
	}

	return mstyle;
}

/*
 * Create an XML subtree of doc equivalent to the given StyleRegion.
 */
static xmlNodePtr
xml_write_style_region (parse_xml_context_t *ctxt, StyleRegion *region)
{
	xmlNodePtr cur, child;

	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "StyleRegion", NULL);
	xml_set_value_int (cur, "startCol", region->range.start.col);
	xml_set_value_int (cur, "startRow", region->range.start.row);
	xml_set_value_int (cur, "endCol",   region->range.end.col);
	xml_set_value_int (cur, "endRow",   region->range.end.row);

	if (region->style != NULL) {
		child = xml_write_style (ctxt, region->style);
		if (child)
			xmlAddChild (cur, child);
	}
	return cur;
}

/*
 * Create a StyleRegion equivalent to the XML subtree of doc.
 */
static void
xml_read_style_region (parse_xml_context_t *ctxt, xmlNodePtr tree)
{
	xmlNodePtr child;
	MStyle    *style = NULL;
	Range      range;

	if (strcmp (tree->name, "StyleRegion")){
		fprintf (stderr,
			 "xml_read_style_region: invalid element type %s, 'StyleRegion' expected`\n",
			 tree->name);
		return;
	}
	xml_get_value_int (tree, "startCol", &range.start.col);
	xml_get_value_int (tree, "startRow", &range.start.row);
	xml_get_value_int (tree, "endCol",   &range.end.col);
	xml_get_value_int (tree, "endRow",   &range.end.row);
	child = tree->childs;

	if (child)
		style = xml_read_style (ctxt, child);

	if (style)
		sheet_style_attach (ctxt->sheet, range, style);
}

/*
 * Create an XML subtree of doc equivalent to the given ColRowInfo.
 */
typedef struct
{
	gboolean is_column;
	xmlNodePtr container;
	parse_xml_context_t *ctxt;
} closure_write_colrow;

static gboolean
xml_write_colrow_info (Sheet *sheet, ColRowInfo *info, void *user_data)
{
	closure_write_colrow * closure = user_data;
	xmlNodePtr cur;

	if (closure->is_column)
		cur = xmlNewDocNode (closure->ctxt->doc,
				     closure->ctxt->ns, "ColInfo", NULL);
	else
		cur = xmlNewDocNode (closure->ctxt->doc,
				     closure->ctxt->ns, "RowInfo", NULL);

	if (cur != NULL) {
		xml_set_value_int (cur, "No", info->pos);
		xml_set_value_double (cur, "Unit", info->units);
		xml_set_value_double (cur, "MarginA", info->margin_a_pt);
		xml_set_value_double (cur, "MarginB", info->margin_b);
		xml_set_value_int (cur, "HardSize", info->hard_size);

		xmlAddChild (closure->container, cur);
	}
	return FALSE;
}

/*
 * Create a ColRowInfo equivalent to the XML subtree of doc.
 */
static ColRowInfo *
xml_read_colrow_info (parse_xml_context_t *ctxt, xmlNodePtr tree, ColRowInfo *ret, double *units)
{
	int col = 0;
	int val;
	
	if (!strcmp (tree->name, "ColInfo")){
		col = 1;
	} else if (!strcmp (tree->name, "RowInfo")){
		col = 0;
	} else {
		fprintf (stderr,
			 "xml_read_colrow_info: invalid element type %s, 'ColInfo/RowInfo' expected`\n",
			 tree->name);
		return NULL;
	}
	if (ret == NULL){
		if (col)
			ret = sheet_col_new (ctxt->sheet);
		else
			ret = sheet_row_new (ctxt->sheet);
	}
	if (ret == NULL)
		return NULL;

	ret->units = -1;
	xml_get_value_int (tree, "No", &ret->pos);
	xml_get_value_double (tree, "Unit", units);
	xml_get_value_double (tree, "MarginA", &ret->margin_a_pt);
	xml_get_value_double (tree, "MarginB", &ret->margin_b_pt);
	if (xml_get_value_int (tree, "HardSize", &val))
		ret->hard_size = val;

	return ret;
}

/*
 * Create an XML subtree of doc equivalent to the given Object.
 */
static xmlNodePtr
xml_write_sheet_object (parse_xml_context_t *ctxt, SheetObject *object)
{
	SheetObjectGraphic *sog;
	xmlNodePtr cur = NULL;

	if (!IS_SHEET_GRAPHIC_OBJECT (object)) {
		static gboolean warned = FALSE;
		if (!warned) {
			g_warning ("Discarding non-graphic embedded objects");
			warned = TRUE;
		}
		return NULL;
	}

	sog = SHEET_OBJECT_GRAPHIC (object);

	switch (sog->type) {
	case SHEET_OBJECT_BOX: {
		SheetObjectFilled *sof = SHEET_OBJECT_FILLED (object);
		
		cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Rectangle", NULL);
		if (sof->fill_color != NULL)
			xml_set_value_string (cur, "FillColor", sof->fill_color);
		xml_set_value_int (cur, "Pattern", sof->pattern);
		break;
	}

	case SHEET_OBJECT_OVAL: {
		SheetObjectFilled *sof = SHEET_OBJECT_FILLED (object);
		
		cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Ellipse", NULL);
		if (sof->fill_color != NULL)
			xml_set_value_string (cur, "FillColor", sof->fill_color);
		xml_set_value_int (cur, "Pattern", sof->pattern);
		break;
	}

	case SHEET_OBJECT_ARROW:
		cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Arrow", NULL);
		break;

	case SHEET_OBJECT_LINE:
		cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Line", NULL);
		break;

	default :
		cur = NULL;
	}
	if (!cur)
		return NULL;
	
	xml_set_gnome_canvas_points (cur, "Points", object->bbox_points);
	xml_set_value_int (cur, "Width", sog->width);
	xml_set_value_string (cur, "Color", sog->color);

	return cur;
}

/*
 * Create a Object equivalent to the XML subtree of doc.
 */
static SheetObject *
xml_read_sheet_object (parse_xml_context_t *ctxt, xmlNodePtr tree)
{
	SheetObject *ret;
	SheetObjectFilled *sof;
	char *color;
	char *fill_color;
	int type;
	double x1, y1, x2, y2;
	int width = 1;
	int pattern;

	if (!strcmp (tree->name, "Rectangle")){
		type = SHEET_OBJECT_BOX;
	} else if (!strcmp (tree->name, "Ellipse")){
		type = SHEET_OBJECT_OVAL;
	} else if (!strcmp (tree->name, "Arrow")){
		type = SHEET_OBJECT_ARROW;
	} else if (!strcmp (tree->name, "Line")){
		type = SHEET_OBJECT_LINE;
	} else {
		fprintf (stderr,
			 "xml_read_sheet_object: invalid element type %s, 'Rectangle/Ellipse ...' expected`\n",
			 tree->name);
		return NULL;
	}
	
	color = (char *) xml_value_get (tree, "Color");
	xml_get_coordinates (tree, "Points", &x1, &y1, &x2, &y2);
	xml_get_value_int (tree, "Width", &width);
	if ((type == SHEET_OBJECT_BOX) ||
	    (type == SHEET_OBJECT_OVAL)){
		fill_color = (char *) xml_value_get (tree, "FillColor");
		xml_get_value_int (tree, "Pattern", &pattern);
		ret = sheet_object_create_filled (
			ctxt->sheet, type,
			x1, y1, x2, y2, fill_color, color, width);
		if (ret != NULL){
			sof = SHEET_OBJECT_FILLED (ret);
			sof->pattern = pattern;
		}
	} else {
		ret = sheet_object_create_line (
			ctxt->sheet, type,
			x1, y1, x2, y2, color, width);
	}
	sheet_object_realize (ret);
	return ret;
}

/*
 * Create an XML subtree of doc equivalent to the given Cell.
 */
static xmlNodePtr
xml_write_cell (parse_xml_context_t *ctxt, Cell *cell)
{
	xmlNodePtr cur;
	char      *text;
	xmlChar   *tstr;

	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Cell", NULL);
	xml_set_value_int (cur, "Col", cell->col->pos);
	xml_set_value_int (cur, "Row", cell->row->pos);
	xml_set_value_int (cur, "Style", 0); /* Backwards compatible */

	text = cell_get_content (cell);
	xmlNewChild (cur, ctxt->ns, "Content",
		     (tstr = xmlEncodeEntitiesReentrant (ctxt->doc, text)));
	if (tstr)
		free (tstr);
	g_free (text);

 	text = cell_get_comment(cell);
 	if (text) {
		xmlNewChild(cur, ctxt->ns, "Comment", 
			    (tstr = xmlEncodeEntitiesReentrant (ctxt->doc, text)));
	if (tstr)
		free (tstr);
		g_free(text);
 	}

	return cur;
}

/*
 * Create a Cell equivalent to the XML subtree of doc.
 */
static Cell *
xml_read_cell (parse_xml_context_t *ctxt, xmlNodePtr tree)
{
	Cell *ret;
	xmlNodePtr childs;
	int row = 0, col = 0;
	char *content = NULL;
	char *comment = NULL;
	int  style_idx;
	gboolean style_read = FALSE;
	
	if (strcmp (tree->name, "Cell")) {
		fprintf (stderr,
		 "xml_read_cell: invalid element type %s, 'Cell' expected`\n",
			 tree->name);
		return NULL;
	}
	xml_get_value_int (tree, "Col", &col);
	xml_get_value_int (tree, "Row", &row);

	cell_deep_freeze_redraws ();
/*	cell_deep_freeze_dependencies (); */

	ret = sheet_cell_get (ctxt->sheet, col, row);
	if (ret == NULL)
		ret = sheet_cell_new (ctxt->sheet, col, row);
	if (ret == NULL) {
		cell_deep_thaw_redraws ();
/*		cell_deep_thaw_dependencies (); */
		return NULL;
	}


	/*
	 * This style code is a gross anachronism that slugs performance
	 * in the common case this data won't exist. In the long term all
	 * files will make the 0.41 - 0.42 transition and this can go.
	 * Newer file format includes an index pointer to the Style
	 * Old format includes the Style online
	 */
	if (xml_get_value_int (tree, "Style", &style_idx)) {
		MStyle *mstyle;
		
		style_read = TRUE;
		mstyle = g_hash_table_lookup (ctxt->style_table,
					      GINT_TO_POINTER (style_idx));
		if (mstyle) {
			mstyle_ref (mstyle);
			sheet_style_attach_single (ctxt->sheet, col, row,
						   mstyle);
		} /* else reading a newer version with style_idx == 0 */
	}
	
	childs = tree->childs;
	while (childs != NULL) {
		/*
		 * This style code is a gross anachronism that slugs performance
		 * in the common case this data won't exist. In the long term all
		 * files will make the 0.41 - 0.42 transition and this can go.
		 * This is even older backwards compatibility than 0.41 - 0.42
		 */
		if (!strcmp (childs->name, "Style")) {
			if (!style_read) {
				MStyle *mstyle;
				mstyle = xml_read_style (ctxt, childs);
				if (mstyle)
					sheet_style_attach_single (ctxt->sheet, col, row,
								   mstyle);
			}
		}
		if (!strcmp (childs->name, "Content"))
			content = xmlNodeGetContent (childs);
		if (!strcmp (childs->name, "Comment")) {
			comment = xmlNodeGetContent (childs);
 			if (comment) {
 				cell_set_comment (ret, comment);
 				free (comment);
			}
 		}
		childs = childs->next;
	}
	if (content == NULL)
		content = xmlNodeGetContent (tree);
	if (content != NULL) {
		char *p = content + strlen (content);

		while (p > content) {
			p--;
			if (*p != ' ' && *p != '\n')
				break;
			*p = 0;
		}

		/*
		 * Handle special case of a non corner element of an array
		 * that has already been created.
		 */
		if (ret->parsed_node == NULL ||
		    OPER_ARRAY != ret->parsed_node->oper)
			cell_set_text_simple (ret, content);
		free (content);
	} else
		cell_set_text_simple (ret, "");

	cell_deep_thaw_redraws ();
/*	cell_deep_thaw_dependencies (); */

	return ret;
}

/*
 * Create an XML subtree equivalent to the given cell and add it to the parent
 */
static void
xml_write_cell_to (gpointer key, gpointer value, gpointer data)
{
	parse_xml_context_t *ctxt = (parse_xml_context_t *) data;
	xmlNodePtr cur;

	cur = xml_write_cell (ctxt, (Cell *) value);
	xmlAddChild (ctxt->parent, cur);
}

static xmlNodePtr
xml_write_styles (parse_xml_context_t *ctxt, GList *l)
{
	xmlNodePtr cur;

	if (!l)
		return NULL;

	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Styles", NULL);
	while (l && l->next) {
		StyleRegion *sr = l->data;
		xmlAddChild (cur, xml_write_style_region (ctxt, sr));
		l = g_list_next (l);
	}
	
	return cur;
}

/*
 * Create an XML subtree of doc equivalent to the given Sheet.
 */
static xmlNodePtr
xml_sheet_write (parse_xml_context_t *ctxt, Sheet *sheet)
{
	xmlNodePtr cur;
	xmlNodePtr child;
	xmlNodePtr rows;
	xmlNodePtr cols;
	xmlNodePtr cells;
	xmlNodePtr objects;
	xmlNodePtr printinfo;
	xmlNodePtr styles;
	GList     *style_regions;

	char     str[50];
	xmlChar *tstr;

	/*
	 * General informations about the Sheet.
	 */
	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Sheet", NULL);
	if (cur == NULL)
		return NULL;
	xmlNewChild (cur, ctxt->ns, "Name", 
	             (tstr = xmlEncodeEntitiesReentrant (ctxt->doc, sheet->name)));
	if (tstr)
		free (tstr);
	sprintf (str, "%d", sheet->cols.max_used);
	xmlNewChild (cur, ctxt->ns, "MaxCol", str);
	sprintf (str, "%d", sheet->rows.max_used);
	xmlNewChild (cur, ctxt->ns, "MaxRow", str);
	sprintf (str, "%f", sheet->last_zoom_factor_used);
	xmlNewChild (cur, ctxt->ns, "Zoom", str);

	child = xml_write_names (ctxt, sheet->names);
	if (child)
		xmlAddChild (cur, child);

	/* 
	 * Print Information
	 */
	printinfo = xml_write_print_info (ctxt, sheet->print_info);
	if (printinfo)
		xmlAddChild (cur, printinfo);

	/*
	 * Styles
	 */
	style_regions = sheet_get_style_list (sheet);
	style_regions = g_list_reverse (style_regions);
	styles = xml_write_styles (ctxt, style_regions);
	g_list_free (style_regions);
	if (styles)
		xmlAddChild (cur, styles);

	/*
	 * Cols informations.
	 */
	cols = xmlNewChild (cur, ctxt->ns, "Cols", NULL);
	{
		closure_write_colrow closure;
		closure.is_column = TRUE;
		closure.container = cols;
		closure.ctxt = ctxt;
		sheet_foreach_colrow (sheet, &sheet->cols,
				      0, SHEET_MAX_COLS-1,
				      &xml_write_colrow_info, &closure);
	}

	/*
	 * Rows informations.
	 */
	rows = xmlNewChild (cur, ctxt->ns, "Rows", NULL);
	{
		closure_write_colrow closure;
		closure.is_column = FALSE;
		closure.container = rows;
		closure.ctxt = ctxt;
		sheet_foreach_colrow (sheet, &sheet->rows,
				      0, SHEET_MAX_ROWS-1,
				      &xml_write_colrow_info, &closure);
	}

	/*
	 * Objects
	 * NOTE: seems that objects == NULL while current_object != NULL
	 * is possible
	 */
	if (sheet->objects != NULL) {
		GList * l = sheet->objects;
		objects = xmlNewChild (cur, ctxt->ns, "Objects", NULL);
		while (l) {
			child = xml_write_sheet_object (ctxt, l->data);
			if (child)
				xmlAddChild (objects, child);
			l = l->next;
		}
	} else if (sheet->current_object != NULL) {
		objects = xmlNewChild (cur, ctxt->ns, "Objects", NULL);
		child = xml_write_sheet_object (ctxt, sheet->current_object);
		if (child)
			xmlAddChild (objects, child);
	}
	/*
	 * Cells informations
	 */
	cells = xmlNewChild (cur, ctxt->ns, "Cells", NULL);
	ctxt->parent = cells;
	g_hash_table_foreach (sheet->cell_hash, xml_write_cell_to, ctxt);

	return cur;
}

static void
xml_read_styles (parse_xml_context_t *ctxt, xmlNodePtr tree)
{
	xmlNodePtr child;
	xmlNodePtr regions;
	
	child = xml_search_child (tree, "Styles");
	if (child == NULL)
		return;
	
	for (regions = child->childs; regions; regions = regions->next)
		xml_read_style_region (ctxt, regions);
}

static void
xml_read_cols_info (parse_xml_context_t *ctxt, Sheet *sheet, xmlNodePtr tree)
{
	xmlNodePtr child, cols;
	ColRowInfo *info;

	child = xml_search_child (tree, "Cols");
	if (child == NULL)
		return;
	
	for (cols = child->childs; cols; cols = cols->next){
		double units;
		
		info = xml_read_colrow_info (ctxt, cols, NULL, &units);
		if (!info)
			continue;
		sheet_col_add (sheet, info);
		sheet_col_set_width_units (ctxt->sheet, info->pos, units);
	}
}

static void
xml_read_rows_info (parse_xml_context_t *ctxt, Sheet *sheet, xmlNodePtr tree)
{
	xmlNodePtr child, rows;
	ColRowInfo *info;

	child = xml_search_child (tree, "Rows");
	if (child == NULL)
		return;
	
	for (rows = child->childs; rows; rows = rows->next){
		double units;
		
		info = xml_read_colrow_info (ctxt, rows, NULL, &units);
		if (!info)
			continue;
		sheet_row_add (sheet, info);
		sheet_row_set_height_units (ctxt->sheet, info->pos, units, info->hard_size);
	}
}

static void
xml_read_cell_styles (parse_xml_context_t *ctxt, xmlNodePtr tree)
{
	xmlNodePtr styles, child;

	ctxt->style_table = g_hash_table_new (g_direct_hash, g_direct_equal);
	
	child = xml_search_child (tree, "CellStyles");
	if (child == NULL)
		return;
	
	for (styles = child->childs; styles; styles = styles->next) {
		MStyle *mstyle;
		int style_idx;
		
		if (xml_get_value_int (styles, "No", &style_idx)) {
			mstyle = xml_read_style (ctxt, styles);
			g_hash_table_insert (
				ctxt->style_table,
				GINT_TO_POINTER (style_idx),
				mstyle);
		}
	}
}
static void
destroy_style (gpointer key, gpointer value, gpointer data)
{
	mstyle_unref (value);
}

static void
xml_dispose_read_cell_styles (parse_xml_context_t *ctxt)
{
	g_hash_table_foreach (ctxt->style_table, destroy_style, NULL);
	g_hash_table_destroy (ctxt->style_table);
}

/*
 * Create a Sheet equivalent to the XML subtree of doc.
 */
static Sheet *
xml_sheet_read (parse_xml_context_t *ctxt, xmlNodePtr tree)
{
	xmlNodePtr child;
	/* xmlNodePtr styles; */
	xmlNodePtr cells;
	xmlNodePtr objects;
	Sheet *ret = NULL;
	char *val;

	if (strcmp (tree->name, "Sheet")){
		fprintf (stderr,
			 "xml_sheet_read: invalid element type %s, 'Sheet' expected\n",
			 tree->name);
	}
	child = tree->childs;

	/*
	 * Get the name of the sheet.  If it does exist, use the existing
	 * name, otherwise create a sheet (ie, for the case of only reading
	 * a new sheet).
	 */
	val = xml_value_get (tree, "Name");
	if (val != NULL){
		ret = workbook_sheet_lookup (ctxt->wb, (const char *) val);
		if (ret == NULL)
			ret = sheet_new (ctxt->wb, (const char *) val);
		free (val);
	} 

	if (ret == NULL)
		return NULL;

	ctxt->sheet = ret;

	xml_get_value_int (tree, "MaxCol", &ret->cols.max_used);
	xml_get_value_int (tree, "MaxRow", &ret->rows.max_used);
	xml_get_value_double (tree, "Zoom", &ret->last_zoom_factor_used);

	xml_read_print_info (ctxt, tree);
	xml_read_styles (ctxt, tree);
	xml_read_cell_styles (ctxt, tree);
	xml_read_cols_info (ctxt, ret, tree);
	xml_read_rows_info (ctxt, ret, tree);

	child = xml_search_child (tree, "Names");
	if (child)
		xml_read_names (ctxt, child, NULL, ret);

	child = xml_search_child (tree, "Objects");
	if (child != NULL){
		objects = child->childs;
		while (objects != NULL){
			xml_read_sheet_object (ctxt, objects);
			objects = objects->next;
		}
	}
	child = xml_search_child (tree, "Cells");
	if (child != NULL){
		cells = child->childs;
		while (cells != NULL){
			xml_read_cell (ctxt, cells);
			cells = cells->next;
		}
	}
	xml_dispose_read_cell_styles (ctxt);

	/* Initialize the ColRowInfo's ->pixels data */
	sheet_set_zoom_factor (ret, ret->last_zoom_factor_used);
	return ret;
}

/*
 * Create an XML subtree of doc equivalent to the given Workbook.
 */
static xmlNodePtr
xml_workbook_write (parse_xml_context_t *ctxt, Workbook *wb)
{
	xmlNodePtr cur;
	xmlNodePtr child;
	GList *sheets;
	char *oldlocale;
	
	/*
	 * General informations about the Sheet.
	 */
	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, "Workbook", NULL);	/* the Workbook name !!! */
	if (cur == NULL)
		return NULL;

	oldlocale = g_strdup (setlocale (LC_NUMERIC, NULL));
	setlocale (LC_NUMERIC, "C");
	
	child = xml_write_summary (ctxt, wb->summary_info);
	if (child)
		xmlAddChild (cur, child);

	child = xml_write_names (ctxt, wb->names);
	if (child)
		xmlAddChild (cur, child);

/*	child = xml_write_style (ctxt, &wb->style, -1);
	if (child)
	xmlAddChild (cur, child);*/

	child = xmlNewDocNode (ctxt->doc, ctxt->ns, "Geometry", NULL);
	xml_set_value_int (child, "Width", wb->toplevel->allocation.width);
	xml_set_value_int (child, "Height", wb->toplevel->allocation.height);
	xmlAddChild (cur, child);

	/*
	 * Cells informations
	 */
	child = xmlNewChild (cur, ctxt->ns, "Sheets", NULL);
	ctxt->parent = child;

	sheets = workbook_sheets (wb);
	while (sheets) {
		xmlNodePtr cur, parent;
		Sheet *sheet = sheets->data;
		
		parent = ctxt->parent;
		cur = xml_sheet_write (ctxt, sheet);
		ctxt->parent = parent;
		xmlAddChild (parent, cur);

		sheets = g_list_next (sheets);
	}
	g_list_free (sheets);

	setlocale (LC_NUMERIC, oldlocale);
	g_free (oldlocale);

	return cur;
}

static void
xml_sheet_create (parse_xml_context_t *ctxt, xmlNodePtr tree)
{
	char *val;
	xmlNodePtr child;
	
	if (strcmp (tree->name, "Sheet")){
		fprintf (stderr,
			 "xml_sheet_create: invalid element type %s, 'Sheet' expected\n",
			 tree->name);
		return;
	}
	child = tree->childs;
	val = xml_value_get (tree, "Name");
	if (val != NULL){
		Sheet *sheet;
		
		sheet = sheet_new (ctxt->wb, (const char *) val);
		workbook_attach_sheet (ctxt->wb, sheet);
		free (val);
	}
	return;
}

/*
 * Create a Workbook equivalent to the XML subtree of doc.
 */
static gboolean
xml_workbook_read (Workbook *wb, parse_xml_context_t *ctxt, xmlNodePtr tree)
{
	Sheet *sheet;
	xmlNodePtr child, c;
	char *oldlocale;
	
	if (strcmp (tree->name, "Workbook")){
		fprintf (stderr,
			 "xml_workbook_read: invalid element type %s, 'Workbook' expected`\n",
			 tree->name);
		return FALSE;
	}
	ctxt->wb = wb;

	oldlocale = g_strdup (setlocale (LC_NUMERIC, NULL));
	setlocale (LC_NUMERIC, "C");
	
	child = xml_search_child (tree, "Summary");
	if (child)
		xml_read_summary (ctxt, child, wb->summary_info);

	child = xml_search_child (tree, "Geometry");
	if (child){
		int width, height;

		xml_get_value_int (child, "Width", &width);
		xml_get_value_int (child, "Height", &height);
/*      gtk_widget_set_usize(wb->toplevel, width, height); */
	}

/*	child = xml_search_child (tree, "Style");
	if (child != NULL)
	xml_read_style (ctxt, child, &wb->style);*/

	child = xml_search_child (tree, "Sheets");
	if (child == NULL)
		return FALSE;

	/*
	 * Pass 1: Create all the sheets, to make sure
	 * all of the references to forward sheets are properly
	 * handled
	 */
	c = child->childs;
	while (c != NULL){
		xml_sheet_create (ctxt, c);
		c = c->next;
	}

	/*
	 * Now read names which can have inter-sheet references
	 * to these sheet titles
	 */
	child = xml_search_child (tree, "Names");
	if (child)
		xml_read_names (ctxt, child, wb, NULL);

	child = xml_search_child (tree, "Sheets");
	/*
	 * Pass 2: read the contents
	 */
	c = child->childs;
	while (c != NULL) {
		sheet = xml_sheet_read (ctxt, c);
		c = c->next;
	}

	setlocale (LC_NUMERIC, oldlocale);
	g_free (oldlocale);
	
	return TRUE;
}

/*
 * We parse and do some limited validation of the XML file, if this
 * passes, then we return TRUE
 */
static gboolean
xml_probe (const char *filename)
{
	xmlDocPtr res;
	xmlNsPtr gmr;

	res = xmlParseFile (filename);
	if (res == NULL) {
		/* FIXME: make probing silent.  */
		fprintf (stderr,
			 "File `%s' does not look like an xml-document\n"
			 "Please ignore complaints about `Extra content at the end of the document'\n",
			 filename);
		return FALSE;
	}

	if (res->root == NULL) {
		xmlFreeDoc (res);
		return FALSE;
	}

	gmr = xmlSearchNsByHref (res, res->root, "http://www.gnome.org/gnumeric/");
	if (gmr == NULL)
		gmr = xmlSearchNsByHref (res, res->root, "http://www.gnome.org/gnumeric/v2");
	
	if (res->root->name == NULL || strcmp (res->root->name, "Workbook") || (gmr == NULL)){
		xmlFreeDoc (res);
		return FALSE;
	}
	xmlFreeDoc (res);
	return TRUE;
}

/*
 * Open an XML file and read a Sheet
 * One parse the XML file, getting a tree, then analyze the tree to build
 * the actual in-memory structure.
 */
Sheet *
gnumeric_xml_read_sheet (const char *filename)
{
	Sheet *sheet;
	xmlDocPtr res;
	xmlNsPtr gmr;
	parse_xml_context_t ctxt;

	g_return_val_if_fail (filename != NULL, NULL);

	/*
	 * Load the file into an XML tree.
	 */
	res = xmlParseFile (filename);
	if (res == NULL)
		return NULL;
	if (res->root == NULL){
		fprintf (stderr, "gnumeric_xml_read_sheet %s: tree is empty\n", filename);
		xmlFreeDoc (res);
		return NULL;
	}
	/*
	 * Do a bit of checking, get the namespaces, and check the top elem.
	 */
	gmr = xmlSearchNsByHref (res, res->root, "http://www.gnome.org/gnumeric/");
	if (strcmp (res->root->name, "Sheet") || (gmr == NULL)){
		fprintf (stderr, "gnumeric_xml_read_sheet %s: not a Sheet file\n",
			 filename);
		xmlFreeDoc (res);
		return NULL;
	}
	ctxt.doc = res;
	ctxt.ns = gmr;
	sheet = xml_sheet_read (&ctxt, res->root);
	
	xmlFreeDoc (res);

	return sheet;
}

/*
 * Save a Sheet in an XML file
 * One build an in-memory XML tree and save it to a file.
 * returns 0 in case of success, -1 otherwise.
 */
int
gnumeric_xml_write_sheet (Sheet *sheet, const char *filename)
{
	parse_xml_context_t ctxt;
	xmlDocPtr xml;
	xmlNsPtr gmr;
	int ret;

	g_return_val_if_fail (sheet != NULL, -1);
	g_return_val_if_fail (IS_SHEET (sheet), -1);
	g_return_val_if_fail (filename != NULL, -1);

	/*
	 * Create the tree
	 */
	xml = xmlNewDoc ("1.0");
	if (xml == NULL){
		return -1;
	}
	gmr = xmlNewGlobalNs (xml, "http://www.gnome.org/gnumeric/", "gmr");
	ctxt.doc = xml;
	ctxt.ns = gmr;
	
	xml->root = xml_sheet_write (&ctxt, sheet);

	/*
	 * Dump it.
	 */
	xmlSetDocCompressMode (xml, 9);
	ret = xmlSaveFile (filename, xml);
	xmlFreeDoc (xml);
	if (ret < 0)
		return -1;
	return 0;
}

/*
 * Open an XML file and read a Workbook
 * One parse the XML file, getting a tree, then analyze the tree to build
 * the actual in-memory structure.
 */

gboolean
gnumeric_xml_read_workbook (Workbook *wb, const char *filename)
{
	xmlDocPtr res;
	xmlNsPtr gmr;
	parse_xml_context_t ctxt;

	g_return_val_if_fail (filename != NULL, FALSE);

	/*
	 * Load the file into an XML tree.
	 */
	res = xmlParseFile (filename);
	if (res == NULL)
		return FALSE;
	if (res->root == NULL){
		fprintf (stderr, "gnumeric_xml_read_workbook %s: tree is empty\n", filename);
		xmlFreeDoc (res);
		return FALSE;
	}
	/*
	 * Do a bit of checking, get the namespaces, and chech the top elem.
	 */
	gmr = xmlSearchNsByHref (res, res->root, "http://www.gnome.org/gnumeric/");
	if (gmr == NULL)
		gmr = xmlSearchNsByHref (res, res->root, "http://www.gnome.org/gnumeric/v2");
	if (strcmp (res->root->name, "Workbook") || (gmr == NULL)){
		fprintf (stderr, "gnumeric_xml_read_workbook %s: not an Workbook file\n",
			 filename);
		xmlFreeDoc (res);
		return FALSE;
	}
	ctxt.doc = res;
	ctxt.ns = gmr;
	xml_workbook_read (wb, &ctxt, res->root);
	workbook_set_filename (wb, (char *) filename);
	workbook_recalc_all (wb);

	xmlFreeDoc (res);
	return TRUE;
}

/*
 * Save a Workbook in an XML file
 * One build an in-memory XML tree and save it to a file.
 * returns 0 in case of success, -1 otherwise.
 */

int
gnumeric_xml_write_workbook (Workbook *wb, const char *filename)
{
	xmlDocPtr xml;
	xmlNsPtr gmr;
	parse_xml_context_t ctxt;
	int ret;

	g_return_val_if_fail (wb != NULL, -1);
	g_return_val_if_fail (filename != NULL, -1);

	/*
	 * Create the tree
	 */
	xml = xmlNewDoc ("1.0");
	if (xml == NULL){
		return -1;
	}
	gmr = xmlNewGlobalNs (xml, "http://www.gnome.org/gnumeric/v2", "gmr");
	ctxt.doc = xml;
	ctxt.ns = gmr;

	xml->root = xml_workbook_write (&ctxt, wb);

	/*
	 * Dump it.
	 */
	xmlSetDocCompressMode (xml, 9);
	ret = xmlSaveFile (filename, xml);
	xmlFreeDoc (xml);
	if (ret < 0)
		return -1;
	return 0;
}

void
xml_init (void)
{
	char *desc = _("Gnumeric XML file format");
	
	file_format_register_open (50, desc, xml_probe, gnumeric_xml_read_workbook);
	file_format_register_save (".gnumeric", desc, gnumeric_xml_write_workbook);
}