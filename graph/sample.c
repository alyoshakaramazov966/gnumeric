/*
 * Test code for the graphics engine
 *
 * Miguel de Icaza (miguel@gnu.org)
 */
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo/gnome-bonobo.h>
#include "Graph.h"
#include "vector.h"

/*
 * This is the handle to the graphics object server
 */
GnomeObjectClient *object;

/*
 * Interface pointer for the Layout interface
 */
GNOME_Graph_Layout layout;

/*
 * Interface pointer for the actual chart
 */
GNOME_Graph_Chart  chart;

CORBA_Environment ev;

#define VECS 5
#define VECLEN 4

struct {
	double data [4];
	Vector *vector;
} vecs [VECS] = {
	{ { 0.0, 1.0, 2.0, 3.0 }, NULL },
	{ { 0.5, 8, 2, 5 }, NULL },
	{ { 2, 3, 4, 5 }, NULL },
	{ { 3, 4, 5, 2 }, NULL }, 
	{ { 4, 3, -2, 1 }, NULL }
};

GNOME_Gnumeric_DoubleVec *
vec_get (Vector *vec, CORBA_short low, CORBA_short high, void  *data)
{
	int idx = GPOINTER_TO_UINT (data);
	int i;
	
	GNOME_Gnumeric_DoubleVec *res;

	res = GNOME_Gnumeric_DoubleVec__alloc ();
	if (res == NULL)
		return NULL;
	res->_maximum = 4;
	res->_length = 4;
	res->_buffer = CORBA_sequence_CORBA_double_allocbuf (4);

	for (i = 0; i < VECLEN; i++)
		res->_buffer [i] = vecs [idx].data [i];

	return res;
}

GNOME_Gnumeric_VecValueVec *
vec_val_get (Vector *vec, CORBA_short low, CORBA_short high, void  *data)
{
	g_error ("not implemented");
	return NULL;
}

gboolean
vec_set (Vector *vec, CORBA_short pos, double val, CORBA_Environment *ev, void *data)
{
	return FALSE;
}

CORBA_short
vec_len (Vector *vec, void *data)
{
	return VECLEN;
}

static void
create_vectors (void)
{
	int i;
	
	for (i = 0; i < VECS; i++){
		Vector *v;

		v = vector_new (vec_get, vec_val_get, vec_set, vec_len, GINT_TO_POINTER (i));
		vecs [i].vector = v;
	}
}

static void
quit_cmd ()
{

	/*
	 * Unref the layout interface
	 */
	GNOME_Unknown_unref ((GNOME_Unknown) layout, &ev);

	GNOME_Unknown_unref ((GNOME_Unknown) chart, &ev);
	
	/*
	 * Destroy our initial handle
	 */
	gtk_object_destroy (GTK_OBJECT (object));

	gtk_main_quit ();
}

#define make(name, method, value) \
static void \
name (GtkWidget *widget) \
{ \
	GNOME_Graph_Chart__##method (chart, GNOME_Graph_##value, &ev);\
}

#define ITEM(l,x) { GNOME_APP_UI_ITEM, l, NULL, x }

static GnomeUIInfo sample_menu_file [] = {
	GNOMEUIINFO_MENU_EXIT_ITEM (quit_cmd, NULL),
	GNOMEUIINFO_END
};

make (chart_type_clustered,    set_chart_type, CHART_TYPE_CLUSTERED);
make (chart_type_stacked,      set_chart_type, CHART_TYPE_STACKED);
make (chart_type_stacked_full, set_chart_type, CHART_TYPE_STACKED_FULL);
make (chart_type_scatter,      set_chart_type, CHART_TYPE_SCATTER);

static GnomeUIInfo sample_chart_type_menu [] = {
	ITEM ("Clustered",    chart_type_clustered),
	ITEM ("Stacked",      chart_type_stacked),
	ITEM ("Stacked 100%", chart_type_stacked_full),
	ITEM ("Scatter",      chart_type_scatter),
	GNOMEUIINFO_END
};

make (plot_mode_colbars,       set_plot_mode, PLOT_COLBAR);
make (plot_mode_lines,         set_plot_mode, PLOT_LINES);
make (plot_mode_area,          set_plot_mode, PLOT_AREA);

static GnomeUIInfo sample_plot_mode_menu [] = {
	ITEM ("Columns/bars", plot_mode_colbars),
	ITEM ("Lines",        plot_mode_lines),
	ITEM ("Area",         plot_mode_area),
	GNOMEUIINFO_END
};

make (colbar_mode_flat,        set_col_bar_mode, COLBAR_FLAT);

static GnomeUIInfo sample_colbar_mode_menu [] = {
	ITEM ("Flat",         colbar_mode_flat),
	GNOMEUIINFO_END
};

make (dir_mode_columns, set_direction, DIR_COL);
make (dir_mode_bars,    set_direction, DIR_BAR);

static GnomeUIInfo sample_dir_mode_menu [] = {
	ITEM ("Columns",      dir_mode_columns),
	ITEM ("Bars",         dir_mode_bars),
	GNOMEUIINFO_END
};

