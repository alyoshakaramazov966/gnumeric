/*
 * fn-database.c:  Built in database functions and functions registration
 *
 * Author:
 *  Jukka-Pekka Iivonen (iivonen@iki.fi)
 */
#include <config.h>
#include <math.h>
#include <glib.h>
#include "utils.h"
#include "func.h"
#include "sheet.h"

/* Type definitions */

typedef struct {
        int    row;
        GSList *conditions;
} database_criteria_t;


/* Callback functions */

int
criteria_test_equal(Value *x, Value *y)
{
        if (VALUE_IS_NUMBER(x) && VALUE_IS_NUMBER(y))
	        if (value_get_as_float (x) == value_get_as_float (y))
		        return 1;
		else
		        return 0;
	else if (x->type == VALUE_STRING && y->type == VALUE_STRING
		 && g_strcasecmp (x->v.str->str, y->v.str->str) == 0)
	        return 1;
	else
	        return 0;
}

int
criteria_test_unequal(Value *x, Value *y)
{
        if (VALUE_IS_NUMBER(x) && VALUE_IS_NUMBER(y))
	        if (value_get_as_float (x) != value_get_as_float (y))
		        return 1;
		else
		        return 0;
	else if (x->type == VALUE_STRING && y->type == VALUE_STRING
		 && g_strcasecmp(x->v.str->str, y->v.str->str) != 0)
	        return 1;
	else
	        return 0;
}

int
criteria_test_less(Value *x, Value *y)
{
        if (VALUE_IS_NUMBER(x) && VALUE_IS_NUMBER(y))
	        if (value_get_as_float (x) < value_get_as_float (y))
		        return 1;
		else
		        return 0;
	else
	        return 0;
}

int
criteria_test_greater(Value *x, Value *y)
{
        if (VALUE_IS_NUMBER(x) && VALUE_IS_NUMBER(y))
	        if (value_get_as_float (x) > value_get_as_float (y))
		        return 1;
		else
		        return 0;
	else
	        return 0;
}

int
criteria_test_less_or_equal(Value *x, Value *y)
{
        if (VALUE_IS_NUMBER(x) && VALUE_IS_NUMBER(y))
	        if (value_get_as_float (x) <= value_get_as_float (y))
		        return 1;
		else
		        return 0;
	else
	        return 0;
}

int
criteria_test_greater_or_equal(Value *x, Value *y)
{
        if (VALUE_IS_NUMBER(x) && VALUE_IS_NUMBER(y))
	        if (value_get_as_float (x) >= value_get_as_float (y))
		        return 1;
		else
		        return 0;
	else
	        return 0;
}


/* Finds a column index of a field. 
 */
static int
find_column_of_field (const EvalPosition *ep, Value *database, Value *field)
{
        Sheet *sheet;
        Cell  *cell;
	gchar *field_name;
	int   begin_col, end_col, row, n, column;
	int   offset;

	offset = database->v.cell_range.cell_b.col -
	  database->v.cell_range.cell_a.col;

	if (field->type == VALUE_INTEGER) 
	        return value_get_as_int (field) + offset - 1;

	if (field->type != VALUE_STRING)
	        return -1;

	sheet = eval_sheet (database->v.cell_range.cell_a.sheet, ep->sheet);
	field_name = value_get_as_string (field);
	column = -1;

	/* find the column that is labeled after `field_name' */
	begin_col = database->v.cell_range.cell_a.col;
	end_col = database->v.cell_range.cell_b.col;
	row = database->v.cell_range.cell_a.row;

	for (n = begin_col; n <= end_col; n++) {
		char *txt;
		gboolean match;

	        cell = sheet_cell_get(sheet, n, row);
		if (cell == NULL)
		        continue;

		txt = cell_get_text (cell);
		match = (g_strcasecmp (field_name, txt) == 0);
		g_free (txt);
		if (match) {
		        column = n;
			break;
		}
	}

	g_free (field_name);
	return column;
}

/* Frees the allocated memory.
 */
static void
free_criterias(GSList *criterias)
{
        GSList *list = criterias;

        while (criterias != NULL) {
		GSList *l;
	        database_criteria_t *criteria = criterias->data;

		for (l = criteria->conditions; l; l = l->next) {
			func_criteria_t *cond = l->data;
			value_release (cond->x);
			g_free (cond);
		}

		g_slist_free(criteria->conditions);
		g_free (criteria);
	        criterias = criterias->next;
	}
	g_slist_free(list);
}

