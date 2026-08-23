/*
    See license.txt in the root of this project.
*/

# include "auxbytemaps.h"
# include "auxmemory.h"
# include <math.h>
# include <stdio.h>
# include <stdint.h>

/* 
    todo: set nz to zero when no data so we have a test less 
    todo: round keep in integer domain (see end of decodelib)
*/

const double INV_255 = 1.0 / 255.0;

static inline unsigned char max_of_three(unsigned char a, unsigned char b, unsigned char c)
{
    if (a > b && a > c) {
        return a;
    } else if (a > c) { /* we know that a <= b */
        return b;
    } else if (b > c) {
        return b;
    } else {
        return c;
    }
}

static inline unsigned char min_of_three(unsigned char a, unsigned char b, unsigned char c)
{
    if (a < b && a < c) {
        return a;
    } else if (a < c) { /* we know that a >= b */
        return b;
    } else if (b < c) {
        return b;
    } else {
        return c;
    }
}

// integer approximation (299*256/1000 ≈ 77, 587*256/1000 ≈ 150, 114*256/1000 ≈ 29)

//define rgb_to_gray(r,g,b) ((int) lround(0.299 * r + 0.587 * g + 0.114 * b))
# define rgb_to_gray(r,g,b) ((77 * r + 150 * g + 29 * b + 128) >> 8)

int bytemap_reset(bytemap_data *bytemap, size_t *count)
{
    int done = 0;
    if (bytemap) {
        if (bytemap->data) { 
            lmt_memory_free(bytemap->data);
            if (count) {
                *count -= bytemap->nx * bytemap->ny * bytemap->nz;
            }
            done = 1;
        }
        *bytemap = (bytemap_data) {
            .data    = NULL,
            .nx      = 0,
            .ny      = 0,
            .nz      = 0,
            .ox      = 0,
            .oy      = 0,
            .options = 0,
            .model   = bytemap_gray,
        };
    }
    return done; 
}

void bytemap_reduce(bytemap_data *bytemap, int method, size_t *count)
{
    if (bytemap && bytemap->data) {
        switch (bytemap->nz) {
            case 1:
                break;
            case 3:
                {
                    int nx      = bytemap->nx;
                    int ny      = bytemap->ny;
                    int ox      = bytemap->ox;
                    int oy      = bytemap->oy;
                    int options = bytemap->options;
                    unsigned char *color = bytemap->data;
                    unsigned char *gray = lmt_memory_malloc(nx*ny);
                    unsigned c = 0;
                    int nxny = nx * ny;
                    switch (method) {
                        case bytemap_reduction_average:
                            for (int g = 0; g < nxny; g++) {
                                int s = lround( (double) (
                                      (unsigned char) color[c]
                                    + (unsigned char) color[c+1]
                                    + (unsigned char) color[c+2]
                                ) / 3.0);
                                c += 3;
                                gray[g] = s > 255 ? 255 : (unsigned char) s;
                            }
                            break;
                        case bytemap_reduction_minmax:
                            for (int g = 0; g < nxny; g++) {
                                int s = lround( (double) (
                                      max_of_three(color[c], color[c+1], color[c+2])
                                    + min_of_three(color[c], color[c+1], color[c+2])
                                ) / 2.0);
                                c += 3;
                                gray[g] = s > 255 ? 255 : (unsigned char) s;
                            }
                            break;
                     // case bytemap_reduction_weighted:
                     //     /* fall through */
                        default:
                            for (int g = 0; g < nxny; g++) {
                                int s = rgb_to_gray(
                                    (unsigned char) color[c],
                                    (unsigned char) color[c+1],
                                    (unsigned char) color[c+2]
                                );
                                c += 3;
                                gray[g] = s > 255 ? 255 : (unsigned char) s;
                            }
                            break;
                    }
                    if (count) {
                        *count -= nxny * 2;
                    }
                    lmt_memory_free(color);
                    *bytemap = (bytemap_data) {
                        .data    = gray,
                        .nx      = nx,
                        .ny      = ny,
                        .nz      = 1,
                        .ox      = ox,
                        .oy      = oy,
                        .options = options,
                        .model   = bytemap_gray,
                    };
                }
                break;
        }
    }
}

