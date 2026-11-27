/*
    See license.txt in the root of this project.
*/

# include <luametatex.h>
# include "utilities/auxbytemaps.h"
# include "lmtbytemaplib.h"
# include "lmtrenderlib.h"

/*tex 

The code was prototyped in \LUA\ with Gemini: formulating a demand, showing a rough setup, pointing
to template code, setting boundary conditions and from there a at first sight simple solution 
evolved so the code became a bit more complex than originally intended. We also had to bring 
runtime down to reasonable and find solutions for (persistent) mis-predictions (a rather visual 
issue but Gemini could be fed with logs and was able to identify potential problems and come up
with solutions. A nice example of \LLM\ assisted editing. 

What was interesting is that at some point in the \quote {discussion} Gemini reflected on some 
artifacts by mentioning that the \quote {a} was a Computer Modern one and when asked how it knew, 
it explained that the serif was typical for CM and that the metrics were also familiar and 
distinctive. We use the outline specifications that \CONTEXT\ extracts from fonts for use in 
e.g.\ \METAPOST, and the conclusions were based on that and the bitmap. At that moment it hadn't 
yet seen that we were in the \CONTEXT|/|\LUAMETATEX\ universe (it normally is quick in figuring 
that out).

The original idea was optical kerning but in the end we focus on logos and (large) display text 
instead. The bitmap rendering was on the agenda for along time and also serves different purposes. 
It is actually the smallest part of the code. 

Here is a Gemini summary askled for: The code draws on classic computer-vision, rasterization, 
and computational|-|typography techniques rather than one single paper. Here is where the core 
ideas come from and how they map to the C code and \LUA\ pipeline. 

\startitemize

\startitem Chamfer Distance Transform (3/4 Weights)

    \startitemize
        \startitem
            Origins: Chamfer distance transforms were introduced by Barrow et al.
            (1977) for template matching and later refined with integer local
            mask weights—most notably Borgefors (1984, 1986)—to approximate
            Euclidean distance quickly on a discrete grid using two linear passes
            (top-down|/|left-right, then bottom-up|/|right-left).
        \stopitem
        \startitem
            The 3 and 4 weights: On a square pixel grid, orthogonal neighbors sit
            at distance \im {1} and diagonal neighbors sit at \im {\sqrt{2}
            \approx 1.4142}. Multiplying by \im {3} gives \im {3} for orthogonal
            steps and \im {4.24} for diagonal steps; rounding the diagonal weight
            to \im {4} lets the entire propagation run in integer arithmetic with
            scale factor \im {3} (\type{px_dist = dist / 3.0}), keeping relative
            error small (~1–8\percent\ depending on angle) while avoiding square
            roots or queues in inner loops.
        \stopitem
        \startitem
            Proximity / heatmap falloff: Turning distance $d$ into coupling or
            heat via $\frac{1}{1 + d / r}$ (rational falloff in
            scorepair/scankern) or $\exp(-d / r)$ (exponential heat in
            chamfer/heatmap) is a standard way to get smooth, bounded influence
            over a characteristic pixel radius $r$ without hard cutoffs.
        \stopitem
    \stopitemize

\stopitem

\startitem Subpixel Scanline Rasterization & Winding Rule

    \startitemize
        \startitem
            Origins: Edge-list scanline rasterization with sorted X-intersections
            and non-zero winding numbers comes straight from classic 2D vector
            graphics (Foley & van Dam; Antikythera|/|FreeType|/|AGG style scanline
            pipelines).
        \stopitem
        \startitem
            Supersampling grid (grid × grid): Sampling subpixel row centers at
            (\typ {i + 0.5) / grid} (midpoint rule) and accumulating non-zero
            winding spans gives anti-aliased coverage in \typ {0 .. grid * grid},
            which maps cleanly to \typ {0 .. 255} bytes for bytemap output or
            \METAPOST\ display.
        \stopitem
        \startitem
            Cubic Bézier flattening: Evaluating Bernstein polynomials at steps
            intervals (\typ {t = j / steps}) is the simplest fixed-step
            flattening approach; adaptive subdivision (midpoint/de Casteljau
            flatness tests) is a common refinement if you ever want fewer edges
            on shallow curves or more on tight ones at huge target_height.
        \stopitem
    \stopitemize

\stopitem

\startitem Algorithmic Kerning & Optical Spacing

    \startitemize
        \startitem
            Origins: Automated/optical kerning and spacing in typography
            generally fall into two families:
            \startitemize
                \startitem
                    Contour / silhouette / distance-field methods: Measuring the
                    gap or overlap between adjacent glyph outlines/bitmaps across
                    candidate kerns and balancing a tightness / rhythm target
                    against minimum clearance and zero overlap (related to
                    optical spacing / Ikora-style visual coupling ideas and
                    distance-field layout).
                \stopitem
                \startitem
                    Calibrated baseline rhythm (calibrate_target_coupling): Using
                    repetitive reference pairs like "nn" and "HH" to measure the
                    font's natural stem/counter coupling and scaling that target
                    (coupling_ratio) is a pragmatic way to adapt spacing to the
                    font's weight and proportions rather than hardcoding a single
                    magic number for every typeface.
                \stopitem
            \stopitemize
        \stopitem
        \startitem
            Coarse-to-fine search (render.scankern): Scanning at coarse_step
            first and then refining within [seed_k - coarse_step, seed_k +
            coarse_step] at fine_step (preferring zero-overlap candidates when
            available) is standard 1D unimodal/grid search—fast, predictable, and
            easy to bound with kern_min / kern_max.
        \stopitem
    \stopitemize
\stopitem

\stopitemize

So, as with other graphic gems we use we're back in the previous century and the older books on my 
bookshelves. 

*/

typedef struct glyph_edge {
    double x0, y0, x1, y1;
    int    dir;
} glyph_edge;

typedef struct glyph_hit {
    double x;
    int    dir;
} glyph_hit;

typedef struct glyph_render_opts {
    int    target_width;
    int    target_height;
    int    padding;
    int    grid;
    int    steps;
    int    flip_y;
    int    use_width;
    int    invert;
    double xoff;
    double yoff;
} glyph_render_opts;

typedef struct glyph_mapper {
    double llx, lly, urx, ury;
    double min_x, max_x, min_y, max_y;
    double scale;
    int    width;
    int    height;
    int    padding;
    int    flip_y;
    double xoff;
    double yoff;
} glyph_mapper;

typedef struct glyph_scratch {
    glyph_edge *edges;
    size_t      n_edges;
    size_t      cap_edges;
    double     *sub_offsets;
    int        *cov_row;
    glyph_hit  *hits;
    size_t      cap_hits;
} glyph_scratch;

static inline void glyph_to_pixel(const glyph_mapper *m, double x, double y, double *px, double *py)
{
    double sx = x + m->xoff;
    double sy = y + m->yoff;
    *px = m->padding + (sx - m->min_x) * m->scale;
    *py = m->flip_y ? (m->padding + (m->max_y - sy) * m->scale) : (m->padding + (sy - m->min_y) * m->scale);
}

static inline void cubic_bezier_pt(
    double p0x, double p0y, double p1x, double p1y,
    double p2x, double p2y, double p3x, double p3y,
    double t, 
    double *ox, double *oy
)
{
    double mt  = 1.0 - t;
    double mt2 = mt * mt;
    double t2  = t * t;
    double a   = mt2 * mt;
    double b   = 3.0 * mt2 * t;
    double c   = 3.0 * mt * t2;
    double d   = t2 * t;
    *ox = a * p0x + b * p1x + c * p2x + d * p3x;
    *oy = a * p0y + b * p1y + c * p2y + d * p3y;
}

static void glyph_scratch_init(glyph_scratch *s)
{
    memset(s, 0, sizeof(*s));
}

static void glyph_scratch_free(glyph_scratch *s)
{
    /* we could memset to zero after freeing */
    if (s->edges)       { lmt_memory_free(s->edges);       s->edges       = NULL; }
    if (s->sub_offsets) { lmt_memory_free(s->sub_offsets); s->sub_offsets = NULL; }
    if (s->cov_row)     { lmt_memory_free(s->cov_row);     s->cov_row     = NULL; }
    if (s->hits)        { lmt_memory_free(s->hits);        s->hits        = NULL; }
    s->n_edges   = 0;
    s->cap_edges = 0;
    s->cap_hits  = 0;
}

static int glyph_push_edge(glyph_scratch *s, double x0, double y0, double x1, double y1)
{
    if (! isfinite(x0) || ! isfinite(y0) || ! isfinite(x1) || ! isfinite(y1) || y0 == y1) {
        return 1;
    }
    if (s->n_edges == s->cap_edges) {
        size_t     next_cap = s->cap_edges ? s->cap_edges * 2 : 64;
        glyph_edge *grown   = lmt_memory_realloc(s->edges, next_cap * sizeof(glyph_edge));
        if (!grown) {
            return 0;
        }
        s->edges     = grown;
        s->cap_edges = next_cap;
    }
    s->edges[s->n_edges++] = (glyph_edge) {
        .x0  = x0,
        .y0  = y0,
        .x1  = x1,
        .y1  = y1,
        .dir = (y1 > y0) ? 1 : -1,
    };
    return 1;
}

static double lua_table_num(lua_State *L, int tbl_idx, int key, double fallback)
{
    double v = fallback;
    lua_rawgeti(L, tbl_idx, key);
    if (lua_type(L, -1) == LUA_TNUMBER) {
        double candidate = lua_tonumber(L, -1);
        if (isfinite(candidate)) {
            v = candidate;
        }
    }
    lua_pop(L, 1);
    return v;
}