void
parse_criteria(const char *criteria, criteria_test_fun_t *fun,
	       Value **test_value)
{
	char    *p;
	float_t tmp;
	int     len;

        if (strncmp(criteria, "<=", 2) == 0) {
	        *fun = (criteria_test_fun_t) criteria_test_less_or_equal;
		len=2;
	} else if (strncmp(criteria, ">=", 2) == 0) {
	        *fun = (criteria_test_fun_t) criteria_test_greater_or_equal;
		len=2;
	} else if (strncmp(criteria, "<>", 2) == 0) {
	        *fun = (criteria_test_fun_t) criteria_test_unequal;
		len=2;
	} else if (*criteria == '<') {
	        *fun = (criteria_test_fun_t) criteria_test_less;
		len=1;
	} else if (*criteria == '=') {
	        *fun = (criteria_test_fun_t) criteria_test_equal;
		len=1;
	} else if (*criteria == '>') {
	        *fun = (criteria_test_fun_t) criteria_test_greater;
		len=1;
	} else {
	        *fun = (criteria_test_fun_t) criteria_test_equal;
		len=0;
	}
	
	tmp = strtod(criteria+len, &p);

	if (p == criteria+len || *p != '\0')
	        *test_value = value_new_string (criteria+len);
	else
	        *test_value = value_new_float (tmp);

}

/* Parses the criteria cell range.
 */
static GSList *
parse_database_criteria (const EvalPosition *ep, Value *database,
			 Value *criteria)
{
	Sheet               *sheet;
	database_criteria_t *new_criteria;
	GSList              *criterias;
	GSList              *conditions;
	Cell                *cell;

        int   i, j;
	int   b_col, b_row, e_col, e_row;
	int   *field_ind;

	func_criteria_t *cond;
	gchar           *cell_str;

	sheet = eval_sheet (database->v.cell_range.cell_a.sheet, ep->sheet);
	b_col = criteria->v.cell_range.cell_a.col;
	b_row = criteria->v.cell_range.cell_a.row;
	e_col = criteria->v.cell_range.cell_b.col;
	e_row = criteria->v.cell_range.cell_b.row;

	conditions = NULL;
	criterias = NULL;

	field_ind = g_new(int, (e_col-b_col+1));

	/* Find the index numbers for the columns of criterias */
	for (i=b_col; i<=e_col; i++) {
	        cell = sheet_cell_get(sheet, i, b_row);
		if (cell == NULL || cell->value == NULL)
		        continue;
		field_ind[i-b_col] =
		        find_column_of_field (ep, database, cell->value);
		if (field_ind[i-b_col] == -1) {
		        g_free(field_ind);
			return NULL;
		}
	}

	for (i=b_row+1; i<=e_row; i++) {
	        new_criteria = g_new(database_criteria_t, 1);
		conditions = NULL;

		for (j=b_col; j<=e_col; j++) {
		        cell = sheet_cell_get(sheet, j, i);
			if (cell == NULL || cell->value == NULL)
			        continue;

			cond = g_new(func_criteria_t, 1);

			/* Equality condition (in number format) */
			if (VALUE_IS_NUMBER(cell->value)) {
			        cond->x = value_duplicate (cell->value);
				cond->fun =
				  (criteria_test_fun_t) criteria_test_equal;
				cond->column = field_ind[j-b_col];
				conditions = g_slist_append(conditions, cond);
				continue;
			}

			/* Other conditions (in string format) */
			cell_str = cell_get_text(cell);
			parse_criteria(cell_str, &cond->fun, &cond->x);
			cond->column = field_ind[j-b_col];
			g_free (cell_str);
	    
			conditions = g_slist_append(conditions, cond);
		}

		new_criteria->conditions = conditions;
		criterias = g_slist_append(criterias, new_criteria);
	}

	g_free(field_ind);
	return criterias;
}

/* Finds the cells from the given column that match the criteria.
 */