/* Clip a half-open rectangle to the allocated bytemap. */

static int bytemap_aux_clip_rectangle(const bytemap_data *bytemap, int *x, int *y, int *dx, int *dy)
{
    if (!bytemap || !bytemap->data || !x || !y || !dx || !dy || *dx <= 0 || *dy <= 0) {
        return 0;
    }
    int64_t x0 = *x;
    int64_t y0 = *y;
    int64_t x1 = x0 + (int64_t) *dx;
    int64_t y1 = y0 + (int64_t) *dy;
    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }
    if (x1 > bytemap->nx) { x1 = bytemap->nx; }
    if (y1 > bytemap->ny) { y1 = bytemap->ny; }
    if (x0 < x1 && y0 < y1) {
        *x  = (int) x0;
        *y  = (int) y0;
        *dx = (int) (x1 - x0);
        *dy = (int) (y1 - y0);
        return 1;
    }
    return 0;
}

void bytemap_slice_gray(bytemap_data *bytemap, int x, int y, int dx, int dy, int s)
{
    if (bytemap_aux_clip_rectangle(bytemap, &x, &y, &dx, &dy)) {
        switch (bytemap->nz) {
            case 1:
                {
                    unsigned char *p = bytemap->data;
                    int w = bytemap->nx;
                    int o = x;
                    o += bm_current_y(bytemap->ny,y) * w;
                    memset(p + o, valid_byte(s), dx);
                    for (int i = bm_first_y(bytemap->ny,y,dy); i <= bm_last_y(bytemap->ny,y,dy); i++) {
                        memcpy(p + x + i * w, p + o, dx);
                    }
                }
                break;
            case 3:
                bytemap_slice_rgb(bytemap, x, y, dx, dy, s, s, s);
                break;
        }

    }
}

void bytemap_slice_rgb(bytemap_data *bytemap, int x, int y, int dx, int dy, int r, int g, int b)
{
    if (bytemap_aux_clip_rectangle(bytemap, &x, &y, &dx, &dy)) {
        switch (bytemap->nz) {
            case 1:
                bytemap_slice_gray(bytemap, x, y, dx, dy, rgb_to_gray(r,g,b));
                break;
            case 3:
                {
                    unsigned char *p = bytemap->data;
                    int w = 3 * bytemap->nx;
                    int o = 3 * x;
                    o += bm_current_y(bytemap->ny,y) * w;
                    bytemap->data[o+0] = valid_byte(r);
                    bytemap->data[o+1] = valid_byte(g);
                    bytemap->data[o+2] = valid_byte(b);
                    for (int i = 1; i < dx; i++) {
                        memcpy(p + o + i * 3, p + o, 3);
                    }
                    for (int i = bm_first_y(bytemap->ny,y,dy); i <= bm_last_y(bytemap->ny,y,dy); i++) {
                        memcpy(p + 3 * x + i * w, p + o, 3 * dx);
                    }
                }
                break;
        }
    }
}

void bytemap_slice_range(bytemap_data *bytemap, int x, int y, int dx, int dy, int min, int max)
{
    if (bytemap_aux_clip_rectangle(bytemap, &x, &y, &dx, &dy)) {
        switch (bytemap->nz) {
            case 1:
            case 3:
                {
                    int w = bytemap->nx * bytemap->nz;
                    double p = min; 
                    double m = ((double) max - (double) min) * INV_255;
                    int xend = x + dx;
                    int yend = y + dy;
                    for (int j = y; j < yend; j++) {
                        int o = bm_current_y(bytemap->ny,j) * w + x * bytemap->nz;
                        for (int i = x; i < xend; i++) {
                            for (int z = 0; z < bytemap->nz; z++) {
                                int b = lround((double) bytemap->data[o] * m + p);
                                bytemap->data[o++] = b > max ? max : b < min ? min : b;
                            }
                        }
                    }
                }
                break;
        }
    }
}