make (line_mode_plain,   set_line_mode, LINE_PLAIN);
make (line_mode_markers, set_line_mode, LINE_MARKERS);

static GnomeUIInfo sample_line_mode_menu [] = {
	ITEM ("Plain",        line_mode_plain),
	ITEM ("Markers",      line_mode_markers),
	GNOMEUIINFO_END
};

make (scatter_mode_none, set_scatter_mode, SCATTER_NONE);
make (scatter_mode_points, set_scatter_mode, SCATTER_POINTS);

static GnomeUIInfo sample_scatter_mode_menu [] = {
	ITEM ("None",      scatter_mode_none),
	ITEM ("Points",    scatter_mode_points),
	GNOMEUIINFO_END
};

make (scatter_conn_none,  set_scatter_conn, SCATTER_CONN_NONE);
make (scatter_conn_smooth,  set_scatter_conn, SCATTER_CONN_SMOOTH);
make (scatter_conn_lines,  set_scatter_conn, SCATTER_CONN_LINES);

static GnomeUIInfo sample_scatter_connection_menu [] = {
	ITEM ("No Connections",    scatter_conn_none),
	ITEM ("Smooth connection", scatter_conn_smooth),
	ITEM ("Line connections",  scatter_conn_lines),
	GNOMEUIINFO_END
};

static GnomeUIInfo sample_menu_view [] = {
	{ GNOME_APP_UI_SUBTREE, N_("Chart type"), NULL, sample_chart_type_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Plot mode"), NULL, sample_plot_mode_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Col/bar mode"), NULL, sample_colbar_mode_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Direction mode"), NULL, sample_dir_mode_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Line mode"), NULL, sample_line_mode_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Scatter mode"), NULL, sample_scatter_mode_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Scatter connection"), NULL, sample_scatter_connection_menu },
	GNOMEUIINFO_END
};

static GnomeUIInfo sample_menu [] = {
        GNOMEUIINFO_MENU_FILE_TREE (sample_menu_file),
	GNOMEUIINFO_MENU_VIEW_TREE (sample_menu_view),

	GNOMEUIINFO_END
};

static void
create_gui (GtkWidget *content)
{
	GnomeUIHandlerMenuItem *list;
	GnomeUIHandler *uih;
	GtkWidget *toplevel;
	
	toplevel = gnome_app_new ("Sample", "Sample");
	gtk_widget_show (toplevel);

	gnome_app_set_contents (GNOME_APP (toplevel), content);
	gtk_widget_show (content);

	/*
	 * Menus
	 */
	uih = gnome_ui_handler_new ();
	gnome_ui_handler_set_app (uih, GNOME_APP (toplevel));
	gnome_ui_handler_create_menubar (uih);
	list = gnome_ui_handler_menu_parse_uiinfo_list_with_data (sample_menu, NULL);
	gnome_ui_handler_menu_add_list (uih, "/", list);
	gnome_ui_handler_menu_free_list (list);
}

static GtkWidget *
create_test ()
{
	GtkWidget *view_widget;
	GnomeClientSite *client_site;
	GnomeViewFrame *view_frame;
	GnomeContainer *container;
	int i;
	
	container = gnome_container_new ();
	client_site = gnome_client_site_new (container);
	
	object = gnome_object_activate ("GOADID:embeddable:Graph:Layout", 0);
	if (!object){
		printf ("Can not activate object\n");
		exit (1);
	}
	
	layout = gnome_object_query_interface (GNOME_OBJECT (object), "IDL:GNOME/Graph/Layout:1.0");
	if (layout == CORBA_OBJECT_NIL)
		g_error ("interface Layout not supported");

	chart = GNOME_Graph_Layout_get_chart (layout, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_error ("Could not retrieve the chart from the Layout object");
	
	for (i = 0; i < VECS; i++){
		GNOME_Graph_Layout_add_series (
			layout,
			gnome_object_corba_objref (GNOME_OBJECT (vecs [i].vector)),
			&ev);
		if (ev._major != CORBA_NO_EXCEPTION){
			g_error ("Error while setting the vectors");
		}
	}
				       
	/*
	 * User interface
	 */
	gnome_client_site_bind_embeddable (client_site, object);

	view_frame = gnome_client_site_new_view (client_site);
	view_widget = gnome_view_frame_get_wrapper (view_frame);

	return view_widget;
}

int
main (int argc, char *argv [])
{
	GtkWidget *graph_widget;
	
	CORBA_exception_init (&ev);
	gnome_CORBA_init ("Sample tester", "1.0", &argc, argv, 0, &ev);

	/*
	 * Initialize Bonobo
	 */
	bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	/*
	 * Start accepting CORBA requests
	 */
	bonobo_activate ();

	/*
	 * Program setup.
	 */
	create_vectors ();
	graph_widget = create_test ();
	create_gui (graph_widget);

	/*
	 * Process user requests
	 */
	printf ("Main looping...\n");
	gtk_main ();
	
	CORBA_exception_free (&ev);

	printf ("Terminating\n");
	return 0;
}