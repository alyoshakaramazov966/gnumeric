#ifndef GRAPH_VIEW_UTIL_H
#define GRAPH_VIEW_UTIL_H

typedef enum {
	SYMBOL_POINT,
	SYMBOL_CROSS_1,
	SYMBOL_CROSS_2,
	SYMBOL_CIRCLE,
	SYMBOL_FILLED_CIRCLE,
	SYMBOL_SQUARE,
	SYMBOL_FILLED_SQUARE,
	SYMBOL_TRIANGLE,
	SYMBOL_FILLED_TRIANGLE,
	SYMBOL_LAST
} Symbol;

typedef struct {
	GraphView   *graph_view;
	Graph       *graph;
	GdkDrawable *drawable;

	GdkGC	    *gc;
	int          x, y;
	int          width, height;
	int          xl, yl;

	/* Dimensions of our symbols, a rough idea of the size */
	int          dim;

	/* Units per slot for columns and bars displays */
	double       units_per_slot;

	/* Margin for column and bar displays */
	int          margin;
	int          inter_item_margin;
	double       scale;
} ViewDrawCtx;

Symbol symbol_setup (ViewDrawCtx *ctx, int series);
void   symbol_draw  (ViewDrawCtx *ctx, Symbol sym, int px, int py);

#define SETUP_VIEW_CTX(ctx,gv,d,vx,vy,w,h) \
	ctx.x = vx;\
	ctx.y = vy;\
	ctx.width = w;\
	ctx.height = h;\
	ctx.drawable = d;\
	ctx.graph_view = gv;\
	ctx.graph = gv->graph;\
	ctx.yl = gv->bbox.y1 - gv->bbox.y0;\
	ctx.xl = gv->bbox.x1 - gv->bbox.x0;

/*
 * Macros used by various plotting modes.
 *
 * Entry points:
 *
 *   MAP_X(Context *ctx, double x)
 *   MAP_Y(Context *ctx, double y)
 *
 * The Column/Bar does not use this one, it uses ctx->scale, because it
 * has to handle 2 different scaling systems depending on column or bar.
 */
 
/*
 * Maps a pixel value in the 0..xl,0..yl ranges to an actual
 * position in the canvas (ctx->{x,y} contains the offset of the pixmap
 * we draw into).
 *
 * y axis also gets flipped.
 */
#define CANVAS_MAP_X(ctx,xv) ((xv) - ctx->x)
#define CANVAS_MAP_Y(ctx,yv) ((ctx->yl - yv) - ctx->y)

#define ADJUST_Y(ctx,y) (y - ctx->graph->low)

/*
 * Maps a x,y coordinate to a pixel value
 */
#define MAP_X(ctx,xv) CANVAS_MAP_X (ctx, (((xv) * ctx->xl) / ctx->graph->x_size))
#define MAP_Y(ctx,yv) CANVAS_MAP_Y (ctx, (((ADJUST_Y (ctx,yv)) * ctx->yl) / ctx->graph->y_size))

#endif /* GRAPH_VIEW_UTIL_H */