static int bytemap_aux_bounds(bytemap_data *bytemap, int value, int *lx, int *ly, int *rx, int *ry, int compensate)
{
    unsigned char *d = bytemap->data;
    int nx = bytemap->nx;
    int ny = bytemap->ny;
    int nz = bytemap->nz;
    int ok = 0;
    /* bounds */
    int llx = nx - 1;
    int lly = ny - 1;
    int urx = 0;
    int ury = 0;
    switch (nz) {
        case 1:
            for (int y = 0; y < ny; y++) {
                for (int x = 0; x < nx; x++) {
                    /* here posit */
                    if (*d != value) {
                        if (y < lly) { lly = y; }
                        if (y > ury) { ury = y; }
                        if (x < llx) { llx = x; }
                        if (x > urx) { urx = x; }
                    }
                    d = d + 1;
                }
                if (llx == 0 && urx == nx - 1 && lly == 0 && ury == ny - 1) {
                    goto DONE;
                }
            }
            break;
        case 3:
            for (int y = 0; y < ny; y++) {
                for (int x = 0; x < nx; x++) {
                    /* here posit */
                    if (*d != value || *(d+1) != value || *(d+2) != value) {
                        if (y < lly) { lly = y; }
                        if (y > ury) { ury = y; }
                        if (x < llx) { llx = x; }
                        if (x > urx) { urx = x; }
                    }
                    d = d + 3;
                }
                if (llx == 0 && urx == nx - 1 && lly == 0 && ury == ny - 1) {
                    goto DONE;
                }
            }
            break;
    }
    DONE:
    if (urx < llx || ury < lly) {
        *lx = 0;
        *ly = 0;
        *rx = nx - 1;
        *ry = ny - 1;
    } else {
        *lx = llx;
        *ly = lly;
        *rx = urx;
        *ry = ury;
    }
    ok = *lx > 0 || *ly > 0 || *rx < nx - 1 || *ry < ny - 1;
    if (compensate) {
        *ly  = bm_current_y(ny,*ly);
        *ry  = bm_current_y(ny,*ry);
    }
    return ok;
}

int bytemap_bounds(bytemap_data *bytemap, int value, int *llx, int *lly, int *urx, int *ury, int compensate)
{
    if (bytemap) {
        *llx = bytemap->nx - 1;
        *lly = bytemap->ny - 1;
        *urx = 0;
        *ury = 0;
        return bytemap_aux_bounds(bytemap, value, llx, lly, urx, ury, compensate);
    } else { 
        return 0;
    }
}

void bytemap_clip(bytemap_data *bytemap, int value, size_t *count)
{
    if (bytemap && bytemap->data) {
        int llx = 0;
        int lly = 0;
        int urx = bytemap->nx;
        int ury = bytemap->ny;
        if (bytemap_aux_bounds(bytemap, value, &llx, &lly, &urx, &ury, 0)) {
            int oldnx = bytemap->nx;
            int oldny = bytemap->ny;
            int oldnz = bytemap->nz;
            int newnx = urx - llx + 1;
            int newny = ury - lly + 1;
            size_t oldsize = oldnx * oldny * oldnz;
            size_t newsize = newnx * newny * oldnz;
            if (newsize > 0 && oldsize != newsize) {
                unsigned char *p = bytemap->data + lly * oldnx * oldnz + llx * oldnz;
                unsigned char *c = lmt_memory_malloc(newsize);
                unsigned char *d = c;
                for (int y=1; y <= newny; y++) {
                    memcpy(c, p, newnx * oldnz);
                    c = c + newnx * oldnz;
                    p = p + oldnx * oldnz;
                }
                lmt_memory_free(bytemap->data);
                if (count) { 
                    /* todo : *count */
                    *count -= oldsize;
                    *count += newsize;
                }
                bytemap->data = d;
                bytemap->ox   = 0;
                bytemap->oy   = 0;
                bytemap->nx   = newnx;
                bytemap->ny   = newny;
            } else { 
                /* todo: warning */
            }
        }
    }
}