static GSList *
find_cells_that_match (const EvalPosition *ep, Value *database,
		       int field, GSList *criterias)
{
        Sheet  *sheet;
	GSList *current, *conditions, *cells;
        int    row, first_row, last_row, add_flag;
	sheet = eval_sheet (database->v.cell_range.cell_a.sheet, ep->sheet);
	last_row = database->v.cell_range.cell_b.row;
	cells = NULL;
	first_row = database->v.cell_range.cell_a.row + 1;

	for (row=first_row; row<=last_row; row++) {
	       Cell   *cell, *test_cell;

	       cell = sheet_cell_get(sheet, field, row);
	       if (cell == NULL || cell->value == NULL)
		       continue;
	       current = criterias;
	       add_flag = 1;
	       for (current = criterias; current != NULL;
		    current=current->next) {
		       database_criteria_t *current_criteria;

		       add_flag = 1;
		       current_criteria = current->data;
		       conditions = current_criteria->conditions;

		       while (conditions != NULL) {
			       func_criteria_t *cond = conditions->data;

			       test_cell =
				 sheet_cell_get(sheet, cond->column, row);
			       if (test_cell == NULL ||
				   test_cell->value == NULL)
				       continue;

			       if (! cond->fun (test_cell->value, cond->x)) {
				       add_flag = 0;
				       break;
			       }
			       conditions = conditions->next;
		       }

		       if (add_flag)
			       break;
	       }
	       if (add_flag)
		       cells = g_slist_append(cells, cell);
	}

	return cells;
}

#define DB_ARGUMENT_HELP \
	   "@database is a range of cells in which rows of related " \
	   "information are records and columns of data are fields. " \
	   "The first row of a database contains labels for each column. " \
	   "\n" \
	   "@field specifies which column is used in the function.  If " \
	   "@field is an integer, i.e. 2, the second column is used. Field " \
	   "can also be the label of a column.  For example, ``Age'' refers " \
	   "to the column with the label ``Age'' in @database range. " \
	   "\n" \
	   "@criteria is the range of cells which contains the specified " \
	   "conditions.  The first row of a @criteria should contain the " \
	   "labels of the fields for which the criterias are for.  Cells " \
	   "below the labels specify conditions, for example, ``>3'' or " \
	   "``<9''.  Equality condition can be given simply by specifing a " \
	   "value, e.g. ``3'' or ``John''.  Each row in @criteria specifies " \
	   "a separate condition, i.e. if a row in @database matches with " \
	   "one of the rows in @criteria then that row is counted in " \
	   "(technically speaking boolean OR between the rows in " \
	   "@criteria).  If @criteria specifies more than one columns then " \
	   "each of the conditions in these columns should be true that " \
	   "the row in @database matches (again technically speaking " \
	   "boolean AND between the columns in each row in @criteria). " \
           "\n" \
	   "@EXAMPLES=\n" \
	   "Let us assume that the range A1:C7 contain the following " \
	   "values:\n" \
	   "Name    Age     Salary\n" \
	   "John    34      54342\n" \
	   "Bill    35      22343\n" \
	   "Clark   29      34323\n" \
	   "Bob     43      47242\n" \
	   "Susan   37      42932\n" \
	   "Jill    45      45324\n" \
	   "\n" \
	   "In addition, the cells A9:B11 contain the following values:\n" \
	   "Age     Salary\n" \
	   "<30\n" \
	   ">40     >46000\n"


/***************************************************************************/

static char *help_daverage = {
        N_("@FUNCTION=DAVERAGE\n"
           "@SYNTAX=DAVERAGE(database,field,criteria)\n"

           "@DESCRIPTION="
           "DAVERAGE function returns the average of the values in a list "
	   "or database that match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DAVERAGE(A1:C7, \"Salary\", A9:A11) equals 42296.3333.\n"
	   "DAVERAGE(A1:C7, \"Age\", A9:A11) equals 39.\n"
           "DAVERAGE(A1:C7, \"Salary\", A9:B11) equals 40782.5.\n"
	   "DAVERAGE(A1:C7, \"Age\", A9:B11) equals 36.\n"
	   "\n"
           "@SEEALSO=DCOUNT")
};

static Value *
gnumeric_daverage (FunctionEvalInfo *ei, Value **argv)
{
        Value       *database, *criteria;
	GSList      *criterias;
	GSList      *cells, *current;
	int         field;
	float_t     sum;
	int         count;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);
	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);

	current = cells;
	count = 0;
	sum = 0;

	while (current != NULL) {
	        Cell *cell = current->data;

		if (VALUE_IS_NUMBER(cell->value)) {
			count++;
			sum += value_get_as_float (cell->value);
		}
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

        return value_new_float (sum / count);
}