static double lua_opt_field_num(lua_State *L, int tbl_idx, const char *key, double fallback)
{
    double v = fallback;
    if (tbl_idx != 0 && lua_type(L, tbl_idx) == LUA_TTABLE) {
        lua_getfield(L, tbl_idx, key);
        if (lua_type(L, -1) == LUA_TNUMBER) {
            double candidate = lua_tonumber(L, -1);
            if (isfinite(candidate)) {
                v = candidate;
            }
        }
        lua_pop(L, 1);
    }
    return v;
}

static int read_glyph_options(lua_State *L, int idx, glyph_render_opts *opts)
{
    opts->target_width  = 0;
    opts->target_height = 24;
    opts->padding       = 10;
    opts->grid          = 4;
    opts->steps         = 12;
    opts->flip_y        = 1;
    opts->use_width     = 1;
    opts->invert        = 0;
    opts->xoff          = 0.0;
    opts->yoff          = 0.0;

    if (lua_type(L, idx) != LUA_TTABLE) {
        return 1;
    }

    lua_getfield(L, idx, "target_width");
    if (lua_type(L, -1) == LUA_TNUMBER) { 
        opts->target_width = lmt_tointeger(L, -1); 
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "target_height");
    if (lua_type(L, -1) == LUA_TNUMBER) { 
        opts->target_height = lmt_tointeger(L, -1); 
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "padding");
    if (lua_type(L, -1) == LUA_TNUMBER) { 
        opts->padding = lmt_tointeger(L, -1); 
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "grid");
    if (lua_type(L, -1) == LUA_TNUMBER) { 
        opts->grid = lmt_tointeger(L, -1); 
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "steps");
    if (lua_type(L, -1) == LUA_TNUMBER) { 
        opts->steps = lmt_tointeger(L, -1); 
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "flip_y");
    if (lua_type(L, -1) == LUA_TBOOLEAN) { 
        opts->flip_y = lua_toboolean(L, -1); 
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "use_width");
    if (lua_type(L, -1) == LUA_TBOOLEAN) { 
        opts->use_width = lua_toboolean(L, -1); 
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "invert");
    if (lua_type(L, -1) == LUA_TBOOLEAN) {
        opts->invert = lua_toboolean(L, -1);
    } else if (lua_type(L, -1) == LUA_TNUMBER) { 
        opts->invert = lmt_tointeger(L, -1) != 0;
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "xoffset");
    if (lua_type(L, -1) == LUA_TNUMBER && isfinite(lua_tonumber(L, -1))) {
        opts->xoff = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "yoffset");
    if (lua_type(L, -1) == LUA_TNUMBER && isfinite(lua_tonumber(L, -1))) {
        opts->yoff = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    /* looks redundant but we don't want to patch the original glyphs table */

    lua_getfield(L, idx, "offset");
    if (lua_type(L, -1) == LUA_TTABLE) {
        lua_getfield(L, -1, "x");
        if (lua_type(L, -1) == LUA_TNUMBER && opts->xoff == 0.0 && isfinite(lua_tonumber(L, -1))) {
            opts->xoff = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);
        lua_getfield(L, -1, "y");
        if (lua_type(L, -1) == LUA_TNUMBER && opts->yoff == 0.0 && isfinite(lua_tonumber(L, -1))) {
            opts->yoff = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    /* */

    return 1;
}

static void build_mapper(
    const glyph_render_opts *opts,
    double                   adv,
    double                   bb_llx,
    double                   bb_lly,
    double                   bb_urx,
    double                   bb_ury,
    glyph_mapper            *m)
{
    m->padding = opts->padding < 0 ? 0 : opts->padding;
    m->flip_y  = opts->flip_y;
    m->xoff    = isfinite(opts->xoff) ? opts->xoff : 0.0;
    m->yoff    = isfinite(opts->yoff) ? opts->yoff : 0.0;
    m->llx     = bb_llx + m->xoff;
    m->lly     = bb_lly + m->yoff;
    m->urx     = bb_urx + m->xoff;
    m->ury     = bb_ury + m->yoff;
    m->min_x   = opts->use_width ? fmin(fmin(0.0, m->xoff), m->llx) : m->llx;
    m->max_x   = opts->use_width ? fmax(adv + m->xoff, m->urx) : m->urx;
    m->min_y   = m->lly;
    m->max_y   = m->ury;

    double span_x = fmax(m->max_x - m->min_x, 1.0);
    double span_y = fmax(m->max_y - m->min_y, 1.0);

    if (opts->target_width > 0 && opts->target_height > 0) {
        m->width       = opts->target_width;
        m->height      = opts->target_height;
        double inner_w = fmax((double) m->width  - 2.0 * m->padding, 1.0);
        double inner_h = fmax((double) m->height - 2.0 * m->padding, 1.0);
        m->scale       = fmin(inner_w / span_x, inner_h / span_y);
    } else {
        int    th      = opts->target_height > 0 ? opts->target_height : 24;
        double inner_h = fmax((double) th - 2.0 * m->padding, 1.0);
        m->scale       = inner_h / span_y;
        double raw_w   = span_x * m->scale + 2.0 * m->padding;
        m->width       = isfinite(raw_w) ? (int) ceil(raw_w) : 1;
        m->height      = th;
        if (m->width < 1) { 
            m->width = 1;
        }
        if (m->height < 1) {
            m->height = 1;
        }
    }
}

static int collect_glyph_segments(
    lua_State          *L,
    int                 seg_tbl_idx,
    const glyph_mapper *m,
    int                 steps,
    glyph_scratch      *s)
{
    if (lua_type(L, seg_tbl_idx) != LUA_TTABLE) {
        return 1;
    }

    int    n_cmds      = (int) lua_rawlen(L, seg_tbl_idx);
    double cur_x       = 0.0;
    double cur_y       = 0.0;
    double start_x     = 0.0;
    double start_y     = 0.0;
    int    has_start   = 0;
    int    has_subpath = 0;

    for (int i = 1; i <= n_cmds; i++) {
        lua_rawgeti(L, seg_tbl_idx, i);
        if (lua_type(L, -1) == LUA_TTABLE) {
            int cmd_idx = lua_gettop(L);
            int len     = (int) lua_rawlen(L, cmd_idx);
            if (len >= 1) {
                lua_rawgeti(L, cmd_idx, len);
                const char *op = (lua_type(L, -1) == LUA_TSTRING) ? lua_tostring(L, -1) : NULL;
                if (op && strcmp(op, "m") == 0 && len >= 3) {
                    if (has_subpath && has_start && (cur_x != start_x || cur_y != start_y)) {
                        if (!glyph_push_edge(s, cur_x, cur_y, start_x, start_y)) {
                            lua_pop(L, 2);
                            return 0;
                        }
                    }
                    glyph_to_pixel(m, lua_table_num(L, cmd_idx, 1, 0.0), lua_table_num(L, cmd_idx, 2, 0.0), &cur_x, &cur_y);
                    start_x     = cur_x;
                    start_y     = cur_y;
                    has_start   = 1;
                    has_subpath = 1;
                } else if (op && strcmp(op, "l") == 0 && len >= 3) {
                    if (! has_subpath) {
                        start_x     = cur_x;
                        start_y     = cur_y;
                        has_start   = 1;
                        has_subpath = 1;
                    }
                    double nx, ny;
                    glyph_to_pixel(m, lua_table_num(L, cmd_idx, 1, 0.0), lua_table_num(L, cmd_idx, 2, 0.0), &nx, &ny);
                    if (! glyph_push_edge(s, cur_x, cur_y, nx, ny)) {
                        lua_pop(L, 2);
                        return 0;
                    }
                    cur_x = nx;
                    cur_y = ny;
                } else if (op && strcmp(op, "c") == 0 && len >= 7) {
                    if (! has_subpath) {
                        start_x     = cur_x;
                        start_y     = cur_y;
                        has_start   = 1;
                        has_subpath = 1;
                    }
                    double x1, y1, x2, y2, ex, ey;
                    glyph_to_pixel(m, lua_table_num(L, cmd_idx, 1, 0.0), lua_table_num(L, cmd_idx, 2, 0.0), &x1, &y1);
                    glyph_to_pixel(m, lua_table_num(L, cmd_idx, 3, 0.0), lua_table_num(L, cmd_idx, 4, 0.0), &x2, &y2);
                    glyph_to_pixel(m, lua_table_num(L, cmd_idx, 5, 0.0), lua_table_num(L, cmd_idx, 6, 0.0), &ex, &ey);
                    double sx = cur_x; 
                    double sy = cur_y;
                    double px = sx;
                    double py = sy;
                    for (int j = 1; j <= steps; j++) {
                        double t = (double) j / (double) steps;
                        double nx, ny;
                        cubic_bezier_pt(sx, sy, x1, y1, x2, y2, ex, ey, t, &nx, &ny);
                        if (! glyph_push_edge(s, px, py, nx, ny)) {
                            lua_pop(L, 2);
                            return 0;
                        }
                        px = nx;
                        py = ny;
                    }
                    cur_x = ex;
                    cur_y = ey;
                } else if (op && strcmp(op, "close") == 0) {
                    if (has_subpath && has_start && (cur_x != start_x || cur_y != start_y)) {
                        if (! glyph_push_edge(s, cur_x, cur_y, start_x, start_y)) {
                            lua_pop(L, 2);
                            return 0;
                        }
                    }
                    cur_x       = start_x;
                    cur_y       = start_y;
                    has_subpath = 0;
                }

                lua_pop(L, 1); /* op */
            }
        }
        lua_pop(L, 1); /* cmd table */
    }
    if (has_subpath && has_start && (cur_x != start_x || cur_y != start_y)) {
        if (! glyph_push_edge(s, cur_x, cur_y, start_x, start_y)) {
            return 0;
        }
    }
    return 1;
}

static int glyph_hit_cmp_x(const void *a, const void *b)
{
    const glyph_hit *ha = (const glyph_hit *) a;
    const glyph_hit *hb = (const glyph_hit *) b;
    if (ha->x < hb->x) {
        return -1;
    }
    if (ha->x > hb->x) {
        return  1;
    }
    return 0;
}

static int glyph_init_subpixel_grid(glyph_scratch *s, int grid)
{
    s->sub_offsets = lmt_memory_malloc((size_t) grid * sizeof(double));
    if (!s->sub_offsets) {
        return 0;
    }
    for (int i = 0; i < grid; i++) {
        s->sub_offsets[i] = ((double) i + 0.5) / (double) grid;
    }
    return 1;
}

static int glyph_add_subrow_coverage(
    glyph_scratch *s,
    double         sy,
    int            width,
    int            grid,
    int            max_cov
)
{
    size_t n_hits = 0;

    for (size_t i = 0; i < s->n_edges; i++) {
        const glyph_edge *e = &s->edges[i];
        double y0 = e->y0;
        double y1 = e->y1;
        if ((y0 <= sy && sy < y1) || (y1 <= sy && sy < y0)) {
            if (n_hits == s->cap_hits) {
                size_t     next_cap = s->cap_hits ? s->cap_hits * 2 : 64;
                glyph_hit *grown    = lmt_memory_realloc(s->hits, next_cap * sizeof(glyph_hit));
                if (! grown) {
                    return 0;
                }
                s->hits     = grown;
                s->cap_hits = next_cap;
            }
            double t     = (sy - y0) / (y1 - y0);
            double x_hit = e->x0 + t * (e->x1 - e->x0);
            s->hits[n_hits++] = (glyph_hit) {
                .x   = x_hit,
                .dir = e->dir,
            };
        }
    }
    if (n_hits > 1) {
        qsort(s->hits, n_hits, sizeof(glyph_hit), glyph_hit_cmp_x);
        int winding = 0;
        for (size_t i = 0; i + 1 < n_hits; i++) {
            winding += s->hits[i].dir;
            if (winding != 0) {
                double x0    = s->hits[i].x;
                double x1    = s->hits[i + 1].x;
                int    x_min = (int) floor(x0);
                int    x_max = (int) floor(x1);
                if (x_min < 0) { 
                    x_min = 0;
                }
                if (x_max >= width) { 
                    x_max = width - 1;
                }
                for (int x = x_min; x <= x_max; x++) {
                    int c = 0;
                    for (int r = 0; r < grid; r++) {
                        double sx = (double) x + s->sub_offsets[r];
                        if (x0 <= sx && sx < x1) {
                            c++;
                        }
                    }
                    if (c > 0) {
                        int next_val = s->cov_row[x] + c;
                        s->cov_row[x] = (next_val > max_cov) ? max_cov : next_val;
                    }
                }
            }
        }
    }
    return 1;
}

static int glyph_rasterize_to_bytemap(
    bytemap_data  *bm,
    glyph_scratch *s,
    int            grid,
    int            invert
)
{
    int width   = bm->nx;
    int height  = bm->ny;
    int max_cov = grid * grid;

    unsigned char bg = invert ? 0 : 255;
    memset(bm->data, bg, (size_t) width * (size_t) height);

    s->cov_row = lmt_memory_malloc((size_t) width * sizeof(int));
    if (! s->cov_row) {
        return 0;
    }

    for (int y = 0; y < height; y++) {
        memset(s->cov_row, 0, (size_t) width * sizeof(int));
        for (int r = 0; r < grid; r++) {
            double sy = (double) y + s->sub_offsets[r];
            if (! glyph_add_subrow_coverage(s, sy, width, grid, max_cov)) {
                return 0;
            }
        }
        unsigned char *dst = bm->data + (size_t) y * (size_t) width;
        for (int x = 0; x < width; x++) {
            int c = s->cov_row[x];
            if (c > 0) {
                if (c > max_cov) {
                    c = max_cov;
                }
                int cov_byte = (int) lround(((double) c / (double) max_cov) * 255.0);
                int out      = invert ? cov_byte : (255 - cov_byte);
                dst[x] = valid_byte(out);
            }
        }
    }

    return 1;
}

/* 
    two-pass chamfer distance in pixel units (1 chamfer unit = 1/3 px). `mask` has length `w * h` 
    (`1` = feature/ink pixel, `0` = background). `out_px_dist` receives `w * h` pixel distances 
*/

static int chamfer_px_from_mask(const unsigned char *mask, int w, int h, double *out_px_dist)
{
    const double INF = 1e12;
    size_t n = (size_t) w * (size_t) h;
    double *dist = lmt_memory_malloc(n * sizeof(double));
    if (! dist) {
        return 0;
    }

    for (size_t i = 0; i < n; i++) {
        dist[i] = mask[i] ? 0.0 : INF;
    }

    for (int y = 0; y < h; y++) {
        size_t row = (size_t) y * (size_t) w;
        for (int x = 0; x < w; x++) {
            size_t idx = row + (size_t) x;
            double d = dist[idx];
            if (x > 0) {
                double v = dist[idx - 1] + 3.0;
                if (v < d) {
                    d = v; 
                }
            }
            if (y > 0) {
                size_t prev_row = row - (size_t) w;
                double v = dist[prev_row + (size_t) x] + 3.0;
                if (v < d) {
                    d = v;
                }
                if (x > 0) {
                    double v2 = dist[prev_row + (size_t) x - 1] + 4.0;
                    if (v2 < d) {
                        d = v2;
                    }
                }
                if (x < w - 1) {
                    double v3 = dist[prev_row + (size_t) x + 1] + 4.0;
                    if (v3 < d) {
                        d = v3;
                    }
                }
            }
            dist[idx] = d;
        }
    }

    for (int y = h - 1; y >= 0; y--) {
        size_t row = (size_t) y * (size_t) w;
        for (int x = w - 1; x >= 0; x--) {
            size_t idx = row + (size_t) x;
            double d = dist[idx];
            if (x < w - 1) {
                double v = dist[idx + 1] + 3.0;
                if (v < d) {
                    d = v;
                }
            }
            if (y < h - 1) {
                size_t next_row = row + (size_t) w;
                double v1       = dist[next_row + (size_t) x] + 3.0;
                if (v1 < d) {
                    d = v1;
                }
                if (x < w - 1) {
                    double v2 = dist[next_row + (size_t) x + 1] + 4.0;
                    if (v2 < d) {
                        d = v2;
                    }
                }
                if (x > 0) {
                    double v3 = dist[next_row + (size_t) x - 1] + 4.0;
                    if (v3 < d) {
                        d = v3;
                    }
                }
            }
            dist[idx] = d;
        }
    }

    for (size_t i = 0; i < n; i++) {
        out_px_dist[i] = dist[i] / 3.0;
    }

    lmt_memory_free(dist);
    return 1;
}

static bytemap_data *alloc_bytemap_userdata(lua_State *L, int nx, int ny, int nz)
{
    if (! bytemap_okay(nx, ny, nz)) {
        return NULL;
    }
    bytemap_data *bm = lua_newuserdatauv(L, sizeof(bytemap_data), 0);
    if (! bm) {
        return NULL;
    }
    lua_get_metatablelua(bytemap_instance);
    lua_setmetatable(L, -2);
    bytemap_allocate(bm, nx, ny, nz, NULL);
    if (! bm->data) {
        lua_pop(L, 1);
        return NULL;
    }
    return bm;
}

static int renderlib_glyph(lua_State *L)
{
    if (lua_type(L, 1) != LUA_TTABLE) {
        return 0;
    }

    int top_before = lua_gettop(L);

    glyph_render_opts opts;
    read_glyph_options(L, 2, &opts);

    lua_getfield(L, 1, "glyph");
    int glyph_idx = 1;
    if (lua_type(L, -1) == LUA_TTABLE) {
        glyph_idx = lua_gettop(L);
        lua_getfield(L, 1, "xoffset");
        if (lua_type(L, -1) == LUA_TNUMBER && isfinite(lua_tonumber(L, -1))) {
            opts.xoff = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);
        lua_getfield(L, 1, "yoffset");
        if (lua_type(L, -1) == LUA_TNUMBER && isfinite(lua_tonumber(L, -1))) {
            opts.yoff = lua_tonumber(L, -1);
        }
    }
    lua_pop(L, 1);

    double adv = 500.0;
    lua_getfield(L, glyph_idx, "width");
    if (lua_type(L, -1) == LUA_TNUMBER) {
        double w = lua_tonumber(L, -1);
        if (isfinite(w) && w > 0.0) {
            adv = w;
        }
    }
    lua_pop(L, 1);

    double bb_llx = fmin(0.0, adv);
    double bb_lly = -200.0;
    double bb_urx = fmax(0.0, adv);
    double bb_ury = 750.0;

    lua_getfield(L, glyph_idx, "boundingbox");
    if (lua_type(L, -1) == LUA_TTABLE) {
        int bb_idx = lua_gettop(L);
        if (lua_rawlen(L, bb_idx) >= 4) {
            bb_llx = lua_table_num(L, bb_idx, 1, bb_llx);
            bb_lly = lua_table_num(L, bb_idx, 2, bb_lly);
            bb_urx = lua_table_num(L, bb_idx, 3, bb_urx);
            bb_ury = lua_table_num(L, bb_idx, 4, bb_ury);
            if (bb_urx < bb_llx) { 
                double tmp = bb_llx; 
                bb_llx = bb_urx; 
                bb_urx = tmp; 
            }
            if (bb_ury < bb_lly) { 
                double tmp = bb_lly; 
                bb_lly = bb_ury; 
                bb_ury = tmp; 
            }
            if (bb_llx == bb_urx) { 
                bb_urx = bb_llx + fmax(adv, 1.0);
            }
            if (bb_lly == bb_ury) {
                bb_ury = bb_lly + 1.0;
            }
        }
    }
    lua_pop(L, 1);

    glyph_mapper mapper;
    build_mapper(&opts, adv, bb_llx, bb_lly, bb_urx, bb_ury, &mapper);

    if (! bytemap_okay(mapper.width, mapper.height, 1)) {
        lua_settop(L, top_before);
        return 0;
    }

    int grid  = opts.grid < 1 ? 1 : opts.grid;
    int steps = opts.steps < 1 ? 1 : opts.steps;

    glyph_scratch scratch;
    glyph_scratch_init(&scratch);

    bytemap_data *bm      = NULL;
    int           ok      = 0;
    int           seg_idx = 0;

    if (!glyph_init_subpixel_grid(&scratch, grid)) {
        goto cleanup;
    }

    lua_getfield(L, glyph_idx, "segments");
    seg_idx = lua_gettop(L);
    if (!collect_glyph_segments(L, seg_idx, &mapper, steps, &scratch)) {
        lua_pop(L, 1);
        goto cleanup;
    }
    lua_pop(L, 1);

    bm = alloc_bytemap_userdata(L, mapper.width, mapper.height, 1);
    if (! bm) {
        goto cleanup;
    }

    bm->ox      = isfinite(mapper.xoff) ? (int) lround(mapper.xoff) : 0;
    bm->oy      = isfinite(mapper.yoff) ? (int) lround(mapper.yoff) : 0;
    bm->usage   = bytemap_usage_glyph;
    bm->scale   = mapper.scale;
    bm->advance = adv;

    if (! glyph_rasterize_to_bytemap(bm, &scratch, grid, opts.invert)) {
        goto cleanup;
    }

    lua_insert(L, top_before + 1);
    lua_settop(L, top_before + 1);
    ok = 1;

  cleanup:
    glyph_scratch_free(&scratch);
    if (! ok) {
        if (bm && bm->data) {
            lmt_memory_free(bm->data);
            bm->data = NULL;
        }
        lua_settop(L, top_before);
        return 0;
    }
    return 1;
}

/* 
    render.chamfer(bm, opts) -> bytemap : 
    
    computes proximity heat (default) or distance map from a 1-channel bytemap. 
*/

static int renderlib_chamfer(lua_State *L)
{
    bytemap_data *src = bytemaplib_valid(L, 1);
    if (! src || ! src->data || src->nz < 1 || src->nx < 1 || src->ny < 1) {
        return 0;
    }

    int    opts_idx   = (lua_type(L, 2) == LUA_TTABLE) ? 2 : 0;
    double ink_thresh = lua_opt_field_num(L, opts_idx, "ink_thresh", 0.25);
    double radius     = lua_opt_field_num(L, opts_idx, "radius", 12.0);
    double max_dist   = lua_opt_field_num(L, opts_idx, "max_dist", 0.0);
    int    as_heat    = 1;

    if (opts_idx) {
        lua_getfield(L, opts_idx, "heat");
        if (lua_type(L, -1) == LUA_TBOOLEAN) {
            as_heat = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
    }

    int    w      = src->nx;
    int    h      = src->ny;
    int    stride = src->nz;
    size_t n      = (size_t) w * (size_t) h;

    unsigned char thresh_byte = (unsigned char) lround(fmax(0.0, fmin(1.0, ink_thresh)) * 255.0);

    unsigned char *mask = lmt_memory_malloc(n);
    double *px_dist = lmt_memory_malloc(n * sizeof(double));
    if (!mask || !px_dist) {
        if (mask) lmt_memory_free(mask);
        if (px_dist) lmt_memory_free(px_dist);
        return 0;
    }

    for (size_t i = 0; i < n; i++) {
        mask[i] = (src->data[i * (size_t) stride] >= thresh_byte) ? 1 : 0;
    }

    if (!chamfer_px_from_mask(mask, w, h, px_dist)) {
        lmt_memory_free(mask);
        lmt_memory_free(px_dist);
        return 0;
    }

    bytemap_data *out = alloc_bytemap_userdata(L, w, h, 1);
    if (!out) {
        lmt_memory_free(mask);
        lmt_memory_free(px_dist);
        return 0;
    }

    out->ox      = src->ox;
    out->oy      = src->oy;
    out->usage   = bytemap_usage_unset;
    out->scale   = src->scale;
    out->advance = src->advance;

    double inv_r  = 1.0 / fmax(radius, 0.001);
    double m_dist = max_dist > 0.0 ? max_dist : fmax(radius * 4.0, 1.0);

    for (size_t i = 0; i < n; i++) {
        double d = px_dist[i];
        double v = as_heat ? exp(-d * inv_r) : (1.0 - fmin(d, m_dist) / m_dist);
        int    b = (int) lround(fmax(0.0, fmin(1.0, v)) * 255.0);
        out->data[i] = valid_byte(b);
    }

    lmt_memory_free(mask);
    lmt_memory_free(px_dist);
    return 1;
}

/* 
    render.scorepair(bm1, bm2, kern, opts) -> score, info_table

    evaluates pair coupling, overlap, and clearance deficit between two pre-rendered
    1-channel glyph bytemaps (`bm1` and `bm2`) at `kern` (in font units)
*/

static int renderlib_scorepair(lua_State *L)
{
    bytemap_data *bm1 = bytemaplib_valid(L, 1);
    bytemap_data *bm2 = bytemaplib_valid(L, 2);

    if (! bm1 || ! bm2 || ! bm1->data || ! bm2->data || bm1->nx < 1 || bm1->ny < 1 || bm2->nx < 1 || bm2->ny < 1) {
        return 0;
    }

    double kern     = (lua_type(L, 3) == LUA_TNUMBER && isfinite(lua_tonumber(L, 3))) ? lua_tonumber(L, 3) : 0.0;
    int    opts_idx = (lua_type(L, 4) == LUA_TTABLE) ? 4 : 0;

    int h1    = bm1->ny;
    int h2    = bm2->ny;
    int h_ref = h1 > h2 ? h1 : h2;

    double scale = (bm1->scale > 0.0 && isfinite(bm1->scale)) ? bm1->scale : lua_opt_field_num(L, opts_idx, "scale", 1.0);
    scale = lua_opt_field_num(L, opts_idx, "scale", scale);
    if (scale <= 0.0 || ! isfinite(scale)) {
        scale = 1.0;
    }

    double adv1 = (bm1->advance > 0.0 && isfinite(bm1->advance)) ? bm1->advance : ((double) bm1->nx / scale);
    adv1 = lua_opt_field_num(L, opts_idx, "advance1", adv1);

    double ink_thresh       = lua_opt_field_num(L, opts_idx, "ink_thresh",         0.25);
    double radius_px        = lua_opt_field_num(L, opts_idx, "coupling_radius_px", lua_opt_field_num(L, opts_idx, "falloff_px", fmax(6.0, (double) h_ref * 0.14)));
    double min_gap_px       = lua_opt_field_num(L, opts_idx, "min_gap_px",         fmax(1.5, (double) h_ref * 0.025));
    double overlap_weight   = lua_opt_field_num(L, opts_idx, "overlap_weight",     500.0);
    double clearance_weight = lua_opt_field_num(L, opts_idx, "clearance_weight",   40.0);
    double target_coupling  = lua_opt_field_num(L, opts_idx, "target_coupling",    0.45);
    double origin_shift_px  = lua_opt_field_num(L, opts_idx, "origin_shift_px",    0.0);

    int    has_edge_depth = 0;
    double edge_depth_px  = 0.0;
    int    is_logo        = 0;

    if (opts_idx) {
        lua_getfield(L, opts_idx, "edge_depth_px");
        if (lua_type(L, -1) == LUA_TNUMBER && isfinite(lua_tonumber(L, -1))) {
            has_edge_depth = 1;
            edge_depth_px  = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, opts_idx, "logo");
        if (lua_type(L, -1) == LUA_TBOOLEAN && lua_toboolean(L, -1)) is_logo = 1;
        lua_pop(L, 1);

        if (!is_logo) {
            lua_getfield(L, opts_idx, "mode");
            if (lua_type(L, -1) == LUA_TSTRING && strcmp(lua_tostring(L, -1), "logo") == 0) is_logo = 1;
            lua_pop(L, 1);
        }
    }

    unsigned char thresh_byte = (unsigned char) lround(fmax(0.0, fmin(1.0, ink_thresh)) * 255.0);

    int w1 = bm1->nx;
    int w2 = bm2->nx;

    double k_min   = lua_opt_field_num(L, opts_idx, "kern_min", -300.0);
    double k_max   = lua_opt_field_num(L, opts_idx, "kern_max", 50.0);
    int    min_dx2 = (int) floor((adv1 + fmin(k_min, 0.0)) * scale + origin_shift_px) - 2;
    int    max_dx2 = (int) ceil ((adv1 + fmax(k_max, 0.0)) * scale + origin_shift_px) + w2 + 2;

    int canvas_min_x = min_dx2 < 0 ? min_dx2 : 0;
    int canvas_max_x = w1 > max_dx2 ? w1 : max_dx2;
    int cw           = canvas_max_x - canvas_min_x;
    int ch           = h_ref;

    if (cw < 1) {
        cw = 1;
    }
    if (ch < 1) {
        ch = 1;
    }

    size_t         cn    = (size_t) cw * (size_t) ch;
    unsigned char *mask1 = lmt_memory_malloc(cn);
    double        *dist1 = lmt_memory_malloc(cn * sizeof(double));
    if (! mask1 || ! dist1) {
        if (mask1) { 
            lmt_memory_free(mask1);
        }
        if (dist1) { 
            lmt_memory_free(dist1);
        }
        return 0;
    }

    memset(mask1, 0, cn);

    int ox1 = -canvas_min_x;
    int s1  = bm1->nz > 0 ? bm1->nz : 1;

    for (int y = 0; y < h1 && y < ch; y++) {
        size_t src_row = (size_t) y * (size_t) w1 * (size_t) s1;
        size_t dst_row = (size_t) y * (size_t) cw;
        for (int x = 0; x < w1; x++) {
            int dx = ox1 + x;
            if (dx >= 0 && dx < cw && bm1->data[src_row + (size_t) x * (size_t) s1] >= thresh_byte) {
                mask1[dst_row + (size_t) dx] = 1;
            }
        }
    }

    if (!chamfer_px_from_mask(mask1, cw, ch, dist1)) {
        lmt_memory_free(mask1);
        lmt_memory_free(dist1);
        return 0;
    }
    lmt_memory_free(mask1);

    int s2                   = bm2->nz > 0 ? bm2->nz : 1;
    int dx2                  = (int) floor(adv1 * scale + kern * scale + origin_shift_px + 0.5);
    double overlap_area      = 0.0;
    double clearance_deficit = 0.0;
    double coupling_sum      = 0.0;
    size_t n_samples         = 0;
    double inv_r             = 1.0 / fmax(radius_px, 0.001);
    double gap_denom         = fmax(min_gap_px, 1.0);

    for (int y = 0; y < h2; y++) {
        size_t r2 = (size_t) y * (size_t) w2 * (size_t) s2;
        int first_x = -1;
        if (has_edge_depth) {
            for (int x = 0; x < w2; x++) {
                if (bm2->data[r2 + (size_t) x * (size_t) s2] >= thresh_byte) {
                    first_x = x;
                    break;
                }
            }
            if (first_x < 0) {
                continue;
            }
        }
        int max_x_in_row = has_edge_depth
            ? (int) fmin((double) (w2 - 1), floor((double) first_x + edge_depth_px))
            : (w2 - 1);

        for (int x = 0; x <= max_x_in_row; x++) {
            unsigned char b2 = bm2->data[r2 + (size_t) x * (size_t) s2];
            if (b2 >= thresh_byte) {
                double c2 = (double) b2 / 255.0;
                n_samples++;
                int cx = dx2 + x - canvas_min_x;
                int cy = y;
                if (cx >= 0 && cx < cw && cy >= 0 && cy < ch) {
                    double d = dist1[(size_t) cy * (size_t) cw + (size_t) cx];
                    int gx1 = dx2 + x;
                    double c1 = 0.0;
                    if (y < h1 && gx1 >= 0 && gx1 < w1) {
                        c1 = (double) bm1->data[(size_t) y * (size_t) w1 * (size_t) s1 + (size_t) gx1 * (size_t) s1] / 255.0;
                    }
                    if (c1 > 0.0 && d <= 0.5) {
                        overlap_area += fmin(c1, c2);
                    }
                    if (d < min_gap_px) {
                        double shortfall = min_gap_px - d;
                        clearance_deficit += shortfall / gap_denom;
                    }
                    double clamped_d = fmax(0.0, d);
                    coupling_sum += 1.0 / (1.0 + clamped_d * inv_r);
                }
            }
        }
    }

    lmt_memory_free(dist1);

    double avg_coupling      = (n_samples > 0) ? (coupling_sum / (double) n_samples) : 0.0;
    double avg_deficit       = (n_samples > 0) ? (clearance_deficit / (double) n_samples) : 0.0;
    double tightness_cost    = is_logo ? fmax(0.0, target_coupling - avg_coupling) : fabs(avg_coupling - target_coupling);
    double overlap_penalty   = overlap_weight * (overlap_area + overlap_area * overlap_area);
    double clearance_penalty = clearance_weight * avg_deficit;
    double total_score       = overlap_penalty + clearance_penalty + tightness_cost;

    lua_pushnumber(L, total_score);
    lua_createtable(L, 0, 4);
    lua_pushnumber(L, overlap_area);
    lua_setfield(L, -2, "overlap");
    lua_pushnumber(L, avg_deficit);
    lua_setfield(L, -2, "avg_deficit");
    lua_pushnumber(L, avg_coupling);
    lua_setfield(L, -2, "avg_coupling");
    lua_pushinteger(L, (lua_Integer) n_samples);
    lua_setfield(L, -2, "active_rows");
    return 2;
}

/* 
    render.heatmap(placements, opts) -> bytemap

   `placements` is an array of { bm = bytemap, x_px = number } (or { bytemap = ..., x_px = ... }).
   Composites placed glyphs onto a shared canvas of height `target_height` (or max bm->ny) and
   returns a 1-channel chamfer proximity heatmap bytemap 
*/

static int renderlib_heatmap(lua_State *L)
{
    if (lua_type(L, 1) != LUA_TTABLE) {
        return 0;
    }

    int n_items = (int) lua_rawlen(L, 1);
    if (n_items < 1) {
        return 0;
    }

    int           opts_idx    = (lua_type(L, 2) == LUA_TTABLE) ? 2 : 0;
    double        ink_thresh  = lua_opt_field_num(L, opts_idx, "ink_thresh", 0.25);
    double        radius      = lua_opt_field_num(L, opts_idx, "radius", 12.0);
    int           opt_h       = (int) lround(lua_opt_field_num(L, opts_idx, "target_height", 0.0));
    unsigned char thresh_byte = (unsigned char) lround(fmax(0.0, fmin(1.0, ink_thresh)) * 255.0);

    int min_x_px = 0;
    int max_x_px = 1;
    int max_h    = opt_h > 0 ? opt_h : 1;

    for (int i = 1; i <= n_items; i++) {
        lua_rawgeti(L, 1, i);
        if (lua_type(L, -1) == LUA_TTABLE) {
            lua_getfield(L, -1, "bm");
            bytemap_data *bm = bytemaplib_valid(L, -1);
            if ( !bm) {
                lua_pop(L, 1);
                lua_getfield(L, -1, "bytemap");
                bm = bytemaplib_valid(L, -1);
            }
            lua_pop(L, 1); /* pop bm/bytemap */
            if (bm && bm->data && bm->nx > 0 && bm->ny > 0) {
                int x_px = (int) lround(lua_opt_field_num(L, -1, "x_px", 0.0));
                if (x_px < min_x_px) {
                    min_x_px = x_px;
                }
                if (x_px + bm->nx > max_x_px) {
                    max_x_px = x_px + bm->nx;
                }
                if (opt_h <= 0 && bm->ny > max_h) {
                    max_h = bm->ny;
                }
            }
        }
        lua_pop(L, 1); /* pop entry */
    }

    int shift_x = -min_x_px;
    int width   = max_x_px - min_x_px;
    int height  = max_h;

    if (width < 1) { 
        width = 1;
    }
    if (height < 1) { 
        height = 1;
    }

    size_t         n       = (size_t) width * (size_t) height;
    unsigned char *mask    = lmt_memory_malloc(n);
    double        *px_dist = lmt_memory_malloc(n * sizeof(double));

    if (! mask || ! px_dist) {
        if (mask) { 
            lmt_memory_free(mask);
        }
        if (px_dist) {
            lmt_memory_free(px_dist);
        }
        return 0;
    }

    memset(mask, 0, n);

    for (int i = 1; i <= n_items; i++) {
        lua_rawgeti(L, 1, i);
        if (lua_type(L, -1) == LUA_TTABLE) {
            lua_getfield(L, -1, "bytemap");
            bytemap_data *bm = bytemaplib_valid(L, -1);
            lua_pop(L, 1);
            if (bm && bm->data && bm->nx > 0 && bm->ny > 0) {
                int x_px   = (int) lround(lua_opt_field_num(L, -1, "x_px", 0.0));
                int ox     = x_px + shift_x;
                int s      = bm->nz > 0 ? bm->nz : 1;
                int copy_h = bm->ny < height ? bm->ny : height;
                for (int y = 0; y < copy_h; y++) {
                    size_t src_row = (size_t) y * (size_t) bm->nx * (size_t) s;
                    size_t dst_row = (size_t) y * (size_t) width;
                    for (int x = 0; x < bm->nx; x++) {
                        int dx = ox + x;
                        if (dx >= 0 && dx < width) {
                            if (bm->data[src_row + (size_t) x * (size_t) s] >= thresh_byte) {
                                mask[dst_row + (size_t) dx] = 1;
                            }
                        }
                    }
                }
            }
        }
        lua_pop(L, 1);
    }

    if (!chamfer_px_from_mask(mask, width, height, px_dist)) {
        lmt_memory_free(mask);
        lmt_memory_free(px_dist);
        return 0;
    }

    bytemap_data *out = alloc_bytemap_userdata(L, width, height, 1);
    if (! out) {
        lmt_memory_free(mask);
        lmt_memory_free(px_dist);
        return 0;
    }

    out->usage = bytemap_usage_unset;
    double inv_r = 1.0 / fmax(radius, 0.001);
    for (size_t i = 0; i < n; i++) {
        double heat = exp(-px_dist[i] * inv_r);
        int    b    = (int) lround(fmax(0.0, fmin(1.0, heat)) * 255.0);
        out->data[i] = valid_byte(b);
    }

    lmt_memory_free(mask);
    lmt_memory_free(px_dist);
    return 1;
}

typedef struct pair_sample {
    int    x;
    int    y;
    double c2;
} pair_sample;

static double eval_kern_against_field(
    const bytemap_data *bm1,
    int                 w1,
    int                 h1,
    int                 s1,
    const double       *dist1,
    int                 cw,
    int                 ch,
    int                 canvas_min_x,
    const pair_sample  *samples,
    size_t              n_samples,
    double              adv1,
    double              scale,
    double              origin_shift_px,
    double              kern,
    double              min_gap_px,
    double              inv_r,
    double              overlap_weight,
    double              clearance_weight,
    double              target_coupling,
    int                 is_logo,
    double             *out_overlap,
    double             *out_deficit,
    double             *out_coupling)
{
    int dx2 = (int) floor(adv1 * scale + kern * scale + origin_shift_px + 0.5);

    double overlap_area      = 0.0;
    double clearance_deficit = 0.0;
    double coupling_sum      = 0.0;
    double gap_denom         = fmax(min_gap_px, 1.0);

    for (size_t i = 0; i < n_samples; i++) {
        const pair_sample *s  = &samples[i];
        int                cx = dx2 + s->x - canvas_min_x;
        int                cy = s->y;
        if (cx >= 0 && cx < cw && cy >= 0 && cy < ch) {
            double d   = dist1[(size_t) cy * (size_t) cw + (size_t) cx];
            int    gx1 = dx2 + s->x;
            double c1  = 0.0;
            if (s->y < h1 && gx1 >= 0 && gx1 < w1) {
                c1 = (double) bm1->data[(size_t) s->y * (size_t) w1 * (size_t) s1 + (size_t) gx1 * (size_t) s1] / 255.0;
            }
            if (c1 > 0.0 && d <= 0.5) {
                overlap_area += fmin(c1, s->c2);
            }
            if (d < min_gap_px) {
                double shortfall = min_gap_px - d;
                clearance_deficit += shortfall / gap_denom;
            }
            double clamped_d = fmax(0.0, d);
            coupling_sum += 1.0 / (1.0 + clamped_d * inv_r);
        }
    }

    double avg_coupling      = (n_samples > 0) ? (coupling_sum      / (double) n_samples) : 0.0;
    double avg_deficit       = (n_samples > 0) ? (clearance_deficit / (double) n_samples) : 0.0;
    double tightness_cost    = is_logo ? fmax(0.0, target_coupling - avg_coupling) : fabs(avg_coupling - target_coupling);
    double overlap_penalty   = overlap_weight   * (overlap_area + overlap_area * overlap_area);
    double clearance_penalty = clearance_weight * avg_deficit;

    if (out_overlap)  { *out_overlap  = overlap_area; }
    if (out_deficit)  { *out_deficit  = avg_deficit;  }
    if (out_coupling) { *out_coupling = avg_coupling; }

    return overlap_penalty + clearance_penalty + tightness_cost;
}

static int renderlib_scankern(lua_State *L)
{
    bytemap_data *bm1 = bytemaplib_valid(L, 1);
    bytemap_data *bm2 = bytemaplib_valid(L, 2);

    if (! bm1 || ! bm2 || ! bm1->data || ! bm2->data || bm1->nx < 1 || bm1->ny < 1 || bm2->nx < 1 || bm2->ny < 1) {
        return 0;
    }

    int    opts_idx = (lua_type(L, 3) == LUA_TTABLE) ? 3 : 0;
    int    h1       = bm1->ny;
    int    h2       = bm2->ny;
    int    h_ref    = h1 > h2 ? h1 : h2;
    double scale    = lua_opt_field_num(L, opts_idx, "scale", (bm1->scale > 0.0 && isfinite(bm1->scale)) ? bm1->scale : 1.0);

    if (scale <= 0.0 || !isfinite(scale)) {
        scale = 1.0;
    }

    double adv1             = lua_opt_field_num(L, opts_idx, "advance1",           (bm1->advance > 0.0 && isfinite(bm1->advance)) ? bm1->advance : ((double) bm1->nx / scale));
    double ink_thresh       = lua_opt_field_num(L, opts_idx, "ink_thresh",         0.25);
    double radius_px        = lua_opt_field_num(L, opts_idx, "coupling_radius_px", lua_opt_field_num(L, opts_idx, "falloff_px", fmax(6.0, (double) h_ref * 0.14)));
    double min_gap_px       = lua_opt_field_num(L, opts_idx, "min_gap_px",         fmax(1.5, (double) h_ref * 0.025));
    double overlap_weight   = lua_opt_field_num(L, opts_idx, "overlap_weight",     500.0);
    double clearance_weight = lua_opt_field_num(L, opts_idx, "clearance_weight",   40.0);
    double target_coupling  = lua_opt_field_num(L, opts_idx, "target_coupling",    0.45);
    double origin_shift_px  = lua_opt_field_num(L, opts_idx, "origin_shift_px",    0.0);
    double k_min            = lua_opt_field_num(L, opts_idx, "kern_min",           -250.0);
    double k_max            = lua_opt_field_num(L, opts_idx, "kern_max",           50.0);
    double coarse_step      = lua_opt_field_num(L, opts_idx, "coarse_step",        20.0);
    double fine_step        = lua_opt_field_num(L, opts_idx, "kern_step",          lua_opt_field_num(L, opts_idx, "fine_step", 5.0));

    if (coarse_step <= 0.0) {
        coarse_step = 20.0;
    }
    if (fine_step <= 0.0) {
        fine_step   = 5.0;
    }
    if (k_max < k_min) { 
        double tmp = k_min; 
        k_min = k_max; 
        k_max = tmp; 
    }

    int    has_edge_depth = 0;
    double edge_depth_px  = 0.0;
    int    is_logo        = 0;

    if (opts_idx) {
        lua_getfield(L, opts_idx, "edge_depth_px");
        if (lua_type(L, -1) == LUA_TNUMBER && isfinite(lua_tonumber(L, -1))) {
            has_edge_depth = 1;
            edge_depth_px = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, opts_idx, "logo");
        if (lua_type(L, -1) == LUA_TBOOLEAN && lua_toboolean(L, -1)) is_logo = 1;
        lua_pop(L, 1);

        if (!is_logo) {
            lua_getfield(L, opts_idx, "mode");
            if (lua_type(L, -1) == LUA_TSTRING && strcmp(lua_tostring(L, -1), "logo") == 0) is_logo = 1;
            lua_pop(L, 1);
        }
    }

    unsigned char thresh_byte = (unsigned char) lround(fmax(0.0, fmin(1.0, ink_thresh)) * 255.0);

    int w1           = bm1->nx;
    int w2           = bm2->nx;
    int min_dx2      = (int) floor((adv1 + fmin(k_min, 0.0)) * scale + origin_shift_px) - 2;
    int max_dx2      = (int) ceil((adv1 + fmax(k_max, 0.0)) * scale + origin_shift_px) + w2 + 2;
    int canvas_min_x = min_dx2 < 0 ? min_dx2 : 0;
    int canvas_max_x = w1 > max_dx2 ? w1 : max_dx2;
    int cw           = canvas_max_x - canvas_min_x;
    int ch           = h_ref < 1 ? 1 : h_ref;

    if (cw < 1) {
        cw = 1;
    }

    size_t         cn          = (size_t) cw * (size_t) ch;
    unsigned char *mask1       = lmt_memory_malloc(cn);
    double        *dist1       = lmt_memory_malloc(cn * sizeof(double));
    size_t         max_samples = (size_t) w2 * (size_t) h2;
    pair_sample   *samples     = max_samples > 0 ? lmt_memory_malloc(max_samples * sizeof(pair_sample)) : NULL;

    if (! mask1 || ! dist1 || (max_samples > 0 && !samples)) {
        if (mask1) { 
            lmt_memory_free(mask1);
        }
        if (dist1) {
            lmt_memory_free(dist1);
        }
        if (samples) {
            lmt_memory_free(samples);
        }
        return 0;
    }

    memset(mask1, 0, cn);

    int ox1 = -canvas_min_x;
    int s1  = bm1->nz > 0 ? bm1->nz : 1;

    for (int y = 0; y < h1 && y < ch; y++) {
        size_t src_row = (size_t) y * (size_t) w1 * (size_t) s1;
        size_t dst_row = (size_t) y * (size_t) cw;
        for (int x = 0; x < w1; x++) {
            int dx = ox1 + x;
            if (dx >= 0 && dx < cw && bm1->data[src_row + (size_t) x * (size_t) s1] >= thresh_byte) {
                mask1[dst_row + (size_t) dx] = 1;
            }
        }
    }

    if (! chamfer_px_from_mask(mask1, cw, ch, dist1)) {
        lmt_memory_free(mask1);
        lmt_memory_free(dist1);
        if (samples) lmt_memory_free(samples);
        return 0;
    }
    lmt_memory_free(mask1);

    int    s2        = bm2->nz > 0 ? bm2->nz : 1;
    size_t n_samples = 0;

    for (int y = 0; y < h2; y++) {
        size_t r2 = (size_t) y * (size_t) w2 * (size_t) s2;
        int first_x = -1;
        if (has_edge_depth) {
            for (int x = 0; x < w2; x++) {
                if (bm2->data[r2 + (size_t) x * (size_t) s2] >= thresh_byte) {
                    first_x = x;
                    break;
                }
            }
            if (first_x < 0) continue;
        }
        int max_x_in_row = has_edge_depth
            ? (int) fmin((double) (w2 - 1), floor((double) first_x + edge_depth_px))
            : (w2 - 1);

        for (int x = 0; x <= max_x_in_row; x++) {
            unsigned char b2 = bm2->data[r2 + (size_t) x * (size_t) s2];
            if (b2 >= thresh_byte) {
                samples[n_samples++] = (pair_sample) {
                    .x  = x,
                    .y  = y,
                    .c2 = (double) b2 / 255.0,
                };
            }
        }
    }

    double inv_r      = 1.0 / fmax(radius_px, 0.001);
    int    has_seed   = 0;
    int    has_best   = 0;
    double seed_k     = 0.0;
    double best_k     = 0.0;
    double best_score = INFINITY;
    double best_ov    = 0.0;
    double best_def   = 0.0;
    double best_coup  = 0.0;

    /* Coarse pass: no-overlap first, fallback to allowing overlap */
    for (int pass = 0; pass < 2 && !has_seed; pass++) {
        int req_no_ov = (pass == 0);
        for (double k = k_min; k <= k_max + 1e-9; k += coarse_step) {
            double ov = 0.0, def = 0.0, coup = 0.0;
            double sc = eval_kern_against_field(
                bm1, w1, h1, s1, dist1, cw, ch, canvas_min_x,
                samples, n_samples, adv1, scale, origin_shift_px, k,
                min_gap_px, inv_r, overlap_weight, clearance_weight, target_coupling, is_logo,
                &ov, &def, &coup
            );
            if ((! req_no_ov || ov == 0.0) && sc < best_score) {
                best_score = sc;
                seed_k     = k;
                has_seed   = 1;
            }
        }
    }
    if (! has_seed) {
        seed_k = 0.0;
    }

    double fine_from = fmax(k_min, seed_k - coarse_step);
    double fine_to   = fmin(k_max, seed_k + coarse_step);

    best_score = INFINITY;

    for (int pass = 0; pass < 2 && !has_best; pass++) {
        int req_no_ov = (pass == 0);
        for (double k = fine_from; k <= fine_to + 1e-9; k += fine_step) {
            double ov   = 0.0;
            double def  = 0.0;
            double coup = 0.0;
            double sc = eval_kern_against_field(
                bm1, w1, h1, s1, dist1, cw, ch, canvas_min_x,
                samples, n_samples, adv1, scale, origin_shift_px, k,
                min_gap_px, inv_r, overlap_weight, clearance_weight, target_coupling, is_logo,
                &ov, &def, &coup
            );
            if ((! req_no_ov || ov == 0.0) && sc < best_score) {
                best_score = sc;
                best_k     = k;
                best_ov    = ov;
                best_def   = def;
                best_coup  = coup;
                has_best   = 1;
            }
        }
    }

    lmt_memory_free(dist1);
    if (samples) { 
        lmt_memory_free(samples);
    }

    lua_pushnumber(L, has_best ? best_k : 0.0);
    lua_pushnumber(L, has_best ? best_score : 0.0);
    lua_createtable(L, 0, 4);
    lua_pushnumber(L, best_ov);
    lua_setfield(L, -2, "overlap");
    lua_pushnumber(L, best_def);
    lua_setfield(L, -2, "avg_deficit");
    lua_pushnumber(L, best_coup);
    lua_setfield(L, -2, "avg_coupling");
    lua_pushinteger(L, (lua_Integer) n_samples);
    lua_setfield(L, -2, "active_rows");
    return 3;
}

/* Replace aux_check_bytemap / lmt_render_protrusion with your exact lmtrenderlib macros/names */

/*tex

Practical Default Baseline for \type{render.protrusion(bm, opts)}:

\startitemize

\startitem
    \type{ink_thresh = 0.25}: Pixel coverage threshold (in \type{[0.0, 1.0]}, mapped to
    \type{0..255}) separating inked edges from subpixel anti-aliasing fringes. Requires
    \type{invert = true} when rasterizing glyphs so ink corresponds to high byte values.
\stopitem

\startitem
    \type{deadzone = 0.03}: Normalized inset fraction subtracted per active scanline
    relative to \type{[g_min_x, g_max_x]} divided by \type{ink_span}, suppressing minor
    stem and rounding noise.
\stopitem

\startitem
    \type{gain = 0.40}: Linear multiplier applied to the mean above-deadzone left and
    right inset ratios across active rows.
\stopitem

\startitem
    \type{max_protrusion = 0.07}: Upper clamp on resulting left and right protrusion
    fractions so overhangs and wide shapes stay restrained prior to class or punctuation
    scaling.
\stopitem

startitem
    \type{left_y_bias = 0.0} and \type{right_y_bias = 0.0}: Vertical row weighting
    biases over active scanlines (\type{y} from \type{0} at top to \type{ny - 1} at
    bottom), centered at \type{0.5} via \type{1.0 + bias * (u - 0.5)} and clamped
    at \type{0.0}. Positive \type{left_y_bias} favors lower rows on the left margin,
    while negative \type{right_y_bias} favors upper rows on the right margin; zero
    leaves left and right row averages unweighted.
\stopitem

\stopitemize

*/

static int renderlib_protrusion(lua_State *L)
{
    bytemap_data *bm = bytemaplib_valid(L, 1);
    if (! bm || ! bm->data || ! bytemap_okay(bm->nx, bm->ny, bm->nz)) {
        lua_createtable(L, 0, 5);
        lua_pushnumber (L, 0.0); lua_setfield(L, -2, "left");
        lua_pushnumber (L, 0.0); lua_setfield(L, -2, "right");
        lua_pushnumber (L, 0.0); lua_setfield(L, -2, "left_ratio");
        lua_pushnumber (L, 0.0); lua_setfield(L, -2, "right_ratio");
        lua_pushinteger(L, 0);   lua_setfield(L, -2, "active_rows");
        return 1;
    }

    int    opts_idx       = (lua_type(L, 2) == LUA_TTABLE) ? 2 : 0;
    double ink_thresh     = lua_opt_field_num(L, opts_idx, "ink_thresh",     0.25);
    double deadzone       = lua_opt_field_num(L, opts_idx, "deadzone",       0.03);
    double gain           = lua_opt_field_num(L, opts_idx, "gain",           0.40);
    double max_protrusion = lua_opt_field_num(L, opts_idx, "max_protrusion", 0.07);
    double left_y_bias    = lua_opt_field_num(L, opts_idx, "left_y_bias",    0.0);
    double right_y_bias   = lua_opt_field_num(L, opts_idx, "right_y_bias",   0.0);

    if (ink_thresh < 0.0)     { ink_thresh = 0.0; }
    if (ink_thresh > 1.0)     { ink_thresh = 1.0; }
    if (deadzone < 0.0)       { deadzone = 0.0; }
    if (gain < 0.0)           { gain = 0.0; }
    if (max_protrusion < 0.0) { max_protrusion = 0.0; }

    int thresh_byte = (int) lround(ink_thresh * 255.0);
    int nx = bm->nx;
    int ny = bm->ny;
    int nz = bm->nz;

    int *row_y     = lmt_memory_malloc((size_t) ny * sizeof(int));
    int *row_left  = lmt_memory_malloc((size_t) ny * sizeof(int));
    int *row_right = lmt_memory_malloc((size_t) ny * sizeof(int));

    if (! row_y || ! row_left || ! row_right) {
        if (row_y)     { lmt_memory_free(row_y); }
        if (row_left)  { lmt_memory_free(row_left); }
        if (row_right) { lmt_memory_free(row_right); }
        return luaL_error(L, "out of memory in render.protrusion");
    }

    int active_rows = 0;
    int g_min_x     = nx;
    int g_max_x     = -1;

    for (int y = 0; y < ny; y++) {
        int l_x = -1;
        int r_x = -1;
        const unsigned char *row_ptr = bm->data + ((size_t) y * (size_t) nx * (size_t) nz);

        for (int x = 0; x < nx; x++) {
            int v;
            if (nz == 1) {
                v = (int) row_ptr[x];
            } else {
                const unsigned char *px = row_ptr + ((size_t) x * (size_t) nz);
                v = (int) px[0];
                for (int c = 1; c < nz; c++) {
                    if ((int) px[c] > v) {
                        v = (int) px[c];
                    }
                }
            }
            if (v >= thresh_byte) {
                if (l_x < 0) {
                    l_x = x;
                }
                r_x = x;
            }
        }

        if (l_x >= 0 && r_x >= l_x) {
            row_y[active_rows]     = y;
            row_left[active_rows]  = l_x;
            row_right[active_rows] = r_x;
            active_rows++;
            if (l_x < g_min_x) { 
                g_min_x = l_x; 
            }
            if (r_x > g_max_x) { 
                g_max_x = r_x; 
            }
        }
    }

    double left_ratio  = 0.0;
    double right_ratio = 0.0;
    double left_p      = 0.0;
    double right_p     = 0.0;

    if (active_rows > 0 && g_max_x >= g_min_x) {
        double ink_span = (double) (g_max_x - g_min_x + 1);
        if (ink_span < 1.0) { 
            ink_span = 1.0; 
        }

        double denom_y      = (ny > 1) ? (double) (ny - 1) : 1.0;
        double left_w_sum   = 0.0;
        double left_wt_sum  = 0.0;
        double right_w_sum  = 0.0;
        double right_wt_sum = 0.0;

        for (int i = 0; i < active_rows; i++) {
            double u        = (double) row_y[i] / denom_y;
            double centered = u - 0.5;

            double w_l = 1.0 + left_y_bias  * centered;
            double w_r = 1.0 + right_y_bias * centered;
            if (w_l < 0.0) { 
                w_l = 0.0; 
            }
            if (w_r < 0.0) { 
                w_r = 0.0; 
            }

            double l_in = ((double) (row_left[i] - g_min_x) / ink_span) - deadzone;
            double r_in = ((double) (g_max_x - row_right[i]) / ink_span) - deadzone;

            if (l_in > 0.0) {
                left_w_sum  += l_in * w_l;
                left_wt_sum += w_l;
            }
            if (r_in > 0.0) {
                right_w_sum  += r_in * w_r;
                right_wt_sum += w_r;
            }
        }

        left_ratio  = (left_wt_sum  > 0.0) ? (left_w_sum  / left_wt_sum)  : 0.0;
        right_ratio = (right_wt_sum > 0.0) ? (right_w_sum / right_wt_sum) : 0.0;

        left_p  = left_ratio  * gain;
        right_p = right_ratio * gain;
        if (left_p  > max_protrusion) {
            left_p  = max_protrusion; 
        }
        if (right_p > max_protrusion) { 
            right_p = max_protrusion; 
        }
    }

    lmt_memory_free(row_y);
    lmt_memory_free(row_left);
    lmt_memory_free(row_right);

    lua_createtable(L, 0, 5);
    lua_pushnumber (L, left_p);      lua_setfield(L, -2, "left");
    lua_pushnumber (L, right_p);     lua_setfield(L, -2, "right");
    lua_pushnumber (L, left_ratio);  lua_setfield(L, -2, "left_ratio");
    lua_pushnumber (L, right_ratio); lua_setfield(L, -2, "right_ratio");
    lua_pushinteger(L, active_rows); lua_setfield(L, -2, "active_rows");
    return 1;
}

/*tex

Asymmetric raster expansion and compression heuristic for \type{render.expansion(bm, opts)}:

\startitemize

\startitem
    \type{ink_thresh = 0.25}: Pixel coverage threshold (in \type{[0.0, 1.0]}, mapped to
    \type{0..255}) separating inked pixels from background/anti-aliasing fringe. Requires
    \type{invert = true} during glyph rasterization so ink corresponds to high values.
\stopitem

\startitem
    \type{stretch_gain = 0.50} and \type{shrink_gain = 0.35}: Separate sensitivity weights
    for stretching (expansion) vs. shrinking (compression). Stretching penalizes wide
    \type{aspect_ratio} and high \type{fill_ratio} so round or wide shapes (\type{O},
    \type{o}, \type{W}) do not look bloated; shrinking penalizes dense/heavy fill more
    cautiously so tight counters and stems do not clog during line compression.
\stopitem

\startitem
    \type{min_exp = 0.45} and \type{max_exp = 1.00} (with optional \type{min_comp} /
    \type{max_comp} falling back to \type{min_exp} / \type{max_exp}): Clamps each directional
    factor independently so both vectors stay in a predictable range.
\stopitem

\stopitemize

*/

static int renderlib_expansion(lua_State *L)
{
    bytemap_data *bm = bytemaplib_valid(L, 1);
    if (! bm || ! bm->data || ! bytemap_okay(bm->nx, bm->ny, bm->nz)) {
        lua_createtable(L, 0, 4);
        lua_pushnumber(L, 1.0); lua_setfield(L, -2, "expansion");
        lua_pushnumber(L, 1.0); lua_setfield(L, -2, "compression");
        lua_pushnumber(L, 0.0); lua_setfield(L, -2, "fill_ratio");
        lua_pushnumber(L, 0.0); lua_setfield(L, -2, "aspect_ratio");
        return 1;
    }

    int    opts_idx     = (lua_type(L, 2) == LUA_TTABLE) ? 2 : 0;
    double ink_thresh   = lua_opt_field_num(L, opts_idx, "ink_thresh",   0.25);
    double stretch_gain = lua_opt_field_num(L, opts_idx, "stretch_gain", lua_opt_field_num(L, opts_idx, "gain", 0.50));
    double shrink_gain  = lua_opt_field_num(L, opts_idx, "shrink_gain",  stretch_gain * 0.70);
    double min_exp      = lua_opt_field_num(L, opts_idx, "min_exp",      0.45);
    double max_exp      = lua_opt_field_num(L, opts_idx, "max_exp",      1.00);
    double min_comp     = lua_opt_field_num(L, opts_idx, "min_comp",     min_exp);
    double max_comp     = lua_opt_field_num(L, opts_idx, "max_comp",     max_exp);

    if (ink_thresh < 0.0)   { ink_thresh = 0.0; }
    if (ink_thresh > 1.0)   { ink_thresh = 1.0; }
    if (stretch_gain < 0.0) { stretch_gain = 0.0; }
    if (shrink_gain < 0.0)  { shrink_gain = 0.0; }
    if (min_exp < 0.0)      { min_exp = 0.0; }
    if (max_exp < min_exp)  { max_exp = min_exp; }
    if (min_comp < 0.0)     { min_comp = 0.0; }
    if (max_comp < min_comp){ max_comp = min_comp; }

    int thresh_byte = (int) lround(ink_thresh * 255.0);
    int nx = bm->nx;
    int ny = bm->ny;
    int nz = bm->nz;

    int       active_rows = 0;
    int       g_min_x     = nx;
    int       g_max_x     = -1;
    long long ink_pixels  = 0;

    for (int y = 0; y < ny; y++) {
        int l_x = -1;
        int r_x = -1;
        const unsigned char *row_ptr = bm->data + ((size_t) y * (size_t) nx * (size_t) nz);

        for (int x = 0; x < nx; x++) {
            int v;
            if (nz == 1) {
                v = (int) row_ptr[x];
            } else {
                const unsigned char *px = row_ptr + ((size_t) x * (size_t) nz);
                v = (int) px[0];
                for (int c = 1; c < nz; c++) {
                    if ((int) px[c] > v) {
                        v = (int) px[c];
                    }
                }
            }
            if (v >= thresh_byte) {
                if (l_x < 0) { l_x = x; }
                r_x = x;
                ink_pixels++;
            }
        }

        if (l_x >= 0 && r_x >= l_x) {
            active_rows++;
            if (l_x < g_min_x) { g_min_x = l_x; }
            if (r_x > g_max_x) { g_max_x = r_x; }
        }
    }

    double expansion    = max_exp;
    double compression  = max_comp;
    double fill_ratio   = 0.0;
    double aspect_ratio = 0.0;

    if (active_rows > 0 && g_max_x >= g_min_x) {
        double ink_span = (double) (g_max_x - g_min_x + 1);
        double box_area = ink_span * (double) active_rows;
        fill_ratio   = (box_area > 0.0) ? ((double) ink_pixels / box_area) : 0.0;
        aspect_ratio = ink_span / (double) active_rows;

        /* Stretch is sensitive to wide aspect + fill; shrink focuses more on fill density */
        double stretch_penalty = (fill_ratio * 0.55 + aspect_ratio * 0.25) * stretch_gain;
        double shrink_penalty  = (fill_ratio * 0.65 + aspect_ratio * 0.10) * shrink_gain;

        expansion   = max_exp  - stretch_penalty;
        compression = max_comp - shrink_penalty;

        if (expansion < min_exp)     { expansion = min_exp; }
        if (expansion > max_exp)     { expansion = max_exp; }
        if (compression < min_comp)  { compression = min_comp; }
        if (compression > max_comp)  { compression = max_comp; }
    }

    lua_createtable(L, 0, 4);
    lua_pushnumber(L, expansion);    lua_setfield(L, -2, "expansion");
    lua_pushnumber(L, compression);  lua_setfield(L, -2, "compression");
    lua_pushnumber(L, fill_ratio);   lua_setfield(L, -2, "fill_ratio");
    lua_pushnumber(L, aspect_ratio); lua_setfield(L, -2, "aspect_ratio");
    return 1;
}

static struct luaL_Reg renderlib_function_list[] = {
    { "glyph",      renderlib_glyph      },
    { "chamfer",    renderlib_chamfer    },
    { "scorepair",  renderlib_scorepair  },
    { "heatmap",    renderlib_heatmap    },
    { "scankern",   renderlib_scankern   },
    { "protrusion", renderlib_protrusion },
    { "expansion",  renderlib_expansion  },
    { NULL,         NULL                 },
};

int luaopen_render(lua_State *L)
{
    lua_newtable(L);
    luaL_setfuncs(L, renderlib_function_list, 0);
    return 1;
}