void bytemap_wipe(bytemap_data *bytemap)
{
    if (bytemap) { 
        *bytemap = (bytemap_data) {
            .data    = NULL,
            .nx      = 0,
            .ny      = 0,
            .nz      = 0,
            .ox      = 0,
            .oy      = 0,
            .options = 0,
            .model   = bytemap_gray,
        };
    }
}

void bytemap_allocate(bytemap_data *bytemap, int nx, int ny, int nz, size_t *count)
{
    if (bytemap) {
        int size = nx * ny * nz;
        *bytemap = (bytemap_data) {
            .data    = lmt_memory_calloc(1, size),
            .nx      = nx,
            .ny      = ny,
            .nz      = nz,
            .ox      = 0,
            .oy      = 0,
            .options = 0,
            .model   = nz == 3 ? bytemap_rgb : bytemap_gray,
        };
        if (count) { 
            *count += size;
        }
    }
}

void bytemap_copy(bytemap_data *source, bytemap_data *target, size_t *count)
{
    if (source && target && source != target && source->data && source->data != target->data) {
        size_t size = (size_t) source->nx * source->ny * source->nz;
        unsigned char *data = lmt_memory_malloc(size);
        if (data) {
            size_t oldsize = target->data ? (size_t) target->nx * target->ny * target->nz : 0;
            memcpy(data, source->data, size);
            if (target->data) {
                lmt_memory_free(target->data);
            }
            *target = (bytemap_data) {
                .data    = data,
                .nx      = source->nx,
                .ny      = source->ny,
                .nz      = source->nz,
                .ox      = source->ox,
                .oy      = source->oy,
                .options = source->options,
                .model   = source->model,
            };
            if (count) {
                *count -= oldsize;
                *count += size;
            }
        }
    }
}

void bytemap_fill_gray(bytemap_data *bytemap, int s)
{
    if (bytemap && bytemap->data) {
        memset(bytemap->data, valid_byte(s), bytemap->nx * bytemap->ny * bytemap->nz);
    }
}

void bytemap_fill_rgb(bytemap_data *bytemap, int r, int g, int b)
{
    if (bytemap && bytemap->data) {
        switch (bytemap->nz) {
            case 1:
                break;
            case 3:
                bytemap->data[0] = valid_byte(r);
                bytemap->data[1] = valid_byte(g);
                bytemap->data[2] = valid_byte(b);
                for (int n = 3; n < bytemap->nx * bytemap->ny * bytemap->nz; n += 3) {
                    memcpy(&(bytemap->data[n]), bytemap->data, 3);
                }
                break;
        }
    }
}

/*tex We assume that bytemap has a value and we hope for inlining. */

# define gray_min(a,b) if (b < a) { a = b; }
# define gray_add(a,b) a = valid_byte(a+b);

void bytemap_set_gray(bytemap_data *bytemap, int x, int y, int s)
{
    if (bytemap && bytemap->data && x >= 0 && y >= 0 && x < bytemap->nx && y < bytemap->ny) {
        switch (bytemap->nz) {
            case 1:
                bytemap->data[bm_current_y(bytemap->ny,y) * bytemap->nx + x] = valid_byte(s);
                break;
            case 3:
                memset(bytemap->data + (bm_current_y(bytemap->ny,y) * bytemap->nx + x) * 3, valid_byte(s), 3);
                break;
        }
    }
}