/***************************************************************************/

static char *help_dcount = {
        N_("@FUNCTION=DCOUNT\n"
           "@SYNTAX=DCOUNT(database,field,criteria)\n"

           "@DESCRIPTION="
           "DCOUNT function counts the cells that contain numbers in a "
	   "database that match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DCOUNT(A1:C7, \"Salary\", A9:A11) equals 3.\n"
           "DCOUNT(A1:C7, \"Salary\", A9:B11) equals 2.\n"
           "DCOUNT(A1:C7, \"Name\", A9:B11) equals 0.\n"
	   "\n"
           "@SEEALSO=DAVERAGE")
};

static Value *
gnumeric_dcount (FunctionEvalInfo *ei, Value **argv)
{
        Value       *database, *criteria;
	GSList      *criterias;
	GSList      *cells, *current;
	int         field;
	int         count;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);
	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);

	current = cells;
	count = 0;

	while (current != NULL) {
	        Cell *cell = current->data;

		if (VALUE_IS_NUMBER(cell->value))
		        count++;
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

        return value_new_int (count);
}

/***************************************************************************/

static char *help_dcounta = {
        N_("@FUNCTION=DCOUNTA\n"
           "@SYNTAX=DCOUNTA(database,field,criteria)\n"

           "@DESCRIPTION="
           "DCOUNTA function counts the cells that contain data in a "
	   "database that match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DCOUNTA(A1:C7, \"Salary\", A9:A11) equals 3.\n"
           "DCOUNTA(A1:C7, \"Salary\", A9:B11) equals 2.\n"
           "DCOUNTA(A1:C7, \"Name\", A9:B11) equals 2.\n"
	   "\n"
           "@SEEALSO=DCOUNT")
};

static Value *
gnumeric_dcounta (FunctionEvalInfo *ei, Value **argv)
{
        Value       *database, *criteria;
	GSList      *criterias;
	GSList      *cells, *current;
	int         field;
	int         count;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);
	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);

	current = cells;
	count = 0;

	while (current != NULL) {
	        count++;
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

        return value_new_int (count);
}

/***************************************************************************/

static char *help_dget = {
        N_("@FUNCTION=DGET\n"
           "@SYNTAX=DGET(database,field,criteria)\n"

           "@DESCRIPTION="
           "DGET function returns a single value from a column that "
	   "match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DGET(A1:C7, \"Salary\", A9:A10) equals 34323.\n"
           "DGET(A1:C7, \"Name\", A9:A10) equals \"Clark\".\n"
           "\n"
	   "If none of the items match the conditions, DGET returns #VALUE! "
	   "error. "
	   "If more than one items match the conditions, DGET returns #NUM! "
	   "error. "
	   "\n"
           "@SEEALSO=DCOUNT")
};

static Value *
gnumeric_dget (FunctionEvalInfo *ei, Value **argv)
{
        Value       *database, *criteria;
	GSList      *criterias;
	GSList      *cells, *current;
	int         field;
	Cell        *cell = NULL;
	int         count;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);

	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);

	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);

	current = cells;
	count = 0;

	while (current != NULL) {
	        cell = current->data;
	        count++;
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

	if (count == 0)
		return value_new_error (&ei->pos, gnumeric_err_VALUE);

	if (count > 1)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

        return value_duplicate (cell->value);
}

static char *help_dmax = {
        N_("@FUNCTION=DMAX\n"
           "@SYNTAX=DMAX(database,field,criteria)\n"

           "@DESCRIPTION="
           "DMAX function returns the largest number in a column that "
	   "match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DMAX(A1:C7, \"Salary\", A9:A11) equals 47242.\n"
           "DMAX(A1:C7, \"Age\", A9:A11) equals 45.\n"
           "DMAX(A1:C7, \"Age\", A9:B11) equals 43.\n"
	   "\n"
           "@SEEALSO=DMIN")
};

/***************************************************************************/

