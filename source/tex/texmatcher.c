/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex

    After playing with a few alternatives in \LUA\ I wondered about delegating some work the
    \CCODE\ end because we need to stop at a partial non-match (in order to be \TEX\
    compatible). The initial idea was passing something |foobar,bar,foo| and getting an index
    back. A bit if chatting came up with a different lookup approach and then I decided that
    a userdata approach might be better so I gave it a try.
    
    Because we scan token by token we need a tree to store the characters, which, contrary to 
    the regular keyword scanner (1), can be \UTF, so we can use numbers. Given that there are 
    several ways to implement such a tree I just asked Gemini for a reasonable approach also 
    given memory usage so it came up with one (actually a few to choise from) that we stepwise 
    adapted to our needs (like a sprted insert to make the longest match working). 
    
    The rest of the code is more or less like the normal keyword scanner but where that one 
    tests against a string and quits when its end is reached here we need to go for the 
    longest match and also explicitly quit when we're an an end point. In a |if ... orelse 
    ... orelse ... orelse ... else ... end| we need to make sure that the longest match 
    comes before a short, but here we need to deal with that immediately. It helps a bit if 
    one knows how \TEX\ scans especially with look-ahead expansion (for instance want a value 
    to be an expandable control sequence (constant or variable in \CONTEXT\ speak) which then 
    should also work when we ignore spaces. Of course there is a bit of optimization. 

    (1) Actually when we use the case sentitive scanner \UTF\ works fine because then there 
    is no \ASCII\ casing going on, as in regular \TEX\ interface keywords. 

    After some further experiments, for instance with a pool I settled for a variant where we 
    use an index in a node array and from that it was natural to index by a 16 bit number and 
    not by pointer. This saves memory and gives a better cache performance. We could gain more 
    by assumigg string with less characters than 256 and not that many brached but we gain very
    little so we stick to the version we have now. The indexed approach that we introduced at 
    the \LUA\ end gives nicer switches at the \TEX\ end too.
    
    With |width height depth| we need 16 slots but we could use the number of characters as 
    estimate; if we maintain some statistics we can actually report this as tracer and pass 
    it as variable with creation (or make a helper). After all we wanted to keep at least the 
    same  performance as before. Tracing showed that quite some defined in \LUA\ are below 32 
    but we also have them larger so we compromise (we're speaking \CONTEXT\ here). Starting 
    out 64 and doubling when we need more is good enough. The initial 64 slots could also 
    be aliased to a static array with a parallel pointer based allocation (just like \LUA\ 
    buffers do). 

    It took quite a bit of editing to convert the (at that point 25) \TEX\ keyword scanners to
    the new approach while keeping the old as reference around. Hopefully no errors crept in.

*/

# define MATCHER_INIT_NODES 64 
# define MATCHER_MAX_INDEX  0xFFFF

keyword_node *tex_matcher_trie_allocate(keyword_matcher *matcher)
{
    if (matcher->nodes_used >= matcher->nodes_cap) {
        if (matcher->nodes_cap >= MATCHER_MAX_INDEX) {
            tex_formatted_warning("matcher", "node capacity exceeded 16-bit index limit");
            return NULL;
        }
        uint32_t new_cap = matcher->nodes_cap ? (uint32_t) matcher->nodes_cap * 2 : MATCHER_INIT_NODES;
        if (new_cap > MATCHER_MAX_INDEX) {
            new_cap = MATCHER_MAX_INDEX;
        }
        matcher->nodes = lmt_memory_realloc_aligned(
            matcher->nodes,
            new_cap * sizeof(keyword_node),
            LMT_MEMORY_ALIGNMENT
        );
        matcher->nodes_cap = (uint16_t) new_cap;
    }
    uint16_t      idx = matcher->nodes_used++;
    keyword_node *n   = &matcher->nodes[idx];
    n->index       = 0;
    n->nofchildren = 0;
    n->keys        = NULL;
    n->children    = NULL;
    return n;
}

static uint16_t tex_matcher_aux_trie_get_or_insert_child(keyword_matcher *matcher, uint16_t curr_idx, int uni)
{
    keyword_node *current = &matcher->nodes[curr_idx];
    int           n       = current->nofchildren;
    int           low     = 0;
    int           high    = n - 1;

    /*tex binary search to see if the child already exists */
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (current->keys[mid] == uni) {
            return current->children[mid];
        } else if (current->keys[mid] < uni) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
     
    int       insert_idx   = low;
    uint16_t  new_count    = (uint16_t) (n + 1);
    int      *new_keys     = lmt_memory_realloc_aligned(current->keys, new_count * sizeof(int), LMT_MEMORY_ALIGNMENT);
    uint16_t *new_children = lmt_memory_realloc_aligned(current->children, new_count * sizeof(uint16_t), LMT_MEMORY_ALIGNMENT);

    if (! new_keys || ! new_children) {
        return 0; 
    }

    current           = &matcher->nodes[curr_idx];
    current->keys     = new_keys;
    current->children = new_children;
    for (int i = new_count - 1; i > insert_idx; i--) {
        current->keys    [i] = current->keys    [i - 1];
        current->children[i] = current->children[i - 1];
    }
    
    keyword_node *new_node = tex_matcher_trie_allocate(matcher);
    if (! new_node) {
        return 0;
    }
    /* Compute index of the newly allocated node */
    uint16_t new_node_idx = (uint16_t) (new_node - matcher->nodes);

    current                       = &matcher->nodes[curr_idx];
    current->nofchildren          = new_count;
    current->keys[insert_idx]     = uni;
    current->children[insert_idx] = new_node_idx;

    return new_node_idx;
}

void tex_matcher_trie_insert(keyword_matcher *matcher, const char *str, int index)
{
    uint16_t             curr_idx  = 0;
    const unsigned char *p         = (const unsigned char *) str;
    size_t               remaining = strlen(str);
    while (remaining > 0) {
        int          len;
        unsigned int uni = aux_str2uni_len(p, remaining, &len);
        curr_idx = tex_matcher_aux_trie_get_or_insert_child(matcher, curr_idx, (int) uni);
        if (curr_idx == 0) {
            break;
        }
        p += len;
        remaining -= (size_t) len;
    }
    matcher->nodes[curr_idx].index = (uint16_t) index;
}

uint16_t tex_matcher_find_child(const keyword_matcher *matcher, uint16_t node_idx, int cp) 
{
    const keyword_node *current = &matcher->nodes[node_idx];
    int                 n       = current->nofchildren;
    if (n < 6) {
        for (int i = 0; i < n; i++) {
            if (current->keys[i] == cp) {
                return current->children[i];
            }
        }
    } else {
        int low  = 0; 
        int high = n - 1;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            if (current->keys[mid] == cp) {
                return current->children[mid];
            } else if (current->keys[mid] < cp) { 
                low = mid + 1;
            } else { 
                high = mid - 1;
            }
        }
    }
    return 0;
}

void tex_matcher_free(keyword_matcher *matcher)
{
    if (! matcher) {
        return;
    }
    if (matcher->nodes) {
        for (int i = 0; i < matcher->nodes_used; i++) {
            lmt_memory_free_aligned(matcher->nodes[i].keys);
            lmt_memory_free_aligned(matcher->nodes[i].children);
        }
        lmt_memory_free_aligned(matcher->nodes);
    }
    matcher->nodes      = NULL;
    matcher->nodes_used = 0;
    matcher->nodes_cap  = 0;
}

/*tex
    As mentioned, one needs to know how \TEX\ picks up tokens, expands, pushed back, and so on,
    but the regular keyword scanners and the ones wrapped above give a clue. The most critital 
    bit is where we decide to quite matching. I had to make sure that existing \CONTEXT\ code was
    still working, just in case we decide to go this route. 

    That is why we have the brace option, it is in the |texideas.c| spec parser (abstraction ) 
    code. If we ever drop in, we need to use another |_use| variant: one that is case insensitive 
    in orde to be compatible in which case we assume \ASCII\.  

    Todo: return a string with expected keys in case of an error. 
*/

#define MAX_MATCHER_DEPTH 32 // We need a "depth first search" so we use a buffer. 

static void tex_matcher_aux_collect_from_node(const keyword_matcher *matcher, uint16_t node_idx, int *buf, int depth, int *first)
{
    const keyword_node *node = &matcher->nodes[node_idx];
    if (! node) {
        return;
    }
    
    if (node->index > 0) {
        if (*first == 0) {
            tex_print_char('|');
        }
        int print_len = depth < MAX_MATCHER_DEPTH ? depth : MAX_MATCHER_DEPTH;
        for (int i = 0; i < print_len; i++) {
            tex_print_uchar(buf[i]);
        }
        if (depth >= MAX_MATCHER_DEPTH) {
            tex_print_str("...");
        }
        *first = 0;
    }
    
    if (depth >= MAX_MATCHER_DEPTH) {
        return; /* Stop descending further if we've hit our display limit */
    }

    for (int i = 0; i < node->nofchildren; i++) {
        buf[depth] = node->keys[i];
        tex_matcher_aux_collect_from_node(matcher, node->children[i], buf, depth + 1, first);
    }
}

static void tex_matcher_aux_collect_expected(const keyword_matcher *matcher, uint16_t node_idx)
{
    int buf[MAX_MATCHER_DEPTH];
    int first = 1;
    tex_matcher_aux_collect_from_node(matcher, node_idx, buf, 0, &first);
}

/*tex

    This one evolved from the original in |lmttokenlib| and first we used the usual \TEX\ approach of 
    keeping a token list but then I realized that here an array made more sense as we in most cases 
    discard the collected tokens. Next the array as made auto-dynamic. Off-loading to a few inline
    helpers made it more readable. The normalizer is inspired by a check in Gemini for a cache 
    friendy solution. I went on till this variant was not worse performant than the original (the 
    optimized character based aproach). It might be extended as we go. 

*/

static inline int tex_matcher_normalize_char(int cp, int is_ascii) 
{
    int is_upper = is_ascii && (cp >= 'A') && (cp <= 'Z');
    return cp + (is_upper * 32);
}

static inline halfword *tex_matcher_ensure_capacity(halfword *arr, halfword *stack_arr, int *capacity, int n) 
{
    if (n >= *capacity) {
        int new_capacity = *capacity * 2;
        halfword *new_arr = lmt_memory_malloc(new_capacity * sizeof(halfword));
        for (int i = 0; i < n; i++) {
            new_arr[i] = arr[i];
        }
        if (arr != stack_arr) {
            lmt_memory_free(arr);
        }
        *capacity = new_capacity;
        return new_arr;
    }
    return arr;
}

static inline halfword tex_matcher_build_token_list(const halfword *arr, int start, int end) 
{
    halfword h = null;
    halfword p = null;
    for (int i = start; i < end; i++) {
        p = tex_store_new_token(p, arr[i]);
        if (! h) { 
            h = p;
        }
    }
    return h;
}

int tex_matcher_use(const keyword_matcher *matcher, int intercept)
{
    halfword  save_cur_cs = cur_cs;
    const int options     = matcher->options;
    halfword  group       = matcher->group;

    /* The usual space skipper: */
    if (options & keyword_relax_option) {
        do {
            tex_get_x_token();
        } while (cur_cmd == spacer_cmd || cur_cmd == relax_cmd);
    } else {
        do {
            tex_get_x_token();
        } while (cur_cmd == spacer_cmd);
    }

    /* A fast first check so that we don't initialize when not needed. */    
    if (group && cur_cmd == left_brace_cmd) {
        cur_cs = save_cur_cs;
        return group;
    }
    if (lmt_unlikely(! matcher->nodes)) {
        tex_back_input(cur_tok);
        cur_cs = save_cur_cs;
        return 0;
    }

    uint16_t  curr_idx    = 0;
    const int is_ascii    = (options & keyword_ascii_option) != 0;
    halfword  stack_arr[32];
    halfword *token_arr   = stack_arr;
    int       capacity    = 32;
    int       best_i      = 0;
    int       n           = 0;
    int       best_n      = 0;

    while (1) {
        switch (cur_cmd) { 
            case letter_cmd:
            case other_char_cmd:
                {
                    int cp = tex_matcher_normalize_char(cur_chr, is_ascii);
                    uint16_t next_idx = tex_matcher_find_child(matcher, curr_idx, cp);
                    if (lmt_unlikely(! next_idx)) {
                        goto BACKUP;
                    }
                    curr_idx  = next_idx;
                    token_arr = tex_matcher_ensure_capacity(token_arr, stack_arr, &capacity, n);
                    token_arr[n++] = cur_tok;
                    const keyword_node *current = &matcher->nodes[curr_idx];
                    if (current->index > 0) {
                        best_i = current->index;
                        best_n = n;                     
                        if (current->nofchildren == 0) {
                            if (options & keyword_terminal_option) { 
                                tex_get_x_token();
                                if (cur_cmd == letter_cmd || cur_cmd == other_char_cmd) {
                                    if (is_ascii) {
                                        int next_cp = tex_matcher_normalize_char(cur_chr, is_ascii);
                                        if (next_cp >= 'a' && next_cp <= 'z') {
                                            goto BACKUP;
                                        }
                                    }
                                    best_i = 0;
                                }
                                goto BACKUP;
                            }              
                            goto DONE;
                        }
                    }
                }
                break;
            case spacer_cmd: 
                if (matcher->nodes[curr_idx].index == 0) {  
                    uint16_t next_idx = tex_matcher_find_child(matcher, curr_idx, ' ');
                    if (lmt_unlikely(! next_idx)) {
                        goto DEFAULT;
                    } else {
                        curr_idx  = next_idx;
                        token_arr = tex_matcher_ensure_capacity(token_arr, stack_arr, &capacity, n);
                        token_arr[n++] = cur_tok;
                        break;
                    }
                } else {
                    goto DEFAULT;
                }
         // case left_brace_cmd: 
         //     if (group) {
         //         best_i = group;
         //         goto DONE;
         //     }
         //  // FALLTHROUGH
         //     goto DEFAULT;
            default:
              DEFAULT:
                if ((options & keyword_exact_option) && (best_n != n)) { 
                    best_i = 0;
                }
              BACKUP:
                /*tex
                    As in the regular keyword scanner we use |back_input| because it compensates
                    the |align_state| for braces. After |DONE| we push back the matched tokens
                    (no match) or the ones after the best match (longer partial match).
                */
                tex_back_input(cur_tok);
                goto DONE;
        }
        tex_get_x_token();
    }
  DONE:
    if (lmt_likely(n > 0)) {
        if (best_i > 0) {
            if (best_n < n) {
                halfword h_rest = tex_matcher_build_token_list(token_arr, best_n, n);
                tex_begin_backed_up_list(h_rest);
            }
        } else {
            halfword h = tex_matcher_build_token_list(token_arr, 0, n);
            /* We use the array so again no need for looping over a list. */
            if (intercept) {
                int saved_selector = lmt_print_state.selector;
                lmt_print_state.selector = new_string_selector_code;
                /* We start up-front and redo some hard work: */
                uint16_t collect_idx   = 0;
                int      matched_count = 0;
                for (int i = 0; i < n; i++) {
                    int cp = tex_matcher_normalize_char(token_chr(token_arr[i]), is_ascii);
                    uint16_t next_idx = tex_matcher_find_child(matcher, collect_idx, cp);
                    if (! next_idx) {
                        break;
                    }
                    collect_idx = next_idx;
                    matched_count++;
                }
                /* Only output the matched part: */
                for (int i = 0; i < matched_count; i++) {
                    tex_print_uchar(token_chr(token_arr[i]));
                }
                /* And the various alternatives. */
                const keyword_node *collect_node = &matcher->nodes[collect_idx];
                if (collect_node && collect_node->index == 0) {
                    tex_print_char('[');
                    tex_matcher_aux_collect_expected(matcher, collect_idx);
                    tex_print_char(']');
                }
                lmt_memory_free(lmt_scanner_state.expected);
                lmt_scanner_state.expected = tex_take_string(NULL);
                lmt_print_state.selector = saved_selector;
            }
            tex_begin_backed_up_list(h);
         }
    }
    if (token_arr != stack_arr) {
        lmt_memory_free(token_arr);
    }
    cur_cs = save_cur_cs;
    return best_i;
}

/* 
    So we keep a registry of (to be defined) matchers. We need that because we want a shared 
    initializer. And, at some point we might want to add more to the item list. We can also let
    the engine report the keyword lists. 

    The tracer can be used to get an idea of hwo much slack we get by over-provisioning but in 
    practice the 64 default is quite okay. There are a few 256 and 512 matchers so we have a few
    reallocs but it can be neglected. 

*/

// todo: use tex_check_matchers

keyword_matcher lmt_matchers     [last_keyword_matcher+1];
matcher_item    lmt_matcher_items[last_keyword_matcher+1];

# if trace_matchers

    void tex_matcher_trace(keyword_matcher *matcher, int index)
    {
        if(index < 0) {
            printf(
                "matcher LUA : %02i keys, %03i nodes, allocated %03i, slack %03i\n", 
                /* */
                matcher->count,
                matcher->nodes_used, 
                matcher->nodes_cap, 
                matcher->nodes_cap - matcher->nodes_used
            );
        } else {
            printf(
                "matcher %03i : %02i keys, %03i nodes, allocated %03i, slack %03i\n", 
                index, 
                matcher->count,
                matcher->nodes_used, 
                matcher->nodes_cap,
                matcher->nodes_cap - matcher->nodes_used
            );
        }
    }

# endif 

static void tex_matcher_aux_build_single(unsigned i)
{
    if (i > last_keyword_matcher) {
        return;
    }
    tex_matcher_free(&lmt_matchers[i]);

    matcher_item item = lmt_matcher_items[i];
    if (item.first <= item.last) { 
        if (! item.words) {
            tex_formatted_warning("matcher", "matcher item index %u has a valid range but null word list", i);
            return;
        }
        keyword_matcher m;
        m.nodes      = NULL;
        m.nodes_used = 0;
        m.nodes_cap  = 0;
        m.count      = (item.last - item.first) + 1;
        m.options    = item.options;
        m.group      = 0;
        tex_matcher_trie_allocate(&m);
        for (unsigned j = item.first; j <= item.last; j++) { 
            tex_matcher_trie_insert(&m, item.words[j], (int) j);
        }
        if (item.options & keyword_group_option) {
            m.group = item.group ? item.group : item.last + 1;
        }
        lmt_matchers[i] = m;
# if trace_matchers
        tex_matcher_trace(&m, i);
# endif
    } else if (item.first != 0 || item.last != 0) {
        tex_formatted_warning("matcher", "matcher item index %u has inverted range: first (%u) > last (%u)", i, item.first, item.last);
    }
}

void tex_reinitialize_matcher(int index)
{
    if (index >= first_keyword_matcher && index <= last_keyword_matcher) {
        tex_matcher_aux_build_single((unsigned) index);
    }
}

void tex_initialize_matchers(void)
{
    for (unsigned i = 0; i <= last_keyword_matcher; i++) {
        /* Is already done @ startup anyway. */
        lmt_matchers[i] = (keyword_matcher) { 
            .nodes      = NULL,
            .nodes_used = 0,
            .nodes_cap  = 0,
            .count      = 0,
            .options    = 0,
            .group      = 0,
        };
    }
# if use_matcher
    tex_set_rule_matcher();
    tex_set_list_matcher();
    tex_set_split_matcher();
    tex_set_adjust_matcher();
    tex_set_specification_matcher();
    tex_set_discretionary_matcher();
    tex_set_insert_matcher();
    tex_set_localboxes_matcher();
    tex_set_parpasses_matcher();
    tex_set_balancepasses_matcher();
    tex_set_balanceshape_matcher();
    tex_set_linesnapping_matcher();
    tex_set_mvl_matcher();
    tex_set_alignment_matcher();
    tex_set_align_matcher();
    tex_set_noalign_matcher();
    tex_set_glyph_matcher();
    tex_set_leader_matcher();
    tex_set_accent_matcher();
    tex_set_showbox_matcher();
    tex_set_mathatom_matcher();
    tex_set_mathradical_matcher();
    tex_set_mathaccent_matcher();
    tex_set_mathdiscretionary_matcher();
    tex_set_mathfraction_matcher();
    tex_set_mathfence_matcher();
# endif 
    tex_set_idea_matcher();
    for (unsigned i = first_keyword_matcher; i <= last_keyword_matcher; i++) {
        tex_matcher_aux_build_single(i);
    }
}

void tex_cleanup_matchers(void)
{
     for (unsigned i = first_keyword_matcher; i <= last_keyword_matcher; i++) {
         tex_matcher_free(&lmt_matchers[i]);
     }
}

void tex_report_matchers(void)
{
    if (lmt_scanner_state.expected) {
        tex_formatted_warning("matcher", "expected keyword: %s", lmt_scanner_state.expected);
        lmt_memory_free(lmt_scanner_state.expected);
        lmt_scanner_state.expected = NULL;
    }
}

static void tex_matcher_aux_collect_keys(int i)
{
    if (i >= first_keyword_matcher && i <= last_keyword_matcher) {
        matcher_item item = lmt_matcher_items[i];
        if (item.first) { 
            int saved_selector = lmt_print_state.selector;
            lmt_print_state.selector = new_string_selector_code;
            for (unsigned j = item.first; j <= item.last; j++) {
                if (j > item.first) { 
                    tex_print_char('|');
                }
                tex_print_str(item.words[j]);
            }
            lmt_memory_free(lmt_scanner_state.collected);
            lmt_scanner_state.collected = tex_take_string(NULL);
            lmt_print_state.selector = saved_selector;
        }
    }
}

void tex_check_matchers(int index)
{
    if (lmt_scanner_state.expected) {
        if (index >= first_keyword_matcher && index <= last_keyword_matcher) {
            matcher_item item = lmt_matcher_items[index];
            if (item.name) { 
                tex_matcher_aux_collect_keys(index);
                tex_handle_error(
                    normal_error_type,
                    "Valid keyword expected for '%s', expected (one of) '%s', possible keys '%s'%h",
                    item.name,
                    lmt_scanner_state.expected ? lmt_scanner_state.expected : "",
                    lmt_scanner_state.collected ? lmt_scanner_state.collected : "",
                    "You started a keyword but it seems to be an invalid one. The first character(s)\n"
                    "might give you a clue. You might want to quit unwanted lookahead with \\relax."
                );
                lmt_memory_free(lmt_scanner_state.expected);
                lmt_scanner_state.expected = NULL;
                lmt_memory_free(lmt_scanner_state.collected);
                lmt_scanner_state.collected = NULL;
                return;
            }
        }
        tex_report_matchers();
    }
}

void tex_ignore_matchers(int index, int key)
{
    if (index >= first_keyword_matcher && index <= last_keyword_matcher) {
        matcher_item item = lmt_matcher_items[index];
        if ((unsigned) key >= item.first && (unsigned) key <= item.last) { 
            tex_handle_error(
                normal_error_type,
                "Keyword '%s' is not supported here%h",
                item.words[key],
                "You entered a keyword that is not recognized or supported at this place, so\n"
                "maybe you should check your source before you go forward.\n"
            );
            lmt_memory_free(lmt_scanner_state.expected);
            lmt_scanner_state.expected = NULL;
            lmt_memory_free(lmt_scanner_state.collected);
            lmt_scanner_state.collected = NULL;
            return;
        }
        tex_report_matchers();
    }
}