void bytemap_set_gray_min(bytemap_data *bytemap, int x, int y, int s1, int s2, int s3)
{
    if (bytemap && bytemap->data && x >= 0 && y >= 0 && x < bytemap->nx && y < bytemap->ny && bytemap->nz == 1) {
        int xm = x - 1;
        int xp = x + 1;
        int ym = y - 1;
        int yp = y + 1;
        unsigned char v1 = valid_byte(s1);
        unsigned char v2 = valid_byte(s2);
        unsigned char v3 = valid_byte(s3);
        bytemap->data[bm_current_y(bytemap->ny,y) * bytemap->nx + x] = v1;
        if (xm >= 0) {
            if (ym >= 0) {
                gray_min(bytemap->data[bm_current_y(bytemap->ny,ym) * bytemap->nx + xm],v3)
            }
            if (yp < bytemap->ny) {
                gray_min(bytemap->data[bm_current_y(bytemap->ny,yp) * bytemap->nx + xm],v3)
            }
            gray_min(bytemap->data[bm_current_y(bytemap->ny,y) * bytemap->nx + xm],v2)
        }
        if (xp < bytemap->nx) {
            if (ym >= 0) {
                gray_min(bytemap->data[bm_current_y(bytemap->ny,ym) * bytemap->nx + xp],v3)
            }
            if (yp < bytemap->ny) {
                gray_min(bytemap->data[bm_current_y(bytemap->ny,yp) * bytemap->nx + xp],v3)
            }
            gray_min(bytemap->data[bm_current_y(bytemap->ny,y) * bytemap->nx + xp],v2)
        }
        if (ym >= 0) {
            gray_min(bytemap->data[bm_current_y(bytemap->ny,ym) * bytemap->nx + x],v2)
        }
        if (yp < bytemap->ny) {
            gray_min(bytemap->data[bm_current_y(bytemap->ny,yp) * bytemap->nx + x],v2)
        }
    }
}

void bytemap_set_gray_add(bytemap_data *bytemap, int x, int y, int s1, int s2, int s3)
{
    if (bytemap && bytemap->data && x >= 0 && y >= 0 && x < bytemap->nx && y < bytemap->ny && bytemap->nz == 1) {
        int xm = x - 1;
        int xp = x + 1;
        int ym = y - 1;
        int yp = y + 1;
        unsigned char v1 = valid_byte(s1);
        unsigned char v2 = valid_byte(s2);
        unsigned char v3 = valid_byte(s3);
        bytemap->data[bm_current_y(bytemap->ny,y) * bytemap->nx + x] = v1;
        if (xm >= 0) {
            if (ym >= 0) {
                gray_add(bytemap->data[bm_current_y(bytemap->ny,ym) * bytemap->nx + xm],v3)
            }
            if (yp < bytemap->ny) {
                gray_add(bytemap->data[bm_current_y(bytemap->ny,yp) * bytemap->nx + xm],v3)
            }
            gray_add(bytemap->data[bm_current_y(bytemap->ny,y) * bytemap->nx + xm],v2)
        }
        if (xp < bytemap->nx) {
            if (ym >= 0) {
                gray_add(bytemap->data[bm_current_y(bytemap->ny,ym) * bytemap->nx + xp],v3)
            }
            if (yp < bytemap->ny) {
                gray_add(bytemap->data[bm_current_y(bytemap->ny,yp) * bytemap->nx + xp],v3)
            }
            gray_add(bytemap->data[bm_current_y(bytemap->ny,y) * bytemap->nx + xp],v2)
        }
        if (ym >= 0) {
            gray_add(bytemap->data[bm_current_y(bytemap->ny,ym) * bytemap->nx + x],v2)
        }
        if (yp < bytemap->ny) {
            gray_add(bytemap->data[bm_current_y(bytemap->ny,yp) * bytemap->nx + x],v2)
        }
    }
}

void bytemap_set_rgb(bytemap_data *bytemap, int x, int y, int r, int g, int b)
{
    if (bytemap && bytemap->data && x >= 0 && y >= 0 && x < bytemap->nx && y < bytemap->ny) {
        switch (bytemap->nz) {
            case 1:
                bytemap->data[bm_current_y(bytemap->ny,y) * bytemap->nx + x] = valid_byte(rgb_to_gray(r,g,b));
                break;
            case 3:
                {
                    int offset = (bm_current_y(bytemap->ny,y) * bytemap->nx + x) * 3;
                    bytemap->data[offset+0] = valid_byte(r);
                    bytemap->data[offset+1] = valid_byte(g);
                    bytemap->data[offset+2] = valid_byte(b);
                }
                break;
        }
    }
}

int bytemap_has_byte_gray(bytemap_data *bytemap, int s)
{
    if (bytemap && bytemap->data) {
        switch (bytemap->nz) {
            case 1:
                for (int i = 0; i < bytemap->nx * bytemap->ny; i++) {
                    if (bytemap->data[i] == (unsigned char) s) {
                        return 1;
                    }
                }
                return 0;
            case 3:
                return bytemap_has_byte_rgb(bytemap, s, s, s);
        }
    }
    return 0;
}

