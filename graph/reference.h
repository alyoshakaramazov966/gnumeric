#ifndef GRAPH_REFERENCE_H
#define GRAPH_REFERENCE_H

#include "data-source.h"

typedef struct _Reference Reference;

Reference *reference_new       (DataSource *data_source, const char *spec);
Reference *reference_duplicate (Reference *reference);
void       reference_destroy   (Reference *reference);
Value     *reference_value     (Reference *reference);

#endif /* GRAPH_REFERENCE_H */