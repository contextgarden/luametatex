/*
    See license.txt in the root of this project.
*/

# ifndef LMT_UTILITIES_ARITHMETIC_H
# define LMT_UTILITIES_ARITHMETIC_H

/* The |fabs| macro is used in mp. */

/*tex

There has always be much attention on accuracy in \TEX, especially in the perspective of portability. 
Keep in mind that \TEX\ was written when there was no IEEE floating point defined so all happens in 
16.16, or actually in 14.16 precision. We could actually consider going 16.16 if we use long integers 
in some places but it needs some checking first. We could just accept wrapping around as that already
happens in some places anyway (not all dimension calculation are checked).

In \LUATEX\ and \LUAMETATEX\ we have the \LUA\ engine and that one was exclusively using doubles till 
5.3 when it went for a more hybrid approach. Because we go a lot between \TEX\ and \LUA\ (in \CONTEXT) 
that had some consequences and rounding happens all over the place. It is also for that reason that 
we now use doubles and rounding in some more places in the \TEX\ part: it is more consistent with what 
happens at the \LUA\ end. And, because IEEE is common now, we are (afaiks) portable enough. 

We don't use round but lround as that one rounds away from zero. In a few places we use llround. Also 
in some places we clip to the official maxima but not always. 

*/


/*
# undef abs
# undef fabs

# define abs(x)    ((int)(x) >= 0 ? (int)(x) : (int)-(x))
# define fabs(x)   ((x) >= 0.0 ? (x) : -(x))
*/

# define odd(x)    ((x) & 1)

# define lfloor(x) ( (lua_Integer)(floor((double)(x))) )
# define tfloor(x) ( (size_t)     (floor((double)(x))) )
# define ifloor(x) ( (int)        (floor((double)(x))) )

static inline int fastfloor (double x) { return (int) x <= x ? (int) x : (int) x - 1; }
static inline int fastfloord(double x) { return (int) x <= x ? (int) x : (int) x - 1; }
static inline int fastfloorf(float  x) { return (int) x <= x ? (int) x : (int) x - 1; }
static inline int fastceil  (double x) { return (int) x >= x ? (int) x : (int) x + 1; }

// static inline int fastfloor (float x) { int i = (int) x;  return i - (x < (float) i); }
// static inline int fastfloorf(float x) { int i = (int) x;  return i - (x < (float) i); }

//define lround(x) ( ((double) x >= 0.0) ? (lua_Integer) ((double) x + 0.5) : (lua_Integer) ((double) x - 0.5) )
//define tround(x) ( ((double) x >= 0.0) ? (size_t)      ((double) x + 0.5) : (size_t)      ((double) x - 0.5) )
//define iround(x) ( ((double) x >= 0.0) ? (int)         ((double) x + 0.5) : (int)         ((double) x - 0.5) )
//define sround(x) ( ((double) x >= 0.0) ? (int)         ((double) x + 0.5) : (int)         ((double) x - 0.5) )

//define lround(x) ( ((double) x >= 0.0) ? (lua_Integer) ((double) x + 0.5) : (lua_Integer) ((double) x - 0.5) )
//define tround(x) ( ((double) x >= 0.0) ? (size_t)      ((double) x + 0.5) : (size_t)      ((double) x - 0.5) )
//define iround(x) ( (int) lround((double) x) )

//define zround(r) ((r>2147483647.0) ? 2147483647 : ((r<-2147483647.0) ? -2147483647 : ((r >= 0.0) ? (int)(r + 0.5) : ((int)(r-0.5)))))
//define zround(r) ((r>2147483647.0) ? 2147483647 : ((r<-2147483647.0) ? -2147483647 : (int) lround(r)))

//define scaledround(x)  ((scaled) lround((double) (x)))
# define longlonground   llround
# define clippedround(r) ((r>2147483647.0) ? 2147483647 : ((r<-2147483647.0) ? -2147483647 : (int) lround(r)))
# define glueround(x)    clippedround((double) (x))

static inline scaled scaledround(double x) { return (scaled) (x >= 0.0 ? (x + 0.5) : (x - 0.5)); }

# if lmt_float_math

    /* nothing */

# else

    /* Helper for 1,000-based scaling: (v * scale1 * scale2) / 1e3 */

    static inline scaled tex_aux_scale_1000(scaled v, long long scale_par)
    {
        if (! v) return 0;
        long long s = scale_par ? scale_par : 1000LL;
        if (s == 1000LL) return v;
        long long prod = s * (long long) v;
        return (scaled) ((prod >= 0) ? (prod + 500LL) / 1000LL : (prod - 500LL) / 1000LL);
    }

    /* Helper for 1,000,000,000-based scaling: (v * scale1 * scale2) / 1e9 */

    static inline scaled tex_aux_scale_1e6(scaled value, long long s1, long long s2)
    {
        if (! value) {
            return 0;
        }
        // Default 0 to 1000 (100% scale)
        s1 = s1 ? s1 : 1000;
        s2 = s2 ? s2 : 1000;
        // Fast-path: no scaling needed
        if (s1 == 1000 && s2 == 1000) {
            return value;
        }
        long long prod = (long long) value * s1 * s2;
        // Symmetric rounding away from zero
        return (scaled) ((prod >= 0 ? prod + 500000LL : prod - 500000LL) / 1000000LL);
    }

    /* Helper for 1,000,000-based scaling: (v * scale1 * scale2) / 1e6 */

    static inline scaled tex_aux_scale_1e9(scaled v, long long s1, long long s2)
    {
        if (! v || ! s2) return 0;
        long long gs = s1 ? s1 : 1000LL;
    # if defined(__SIZEOF_INT128__)
        __int128 num = (__int128) gs * s2 * v;
        return (scaled) ((num >= 0) ? (num + 500000000LL) / 1000000000LL
                                    : (num - 500000000LL) / 1000000000LL);
    # else
        long long factor = (gs * s2 + 500LL) / 1000LL; /* Combine scales to 1e6 */
        long long prod = factor * (long long) v;
        return (scaled) ((prod >= 0) ? (prod + 500000LL) / 1000000LL
                                     : (prod - 500000LL) / 1000000LL);
    # endif
    }

# endif

# endif