int bytemap_has_byte_range(bytemap_data *bytemap, int min, int max)
{
    if (bytemap && bytemap->data) { 
        switch (bytemap->nz) {
            case 1:
                for (int i = 0; i < bytemap->nx * bytemap->ny; i++) {
                    if  (bytemap->data[i] >= (unsigned char) min && bytemap->data[i] <= (unsigned char) max) {
                        return 1;
                    }
                }
                return 0;
            case 3:
                return 0;
        }
    }
    return 0;
}

int bytemap_has_byte_rgb(bytemap_data *bytemap, int r, int g, int b)
{
    if (bytemap && bytemap->data) { 
        switch (bytemap->nz) {
            case 1:
                return bytemap_has_byte_gray(bytemap, rgb_to_gray(r,g,b));
            case 3:
                /* todo: fast search in mem range */
                for (int i = 0; i < bytemap->nx * bytemap->ny * bytemap->nz; i += 3) {
                    if (bytemap->data[i+0] == (unsigned char) r &&
                        bytemap->data[i+1] == (unsigned char) g &&
                        bytemap->data[i+2] == (unsigned char) b
                    ) {
                        return 1;
                    }
                }
                return 0;
        }
    }
    return 0;
}

int bytemap_get_byte(bytemap_data *bytemap, int x, int y, int z)
{
    if (bytemap && bytemap->data) {
        int nx = bytemap->nx;
        int ny = bytemap->ny;
        if (x >= 0 && y >= 0 && x < nx && y < ny) {
            int nz = bytemap->nz;
            switch (nz) {
                case 1:
                    return bytemap->data[bm_current_y(ny,y) * nx + x];
                case 3:
                    {
                        int p = bm_current_y(ny,y) * nx * 3 + x * 3;
                        if (z >= 0 && z < 3) {
                            return bytemap->data[p+z];
                        } else {
                            return rgb_to_gray(
                                bytemap->data[p],
                                bytemap->data[p+1],
                                bytemap->data[p+2]
                            );
                        }
                    }
            }
        }
    }
    return 0;
}

void bytemap_get_bytes(bytemap_data *bytemap, int x, int y, unsigned char *b1, unsigned char *b2, unsigned char *b3)
{
    if (bytemap && bytemap->data) {
        int nx = bytemap->nx;
        int ny = bytemap->ny;
        if (x >= 0 && y >= 0 && x < nx && y < ny) {
            int nz = bytemap->nz;
            switch (nz) {
                case 1:
                    {
                        *b1 = bytemap->data[bm_current_y(ny,y) * nx + x];
                        *b2 = '\0';
                        *b3 = '\0';
                        return;
                    }
                case 3:
                    {
                        int p = bm_current_y(ny,y) * nx * 3 + x * 3;
                        *b1 = bytemap->data[p++];
                        *b2 = bytemap->data[p++];
                        *b3 = bytemap->data[p];
                        return;
                    }
            }
        }
    }
    *b1 = '\0';
    *b2 = '\0';
    *b3 = '\0';
}

double bytemap_get_luminance(bytemap_data *bytemap, int x, int y)
{
    if (bytemap && bytemap->data) {
        int nx = bytemap->nx;
        int ny = bytemap->ny;
        if (x >= 0 && y >= 0 && x < nx && y < ny) {
            int nz = bytemap->nz;
            switch (nz) {
                case 1:
                    {
                        unsigned char s = bytemap->data[bm_current_y(ny,y) * nx + x];
                        return (double) s * INV_255;
                    }
                case 3:
                    {
                        int p = bm_current_y(ny,y) * nx * 3 + x * 3;
                        unsigned char r = bytemap->data[p++];
                        unsigned char g = bytemap->data[p++];
                        unsigned char b = bytemap->data[p];
                        return (0.2126 * r + 0.7152 * g + 0.0722 * b) * INV_255;
                    }
            }
        }
    }
    return 0;
}


