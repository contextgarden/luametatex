/*
    See license.txt in the root of this project.
*/

# ifndef LMT_STRINGPOOL_H
# define LMT_STRINGPOOL_H

/*tex

    Both \LUA\ and |TEX\ strings can contain |nul| characters, but \CCODE\ strings cannot. The pool
    is implemented differently anyway. The |init_str_ptr| is an offset that indicates how many strings
    are in the format. Does it still make sense to have that distinction? Do we care?

    We store the used bytes (in strings) in the |real| field so that it is carried with the data blob
    (and ends up in statistics).

*/

/*

    There is a bit of overkill in the |lstring| objects and the reason is that byte streams (aka
    char arrays) come in various pointer forms. In order to make clear how we cast we use unions
    and we trust the compiler to deal with all this properly. For that reason we also use inline
    accessors instead of macros; we compile with aggressive optimization and link time.
    optimization.

    We padd so we can either go for size_t or we can use two smaller integers and ponder a future 
    use for the extra field. We can use if for a hash but we only compare when we trace. We could 
    store some properties here like if we have an active character. We could also (abuse) it for 
    some more primitive related properties as these don't change.

*/

typedef uint32_t        lstring_length;
typedef uint32_t        lstring_mode;
typedef unsigned char * lstring_string;

typedef enum lstring_modes {
    lstring_active_mode = 0x01,
} lstring_modes;

typedef struct lstring {
    union {
        lstring_string  str;
        const char     *con;
        char           *chr;
        unsigned char  *uns;
    };
    lstring_length len;
    lstring_mode   mod;
} lstring;

typedef struct string_pool_info {
    lstring       *string_pool;
    memory_data    string_pool_data;
    memory_data    string_body_data;
    strnumber      reserved;
    /*tex only when format is made and loaded */
    int            string_max_length;
    /*tex used for temporary string building: */
    unsigned char *string_temp;
    int            string_temp_allocated;
    int            string_temp_top;
} string_pool_info;

extern string_pool_info lmt_string_pool_state;

# define STRING_EXTRA_AMOUNT 512

/*tex This is the reference of the empty string: */

# define get_nullstr() cs_offset_value

/*tex

    Several of the elementary string operations are performed using macros instead of procedures,
    because many of the operations are done quite frequently and we want to avoid the overhead of
    procedure calls. For example, here is a simple macro that computes the length of a string.

    Keep in mind that we are talking of a |string_pool| table that officially starts with the
    \UNICODE\ characters (as in \TEX\ with \ASCII) but that we use an offset to jump ove that. So the
    real size doesn't include those single character code points.

*/

static inline lstring_string str_getstr    (int a) { return lmt_string_pool_state.string_pool[a - cs_offset_value].str; }
static inline char *         str_getchrstr (int a) { return lmt_string_pool_state.string_pool[a - cs_offset_value].chr; }
static inline const char *   str_getconstr (int a) { return lmt_string_pool_state.string_pool[a - cs_offset_value].con; }
static inline lstring_string str_getactstr (int a) { return lmt_string_pool_state.string_pool[a - cs_offset_value].str + 3; }
static inline lstring_length str_getlen    (int a) { return          lmt_string_pool_state.string_pool[a - cs_offset_value].len; }
static inline int            str_getintlen (int a) { return (int)    lmt_string_pool_state.string_pool[a - cs_offset_value].len; }
static inline size_t         str_getsizlen (int a) { return (size_t) lmt_string_pool_state.string_pool[a - cs_offset_value].len; }
static inline lstring_mode   str_getmod    (int a) { return          lmt_string_pool_state.string_pool[a - cs_offset_value].mod; }

static inline void str_setstr    (int a, lstring_string *str) { lmt_string_pool_state.string_pool[a - cs_offset_value].str = (lstring_string) str; }
static inline void str_setchrstr (int a, char           *str) { lmt_string_pool_state.string_pool[a - cs_offset_value].chr =                  str; }
static inline void str_setunsstr (int a, unsigned char  *str) { lmt_string_pool_state.string_pool[a - cs_offset_value].uns =                  str; }
static inline void str_setconstr (int a, const char     *str) { lmt_string_pool_state.string_pool[a - cs_offset_value].con =                  str; }
static inline void str_setmod    (int a, lstring_mode    mod) { lmt_string_pool_state.string_pool[a - cs_offset_value].mod = (lstring_mode  ) mod; }
static inline void str_setlen    (int a, lstring_length  len) { lmt_string_pool_state.string_pool[a - cs_offset_value].len =                  len; }
static inline void str_setintlen (int a, int             len) { lmt_string_pool_state.string_pool[a - cs_offset_value].len = (lstring_length) len; }
static inline void str_setsizlen (int a, size_t          len) { lmt_string_pool_state.string_pool[a - cs_offset_value].len = (lstring_length) len; }

