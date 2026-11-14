/*
    See license.txt in the root of this project.
*/

# ifndef LMT_MATCHER_H
# define LMT_MATCHER_H

/*tex 
    This started out as an experiment in |lmtnodelib| but I will also play with this at the \TEX\ 
    end so now the code moved to the |texscanner| module. 
*/

typedef struct keyword_child {
    union {
        int key;
        struct {
            int lower;
            int upper;
        } case_pair;
    };
    uint16_t  child;        /* 16-bit node indices into matcher->nodes[] */
} keyword_child;

typedef struct keyword_node {
    uint16_t       index;
    uint16_t       nofchildren;
    keyword_child *children;
} keyword_node;

typedef enum keyword_options {
    keyword_exact_option    = 0x01,
    keyword_terminal_option = 0x02,
    keyword_ascii_option    = 0x04,
    keyword_relax_option    = 0x08,
    keyword_group_option    = 0x10,
    keyword_error_option    = 0x20, /* not yet used: show error in scanner */
} keyword_options;

typedef struct keyword_matcher {
    keyword_node *nodes;
    uint16_t      nodes_used;
    uint16_t      nodes_cap;
    int           count;
    unsigned      options; 
    unsigned      group;
} keyword_matcher;

keyword_node *tex_matcher_trie_allocate (keyword_matcher *matcher);
void          tex_matcher_trie_insert   (keyword_matcher *matcher, const char *str, int index);
int           tex_matcher_use           (const keyword_matcher *matcher, int intercept);
void          tex_matcher_free          (keyword_matcher *matcher);
uint16_t      tex_matcher_find_child    (const keyword_matcher *matcher, uint16_t node_idx, int cp);
void          tex_matcher_trace         (keyword_matcher *matcher, int index);

# define use_matcher    1
# define trace_matchers 0

# define default_matcher_options (keyword_ascii_option | keyword_error_option)

typedef enum keyword_matchers { 
    rule_matcher,
    list_matcher,
    split_matcher,
    adjust_matcher,
    specification_matcher,
    discretionary_matcher,
    insert_matcher,
    localboxes_matcher,
    parpasses_matcher,
    balancepasses_matcher,
    balanceshape_matcher,
    linesnapping_matcher,
    mvl_matcher,
    alignment_matcher,
    align_matcher,
    noalign_matcher,
    glyph_matcher,
    leader_matcher,
    accent_matcher,
    showbox_matcher,
    mathatom_matcher,
    mathradical_matcher,
    mathaccent_matcher,
    mathdiscretionary_matcher,
    mathfraction_matcher,
    mathfence_matcher,
    private_idea_matcher,
} keyword_matchers;

# define first_keyword_matcher rule_matcher
# define last_keyword_matcher  private_idea_matcher

typedef struct matcher_item {
    const char **words;
    const char  *name;
    unsigned     first;
    unsigned     last;
    unsigned     options; 
    unsigned     group;
} matcher_item;

extern keyword_matcher lmt_matchers     [last_keyword_matcher+1];
extern matcher_item    lmt_matcher_items[last_keyword_matcher+1];

void tex_initialize_matchers    (void);
void tex_reinitialize_matcher   (int index);
void tex_cleanup_matchers       (void);
void tex_report_matchers        (void);
void tex_check_matchers         (int index);
void tex_ignore_matchers        (int index, int key);

# endif