char *bytemap_get_value(bytemap_data *bytemap, int *nx, int *ny, int *nz) /* todo */
{
    if (bytemap && bytemap->data) {
        *nx = bytemap->nx;
        *ny = bytemap->ny;
        *nz = bytemap->nz;
        if (*nx > 0 && *ny > 0) {
            size_t length = (size_t) ((*nx) * (*ny) * (*nz));
            char *result = lmt_memory_malloc(length);
            memcpy(result, bytemap->data, length);
            return result;
        }
    }
    *nx = 0;
    *ny = 0;
    *nz = 0;
    return NULL;
}

void bytemap_downsample(bytemap_data *source, bytemap_data *target, int r)
{
    /* 
        Todo: when source and target are the same, we have to use a temporary bytemap. 
    */
    if (source && target && source != target && source->data != target->data && source->data) {
        int nx      = source->nx;
        int ny      = source->ny; 
        int nz      = source->nz;
        int ox      = source->ox;
        int oy      = source->oy;
        int options = source->options;
        int model   = source->model;
        if (r < 2) {
            r = 2;
        }
        if (r <= nx && r <= ny) {
            int dy = nx * nz; 
            int mx = nx / r;
            int my = ny / r;
            nx = mx * r;
            ny = my * r;
            unsigned char *q = lmt_memory_malloc(mx * my * nz);
            if (q) {
                int rr = r * r;
                if (target->data) {
                    lmt_memory_free(target->data);
                }
                *target = (bytemap_data) {
                    .data    = q,
                    .nx      = mx,
                    .ny      = my,
                    .nz      = nz,
                    .ox      = ox,
                    .oy      = oy,
                    .options = options,
                    .model   = model,
                };
                switch (nz) {
                    case 1:
                        for (int y = 0; y < ny; y += r) {
                            for (int x = 0; x < nx; x += r) {
                                int s = 0;
                                for (int j = y; j < y + r; j++) {
                                    unsigned char *p = &(source->data[j*dy+x]);
                                    for (int i = 0; i < r; i++) {
                                        s += (unsigned char) *(p++);
                                    }
                                }
                              *(q++) = (unsigned char) (s / rr);
                            }
                        }
                        break;
                    case 3:
                        for (int y = 0; y < ny; y += r) {
                            for (int x = 0; x < nx; x += r) {
                                int rc = 0;
                                int gc = 0;
                                int bc = 0;
                                int dx = x * nz;
                                for (int j = y; j < y + r; j++) {
                                    unsigned char *p = &(source->data[j*dy+dx]);
                                    for (int i = 0; i < r; i++) {
                                        rc += (unsigned char) *(p++);
                                        gc += (unsigned char) *(p++);
                                        bc += (unsigned char) *(p++);
                                    }
                                }
                                *(q++) = (unsigned char) (rc / rr);
                                *(q++) = (unsigned char) (gc / rr);
                                *(q++) = (unsigned char) (bc / rr);
                            }
                        }
                        break;
                }
            }
        }
    }
}

void bytemap_downgrade(bytemap_data *source, bytemap_data *target, int r)
{
    /* 
        Todo: when source and target are the same, we have to use a temporary bytemap. 
    */
    if (source && target && source != target && source->data != target->data && source->data) {
        int nx      = source->nx;
        int ny      = source->ny; 
        int nz      = source->nz;
        int ox      = source->ox;
        int oy      = source->oy;
        int options = source->options;
        int model   = source->model;
        int size = nx * ny * nz;
        unsigned char *q = lmt_memory_malloc(size);
        if (q) {
            unsigned char *p = source->data;
            if (target->data) {
                lmt_memory_free(target->data);
            }
            *target = (bytemap_data) {
                .data    = q,
                .nx      = nx,
                .ny      = ny,
                .nz      = nz,
                .ox      = ox,
                .oy      = oy,
                .options = options,
                .model   = model,
            };
            /* todo: fast path for 2 and 4 */
            if (r > 255) {
                r = 255;
            } else if (r < 1) {
                r = 1; 
            }
            for (int i = 0; i < size; i++) {
                int l = r * lround(((double) ((unsigned char) p[i]))/r);
                q[i] = l > 0xFF ? 0xFF : (unsigned char) l;
            }
        }
    }
}