static inline int str_getindex      (int a) { return a - cs_offset_value; }
static inline int str_getnofstrings (void)  { return lmt_string_pool_state.string_pool_data.ptr - cs_offset_value; }

static inline int tex_single_letter(strnumber s)
{
    const lstring_length len = str_getlen(s);
    if (len == 1) {
        return 1;
    } else {
        /* the compiler will nicely optimize this */
        const unsigned char lead = *(const unsigned char *) str_getstr(s);
        switch (len) {
            case 2: return (lead >= 0xC0 && lead <= 0xDF);
            case 3: return (lead >= 0xE0 && lead <= 0xEF);
            case 4: return (lead >= 0xF0 && lead <= 0xF4);
            default: return 0;
        }
    }
}

static inline int tex_is_active_cs(strnumber s)
{
# if (1)
    return s && str_getmod(s) == lstring_active_mode;
# else
    if (s && str_getlen(s) > 3) {
     // return memcmp(str_getstr(s), ACTIVE_CHAR_NAMESPACE, 3) == 0;
        lstring_string ss = str_getstr(s);
     // if (str_getmod(s) == lstring_active_mode) {
     //     int index = str_getindex(s);
     //     printf("IS ACTIVE: index %i 0x%X\n",index, index);
     // }
        return (ss[0] == active_character_first) && (ss[1] == active_character_second) && (ss[2] == active_character_third);
    } else {
        return 0;
    }
# endif
}

static inline unsigned tex_active_cs_value(strnumber s)
{
    return aux_str2uni(str_getactstr(s));
}

/*tex

    Strings are created by appending character codes to |str_pool|. The |append_char| macro,
    defined here, does not check to see if the value of |pool_ptr| has gotten too high; this test
    is supposed to be made before |append_char| is used. There is also a |flush_char| macro, which
    erases the last character appended.

    To test if there is room to append |l| more characters to |str_pool|, we shall write |str_room
    (l)|, which aborts \TEX\ and gives an apologetic error message if there isn't enough room. The
    length of the current string is called |cur_length|.

*/

/*tex Forget the last character in the pool. */

static inline void  tex_flush_char(void)       { --lmt_string_pool_state.string_temp_top; }
                   
extern strnumber  tex_make_string            (void);
extern strnumber  tex_push_string            (const unsigned char *s, int l);
extern char      *tex_take_string            (int *len);
extern int        tex_str_eq_buf             (strnumber s, int k, lstring_length l);
extern int        tex_str_eq_str             (strnumber s, strnumber t);
extern int        tex_str_eq_cstr            (strnumber s, const char *, size_t);
extern int        tex_get_strings_started    (void);
extern void       tex_reset_cur_string       (void);
/*     strnumber  tex_search_string          (strnumber search); */
/*     int        tex_used_strings           (void); */
extern strnumber  tex_maketexstring          (const char *s);
extern strnumber  tex_maketexlstring         (const char *s, size_t);
extern void       tex_append_char            (unsigned char c);
extern void       tex_append_string          (const unsigned char *s, unsigned l);
extern char      *tex_makecstring            (int s, int *allocated);
extern char      *tex_makeclstring           (int s, size_t *len);
extern void       tex_dump_string_pool       (dumpstream f);
extern void       tex_undump_string_pool     (dumpstream f);
extern void       tex_initialize_string_pool (void);
extern void       tex_initialize_string_mem  (void);
extern void       tex_flush_str              (strnumber s);
extern strnumber  tex_save_cur_string        (void);
extern void       tex_restore_cur_string     (strnumber u);
extern void       tex_compact_string_pool    (void);
/*     void       tex_increment_pool_string  (int n); */
/*     void       tex_decrement_pool_string  (int n); */
                                   
# endif