static Value *
gnumeric_dmax (FunctionEvalInfo *ei, Value **argv)
{
        Value       *database, *criteria;
	GSList      *criterias;
	GSList      *cells, *current;
	Cell        *cell;
	int         field;
	float_t     max;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);
	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);
	if (cells == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	current = cells;
	cell = current->data;
	max = value_get_as_float (cell->value);

	while (current != NULL) {
	        float_t v;

	        cell = current->data;
		if (VALUE_IS_NUMBER(cell->value)) {
			v = value_get_as_float (cell->value);
			if (max < v)
				max = v;
		}
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

        return value_new_float (max);
}

/***************************************************************************/

static char *help_dmin = {
        N_("@FUNCTION=DMIN\n"
           "@SYNTAX=DMIN(database,field,criteria)\n"

           "@DESCRIPTION="
           "DMIN function returns the smallest number in a column that "
	   "match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DMIN(A1:C7, \"Salary\", A9:B11) equals 34323.\n"
           "DMIN(A1:C7, \"Age\", A9:B11) equals 29.\n"
	   "\n"
           "@SEEALSO=DMAX")
};

static Value *
gnumeric_dmin (FunctionEvalInfo *ei, Value **argv)
{
        Value       *database, *criteria;
	GSList      *criterias;
	GSList      *cells, *current;
	Cell        *cell;
	int         field;
	float_t     min;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);
	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);
	if (cells == NULL) {
		free_criterias (criterias);
		return value_new_error (&ei->pos, gnumeric_err_NUM);
	}

	current = cells;
	cell = current->data;
	min = value_get_as_float (cell->value);

	while (current != NULL) {
	        float_t v;

	        cell = current->data;
		if (VALUE_IS_NUMBER(cell->value)) {
			v = value_get_as_float (cell->value);
			if (min > v)
				min = v;
		}
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

        return value_new_float (min);
}

/***************************************************************************/

static char *help_dproduct = {
        N_("@FUNCTION=DPRODUCT\n"
           "@SYNTAX=DPRODUCT(database,field,criteria)\n"

           "@DESCRIPTION="
           "DPRODUCT function returns the product of numbers in a column "
	   "that match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DPRODUCT(A1:C7, \"Age\", A9:B11) equals 1247.\n"
	   "\n"
           "@SEEALSO=DSUM")
};

static Value *
gnumeric_dproduct (FunctionEvalInfo *ei, Value **argv)
{
        Value       *database, *criteria;
	GSList      *criterias;
	GSList      *cells, *current;
	Cell        *cell;
	int         field;
	float_t     product;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);

	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);

	if (cells == NULL) {
		free_criterias (criterias);
		return value_new_error (&ei->pos, gnumeric_err_NUM);
	}

	current = cells;
	product = 1;
	cell = current->data;

	while (current != NULL) {
	        float_t v;

	        cell = current->data;
		v = value_get_as_float (cell->value);
		product *= v;
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

        return value_new_float (product);
}

/***************************************************************************/

static char *help_dstdev = {
        N_("@FUNCTION=DSTDEV\n"
           "@SYNTAX=DSTDEV(database,field,criteria)\n"

           "@DESCRIPTION="
           "DSTDEV function returns the estimate of the standard deviation "
	   "of a population based on a sample. The populations consists of "
	   "numbers that match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DSTDEV(A1:C7, \"Age\", A9:B11) equals 9.89949.\n"
           "DSTDEV(A1:C7, \"Salary\", A9:B11) equals 9135.112506.\n"
	   "\n"
           "@SEEALSO=DSTDEVP")
};

