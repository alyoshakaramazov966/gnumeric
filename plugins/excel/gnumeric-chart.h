#ifndef GNUMERIC_CHART_H
#define GNUMERIC_CHART_H

#include <glib.h>

/**
 * gnumeric-chart.h: Gnumeric Charts
 *
 * Author:
 *    Jody Goldberg (jgoldberg@home.com)
 *
 * (C) 1999 Jody Goldberg
 **/

typedef struct _GnumericChartSeries
{
	gint dummy;
} GnumericChartSeries;

typedef struct _GnumericChart
{
	GPtrArray  *series;
} GnumericChart;

extern GnumericChart * gnumeric_chart_new (void);
extern void gnumeric_chart_destroy (GnumericChart * chart);

#endif /* GNUMERIC_CHART_H */