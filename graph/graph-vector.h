#ifndef _GRAPH_VECTOR_H_
#define _GRAPH_VECTOR_H_

typedef struct _GraphVector GraphVector;

typedef void (*GraphVectorChangeNotifyFn)(GraphVector *gv, CORBA_short low, CORBA_short high, void *data);
					  
struct _GraphVector {
	GNOME_Gnumeric_Vector vector_object;

	gboolean contains_numbers;
	union {
		GNOME_Gnumeric_DoubleVec *double_vec;
		GNOME_Gnumeric_VecValueVec *values_vec;
	} u;
	GraphVectorChangeNotifyFn  change;
	void                      *change_data;
};

GraphVector *graph_vector_new         (GNOME_Gnumeric_Vector vector,
				       GraphVectorChangeNotifyFn change,
				       void *change_data,
				       gboolean guess);
void         graph_vector_destroy     (GraphVector *graph_vector);

int          graph_vector_count       (GraphVector *vector);
char        *graph_vector_get_string  (GraphVector *vector, int pos);
double       graph_vector_get_double  (GraphVector *vector, int pos);
void         graph_vector_low_high    (GraphVector *vector, double *low, double *high);
int          graph_vector_buffer_size (GraphVector *vector);


#endif