static Value *
gnumeric_dstdev (FunctionEvalInfo *ei, Value **argv)
{
        Value          *database, *criteria;
	GSList         *criterias;
	GSList         *cells, *current;
	int            field;
	stat_closure_t p;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);
	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);
	if (cells == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	current = cells;
	setup_stat_closure (&p);

	while (current != NULL) {
	        Cell *cell = current->data;

		/* FIXME : What about errors ? */
		callback_function_stat (NULL, cell->value, &p);
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

	if (p.N - 1 == 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

        return value_new_float (sqrt(p.Q / (p.N - 1)));
}

/***************************************************************************/

static char *help_dstdevp = {
        N_("@FUNCTION=DSTDEVP\n"
           "@SYNTAX=DSTDEVP(database,field,criteria)\n"

           "@DESCRIPTION="
           "DSTDEVP function returns the standard deviation of a population "
	   "based on the entire populations. The populations consists of "
	   "numbers that match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DSTDEVP(A1:C7, \"Age\", A9:B11) equals 7.\n"
           "DSTDEVP(A1:C7, \"Salary\", A9:B11) equals 6459.5.\n"
	   "\n"
           "@SEEALSO=DSTDEV")
};

static Value *
gnumeric_dstdevp (FunctionEvalInfo *ei, Value **argv)
{
        Value          *database, *criteria;
	GSList         *criterias;
	GSList         *cells, *current;
	int            field;
	stat_closure_t p;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);
	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);
	if (cells == NULL) {
		free_criterias (criterias);
		return value_new_error (&ei->pos, gnumeric_err_NUM);
	}

	current = cells;
	setup_stat_closure (&p);

	while (current != NULL) {
	        Cell *cell = current->data;

		/* FIXME : What about errors ? */
		callback_function_stat (NULL, cell->value, &p);
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

	if (p.N == 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

        return value_new_float (sqrt(p.Q / p.N));
}

/***************************************************************************/

static char *help_dsum = {
        N_("@FUNCTION=DSUM\n"
           "@SYNTAX=DSUM(database,field,criteria)\n"

           "@DESCRIPTION="
           "DSUM function returns the sum of numbers in a column "
	   "that match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DSUM(A1:C7, \"Age\", A9:B11) equals 72.\n"
           "DSUM(A1:C7, \"Salary\", A9:B11) equals 81565.\n"
	   "\n"
           "@SEEALSO=DPRODUCT")
};

static Value *
gnumeric_dsum (FunctionEvalInfo *ei, Value **argv)
{
        Value       *database, *criteria;
	GSList      *criterias;
	GSList      *cells, *current;
	Cell        *cell;
	int         field;
	float_t     sum;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);
	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);
	if (cells == NULL) {
		free_criterias (criterias);
		return value_new_error (&ei->pos, gnumeric_err_NUM);
	}

	current = cells;
	sum = 0;
	cell = current->data;

	while (current != NULL) {
	        float_t v;

	        cell = current->data;
		v = value_get_as_float (cell->value);
		sum += v;
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

        return value_new_float (sum);
}

/***************************************************************************/

static char *help_dvar = {
        N_("@FUNCTION=DVAR\n"
           "@SYNTAX=DVAR(database,field,criteria)\n"

           "@DESCRIPTION="
           "DVAR function returns the estimate of variance of a population "
	   "based on a sample. The populations consists of numbers "
	   "that match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DVAR(A1:C7, \"Age\", A9:B11) equals 98.\n"
           "DVAR(A1:C7, \"Salary\", A9:B11) equals 83450280.5.\n"
	   "\n"
           "@SEEALSO=DVARP")
};

static Value *
gnumeric_dvar (FunctionEvalInfo *ei, Value **argv)
{
        Value          *database, *criteria;
	GSList         *criterias;
	GSList         *cells, *current;
	int            field;
	stat_closure_t p;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);
	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);
	if (cells == NULL) {
		free_criterias (criterias);
		return value_new_error (&ei->pos, gnumeric_err_NUM);
	}

	current = cells;
	setup_stat_closure (&p);

	while (current != NULL) {
		Cell *cell = current->data;

		/* FIXME : What about errors ? */
		callback_function_stat (NULL, cell->value, &p);
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

	if (p.N - 1 == 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

        return value_new_float (p.Q / (p.N - 1));
}

/***************************************************************************/

static char *help_dvarp = {
        N_("@FUNCTION=DVARP\n"
           "@SYNTAX=DVARP(database,field,criteria)\n"

           "@DESCRIPTION="
           "DVARP function returns the variance of a population based "
	   "on the entire populations. The populations consists of numbers "
	   "that match conditions specified. "
	   "\n"
	   DB_ARGUMENT_HELP
	   "\n"
           "DVARP(A1:C7, \"Age\", A9:B11) equals 49.\n"
           "DVARP(A1:C7, \"Salary\", A9:B11) equals 41725140.25.\n"
	   "\n"
           "@SEEALSO=DVAR")
};

static Value *
gnumeric_dvarp (FunctionEvalInfo *ei, Value **argv)
{
        Value          *database, *criteria;
	GSList         *criterias;
	GSList         *cells, *current;
	int            field;
	stat_closure_t p;

	database = argv[0];
	criteria = argv[2];

	field = find_column_of_field (&ei->pos, database, argv[1]);
	if (field < 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	criterias = parse_database_criteria (&ei->pos, database, criteria);

	if (criterias == NULL)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

	cells = find_cells_that_match (&ei->pos, database, field, criterias);

	if (cells == NULL) {
		free_criterias (criterias);
		return value_new_error (&ei->pos, gnumeric_err_NUM);
	}

	current = cells;
	setup_stat_closure (&p);
	
	while (current != NULL) {
	        Cell *cell = current->data;

		/* FIXME : What about errors ? */
		callback_function_stat (NULL, cell->value, &p);
		current = g_slist_next(current);
	}

	g_slist_free(cells);
	free_criterias(criterias);

	if (p.N == 0)
		return value_new_error (&ei->pos, gnumeric_err_NUM);

        return  value_new_float (p.Q / p.N);
}

/***************************************************************************/

static char *help_getpivotdata = {
        N_("@FUNCTION=GETPIVOTDATA\n"
           "@SYNTAX=GETPIVOTDATA(pivot_table,field_name)\n"

           "@DESCRIPTION="
           "GETPIVOTDATA function fetches summary data from a pivot table. "
	   "@pivot_table is a cell range containing the pivot table. "
	   "@field_name is the name of the field of which you want the "
	   "summary data. "
	   "\n"
	   "If the summary data is unavailable, GETPIVOTDATA returns #REF! "
	   "error. "
	   "\n"
           "@SEEALSO=")
};

static Value *
gnumeric_getpivotdata (FunctionEvalInfo *ei, Value **argv)
{
	int  col, row;
	Cell *cell;

	col = find_column_of_field (&ei->pos, argv[0], argv[1]);
	if (col == -1)
		return value_new_error (&ei->pos, gnumeric_err_REF);

	row = argv[0]->v.cell_range.cell_b.row;
	cell = sheet_cell_get(ei->pos.sheet, col, row);

	/* FIXME: Lots of stuff missing */

	if (cell == NULL || cell->value == NULL ||
	    !VALUE_IS_NUMBER(cell->value))
		return value_new_error (&ei->pos, gnumeric_err_REF);

        return value_new_float (value_get_as_float(cell->value));
}

/***************************************************************************/

void
database_functions_init(void)
{
	FunctionCategory *cat = function_get_category (_("Database"));
	
	function_add_args (cat,  "daverage", "r?r",
			   "database,field,criteria",
			   &help_daverage,   gnumeric_daverage );
	function_add_args (cat,  "dcount",   "r?r",
			   "database,field,criteria",
			   &help_dcount,     gnumeric_dcount );
	function_add_args (cat,  "dcounta",  "r?r",
			   "database,field,criteria",
			   &help_dcounta,    gnumeric_dcounta );
	function_add_args (cat,  "dget",     "r?r",
			   "database,field,criteria",
			   &help_dget,       gnumeric_dget );
	function_add_args (cat,  "dmax",     "r?r",
			   "database,field,criteria",
			   &help_dmax,       gnumeric_dmax );
	function_add_args (cat,  "dmin",     "r?r",
			   "database,field,criteria",
			   &help_dmin,       gnumeric_dmin );
	function_add_args (cat,  "dproduct", "r?r",
			   "database,field,criteria",
			   &help_dproduct,   gnumeric_dproduct );
	function_add_args (cat,  "dstdev",   "r?r",
			   "database,field,criteria",
			   &help_dstdev,     gnumeric_dstdev );
	function_add_args (cat,  "dstdevp",  "r?r",
			   "database,field,criteria",
			   &help_dstdevp,    gnumeric_dstdevp );
	function_add_args (cat,  "dsum",     "r?r",
			   "database,field,criteria",
			   &help_dsum,       gnumeric_dsum );
	function_add_args (cat,  "dvar",     "r?r",
			   "database,field,criteria",
			   &help_dvar,       gnumeric_dvar );
	function_add_args (cat,  "dvarp",    "r?r",
			   "database,field,criteria",
			   &help_dvarp,      gnumeric_dvarp );
	function_add_args (cat,  "getpivotdata", "rs",
			   "pivot_table,field_name",
			   &help_getpivotdata, gnumeric_getpivotdata );
}