void bytemap_filter(bytemap_data *source, bytemap_data *target, int wx, int wy, double *map)
{
    if (source && target && source != target && source->data != target->data && source->data && map) {
        int nx = source->nx;
        int ny = source->ny;
        int nz = source->nz;
        if (nx == target->nx && ny == target->ny && nz == target->nz) {
            if (wx > 2 && wy > 2 && (wx % 2) && (wy % 2)) {
                int fx = - (wx / 2);
                int lx =   (wx / 2);
                int fy = - (wy / 2);
                int ly =   (wy / 2);
                switch (nz) {
                    case 1:
                        {
                            int t = 0;
                            for (int y = 0; y < ny; y++) {
                                for (int x = 0; x < nx; x++) {
                                    double s = 0;
                                    int    n = 0;
                                    for (int yy = fy; yy <= ly; yy++) {
                                        int by = y + yy;
                                        if (by >= 0 && by < ny) {
                                            int sy = by * nx;
                                            for (int xx = fx; xx <= lx; xx++) {
                                                int bx = x + xx;
                                                if (bx >= 0 && bx < nx) {
                                                    s += map[n] * (unsigned char) source->data[sy + bx];
                                                }
                                                n++;
                                            }
                                        } else {
                                            n += wx;
                                        }
                                    }
                                    target->data[t++] = valid_byte(lround(s));
                                }
                            }
                            break;
                        }
                    case 3:
                        {
                            int t = 0;
                            for (int y = 0; y < ny; y++) {
                                for (int x = 0; x < nx; x++) {
                                    double r = 0;
                                    double g = 0;
                                    double b = 0;
                                    int    n = 0;
                                    for (int yy = fy; yy <= ly; yy++) {
                                        int by = y + yy;
                                        if (by >= 0 && by < ny) {
                                            int sy = by * nx * 3;
                                            for (int xx = fx; xx <= lx; xx++) {
                                                int bx = x + xx;
                                                if (bx >= 0 && bx < nx) {
                                                    int sx = sy + bx * 3;
                                                    r += map[n] * (unsigned char) source->data[sx++];
                                                    g += map[n] * (unsigned char) source->data[sx++];
                                                    b += map[n] * (unsigned char) source->data[sx  ];
                                                }
                                                n++;
                                            }
                                        } else {
                                            n += wx;
                                        }
                                    }
                                    target->data[t++] = valid_byte(lround(r));
                                    target->data[t++] = valid_byte(lround(g));
                                    target->data[t++] = valid_byte(lround(b));
                                }
                            }
                            break;
                        }
                }
            }
        }
    }
}

void bytemap_overlay(bytemap_data *source, bytemap_data *target, int sx, int sy, int tx, int ty, int nx, int ny)
{
    if (source && target && source->data && target->data && source->nz == target->nz) {
        if (sx < 0) { sx = 0; } else if (sx >= source->nx) { sx = source->nx - 1; }
        if (sy < 0) { sy = 0; } else if (sy >= source->ny) { sy = source->ny - 1; }
        if (tx < 0) { tx = 0; } else if (tx >= target->nx) { tx = target->nx - 1; }
        if (ty < 0) { ty = 0; } else if (ty >= target->ny) { ty = target->ny - 1; }
        if (sx + nx > source->nx) { nx = source->nx - sx; }
        if (sy + ny > source->ny) { ny = source->ny - sy; }
        if (tx + nx > target->nx) { nx = target->nx - tx; }
        if (ty + ny > target->ny) { ny = target->ny - ty; }
        for (int i = 1; i <= ny; i++) {
            int s = bm_current_y(source->ny,sy++) * source->nx * source->nz;
            int t = bm_current_y(target->ny,ty++) * target->nx * target->nz;
            memcpy(&(target->data[t]), &(source->data[s]), nx * source->nz);
        }
    }
}
