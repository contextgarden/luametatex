/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex
    We use \quote {constant} to prevent copying and freeing but we can also consider using a ref
    count instead. But it gets hairy when specifications themselves refer to specifications.
*/

static int valid_specification_options[number_specification_pars] = {
    [par_shape_code]               = specification_option_repeat,
    [par_passes_code]              = specification_option_presets
                                   | specification_option_constant,
    [par_passes_exception_code]    = specification_option_presets,
    [line_snapping_code]           = specification_option_factors
                                   | specification_option_constant
                                   | specification_option_global,
    [math_snapping_code]           = specification_option_factors
                                   | specification_option_constant
                                   | specification_option_global,
    [align_snapping_code]          = specification_option_factors
                                   | specification_option_constant
                                   | specification_option_global,
    [text_spacing_code]            = specification_option_constant,
    [balance_shape_code]           = 0,
    [balance_passes_code]          = specification_option_presets
                                   | specification_option_constant,
    [balance_final_penalties_code] = specification_option_constant,
    [inter_line_penalties_code]    = specification_option_final
                                   | specification_option_constant,
    [club_penalties_code]          = specification_option_double 
                                   | specification_option_largest 
                                   | specification_option_final
                                   | specification_option_constant,
    [widow_penalties_code]         = specification_option_double 
                                   | specification_option_largest 
                                   | specification_option_final
                                   | specification_option_constant,
    [display_widow_penalties_code] = specification_option_double 
                                   | specification_option_largest 
                                   | specification_option_final
                                   | specification_option_constant,
    [broken_penalties_code]        = specification_option_double
                                   | specification_option_constant,
    [orphan_penalties_code]        = specification_option_constant,
    [toddler_penalties_code]       = specification_option_constant,
    [fitness_classes_code]         = specification_option_constant,
    [adjacent_demerits_code]       = specification_option_double
                                   | specification_option_constant,
    [orphan_line_factors_code]     = specification_option_constant,
    [math_forward_penalties_code]  = specification_option_constant,
    [math_backward_penalties_code] = specification_option_constant,
    [integer_list_code]            = specification_option_double 
                                   | specification_option_integer 
                                   | specification_option_default 
                                   | specification_option_rotate,
    [dimension_list_code]          = specification_option_double 
                                   | specification_option_integer 
                                   | specification_option_default 
                                   | specification_option_rotate,
    [posit_list_code]              = specification_option_double 
                                   | specification_option_integer 
                                   | specification_option_default 
                                   | specification_option_rotate,
};

/*tex

    The scanners are only called with the main properties already checked. For instance, count is
    non-zero because zero means \quote {reset} in which case we never go in.

*/

typedef enum specification_keys { 
    specification_key_no_match = 0,
    /* */ 
    specification_key_constant,
    specification_key_default,
    specification_key_double,
    specification_key_factors,
    specification_key_final,
    specification_key_global,
    specification_key_integer,
    specification_key_largest,
    specification_key_options,
    specification_key_presets,
    specification_key_repeat,
    specification_key_rotate,
    /* */
    specification_key_left_brace,
} specification_keys;

static const char *specification_words[] = { 
    NULL,
    /* */
    "constant",
    "default",
    "double",
    "factors",
    "final",
    "global",
    "integer",
    "largest",
    "options",
    "presets",
    "repeat",
    "rotate",
};

void tex_set_specification_matcher(void)
{
    lmt_matcher_items[specification_matcher] = (matcher_item) { 
        .words   = specification_words, 
        .name    = "specification",
        .first   = specification_key_constant,
        .last    = specification_key_rotate,
        .group   = specification_key_left_brace,
        .options = default_matcher_options, 
    };
}

# if use_matcher 

static halfword tex_aux_scan_specification_options(quarterword code)
{
    halfword         options = 0; 
    halfword         valid   = valid_specification_options[code];
    keyword_matcher *matcher = &lmt_matchers[specification_matcher];
    int              key     = 0;
    while (1) {
        /*tex Maybe |migrate <int>| makes sense here. */
        key = tex_matcher_use(matcher, 1);
        switch (key) {
            case specification_key_no_match:
                tex_check_matchers(specification_matcher);
                return options;
            case specification_key_constant:
                if (valid & specification_option_constant) {
                    options |= specification_option_constant;
                    break;
                } else { 
                    goto ISSUE;
                }
            case specification_key_default:
                if (valid & specification_option_default) {
                    options |= specification_option_default;
                    break;
                } else { 
                    goto ISSUE;
                }
            case specification_key_double:
                if (valid & specification_option_double) {
                    options |= specification_option_double;
                    break;
                } else { 
                    goto ISSUE;
                }
            case specification_key_factors:
                if (valid & specification_option_factors) {
                    options |= specification_option_factors;
                    break;
                } else { 
                    goto ISSUE;
                }
            case specification_key_final:
                if (valid & specification_option_final) {
                    options |= specification_option_final;
                    break;
                } else { 
                    goto ISSUE;
                }
            case specification_key_global:
                if (valid & specification_option_global) {
                    options |= specification_option_global;
                    break;
                } else { 
                    goto ISSUE;
                }
            case specification_key_integer:
                if (valid & specification_option_integer) {
                    options |= specification_option_integer;
                    break;
                } else { 
                    goto ISSUE;
                }
            case specification_key_largest:
                if (valid & specification_option_largest) {
                    options |= specification_option_largest;
                    break;
                } else { 
                    goto ISSUE;
                }
            case specification_key_options:
                options |= tex_scan_integer(0, NULL, NULL);
                break;
            case specification_key_presets:
                if (valid & specification_option_presets) {
                    options |= specification_option_presets;
                    break;
                } else { 
                    goto ISSUE;
                }
            case specification_key_repeat:
                if (valid & specification_option_repeat) {
                    options |= specification_option_repeat;
                    break;
                } else { 
                    goto ISSUE;
                }
            case specification_key_rotate:
                if (valid & specification_option_rotate) {
                    options |= specification_option_rotate;
                    break;
                } else { 
                    goto ISSUE;
                }
           default:
                return options;
        }
    }
  ISSUE:
    if (key) {
        tex_ignore_matchers(specification_matcher, key);
    }
    return options;
}

# else 

static halfword tex_aux_scan_specification_options(quarterword code)
{
    halfword options = 0; 
    halfword valid = valid_specification_options[code];
    while (1) {
        /*tex Maybe |migrate <int>| makes sense here. */
        switch (tex_scan_character("orcdlpigf", 0, 1, 0)) {
            case 0:
                return options;
            case 'o':
                if (tex_scan_mandate_keyword("options", 1)) {
                    options |= tex_scan_integer(0, NULL, NULL);
                }
                break;
            case 'r':
                switch (tex_scan_character("eo", 0, 0, 1)) {
                    case 'e':
                        if ((valid & specification_option_repeat) && tex_scan_mandate_keyword("repeat", 2)) {
                            options |= specification_option_repeat;
                        }
                        break;
                    case 'o':
                        if ((valid & specification_option_rotate) && tex_scan_mandate_keyword("rotate", 2)) {
                            options |= specification_option_rotate;
                        }
                        break;
                    default:
                        tex_aux_show_keyword_error("repeat|rotate");
                        return options;
                }
                break;
            case 'd':
                switch (tex_scan_character("eo", 0, 0, 1)) {
                    case 'e':
                        if ((valid & specification_option_default) && tex_scan_mandate_keyword("default", 2)) {
                            options |= specification_option_default;
                        }
                        break;
                    case 'o':
                        if ((valid & specification_option_double) && tex_scan_mandate_keyword("double", 2)) {
                            options |= specification_option_double;
                        }
                        break;
                    default:
                        tex_aux_show_keyword_error("default|double");
                        return options;
                }
                break;
            case 'l':
                if ((valid & specification_option_largest) && tex_scan_mandate_keyword("largest", 1)) {
                    options |= specification_option_largest;
                }
                break;
            case 'p':
                if ((valid & specification_option_presets) && tex_scan_mandate_keyword("presets", 1)) {
                    options |= specification_option_presets;
                }
                break;
            case 'i':
                if ((valid & specification_option_integer) && tex_scan_mandate_keyword("integer", 1)) {
                    options |= specification_option_integer;
                }
                break;
            case 'f':
                switch (tex_scan_character("ai", 0, 0, 1)) {
                    case 'a':
                        if ((valid & specification_option_factors) && tex_scan_mandate_keyword("factors", 2)) {
                            options |= specification_option_factors;
                        }
                        break;
                    case 'i':
                        if ((valid & specification_option_final) && tex_scan_mandate_keyword("final", 2)) {
                            options |= specification_option_final;
                        }
                        break;
                    default:
                        tex_aux_show_keyword_error("factors|final");
                        return options;
                }
                break;
            case 'c':
                if ((valid & specification_option_constant) && tex_scan_mandate_keyword("constant", 1)) {
                    options |= specification_option_constant;
                }
                break;
            case 'g':
                if ((valid & specification_option_global) && tex_scan_mandate_keyword("global", 1)) {
                    options |= specification_option_global;
                }
                break;
           default:
                return options;
        }
    }
}

# endif 

/*tex 
    We could have one function but this is cleaner because we have no parameters related to these
    list specifications. 
*/

/* todo: set/get a specific slot */

static void tex_aux_scan_specification_list_default(halfword p, halfword count, int pair, halfword first, halfword second)
{
    for (int n = 1; n <= count; n++) {
        if (pair) {
            tex_set_specification_nepalty(p, n, first);
            tex_set_specification_penalty(p, n, second);
        } else {
            tex_set_specification_penalty(p, n, first);
        }
    }
}

static halfword tex_aux_scan_specification_list(quarterword code)
{
    halfword p = null;
    halfword count = tex_scan_integer(1, NULL, NULL);
    if (count > 0) {
        halfword options = tex_aux_scan_specification_options(code);
        int pair = specification_option_double(options);
        int isint = specification_option_integer(options);
        switch (code) { 
            case integer_list_code:
                p = tex_new_specification_node(count, integer_list_code, options);
                if (specification_option_default(options)) {
                    tex_aux_scan_specification_list_default(p, count, pair, 
                        tex_scan_integer(0, NULL, NULL), pair ? tex_scan_integer(0, NULL, NULL) : 0   
                    );
                } else { 
                    for (int n = 1; n <= count; n++) {
                        if (pair) {
                            tex_set_specification_nepalty(p, n, tex_scan_integer(0, NULL, NULL));   
                        }
                        tex_set_specification_penalty(p, n, tex_scan_integer(0, NULL, NULL));   
                    }
                }
                break;
            case dimension_list_code:
                p = tex_new_specification_node(count, dimension_list_code, options);
                if (specification_option_default(options)) {
                    tex_aux_scan_specification_list_default(p, count, pair, 
                        isint ? tex_scan_integer(0, NULL, NULL) : tex_scan_dimension(0, 0, 0, 0, NULL, NULL),   
                        pair ? tex_scan_dimension(0, 0, 0, 0, NULL, NULL) : 0
                    );
                } else { 
                    for (int n = 1; n <= count; n++) {
                        if (pair) {
                            tex_set_specification_nepalty(p, n, isint ? tex_scan_integer(0, NULL, NULL) : tex_scan_dimension(0, 0, 0, 0, NULL, NULL));   
                        }
                        tex_set_specification_penalty(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));   
                    }
                }
                break;
            case posit_list_code:
                p = tex_new_specification_node(count, posit_list_code, options);
                if (specification_option_default(options)) {
                    tex_aux_scan_specification_list_default(p, count, pair, 
                        isint ? tex_scan_integer(0, NULL, NULL) : tex_scan_posit(0),   
                        pair ? tex_scan_posit(0) : 0   
                    );
                } else { 
                    for (int n = 1; n <= count; n++) {
                        if (pair) {
                            tex_set_specification_nepalty(p, n, isint ? tex_scan_integer(0, NULL, NULL) : tex_scan_posit(0));   
                        }
                        tex_set_specification_penalty(p, n, tex_scan_posit(0));   
                    }
                }
                break;
            default:
                /* can't happen */
                break;
        }
    }
    return p;
}

/*tex 
    Of course we could split this one up and we might do that some day but it's not that important
    right now.

    If we have a penalties array we could first scan for a specification reference command and when 
    it is of the requested type we could copy its values. But it's not that often needed. Like: 

    \starttyping
    \specificationdef\myclubpenalties \clubpenalties \mywidowpenalties 
    \stoptyping

    Also, we tend to have different setups for widow penalties for odd and even pages in a spread 
    but not for club penalties which makes it even less urgent. 
*/

static halfword tex_aux_scan_specification_par_shape(void)
{
    halfword count = tex_scan_integer(1, NULL, NULL);
    if (count > 0) {
        halfword options = tex_aux_scan_specification_options(par_shape_code) | specification_option_double;
        halfword spec = tex_new_specification_node(count, par_shape_code, options);
        for (int n = 1; n <= count; n++) {
            tex_set_specification_indent(spec, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
            tex_set_specification_width(spec, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL)); 
        }
        return spec; 
    } else {
        return null; 
    }
}

static halfword tex_aux_scan_specification_fitness_classes(void)
{
    halfword count = tex_scan_integer(1, NULL, NULL);
    halfword spec = null;
    halfword scanned = count;
    if (count >= max_n_of_fitness_values) {
        /*tex Todo: warning. */
        count = max_n_of_fitness_values - 1;
    }
    if (count > 0) {
        halfword options = tex_aux_scan_specification_options(fitness_classes_code);
        spec = tex_new_specification_node(count, fitness_classes_code, options);
        for (int n = 1; n <= scanned; n++) {
            halfword value = tex_scan_integer(0, NULL, NULL);
            if (n <= count) {
                tex_set_specification_fitness_class(spec, n, value);
            }
        }
        tex_check_fitness_classes(spec);
    } else {
        spec = tex_default_fitness_classes();
    }
    return spec;
}

static halfword tex_aux_scan_specification_adjacent_demerits(void)
{
    halfword count = tex_scan_integer(1, NULL, NULL);
    halfword spec = null;
    if (count > max_n_of_fitness_values) {
        /*tex Todo: warning. */
        count = max_n_of_fitness_values;
    }
    /*tex This one can be negative, signal that we overload the singular version! */
    if (count == -1 || count > 0) {
        halfword options = tex_aux_scan_specification_options(adjacent_demerits_code);
        halfword duplex = specification_option_double(options);
        halfword max = 0;
        if (count == -1 && ! duplex) {
            /*tex This permits an efficient redefinition of the traditional |\adjdemerits|. */
            spec = tex_new_specification_node(0, adjacent_demerits_code, options);
            specification_count(spec) = 1;
            max = tex_scan_integer(1, NULL, NULL);
            specification_adjacent_adj(spec) = max;
        } else if (count > 0) {
            spec = tex_new_specification_node(count, adjacent_demerits_code, options);
            for (int n = 1; n <= count; n++) {
                halfword value = tex_scan_integer(n == 1 ? 1 : 0, NULL, NULL);
                tex_set_specification_adjacent_u(spec, n, value);
                if (value > max) {
                    max = value; 
                }
                if (duplex) { 
                    value = tex_scan_integer(0, NULL, NULL);
                    if (value > max) {
                        max = value; 
                    }
                }
                tex_set_specification_adjacent_d(spec, n, value);  
            }
        }
        if (spec) {
            specification_adjacent_max(spec) = abs(max);
        }
    }
    return spec;
}

/*tex 
    This scanner is a bit over the top but making a different one does not make sense not does 
    simple scan_keyword and plenty pushback. We just have these long keywords. On a test that 
    scans al keywords the tree based variant is more than three times faster than the sequential 
    push back one. 
*/

static int tex_aux_first_with_criterium(halfword passes, int subpasses) 
{
    for (halfword subpass = 1; subpass <= subpasses; subpass++) {
        if (tex_get_passes_features(passes, subpass) & passes_criterium_set) {
            return subpass;
        }
    }
    return 0;
}

static int tex_aux_first_with_quit(halfword passes, int subpasses) 
{
    for (halfword subpass = 1; subpass <= subpasses; subpass++) {
        if (tex_get_passes_features(passes, subpass) & passes_quit_pass) {
            return subpass; 
        }
    }
    return 0;
}

static int tex_aux_first_with_balance_criterium(halfword passes, int subpasses)
{
    for (halfword subpass = 1; subpass <= subpasses; subpass++) {
        if (tex_get_balance_passes_features(passes, subpass) & passes_criterium_set) {
            return subpass;
        }
    }
    return 0;
}

static int tex_aux_first_with_balance_quit(halfword passes, int subpasses)
{
    for (halfword subpass = 1; subpass <= subpasses; subpass++) {
        if (tex_get_balance_passes_features(passes, subpass) & passes_quit_pass) {
            return subpass;
        }
    }
    return 0;
}

static halfword tex_aux_scan_par_specification(halfword code, halfword (*scan)(void))
{
    do {
        tex_get_x_token();
    } while (cur_cmd == spacer_cmd);
    if (cur_cmd == specificationspec_cmd && node_subtype(cur_chr) == code) { 
        halfword spec = eq_value(cur_cs);
        if (spec) {
            spec = tex_copy_node(spec);
            tex_remove_specification_option(spec, specification_option_constant);
        }
        return spec;
    } else { 
        tex_back_input(cur_tok);
        return scan();
    }
}

static halfword tex_aux_scan_specification_text_spacing(void)
{
    halfword p = null;
    halfword count = tex_scan_integer(1, NULL, NULL);
    if (count > 0) {
        halfword options = tex_aux_scan_specification_options(text_spacing_code);
        p = tex_new_specification_node(count, text_spacing_code, options);
        for (int n = 1; n <= count; n++) {
            tex_set_specification_nepalty(p, n, tex_scan_integer(0, NULL, NULL));
            tex_set_specification_penalty(p, n, tex_scan_integer(0, NULL, NULL));
        }
    }
    return p;
}

static halfword tex_aux_scan_specification_penalties(quarterword code)
{
    halfword p = null;
    halfword count = tex_scan_integer(1, NULL, NULL);
    int pairs = 0;
    switch (code) { 
        case broken_penalties_code: 
            if lmt_unlikely(count > 1) {
                tex_handle_error(
                    normal_error_type,
                    "count has to be 1 for \\brokenpenalties"
                );
                count = 1;
            }
            FALLTHROUGH
        case balance_final_penalties_code: 
        case club_penalties_code: 
        case widow_penalties_code: 
        case display_widow_penalties_code: 
        case toddler_penalties_code: 
            pairs = 1;
     /* case inter_line_penalties_code: */
     /* case orphan_penalties_code: */
     /* case math_forward_penalties_code: */
     /* case math_backward_penalties_code: */
    }
    if (count != 0) { 
        halfword options = tex_aux_scan_specification_options(code);
        int pair = pairs ? specification_option_double(options) : 0;
        if (count == 1 || count == -1) {
            halfword nepalty = pair ? tex_scan_integer(1, NULL, NULL) : 0;
            halfword penalty = tex_scan_integer(pair ? 0 : 1, NULL, NULL);
            /*tex 
                We always need a node unless we introduce a zero_specification_cmd which is a bit
                of overkill. 
            */
         /* if (penalty || nepalty) { */
                if (count == -1) { 
                    options |= specification_option_final;
                    count = 1; 
                }
                p = tex_new_specification_node(0, code, options);
                specification_count(p) = count;
                tex_set_specification_nepalty(p, 0, nepalty); 
                tex_set_specification_penalty(p, 0, penalty);
         /* } */
        } else if (count > 0) {
            int final = specification_option_final(options);
            p = tex_new_specification_node(final ? count + 1 : count, code, options);
            for (int n = 1; n <= count; n++) {
                if (pair) {
                    tex_set_specification_nepalty(p, n, tex_scan_integer(0, NULL, NULL)); 
                }
                tex_set_specification_penalty(p, n, tex_scan_integer(0, NULL, NULL)); 
            }
            if (final) { 
                if (pair) {
                    tex_set_specification_nepalty(p, count + 1, 0); 
                }
                tex_set_specification_penalty(p, count + 1, 0);
            }
        }
        if (p && ! pair) { 
            tex_remove_specification_option(p, specification_option_double);
        }
    }
    return p;
}

static halfword tex_aux_scan_specification_orphan_penalties(void)
{
    return tex_aux_scan_specification_penalties(orphan_penalties_code);
}

static halfword tex_aux_scan_specification_toddler_penalties(void)
{
    return tex_aux_scan_specification_penalties(toddler_penalties_code);
}

static halfword tex_aux_scan_specification_orphan_line_factors(void)
{
    return tex_aux_scan_specification_penalties(orphan_line_factors_code);
}

# if use_matcher 

typedef enum parpasses_keys { 
    parpasses_key_no_match = 0,
    /* */ 
    parpasses_key_adjdemerits,
    parpasses_key_adjacentdemerits,
    parpasses_key_adjustspacing,
    parpasses_key_adjustspacingstep,
    parpasses_key_adjustspacingstretch,
    parpasses_key_adjustspacingshrink,
    parpasses_key_callback,
    parpasses_key_classes,
    parpasses_key_demerits,
    parpasses_key_doublehyphendemerits,
    parpasses_key_emergencyfactor,
    parpasses_key_emergencyleftextra,
    parpasses_key_emergencypercentage,
    parpasses_key_emergencyrightextra,
    parpasses_key_emergencystretch,
    parpasses_key_emergencyunit,
    parpasses_key_emergencywidthextra,
    parpasses_key_extrahyphenpenalty,
    parpasses_key_finalhyphendemerits,
    parpasses_key_fitnessclasses,
    parpasses_key_hyphenation,
    parpasses_key_identifier,
    parpasses_key_ifadjustspacing,
    parpasses_key_ifemergencystretch,
    parpasses_key_iffactor,
    parpasses_key_ifglue,
    parpasses_key_iflooseness,
    parpasses_key_ifmath,
    parpasses_key_iftext,
    parpasses_key_lefttwindemerits, 
    parpasses_key_linebreakchecks,
    parpasses_key_linebreakoptional,
    parpasses_key_linepenalty,
    parpasses_key_looseness,
    parpasses_key_mathpenaltyfactor,
    parpasses_key_next,
    parpasses_key_orphanpenalties,
    parpasses_key_orphanlinefactors,
    parpasses_key_quit,
    parpasses_key_raggedness,
    parpasses_key_righttwindemerits,
    parpasses_key_skip,
    parpasses_key_sffactor,
    parpasses_key_sfstretchfactor,
    parpasses_key_threshold,
    parpasses_key_toddlerpenalties,
    parpasses_key_tolerance,
    parpasses_key_unlessmath,
    /* */
    parpasses_key_left_brace,
} parpasses_keys;

static const char *parpasses_words[] = { 
    NULL,
    /* */
    "adjdemerits",
    "adjacentdemerits",
    "adjustspacing",
    "adjustspacingstep",
    "adjustspacingstretch",
    "adjustspacingshrink",
    "callback",
    "classes",
    "demerits",
    "doublehyphendemerits",
    "emergencyfactor",
    "emergencyleftextra",
    "emergencypercentage",
    "emergencyrightextra",
    "emergencystretch",
    "emergencyunit",
    "emergencywidthextra",
    "extrahyphenpenalty",
    "finalhyphendemerits",
    "fitnessclasses",
    "hyphenation",
    "identifier",
    "ifadjustspacing",
    "ifemergencystretch",
    "iffactor",
    "ifglue",
    "iflooseness",
    "ifmath",
    "iftext",
    "lefttwindemerits", 
    "linebreakchecks",
    "linebreakoptional",
    "linepenalty",
    "looseness",
    "mathpenaltyfactor",
    "next",
    "orphanpenalties",
    "orphanlinefactors",
    "quit",
    "raggedness",
    "righttwindemerits",
    "skip",
    "sffactor",
    "sfstretchfactor",
    "threshold",
    "toddlerpenalties",
    "tolerance",
    "unlessmath",
};

void tex_set_parpasses_matcher(void)
{
    lmt_matcher_items[parpasses_matcher] = (matcher_item) { 
        .words   = parpasses_words, 
        .name    = "parpasses",
        .first   = parpasses_key_adjdemerits,
        .last    = parpasses_key_unlessmath,
        .group   = parpasses_key_left_brace,
        .options = default_matcher_options, 
    };
}

# endif 

static halfword tex_aux_scan_specification_par_passes(void)
{
    halfword p = null;
    halfword count = tex_scan_integer(1, NULL, NULL);
    if (count > 0) {
        /*tex 
            We have no named options here. Presets are automatically set anyway. We have too many 
            keys that can mess up things. 
        */
        halfword options = tex_scan_partial_keyword("options") ? tex_scan_integer(0, NULL, NULL) : 0;
        halfword n = 1;
        if (count > 0xFF) {
            /* todo: message */
            count = 0xFF;
        }
        p = tex_new_specification_node(count, par_passes_code, options);

# if use_matcher 

        keyword_matcher *matcher = &lmt_matchers[parpasses_matcher];

        while (n <= count) {
            switch (tex_matcher_use(matcher, 1)) {
                case parpasses_key_no_match:
                    goto DONE;
                case parpasses_key_adjdemerits:
                    tex_set_passes_adjdemerits(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_adjdemerits_okay);
                    break;
                case parpasses_key_adjacentdemerits:
                    tex_set_passes_adjacentdemerits(p, n, tex_aux_scan_par_specification(adjacent_demerits_code, tex_aux_scan_specification_adjacent_demerits));
                    tex_set_passes_okay(p, n, passes_adjacentdemerits_okay);
                    break;
                case parpasses_key_adjustspacingstep:
                    tex_set_passes_adjustspacingstep(p, n, tex_scan_integer(0, NULL, NULL));   
                    tex_set_passes_okay(p, n, passes_adjustspacingstep_okay);
                    break;
                case parpasses_key_adjustspacingstretch:
                    tex_set_passes_adjustspacingstretch(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_adjustspacingstretch_okay);
                    break;
                case parpasses_key_adjustspacingshrink:
                    tex_set_passes_adjustspacingshrink(p, n, tex_scan_integer(0, NULL, NULL)); 
                    tex_set_passes_okay(p, n, passes_adjustspacingshrink_okay);
                    break;
                case parpasses_key_adjustspacing:
                    tex_set_passes_adjustspacing(p, n, tex_scan_integer(0, NULL, NULL));   
                    tex_set_passes_okay(p, n, passes_adjustspacing_okay);
                    break;
                case parpasses_key_callback:
                    tex_set_passes_callback(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_features(p, n, passes_callback_set);
                    tex_set_passes_okay(p, n, passes_callback_okay);
                    break;
                case parpasses_key_classes:
                    tex_set_passes_classes(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_features(p, n, passes_criterium_set);
                    tex_set_passes_okay(p, n, passes_classes_okay);
                    break;
                case parpasses_key_demerits:
                    tex_set_passes_demerits(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_features(p, n, passes_criterium_set);
                    tex_set_passes_okay(p, n, passes_demerits_okay);
                    break;
                case parpasses_key_doublehyphendemerits:
                    tex_set_passes_doublehyphendemerits(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_doublehyphendemerits_okay);
                    break;
                case parpasses_key_emergencyfactor:
                    /* tex 
                        Using a factor is better from the perspective 
                        of |\specificationdef| usage because we don't 
                        want hardcoded dimensions then. 
                    */
                    tex_set_passes_emergencyfactor(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_emergencyfactor_okay);
                    break;
                case parpasses_key_emergencyleftextra:
                    tex_set_passes_emergencyleftextra(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_emergencyleftextra_okay);
                    break;
                case parpasses_key_emergencypercentage:
                    tex_set_passes_emergencypercentage(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_emergencypercentage_okay);
                    break;
                case parpasses_key_emergencyrightextra:
                    tex_set_passes_emergencyrightextra(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_emergencyrightextra_okay);
                    break;
                case parpasses_key_emergencystretch:
                    tex_set_passes_emergencystretch(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_emergencystretch_okay);
                    break;
                case parpasses_key_emergencyunit:
                    tex_set_passes_emergencyunit(p, n, tex_scan_unit_register_number(0));
                    tex_set_passes_okay(p, n, passes_emergencyunit_okay);
                    break;
                case parpasses_key_emergencywidthextra:
                    tex_set_passes_emergencywidthextra(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_emergencywidthextra_okay);
                    break;
                case parpasses_key_extrahyphenpenalty:
                    tex_set_passes_extrahyphenpenalty(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_extrahyphenpenalty_okay);
                    break;
                case parpasses_key_finalhyphendemerits:
                    tex_set_passes_finalhyphendemerits(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_finalhyphendemerits_okay);
                    break;
                case parpasses_key_fitnessclasses:
                    tex_set_passes_fitnessclasses(p, n, tex_aux_scan_par_specification(fitness_classes_code, tex_aux_scan_specification_fitness_classes));
                    tex_set_passes_okay(p, n, passes_fitnessclasses_okay);
                    break;
                case parpasses_key_hyphenation:
                    tex_set_passes_hyphenation(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_hyphenation_okay);
                    break;
                case parpasses_key_identifier:
                    passes_identifier(p) = tex_scan_integer(0, NULL, NULL);
                    break;
                case parpasses_key_ifadjustspacing:
                    tex_set_passes_features(p, n, passes_if_adjust_spacing);
                    break;
                case parpasses_key_ifemergencystretch:
                    tex_set_passes_features(p, n, passes_if_emergency_stretch);
                    break;
                case parpasses_key_iffactor:
                    tex_set_passes_features(p, n, passes_if_space_factor);
                    break;
                case parpasses_key_ifglue:
                    tex_set_passes_features(p, n, passes_if_glue);
                    break;
                case parpasses_key_iflooseness:
                    tex_set_passes_features(p, n, passes_if_looseness);
                    break;
                case parpasses_key_ifmath:
                    tex_set_passes_features(p, n, passes_if_math);
                    break;
                case parpasses_key_iftext:
                    tex_set_passes_features(p, n, passes_if_text);
                    break;
                case parpasses_key_lefttwindemerits:
                    tex_set_passes_lefttwindemerits(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_lefttwindemerits_okay);
                    break;
                case parpasses_key_linebreakchecks:
                    tex_set_passes_linebreakchecks(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_linebreakchecks_okay);
                    break;
                case parpasses_key_linebreakoptional:
                    tex_set_passes_linebreakoptional(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_linebreakoptional_okay);
                    break;
                case parpasses_key_linepenalty:
                    tex_set_passes_linepenalty(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_linepenalty_okay);
                    break;
                case parpasses_key_looseness:
                    tex_set_passes_looseness(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_looseness_okay);
                    break;
                case parpasses_key_mathpenaltyfactor:
                    {
                        halfword v = tex_scan_integer(0, NULL, NULL);
                        if (v < 0) {
                            v = 0;
                        } else if (v == scaling_factor) {
                            v = 0;
                        }
                        tex_set_passes_mathpenaltyfactor(p, n, v);
                        tex_set_passes_okay(p, n, passes_mathpenaltyfactor_okay);
                    }
                    break;
                case parpasses_key_next:
                    n++;
                    break;
                case parpasses_key_orphanpenalties:
                    tex_set_passes_orphanpenalties(p, n, tex_aux_scan_par_specification(orphan_penalties_code, tex_aux_scan_specification_orphan_penalties));
                    tex_set_passes_okay(p, n, passes_orphanpenalties_okay);
                    break;
                case parpasses_key_orphanlinefactors:
                    tex_set_passes_orphanlinefactors(p, n, tex_aux_scan_par_specification(orphan_line_factors_code, tex_aux_scan_specification_orphan_line_factors));
                    tex_set_passes_okay(p, n, passes_orphanlinefactors_okay);
                    break;
                case parpasses_key_quit:
                    tex_set_passes_features(p, n, passes_quit_pass);
                    break;
                case parpasses_key_raggedness:
                    tex_set_passes_raggedness(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_raggedness_okay);
                    break;
                case parpasses_key_righttwindemerits:
                    tex_set_passes_righttwindemerits(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_righttwindemerits_okay);
                    break;
                case parpasses_key_skip:
                    tex_set_passes_features(p, n, passes_skip_pass);
                    break;
                case parpasses_key_sffactor:
                    tex_set_passes_sffactor(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_sffactor_okay);
                    break;
                case parpasses_key_sfstretchfactor:
                    tex_set_passes_sfstretchfactor(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_passes_okay(p, n, passes_sfstretchfactor_okay);
                    break;
                case parpasses_key_threshold:
                    tex_set_passes_threshold(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
                    tex_set_passes_features(p, n, passes_criterium_set);
                    tex_set_passes_okay(p, n, passes_threshold_okay);
                    break;
                case parpasses_key_toddlerpenalties:
                    tex_set_passes_toddlerpenalties(p, n, tex_aux_scan_par_specification(toddler_penalties_code, tex_aux_scan_specification_toddler_penalties));
                    tex_set_passes_okay(p, n, passes_toddlerpenalties_okay);
                    break;
                case parpasses_key_tolerance:
                    tex_set_passes_tolerance(p, n, tex_scan_integer(0, NULL, NULL));
                    /* Not here! */ /* tex_set_passes_features(p, n, passes_criterium_set); */
                    tex_set_passes_okay(p, n, passes_tolerance_okay);
                    break;
                case parpasses_key_unlessmath:
                    tex_set_passes_features(p, n, passes_unless_math);
                    break;
                default:
                    goto DONE;
            }
        }
      DONE:
        tex_check_matchers(parpasses_matcher);

# else 

        while (n <= count) {
            switch (tex_scan_character("acdefhilmnoqrstu", 0, 1, 0)) {
                case 0:
                    goto DONE;
                case 'a':
                    if (tex_scan_mandate_keyword("adj", 1)) {
                        switch (tex_scan_character("adu", 0, 0, 0)) {
                            case 'd':
                                if (tex_scan_mandate_keyword("adjdemerits", 4)) {
                                    tex_set_passes_adjdemerits(p, n, tex_scan_integer(0, NULL, NULL));
                                    tex_set_passes_okay(p, n, passes_adjdemerits_okay);
                                } break;
                            case 'a':
                                if (tex_scan_mandate_keyword("adjacentdemerits", 4)) {
                                    tex_set_passes_adjacentdemerits(p, n, tex_aux_scan_par_specification(adjacent_demerits_code, tex_aux_scan_specification_adjacent_demerits));
                                    tex_set_passes_okay(p, n, passes_adjacentdemerits_okay);
                                }
                                break;
                            case 'u':
                                if (tex_scan_mandate_keyword("adjustspacing", 4)) {
                                    if (tex_scan_character("s", 0, 0, 0)) {
                                        switch (tex_scan_character("th", 0, 0, 0)) {
                                            case 't':
                                                switch (tex_scan_character("er", 0, 0, 0)) {
                                                    case 'e':
                                                        if (tex_scan_mandate_keyword("adjustspacingstep", 16)) {
                                                            tex_set_passes_adjustspacingstep(p, n, tex_scan_integer(0, NULL, NULL));   
                                                            tex_set_passes_okay(p, n, passes_adjustspacingstep_okay);
                                                        }
                                                        break;
                                                    case 'r':
                                                        if (tex_scan_mandate_keyword("adjustspacingstretch", 16)) {
                                                            tex_set_passes_adjustspacingstretch(p, n, tex_scan_integer(0, NULL, NULL));
                                                            tex_set_passes_okay(p, n, passes_adjustspacingstretch_okay);
                                                        }
                                                        break;
                                                    default:
                                                        tex_aux_show_keyword_error("adjustspacingstep|adjustspacingstretch");
                                                        goto DONE;
                                                }
                                                break;
                                            case 'h':
                                                if (tex_scan_mandate_keyword("adjustspacingshrink", 15)) {
                                                    tex_set_passes_adjustspacingshrink(p, n, tex_scan_integer(0, NULL, NULL)); 
                                                    tex_set_passes_okay(p, n, passes_adjustspacingshrink_okay);
                                                }
                                                break;
                                            default:
                                                tex_aux_show_keyword_error("adjustspacingstep|adjustspacingshrink|adjustspacingstretch");
                                                goto DONE;
                                        }
                                    } else {
                                        tex_set_passes_adjustspacing(p, n, tex_scan_integer(0, NULL, NULL));   
                                        tex_set_passes_okay(p, n, passes_adjustspacing_okay);
                                    } 
                                }
                                break;
                            default:
                                goto NOTDONE1;
                        }
                    } else {
                      NOTDONE1:
                        tex_aux_show_keyword_error("adjdemerits|adjacentdemerits|adjustspacing|adjustspacingstep|adjustspacingshrink|adjustspacingstretch");
                        goto DONE;
                    }
                    break;
                case 'c':
                    switch (tex_scan_character("al", 0, 0, 0)) {
                        case 'a':
                            if (tex_scan_mandate_keyword("callback", 2)) {
                                tex_set_passes_callback(p, n, tex_scan_integer(0, NULL, NULL));
                                tex_set_passes_features(p, n, passes_callback_set);
                                tex_set_passes_okay(p, n, passes_callback_okay);
                            }
                            break;
                        case 'l':
                            if (tex_scan_mandate_keyword("classes", 2)) {
                                tex_set_passes_classes(p, n, tex_scan_integer(0, NULL, NULL));
                                tex_set_passes_features(p, n, passes_criterium_set);
                                tex_set_passes_okay(p, n, passes_classes_okay);
                            }
                            break;
                        default:
                            tex_aux_show_keyword_error("classes|callback");
                            goto DONE;
                    }
                    break;
                case 'd':
                    switch (tex_scan_character("oe", 0, 0, 0)) {
                        case 'e':
                            if (tex_scan_mandate_keyword("demerits", 2)) {
                                tex_set_passes_demerits(p, n, tex_scan_integer(0, NULL, NULL));
                                tex_set_passes_features(p, n, passes_criterium_set);
                                tex_set_passes_okay(p, n, passes_demerits_okay);
                            }
                            break;
                        case 'o':
                            if (tex_scan_mandate_keyword("doublehyphendemerits", 2)) {
                                tex_set_passes_doublehyphendemerits(p, n, tex_scan_integer(0, NULL, NULL));
                                tex_set_passes_okay(p, n, passes_doublehyphendemerits_okay);
                            }
                            break;
                        default:
                            tex_aux_show_keyword_error("demerits|doublehyphendemerits");
                            goto DONE;
                    }
                    break;
                case 'e':
                    switch (tex_scan_character("mx", 0, 0, 0)) {
                        case 'm':
                            if (tex_scan_mandate_keyword("emergency", 2)) {
                                switch (tex_scan_character("flspruw", 0, 0, 0)) {
                                    case 'f':
                                        /* tex 
                                            Using a factor is better from the perspective 
                                            of |\specificationdef| usage because we don't 
                                            want hardcoded dimensions then. 
                                        */
                                        if (tex_scan_mandate_keyword("emergencyfactor", 10)) {
                                            tex_set_passes_emergencyfactor(p, n, tex_scan_integer(0, NULL, NULL));
                                            tex_set_passes_okay(p, n, passes_emergencyfactor_okay);
                                        }
                                        break;
                                    case 'l':
                                        if (tex_scan_mandate_keyword("emergencyleftextra", 10)) {
                                            tex_set_passes_emergencyleftextra(p, n, tex_scan_integer(0, NULL, NULL));
                                            tex_set_passes_okay(p, n, passes_emergencyleftextra_okay);
                                        }
                                        break;
                                    case 'p':
                                        if (tex_scan_mandate_keyword("emergencypercentage", 10)) {
                                            tex_set_passes_emergencypercentage(p, n, tex_scan_integer(0, NULL, NULL));
                                            tex_set_passes_okay(p, n, passes_emergencypercentage_okay);
                                        }
                                        break;
                                    case 'r':
                                        if (tex_scan_mandate_keyword("emergencyrightextra", 10)) {
                                            tex_set_passes_emergencyrightextra(p, n, tex_scan_integer(0, NULL, NULL));
                                            tex_set_passes_okay(p, n, passes_emergencyrightextra_okay);
                                        }
                                        break;
                                    case 's':
                                        if (tex_scan_mandate_keyword("emergencystretch", 10)) {
                                            tex_set_passes_emergencystretch(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
                                            tex_set_passes_okay(p, n, passes_emergencystretch_okay);
                                        }
                                        break;
                                    case 'u':
                                        if (tex_scan_mandate_keyword("emergencyunit", 10)) {
                                            tex_set_passes_emergencyunit(p, n, tex_scan_unit_register_number(0));
                                            tex_set_passes_okay(p, n, passes_emergencyunit_okay);
                                        }
                                        break;
                                    case 'w':
                                        if (tex_scan_mandate_keyword("emergencywidthextra", 10)) {
                                            tex_set_passes_emergencywidthextra(p, n, tex_scan_integer(0, NULL, NULL));
                                            tex_set_passes_okay(p, n, passes_emergencywidthextra_okay);
                                        }
                                        break;
                                    default:
                                        goto NOTDONE4;
                                }
                            } else {
                                NOTDONE4:
                                tex_aux_show_keyword_error("emergencyfactor|emergencystretch|emergencypercentage|emergencyleftextra|emergencyunit|emergencyrightextra");
                                goto DONE;
                            }
                            break;
                        case 'x':
                            if (tex_scan_mandate_keyword("extrahyphenpenalty", 2)) {
                                tex_set_passes_extrahyphenpenalty(p, n, tex_scan_integer(0, NULL, NULL));
                                tex_set_passes_okay(p, n, passes_extrahyphenpenalty_okay);
                            }
                            break;
                        default:
                            tex_aux_show_keyword_error("emergencyfactor|extrahyphenpenalty");
                            goto DONE;
                    }
                    break;
                case 'f':
                    if (tex_scan_mandate_keyword("fi", 1)) {
                        switch (tex_scan_character("nt", 0, 0, 0)) {
                            case 'n':
                                if (tex_scan_mandate_keyword("finalhyphendemerits", 3)) {
                                    tex_set_passes_finalhyphendemerits(p, n, tex_scan_integer(0, NULL, NULL));
                                    tex_set_passes_okay(p, n, passes_finalhyphendemerits_okay);
                                }
                                break;
                            case 't':
                                if (tex_scan_mandate_keyword("fitnessclasses", 3)) {
                                    tex_set_passes_fitnessclasses(p, n, tex_aux_scan_par_specification(fitness_classes_code, tex_aux_scan_specification_fitness_classes));
                                    tex_set_passes_okay(p, n, passes_fitnessclasses_okay);
                                }
                                break;
                            default:
                                goto NOTDONE2;
                        }
                    } else {
                      NOTDONE2:
                        tex_aux_show_keyword_error("finalhyphendemerits|fitnessclasses");
                        goto DONE;
                    }
                    break;
                case 'h':
                    if (tex_scan_mandate_keyword("hyphenation", 1)) {
                        tex_set_passes_hyphenation(p, n, tex_scan_integer(0, NULL, NULL));
                        tex_set_passes_okay(p, n, passes_hyphenation_okay);
                    }
                    break;
                case 'i':
                    switch (tex_scan_character("df", 0, 0, 0)) {
                        case 'd':
                            if (tex_scan_mandate_keyword("identifier", 2)) {
                                passes_identifier(p) = tex_scan_integer(0, NULL, NULL);
                            }
                            break;
                        case 'f':
                            switch (tex_scan_character("aefgmlt", 0, 0, 0)) {
                                case 'a':
                                    if (tex_scan_mandate_keyword("ifadjustspacing", 3)) {
                                        tex_set_passes_features(p, n, passes_if_adjust_spacing);
                                    } 
                                    break;
                                case 'e':
                                    if (tex_scan_mandate_keyword("ifemergencystretch", 3)) {
                                        tex_set_passes_features(p, n, passes_if_emergency_stretch);
                                    } 
                                    break;
                                case 'f':
                                    if (tex_scan_mandate_keyword("iffactor", 3)) {
                                        tex_set_passes_features(p, n, passes_if_space_factor);
                                    } 
                                    break;
                                case 'g':
                                    if (tex_scan_mandate_keyword("ifglue", 3)) {
                                        tex_set_passes_features(p, n, passes_if_glue);
                                    } 
                                    break;
                                case 'l':
                                    if (tex_scan_mandate_keyword("iflooseness", 3)) {
                                        tex_set_passes_features(p, n, passes_if_looseness);
                                    } 
                                    break;
                                case 'm':
                                    if (tex_scan_mandate_keyword("ifmath", 3)) {
                                        tex_set_passes_features(p, n, passes_if_math);
                                    } 
                                    break;
                                case 't':
                                    if (tex_scan_mandate_keyword("iftext", 3)) {
                                        tex_set_passes_features(p, n, passes_if_text);
                                    } 
                                    break;
                                default:
                                    tex_aux_show_keyword_error("if[adjustspacing|emergencystretch|factor|glue|looseness|math|text]");
                                    goto DONE;
                            }
                            break;
                        default:
                            tex_aux_show_keyword_error("identifier|if[...]");
                            goto DONE;
                    }
                    break;
                case 'l':
                    switch (tex_scan_character("ieo", 0, 0, 0)) {
                        case 'e':
                            if (tex_scan_mandate_keyword("lefttwindemerits", 2)) {
                                tex_set_passes_lefttwindemerits(p, n, tex_scan_integer(0, NULL, NULL));
                                tex_set_passes_okay(p, n, passes_lefttwindemerits_okay);
                            } 
                            break;
                        case 'i':
                            if (tex_scan_mandate_keyword("line", 2)) {
                                switch (tex_scan_character("bp", 0, 0, 0)) {
                                    case 'b':
                                        if (tex_scan_mandate_keyword("linebreak", 5)) {
                                            switch (tex_scan_character("co", 0, 0, 0)) {
                                                case 'c':
                                                    if (tex_scan_mandate_keyword("linebreakchecks", 10)) {
                                                        tex_set_passes_linebreakchecks(p, n, tex_scan_integer(0, NULL, NULL));
                                                        tex_set_passes_okay(p, n, passes_linebreakchecks_okay);
                                                    } 
                                                    break;
                                                case 'o':
                                                    if (tex_scan_mandate_keyword("linebreakoptional", 10)) {
                                                        tex_set_passes_linebreakoptional(p, n, tex_scan_integer(0, NULL, NULL));
                                                        tex_set_passes_okay(p, n, passes_linebreakoptional_okay);
                                                    } 
                                                    break;
                                                default:
                                                    tex_aux_show_keyword_error("linebreakoptional|linebreakchecks");
                                                    goto DONE;
                                            }
                                        }
                                        break;
                                    case 'p':
                                        if (tex_scan_mandate_keyword("linepenalty", 5)) {
                                            tex_set_passes_linepenalty(p, n, tex_scan_integer(0, NULL, NULL));
                                            tex_set_passes_okay(p, n, passes_linepenalty_okay);
                                        } 
                                        break;
                                    default:
                                        tex_aux_show_keyword_error("linebreakoptional|linebreakchecks|linepenalty");
                                        goto DONE;
                                }
                            }
                            break;
                        case 'o':
                            if (tex_scan_mandate_keyword("looseness", 2)) {
                                tex_set_passes_looseness(p, n, tex_scan_integer(0, NULL, NULL));
                                tex_set_passes_okay(p, n, passes_looseness_okay);
                            } 
                            break;
                        default:
                            tex_aux_show_keyword_error("lefttwindemerits|looseness|line[...]");
                            goto DONE;
                    }
                    break;
                case 'm':
                    if (tex_scan_mandate_keyword("mathpenaltyfactor", 1)) {
                        halfword v = tex_scan_integer(0, NULL, NULL);
                        if (v < 0) {
                            v = 0;
                        } else if (v == scaling_factor) {
                            v = 0;
                        }
                        tex_set_passes_mathpenaltyfactor(p, n, v);
                        tex_set_passes_okay(p, n, passes_mathpenaltyfactor_okay);
                    }
                    break;
                case 'n':
                    if (tex_scan_mandate_keyword("next", 1)) {
                        n++;
                    }
                    break;
                case 'o':
                    if (tex_scan_mandate_keyword("orphan", 1)) {
                        switch (tex_scan_character("pl", 0, 0, 0)) {
                            case 'p':
                                if (tex_scan_mandate_keyword("orphanpenalties", 7)) {
                                    tex_set_passes_orphanpenalties(p, n, tex_aux_scan_par_specification(orphan_penalties_code, tex_aux_scan_specification_orphan_penalties));
                                    tex_set_passes_okay(p, n, passes_orphanpenalties_okay);
                                }
                                break;
                            case 'l':
                                if (tex_scan_mandate_keyword("orphanlinefactors", 7)) {
                                    tex_set_passes_orphanlinefactors(p, n, tex_aux_scan_par_specification(orphan_line_factors_code, tex_aux_scan_specification_orphan_line_factors));
                                    tex_set_passes_okay(p, n, passes_orphanlinefactors_okay);
                                }
                                break;
                            default:
                                tex_aux_show_keyword_error("orphanpenalties|orphanlinefactors");
                                goto DONE;
                        }
                    }
                    break;
                case 'q':
                    if (tex_scan_mandate_keyword("quit", 1)) {
                        tex_set_passes_features(p, n, passes_quit_pass);
                    }
                    break;
                case 'r':
                    switch (tex_scan_character("ai", 0, 0, 0)) {
                        case 'a':
                            if (tex_scan_mandate_keyword("raggedness", 2)) {
                                tex_set_passes_raggedness(p, n, tex_scan_integer(0, NULL, NULL));
                                tex_set_passes_okay(p, n, passes_raggedness_okay);
                            }
                            break;
                        case 'i':
                            if (tex_scan_mandate_keyword("righttwindemerits", 2)) {
                                tex_set_passes_righttwindemerits(p, n, tex_scan_integer(0, NULL, NULL));
                                tex_set_passes_okay(p, n, passes_righttwindemerits_okay);
                            }
                            break;
                        default:
                            tex_aux_show_keyword_error("raggedness|righttwindemerits");
                            goto DONE;
                    }
                    break;
                case 's':
                    switch (tex_scan_character("kf", 0, 0, 0)) {
                        case 'k':
                            if (tex_scan_mandate_keyword("skip", 2)) {
                                tex_set_passes_features(p, n, passes_skip_pass);
                            }
                            break;
                        case 'f':
                            switch (tex_scan_character("fs", 0, 0, 0)) {
                                case 'f':
                                    if (tex_scan_mandate_keyword("sffactor", 3)) {
                                        tex_set_passes_sffactor(p, n, tex_scan_integer(0, NULL, NULL));
                                        tex_set_passes_okay(p, n, passes_sffactor_okay);
                                    }
                                    break;
                                case 's':
                                    if (tex_scan_mandate_keyword("sfstretchfactor", 3)) {
                                        tex_set_passes_sfstretchfactor(p, n, tex_scan_integer(0, NULL, NULL));
                                        tex_set_passes_okay(p, n, passes_sfstretchfactor_okay);
                                    }
                                    break;
                                default: 
                                    goto NOTDONE5;
                            }
                            break;
                        default:
                          NOTDONE5:
                            tex_aux_show_keyword_error("skip|sffactor|sfstretchfactor");
                            goto DONE;
                    }
                    break;
                case 't':
                    switch (tex_scan_character("ho", 0, 0, 0)) {
                        case 'h':
                            if (tex_scan_mandate_keyword("threshold", 2)) {
                                tex_set_passes_threshold(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
                                tex_set_passes_features(p, n, passes_criterium_set);
                                tex_set_passes_okay(p, n, passes_threshold_okay);
                            }
                            break;
                        case 'o':
                            switch (tex_scan_character("dl", 0, 0, 0)) {
                                case 'd':
                                    if (tex_scan_mandate_keyword("toddlerpenalties", 3)) {
                                        tex_set_passes_toddlerpenalties(p, n, tex_aux_scan_par_specification(toddler_penalties_code, tex_aux_scan_specification_toddler_penalties));
                                        tex_set_passes_okay(p, n, passes_toddlerpenalties_okay);
                                    }

                                    break;
                                case 'l':
                                    if (tex_scan_mandate_keyword("tolerance", 3)) {
                                        tex_set_passes_tolerance(p, n, tex_scan_integer(0, NULL, NULL));
                        /* Not here! */ /* tex_set_passes_features(p, n, passes_criterium_set); */
                                        tex_set_passes_okay(p, n, passes_tolerance_okay);
                                    }
                                    break;
                                default:
                                    goto NOTDONE3;
                            }
                            break;
                        default:
                          NOTDONE3:
                            tex_aux_show_keyword_error("threshold|tolerance|toddlerpenalties");
                            goto DONE;
                    }
                    break;
                case 'u':
                    if (tex_scan_mandate_keyword("unlessmath", 1)) {
                        tex_set_passes_features(p, n, passes_unless_math);
                    } 
                    break;
                default:
                    goto DONE;
            }
        }
      DONE:

# endif

        if lmt_unlikely(n < count) {
            tex_handle_error(
                normal_error_type,
                "there %s only %i of %i %s specified for \\parpasses",
                n == 1 ? "is" : "are", n, count, count == 1 ? "pass" : "passes"
            );
        }
        {
            halfword first = tex_aux_first_with_criterium(p, count);
            halfword quit = tex_aux_first_with_quit(p, count);
            if (first == 0) { 
                tex_add_specification_option(p, specification_option_presets);
                passes_first_final(p) = count;
            } else if (first == 1) { 
                tex_remove_specification_option(p, specification_option_presets);
                passes_first_final(p) = 2;
            } else { 
                tex_add_specification_option(p, specification_option_presets);
                passes_first_final(p) = first - 1;
            }
            if (quit) { 
                /*tex We always want a result. */
                passes_first_final(p) = quit == 1 ? 1 : quit - 1;
            }
        }
    }
    return p;
}

# if use_matcher

typedef enum balancepasses_keys { 
    balancepasses_key_no_match = 0,
    /* */ 
    balancepasses_key_adjdemerits,
    balancepasses_key_classes,
    balancepasses_key_demerits,
    balancepasses_key_emergencyfactor,
    balancepasses_key_emergencypercentage,
    balancepasses_key_emergencystretch,
    balancepasses_key_emergencyshrink,
    balancepasses_key_fitnessclasses,
    balancepasses_key_identifier,
    balancepasses_key_ifemergencystretch,
    balancepasses_key_ifemergencyshrink,
    balancepasses_key_iflooseness,
    balancepasses_key_looseness,
    balancepasses_key_next,
    balancepasses_key_pagebreakchecks,
    balancepasses_key_pagepenalty,
    balancepasses_key_quit,
    balancepasses_key_threshold,
    balancepasses_key_tolerance,
    /* */
    balancepasses_key_left_brace,
} balancepasses_keys;

static const char *balancepasses_words[] = { 
    NULL,
    /* */
    "adjdemerits",
    "classes",
    "demerits",
    "emergencyfactor",
    "emergencypercentage",
    "emergencystretch",
    "emergencyshrink",
    "fitnessclasses",
    "identifier",
    "ifemergencystretch",
    "ifemergencyshrink",
    "iflooseness",
    "looseness",
    "next",
    "pagebreakchecks",
    "pagepenalty",
    "quit",
    "threshold",
    "tolerance",
};

void tex_set_balancepasses_matcher(void)
{
    lmt_matcher_items[balancepasses_matcher] = (matcher_item) { 
        .words   = balancepasses_words, 
        .name    = "balancepasses",
        .first   = balancepasses_key_adjdemerits,
        .last    = balancepasses_key_tolerance,
        .group   = balancepasses_key_left_brace,
        .options = default_matcher_options, 
    };
}

# endif

static halfword tex_aux_scan_specification_balance_passes(void)
{
    halfword p = null;
    halfword count = tex_scan_integer(1, NULL, NULL);
    if (count > 0) {
        /*tex 
            We have no named options here. Presets are automatically set anyway. We might even drop 
            the option scanning here.
        */
        halfword options = tex_scan_partial_keyword("options") ? tex_scan_integer(0, NULL, NULL) : 0;
        halfword n = 1;
        if (count > 0xFF) {
            /* todo: message */
            count = 0xFF;
        }
        p = tex_new_specification_node(count, balance_passes_code, options);

# if use_matcher

        keyword_matcher *matcher = &lmt_matchers[balancepasses_matcher];

        while (n <= count) {
            switch (tex_matcher_use(matcher, 1)) {
                case balancepasses_key_no_match:
                    goto DONE;
                case balancepasses_key_adjdemerits:
                    tex_set_balance_passes_adjdemerits(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_adjdemerits_okay);
                    break;
                case balancepasses_key_classes:
                    tex_set_balance_passes_classes(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_balance_passes_features(p, n, passes_criterium_set);
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_classes_okay);
                    break;
                case balancepasses_key_demerits:
                    tex_set_balance_passes_demerits(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_balance_passes_features(p, n, passes_criterium_set);
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_demerits_okay);
                    break;
                case balancepasses_key_emergencyfactor:
                    tex_set_balance_passes_emergencyfactor(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_emergencyfactor_okay);
                    break;
                case balancepasses_key_emergencypercentage:
                    tex_set_balance_passes_emergencypercentage(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_emergencypercentage_okay);
                    break;
                case balancepasses_key_emergencystretch:
                    tex_set_balance_passes_emergencystretch(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_emergencystretch_okay);
                    break;
                case balancepasses_key_emergencyshrink:
                    tex_set_balance_passes_emergencyshrink(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_emergencyshrink_okay);
                    break;
                case balancepasses_key_fitnessclasses:
                    tex_set_balance_passes_fitnessclasses(p, n, tex_aux_scan_par_specification(fitness_classes_code, tex_aux_scan_specification_fitness_classes));
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_fitnessclasses_okay);
                    break;
                case balancepasses_key_identifier:
                    passes_identifier(p) = tex_scan_integer(0, NULL, NULL);
                    break;
                case balancepasses_key_ifemergencystretch:
                    tex_set_balance_passes_features(p, n, passes_if_emergency_stretch);
                    break;
                case balancepasses_key_ifemergencyshrink:
                    tex_set_balance_passes_features(p, n, passes_if_emergency_shrink);
                    break;
                case balancepasses_key_iflooseness:
                    tex_set_balance_passes_features(p, n, passes_if_looseness);
                    break;
                case balancepasses_key_looseness:
                    tex_set_balance_passes_looseness(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_looseness_okay);
                    break;
                case balancepasses_key_next:
                    n++;
                    break;
                case balancepasses_key_pagebreakchecks:
                    tex_set_balance_passes_pagebreakchecks(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_balancechecks_okay);
                    break;
                case balancepasses_key_pagepenalty:
                    tex_set_balance_passes_pagepenalty(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_balancepenalty_okay);
                    break;
                case balancepasses_key_quit:
                    tex_set_balance_passes_features(p, n, passes_quit_pass);
                    break;
                case balancepasses_key_threshold:
                    tex_set_balance_passes_threshold(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
                    tex_set_balance_passes_features(p, n, passes_criterium_set);
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_threshold_okay);
                    break;
                case balancepasses_key_tolerance:
                    tex_set_balance_passes_tolerance(p, n, tex_scan_integer(0, NULL, NULL));
                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_tolerance_okay);
                    break;
                default:
                    goto DONE;
            }
        }
      DONE:
        tex_check_matchers(balancepasses_matcher);

# else 

        while (n <= count) {
            switch (tex_scan_character("acdefilnpqt", 0, 1, 0)) {
                case 0:
                    goto DONE;
                case 'a':
                    if (tex_scan_mandate_keyword("adjdemerits", 1)) {
                        tex_set_balance_passes_adjdemerits(p, n, tex_scan_integer(0, NULL, NULL));
                        tex_set_balance_passes_okay(p, n, (uint64_t) passes_adjdemerits_okay);
                    }
                    break;
                case 'c':
                    if (tex_scan_mandate_keyword("classes", 1)) {
                        tex_set_balance_passes_classes(p, n, tex_scan_integer(0, NULL, NULL));
                        tex_set_balance_passes_features(p, n, passes_criterium_set);
                        tex_set_balance_passes_okay(p, n, (uint64_t) passes_classes_okay);
                    }
                    break;
                case 'd':
                    if (tex_scan_mandate_keyword("demerits", 1)) {
                        tex_set_balance_passes_demerits(p, n, tex_scan_integer(0, NULL, NULL));
                        tex_set_balance_passes_features(p, n, passes_criterium_set);
                        tex_set_balance_passes_okay(p, n, (uint64_t) passes_demerits_okay);
                    }
                    break;
                case 'e':
                    switch (tex_scan_character("mx", 0, 0, 0)) {
                        case 'm':
                            if (tex_scan_mandate_keyword("emergency", 2)) {
                                switch (tex_scan_character("fps", 0, 0, 0)) {
                                    case 'f':
                                        if (tex_scan_mandate_keyword("emergencyfactor", 10)) {
                                            tex_set_balance_passes_emergencyfactor(p, n, tex_scan_integer(0, NULL, NULL));
                                            tex_set_balance_passes_okay(p, n, (uint64_t) passes_emergencyfactor_okay);
                                        }
                                        break;
                                    case 'p':
                                        if (tex_scan_mandate_keyword("emergencypercentage", 10)) {
                                            tex_set_balance_passes_emergencypercentage(p, n, tex_scan_integer(0, NULL, NULL));
                                            tex_set_balance_passes_okay(p, n, (uint64_t) passes_emergencypercentage_okay);
                                        }
                                        break;
                                    case 's':
                                        switch (tex_scan_character("th", 0, 0, 0)) {
                                            case 't':
                                                if (tex_scan_mandate_keyword("emergencystretch", 11)) {
                                                    tex_set_balance_passes_emergencystretch(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
                                                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_emergencystretch_okay);
                                                }
                                                break;
                                            case 'h':
                                                if (tex_scan_mandate_keyword("emergencyshrink", 11)) {
                                                    tex_set_balance_passes_emergencyshrink(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
                                                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_emergencyshrink_okay);
                                                }
                                                break;
                                            default:
                                                tex_aux_show_keyword_error("emergencystretch|emergencyshrink");
                                                goto DONE;
                                        }
                                        break;
                                    default:
                                        goto NOTDONE4;
                                }
                            } else { 
                             // NOTDONE4:
                             // tex_aux_show_keyword_error("emergencyfactor|emergencystretch|emergencypercentage");
                             // goto DONE;
                                goto NOTDONE4;
                            }
                            break;
                        default:
                            NOTDONE4:
                            tex_aux_show_keyword_error("emergencyfactor|emergencystretch|emergencyshrink|emergencypercentage");
                            goto DONE;
                    }
                    break;
                case 'f':
                    if (tex_scan_mandate_keyword("fitnessclasses", 1)) {
                        tex_set_balance_passes_fitnessclasses(p, n, tex_aux_scan_par_specification(fitness_classes_code, tex_aux_scan_specification_fitness_classes));
                        tex_set_balance_passes_okay(p, n, (uint64_t) passes_fitnessclasses_okay);
                    }
                    break;
                case 'i':
                    switch (tex_scan_character("df", 0, 0, 0)) {
                        case 'd':
                            if (tex_scan_mandate_keyword("identifier", 2)) {
                                passes_identifier(p) = tex_scan_integer(0, NULL, NULL);
                            }
                            break;
                        case 'f':
                            switch (tex_scan_character("el", 0, 0, 0)) {
                                case 'e':
                                    if (tex_scan_mandate_keyword("ifemergency", 3)) {
                                        if (tex_scan_character("s", 0, 0, 0)) {
                                            switch (tex_scan_character("th", 0, 0, 0)) {
                                                case 't':
                                                    if (tex_scan_mandate_keyword("ifemergencystretch", 13)) {
                                                        tex_set_balance_passes_features(p, n, passes_if_emergency_stretch);
                                                    }
                                                    break;
                                                case 'h':
                                                    if (tex_scan_mandate_keyword("ifemergencyshrink", 13)) {
                                                        tex_set_balance_passes_features(p, n, passes_if_emergency_shrink);
                                                    }
                                                    break;
                                                default:
                                                    tex_aux_show_keyword_error("ifemergencystretch|ifemergencyshrink");
                                                    goto DONE;
                                            }
                                        } else {
                                            tex_aux_show_keyword_error("ifemergencystretch|ifemergencyshrink");
                                            goto DONE;
                                        }
                                    }
                                    break;
                                case 'l':
                                    if (tex_scan_mandate_keyword("iflooseness", 3)) {
                                        tex_set_balance_passes_features(p, n, passes_if_looseness);
                                    } 
                                    break;
                                default:
                                    tex_aux_show_keyword_error("if[emergencystretch|emergencyshrink|looseness]");
                                    goto DONE;
                            }
                            break;
                        default:
                            tex_aux_show_keyword_error("identifier|if[...]");
                            goto DONE;
                    }
                    break;
                case 'l':
                    if (tex_scan_mandate_keyword("looseness", 1)) {
                        tex_set_balance_passes_looseness(p, n, tex_scan_integer(0, NULL, NULL));
                        tex_set_balance_passes_okay(p, n, (uint64_t) passes_looseness_okay);
                    } 
                    break;
                case 'n':
                    if (tex_scan_mandate_keyword("next", 1)) {
                        n++;
                    }
                    break;
                case 'p':
                    if (tex_scan_mandate_keyword("page", 1)) {
                        switch (tex_scan_character("bp", 0, 0, 0)) {
                            case 'b':
                                if (tex_scan_mandate_keyword("pagebreakchecks", 5)) {
                                    tex_set_balance_passes_pagebreakchecks(p, n, tex_scan_integer(0, NULL, NULL));
                                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_balancechecks_okay);
                                } 
                                break;
                            case 'p':
                                if (tex_scan_mandate_keyword("pagepenalty", 5)) {
                                    tex_set_balance_passes_pagepenalty(p, n, tex_scan_integer(0, NULL, NULL));
                                    tex_set_balance_passes_okay(p, n, (uint64_t) passes_balancepenalty_okay);
                                } 
                                break;
                            default:
                                tex_aux_show_keyword_error("pagebreakchecks|pagepenalty");
                                goto DONE;
                        }
                    }
                    break;
                case 'q':
                    if (tex_scan_mandate_keyword("quit", 1)) {
                        tex_set_balance_passes_features(p, n, passes_quit_pass);
                    }
                    break;
                case 't':
                    switch (tex_scan_character("ho", 0, 0, 0)) {
                        case 'h':
                            if (tex_scan_mandate_keyword("threshold", 2)) {
                                tex_set_balance_passes_threshold(p, n, tex_scan_dimension(0, 0, 0, 0, NULL, NULL));
                                tex_set_balance_passes_features(p, n, passes_criterium_set);
                                tex_set_balance_passes_okay(p, n, (uint64_t) passes_threshold_okay);
                            }
                            break;
                        case 'o':
                            switch (tex_scan_character("dl", 0, 0, 0)) {
                                case 'l':
                                    if (tex_scan_mandate_keyword("tolerance", 3)) {
                                        tex_set_balance_passes_tolerance(p, n, tex_scan_integer(0, NULL, NULL));
                                        tex_set_balance_passes_okay(p, n, (uint64_t) passes_tolerance_okay);
                                    }
                                    break;
                                default:
                                    goto NOTDONE3;
                            }
                            break;
                        default:
                            NOTDONE3:
                            tex_aux_show_keyword_error("threshold|tolerance");
                            goto DONE;
                    }
                    break;
                default:
                    goto DONE;
            }
        }
      DONE:

# endif

        if lmt_unlikely(n < count) {
            tex_handle_error(
                normal_error_type,
                "there %s only %i of %i %s specified for \\balancepasses",
                n == 1 ? "is" : "are", n, count, count == 1 ? "pass" : "passes"
            );
        }
        {
            halfword first = tex_aux_first_with_balance_criterium(p, count);
            halfword quit = tex_aux_first_with_balance_quit(p, count);
            if (first == 0) { 
                tex_add_specification_option(p, specification_option_presets);
                passes_first_final(p) = count;
            } else if (first == 1) { 
                tex_remove_specification_option(p, specification_option_presets);
                passes_first_final(p) = 2;
            } else { 
                tex_add_specification_option(p, specification_option_presets);
                passes_first_final(p) = first - 1;
            }
            if (quit) { 
                /*tex We always want a result. */
                passes_first_final(p) = quit == 1 ? 1 : quit - 1;
            }
        }
    }
    return p;
}

# if use_matcher

typedef enum balanceshape_keys { 
    balanceshape_key_no_match = 0,
    /* */ 
    balanceshape_key_bottomskip,
    balanceshape_key_identifier,
    balanceshape_key_index,
    balanceshape_key_next,
    balanceshape_key_options,
    balanceshape_key_topskip,
    balanceshape_key_vsize,
    /* */
    balanceshape_key_left_brace,
} balanceshape_keys;

static const char *balanceshape_words[] = { 
    NULL,
    /* */
    "bottomskip",
    "identifier",
    "index",
    "next",
    "options",
    "topskip",
    "vsize",
};

void tex_set_balanceshape_matcher(void)
{
    lmt_matcher_items[balanceshape_matcher] = (matcher_item) { 
        .words   = balanceshape_words, 
        .name    = "balanceshape",
        .first   = balanceshape_key_bottomskip,
        .last    = balanceshape_key_vsize,
        .group   = balanceshape_key_left_brace,
        .options = default_matcher_options, 
    };
}

# endif

static halfword tex_aux_scan_specification_balance_shape(void)
{
    halfword p = null;
    halfword count = tex_scan_integer(1, NULL, NULL);
    if (count > 0) {
        /*tex 
            We have no named options here. Presets are automatically set anyway. We might even drop 
            the option scanning here.
        */
        halfword options = tex_scan_partial_keyword("options") ? tex_scan_integer(0, NULL, NULL) : 0;
        halfword n = 1;
        if (count > 0xFF) {
            /* todo: message */
            count = 0xFF;
        }
        p = tex_new_specification_node(count, balance_shape_code, options);

# if use_matcher

        keyword_matcher *matcher = &lmt_matchers[balanceshape_matcher];
        while (n <= count) {
            switch (tex_matcher_use(matcher, 1)) {
                case balanceshape_key_no_match:
                    goto DONE;
                case balanceshape_key_bottomskip:
                    tex_set_balance_bottomskip(p, n, tex_scan_glue(glue_val_level, 0, 0, NULL));
                    break;
                case balanceshape_key_identifier:
                    balance_shape_identifier(p) = tex_scan_integer(0, NULL, NULL);
                    break;
                case balanceshape_key_index:
                    tex_set_balance_index(p, n, tex_scan_integer(0, NULL, NULL));
                    break;
                case balanceshape_key_next:
                    n++;
                    break;
                case balanceshape_key_options:
                    tex_set_balance_options(p, n, tex_scan_integer(0, NULL, NULL));
                    break;
                case balanceshape_key_topskip:
                    tex_set_balance_topskip(p, n, tex_scan_glue(glue_val_level, 0, 0, NULL));
                    break;
                case balanceshape_key_vsize:
                    tex_set_balance_vsize(p, n, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
                    break;
                default:
                    goto DONE;
            }
        }
      DONE:
        tex_check_matchers(balanceshape_matcher);

# else

        while (n <= count) {
            switch (tex_scan_character("itbonv", 0, 1, 0)) {
                case 0:
                    goto DONE;
                case 'i':
                    switch (tex_scan_character("dn", 0, 0, 0)) {
                        case 'd':
                            if (tex_scan_mandate_keyword("identifier", 2)) {
                                balance_shape_identifier(p) = tex_scan_integer(0, NULL, NULL);
                            }
                            break;
                        case 'n':
                            if (tex_scan_mandate_keyword("index", 2)) {
                                tex_set_balance_index(p, n, tex_scan_integer(0, NULL, NULL));
                            }
                            break;
                        default:
                            tex_aux_show_keyword_error("identifier|index");
                            goto DONE;
                    } 
                    break;
                case 'v':
                    if (tex_scan_mandate_keyword("vsize", 1)) {
                        tex_set_balance_vsize(p, n, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
                    }
                    break;
                case 't':
                    if (tex_scan_mandate_keyword("topskip", 1)) {
                        tex_set_balance_topskip(p, n, tex_scan_glue(glue_val_level, 0, 0, NULL));
                    }
                    break;
                case 'b':
                    if (tex_scan_mandate_keyword("bottomskip", 1)) {
                        tex_set_balance_bottomskip(p, n, tex_scan_glue(glue_val_level, 0, 0, NULL));
                    }
                    break;
                case 'o':
                    if (tex_scan_mandate_keyword("options", 1)) {
                        tex_set_balance_options(p, n, tex_scan_integer(0, NULL, NULL));
                    }
                    break;
                case 'n':
                    if (tex_scan_mandate_keyword("next", 1)) {
                        n++;
                    }
                    break;
                default:
                    goto DONE;
            }
        }
      DONE:

# endif

        if lmt_unlikely(n < count) {
            tex_handle_error(
                normal_error_type,
                "there %s only %i of %i %s specified for \\balanceshape",
                n == 1 ? "is" : "are", n, count, count == 1 ? "page" : "pages"
            );
        }
    }
    return p;
}

static halfword tex_aux_scan_snapper(int factor)
{
    return factor ? tex_scan_integer(0, NULL, NULL) : tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
}

# if use_matcher

typedef enum linesnapping_keys { 
    linesnapping_key_no_match = 0,
    /* */ 
    linesnapping_key_bottom,
    linesnapping_key_depth,
    linesnapping_key_dptolerance,
    linesnapping_key_height,
    linesnapping_key_httolerance,
    linesnapping_key_next,
    linesnapping_key_step,
    linesnapping_key_top,
    /* */
    linesnapping_key_left_brace,
} linesnapping_keys;

static const char *linesnapping_words[] = { 
    NULL,
    /* */
    "bottom",
    "depth",
    "dptolerance",
    "height",
    "httolerance",
    "next",
    "step",
    "top",
};

void tex_set_linesnapping_matcher(void)
{
    lmt_matcher_items[linesnapping_matcher] = (matcher_item) { 
        .words   = linesnapping_words, 
        .name    = "linesnapping",
        .first   = linesnapping_key_bottom,
        .last    = linesnapping_key_top,
        .group   = linesnapping_key_left_brace,
        .options = default_matcher_options, 
    };
}

# endif 

static halfword tex_aux_scan_specification_line_snapping(quarterword code)
{
    halfword p = null;
    halfword count = tex_scan_integer(1, NULL, NULL);
    if (count > 0) {
        /*tex
            We have no named options here. Presets are automatically set anyway. We might even drop
            the option scanning here.
        */
        halfword options = tex_aux_scan_specification_options(code);
        int factors = specification_option_factors(options);
        halfword n = 1;
        p = tex_new_specification_node(count, code, options);

# if use_matcher

        keyword_matcher *matcher = &lmt_matchers[linesnapping_matcher];
        while (n <= count) {
            switch (tex_matcher_use(matcher, 1)) {
                case linesnapping_key_no_match:
                    goto DONE;
                case linesnapping_key_bottom:
                    tex_add_line_snapping_options(p, n, line_snapping_option_bottom);
                    break;
                case linesnapping_key_depth:
                    tex_set_line_snapping_depth(p, n, tex_aux_scan_snapper(factors));
                    break;
                case linesnapping_key_dptolerance:
                    tex_set_line_snapping_dptolerance(p, n, tex_aux_scan_snapper(factors));
                    break;
             // case linesnapping_key_global:
             //     tex_add_line_snapping_options(p, n, line_snapping_option_global);
             //     break;
                case linesnapping_key_height:
                    tex_set_line_snapping_height(p, n, tex_aux_scan_snapper(factors));
                    break;
                case linesnapping_key_httolerance:
                    tex_set_line_snapping_httolerance(p, n, tex_aux_scan_snapper(factors));
                    break;
             // case linesnapping_key_line:
             //     /* reserved, not yet used */
             //     tex_add_line_snapping_options(p, n, line_snapping_option_line);
             //     break;
                case linesnapping_key_next:
                    n++;
                    break;
                case linesnapping_key_step:
                    tex_set_line_snapping_step(p, n, tex_scan_integer(0, NULL, NULL));
                    break;
                case linesnapping_key_top:
                    tex_add_line_snapping_options(p, n, line_snapping_option_top);
                    break;
                default:
                    goto DONE;
            }
        }
      DONE:
        tex_check_matchers(linesnapping_matcher);

# else 

        while (n <= count) {
         // switch (tex_scan_character("hdstbngl", 0, 1, 0)) {
            switch (tex_scan_character("hdstbn", 0, 1, 0)) {
                case 0:
                    goto DONE;
                case 'h':
                    switch (tex_scan_character("et", 0, 0, 0)) {
                        case 'e':
                            if (tex_scan_mandate_keyword("height", 2)) {
                                tex_set_line_snapping_height(p, n, tex_aux_scan_snapper(factors));
                            }
                            break;
                        case 't':
                            if (tex_scan_mandate_keyword("httolerance", 2)) {
                                tex_set_line_snapping_httolerance(p, n, tex_aux_scan_snapper(factors));
                            }
                            break;
                        default:
                            tex_aux_show_keyword_error("height|httolerance");
                            goto DONE;
                    }
                    break;
                case 'd':
                    switch (tex_scan_character("ep", 0, 0, 0)) {
                        case 'e':
                            if (tex_scan_mandate_keyword("depth", 2)) {
                                tex_set_line_snapping_depth(p, n, tex_aux_scan_snapper(factors));
                            }
                            break;
                        case 'p':
                            if (tex_scan_mandate_keyword("dptolerance", 2)) {
                                tex_set_line_snapping_dptolerance(p, n, tex_aux_scan_snapper(factors));
                            }
                            break;
                        default:
                            tex_aux_show_keyword_error("depth|dptolerance");
                            goto DONE;
                    }
                    break;
                case 's':
                    if (tex_scan_mandate_keyword("step", 1)) {
                        tex_set_line_snapping_step(p, n, tex_scan_integer(0, NULL, NULL));
                    }
                    break;
                case 'b':
                    if (tex_scan_mandate_keyword("bottom", 1)) {
                        tex_add_line_snapping_options(p, n, line_snapping_option_bottom);
                    }
                    break;
                case 't':
                    if (tex_scan_mandate_keyword("top", 1)) {
                        tex_add_line_snapping_options(p, n, line_snapping_option_top);
                    }
                    break;
             // case 'g':
             //     if (tex_scan_mandate_keyword("global", 1)) {
             //         tex_add_line_snapping_options(p, n, line_snapping_option_global);
             //     }
             //     break;
             // case 'l':
             //     /* reserved, not yet used */
             //     if (tex_scan_mandate_keyword("line", 1)) {
             //         tex_add_line_snapping_options(p, n, line_snapping_option_line);
             //     }
             //     break;
                case 'n':
                    if (tex_scan_mandate_keyword("next", 1)) {
                        n++;
                    }
                    break;
                default:
                    goto DONE;
            }
        }
      DONE:

# endif 

        if lmt_unlikely(n < count) {
            tex_handle_error(
                normal_error_type,
                "there %s only %i of %i %s specified for \\linesnapspec",
                n == 1 ? "is" : "are", n, count, count == 1 ? "line" : "lines"
            );
        }
    }
    return p;
}

static halfword tex_aux_scan_specification(quarterword code)
{
    switch (code) { 
        case par_shape_code: 
            return tex_aux_scan_specification_par_shape();
        case balance_shape_code: 
            return tex_aux_scan_specification_balance_shape();
        case fitness_classes_code: 
            return tex_aux_scan_specification_fitness_classes();
        case adjacent_demerits_code: 
            return tex_aux_scan_specification_adjacent_demerits();
        case par_passes_code: 
        case par_passes_exception_code: 
            return tex_aux_scan_specification_par_passes();
        case balance_passes_code: 
            return tex_aux_scan_specification_balance_passes();
        case line_snapping_code:
        case math_snapping_code:
        case align_snapping_code:
            return tex_aux_scan_specification_line_snapping(code);
        case text_spacing_code:
            return tex_aux_scan_specification_text_spacing();
        default: 
            return tex_aux_scan_specification_penalties(code);
    }
}

// void tex_aux_set_specification(int a, halfword target)
// {
//     quarterword code = (quarterword) internal_specification_number(target);
//     halfword p = tex_aux_scan_specification(code);
//     tex_define(a, target, specification_reference_cmd, p);
//     if (is_frozen(a) && cur_mode == hmode) {
//         tex_update_par_par(specification_reference_cmd, code);
//     }
// }

static int tex_aux_compatible_specification_codes(quarterword target, quarterword source)
{
    return target == source
        || (target == par_passes_exception_code && source == par_passes_code);
}

void tex_aux_set_specification(int a, halfword target)
{
    quarterword code = (quarterword) internal_specification_number(target);
    halfword spec = null;
    do {
        tex_get_x_token();
    } while (cur_cmd == spacer_cmd);
    switch (cur_cmd) { 
        case specificationspec_cmd: 
            spec = eq_value(cur_cs); 
            if (spec && ! tex_aux_compatible_specification_codes(code, node_subtype(spec))) {
                tex_handle_error(normal_error_type, "incompatible specification type");
                spec = null;
            } else {
                spec = tex_copy_specification_node(spec);
            }
            break;
        default: 
            tex_back_input(cur_tok);
            spec = tex_aux_scan_specification(code);
            break;
    }
    tex_define(a, target, specification_reference_cmd, spec);
    if (is_frozen(a) && cur_mode == hmode) {
        tex_update_par_par(specification_reference_cmd, code);
    }
}

void tex_specification_range_error(halfword target)
{
    tex_handle_error(
        normal_error_type,
        "Specification index should be in the range [1,%i].",
        specification_count(target)
    );
}

void tex_run_specification_spec(void)
{
    if (cur_chr) { 
        quarterword code = node_subtype(cur_chr);
        switch (code) {
            case integer_list_code:
            case dimension_list_code:
            case posit_list_code:
                {
                    halfword target = cur_chr;
                    halfword duplex = specification_double(target);
                    halfword index = tex_scan_integer(0, NULL, NULL);
                    halfword first = 0; /*tex Clang doesn't notice that we have three cases only. */ 
                    halfword second = 0; 
                    switch (code) {
                        case integer_list_code:
                            first = tex_scan_integer(1, NULL, NULL);
                            second = duplex ? tex_scan_integer(0, NULL, NULL) : 0;
                            break;
                        case dimension_list_code:
                            first = specification_integer(target) ? tex_scan_integer(1, NULL, NULL) : tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                            second = duplex ? tex_scan_dimension(0, 0, 0, 0, NULL, NULL) : 0;
                            break;
                        case posit_list_code:
                            first = specification_integer(target) ? tex_scan_integer(0, NULL, NULL) : tex_scan_posit(0);
                            second = duplex ? tex_scan_posit(0) : 0;
                            break;
                    }
                    if (index < 0) {
                        index = specification_count(target) + index + 1;
                    }
                    if (index > specification_count(target) && specification_rotate(target)) {
                        index = (index % specification_count(target));
                        if (index == 0) { 
                            index = specification_count(target);
                        }
                    } 
                    if lmt_likely(index >= 1 && index <= specification_count(target)) {
                        if (duplex) {
                            tex_set_specification_penalty(target, index, second);
                            tex_set_specification_nepalty(target, index, first);
                        } else {
                            tex_set_specification_penalty(target, index, first);
                        }
                    } else {
                        tex_specification_range_error(target);
                    }
                    break;
                }
            default: 
                {
                    halfword target = internal_specification_location(code);
                    halfword a = 0; /* local */
                    halfword p = tex_copy_specification_node(cur_chr);
                    tex_define(a, target, specification_reference_cmd, p);
                    if (is_frozen(a) && cur_mode == hmode) {
                        tex_update_par_par(specification_reference_cmd, code);
                    }
                    break;
                }
        }
    }
}

halfword tex_scan_specifier(void)
{
    do {
        tex_get_x_token();
    } while (cur_cmd == spacer_cmd);
    switch (cur_cmd) { 
        case specificationspec_cmd: 
            {
                halfword spec = eq_value(cur_cs); 
                return spec ? tex_copy_node(spec) : null;
            }
        case specification_cmd:
            {
                quarterword code = (quarterword) internal_specification_number(cur_chr);
                halfword spec = tex_aux_scan_specification(code);
                if (! spec) { 
                    /* We want to be able to reset. */
                    spec = tex_new_specification_node(0, code, 0);
                }
                return spec; 
            }
        case register_cmd:
            switch (cur_chr) { 
                case integer_val_level:
                    return tex_aux_scan_specification_list(integer_list_code);
                case dimension_val_level:
                    return tex_aux_scan_specification_list(dimension_list_code);
                case posit_val_level:
                    return tex_aux_scan_specification_list(posit_list_code);
                default:
                    break;
            }
    }
    tex_handle_error(
        back_error_type,
        "Missing or invalid specification%h",
        "I expect to see classification command like \\widowpenalties."
    );
    return null;
}

void tex_aux_get_specification_value(halfword specification)
{
    quarterword code = node_subtype(specification);
    halfword count = tex_get_specification_count(specification);
    if (count) {
        switch (code) {
            case integer_list_code:
            case dimension_list_code:
            case posit_list_code:
            case balance_final_penalties_code:
            case inter_line_penalties_code:
            case club_penalties_code:
            case widow_penalties_code:
            case display_widow_penalties_code:
            case broken_penalties_code:
            case orphan_penalties_code:
            case toddler_penalties_code:
            case fitness_classes_code:
            case adjacent_demerits_code:
            case orphan_line_factors_code:
            case math_forward_penalties_code:
            case math_backward_penalties_code:
                {
                    /* todo: repeat */
                    halfword index = tex_scan_integer(0, NULL, NULL);
                    if (index == 0) {
                        cur_val = count;
                        cur_val_level = integer_val_level;
                    } else if (index == max_integer) {
                        cur_val = specification_double(specification) ? 1 : 0;
                        cur_val_level = integer_val_level;
                    } else if (code == adjacent_demerits_code) {
                        if (index == -1 || (index == 1 && count == 1)) {
                            cur_val = specification_adjacent_adj(specification);
                        } else { 
                            cur_val = tex_get_specification_adjacent_u(specification, index);
                        }
                        cur_val_level = integer_val_level;
                    } else { 
                        halfword value = 0;
                        if (index < 0) {
                            index = count + index + 1;
                        }
                        if (index > count && specification_rotate(specification)) {
                            index = (index % specification_count(specification));
                            if (index == 0) { 
                                index = specification_count(specification);
                            }
                        } 
                        if (index >= 1 && index <= count) {
                            if (specification_double(specification)) {
                                halfword subindex = tex_scan_integer(0, NULL, NULL);
                                switch (subindex) {
                                    case 1:
                                        value = tex_get_specification_nepalty(specification, index);
                                        if (specification_integer(specification)) {
                                            code = integer_list_code;
                                        }
                                        break;
                                    case 2:
                                        value = tex_get_specification_penalty(specification, index);
                                        break;
                                }
                            } else {
                                value = tex_get_specification_penalty(specification, index);
                            }
                        } else {
                            tex_specification_range_error(specification);
                        }
                        switch (code) {
                            case integer_list_code:
                                cur_val = value;
                                cur_val_level = integer_val_level;
                                break ;
                            case dimension_list_code:
                                cur_val = value;
                                cur_val_level = dimension_val_level;
                                break;
                            case posit_list_code:
                                cur_val = value;
                                cur_val_level = posit_val_level;
                                break;
                            default: 
                                /* the penalties */
                                cur_val = value;
                                cur_val_level = integer_val_level;
                                break ;
                        }
                    }
                }
                break;
            default:
                cur_val = count;
                cur_val_level = integer_val_level;
                break;
        }
    } else {
        cur_val = count;
        cur_val_level = integer_val_level;
    }
}

void tex_aux_get_specification_index(halfword specification, int subindex)
{
    quarterword code = node_subtype(specification);
    halfword count = tex_get_specification_count(specification);
    if (count) {
        switch (code) {
            case par_shape_code:
         /* case unsupported_code: */
            case integer_list_code:
            case dimension_list_code:
            case posit_list_code:
            case balance_final_penalties_code:
            case inter_line_penalties_code:
            case club_penalties_code:
            case widow_penalties_code:
            case display_widow_penalties_code:
            case broken_penalties_code:
            case orphan_penalties_code:
            case toddler_penalties_code:
            case fitness_classes_code:
            case adjacent_demerits_code:
            case orphan_line_factors_code:
            case math_forward_penalties_code:
            case math_backward_penalties_code:
                {
                    /* todo: repeat */
                    halfword index = tex_scan_integer(0, NULL, NULL);
                    if (index == 0 || index == max_integer) {
                        goto BADNEWS;
                    } else { 
                        switch (code) { 
                            case adjacent_demerits_code: 
                                {
                                    if (index == -1 || (index == 1 && count == 1)) {
                                        cur_val = specification_adjacent_adj(specification);
                                    } else { 
                                        cur_val = subindex == 2
                                            ? tex_get_specification_adjacent_d(specification, index)
                                            : tex_get_specification_adjacent_u(specification, index);
                                    }
                                    cur_val_level = integer_val_level;
                                    break;
                                }
                            case par_shape_code:
                                {
                                    /* code = dimension_list_entry */
                                    if (index >= 1 && index <= specification_count(specification)) {
                                        cur_val = subindex == 1 ? tex_get_specification_indent(specification, index) : tex_get_specification_width(specification, index);
                                    } else {
                                        cur_val = 0;
                                    }
                                    cur_val_level = dimension_val_level;
                                    break;
                                } 
                            default: 
                                { 
                                    halfword value = 0;
                                    if (index < 0) {
                                        index = count + index + 1;
                                    }
                                    if (index > count && specification_rotate(specification)) {
                                        index = (index % specification_count(specification));
                                        if (index == 0) { 
                                            index = specification_count(specification);
                                        }
                                    } 
                                    if (index >= 1 && index <= count) {
                                        if (specification_double(specification)) {
                                            switch (subindex) {
                                                case 1:
                                                    value = tex_get_specification_nepalty(specification, index);
                                                    if (specification_integer(specification)) {
                                                        code = integer_list_code;
                                                    }
                                                    break;
                                                case 2:
                                                    value = tex_get_specification_penalty(specification, index);
                                                    break;
                                            }
                                        } else {
                                            value = tex_get_specification_penalty(specification, index);
                                        }
                                    } else {    
                                        goto BADNEWS;
                                    }
                                    switch (code) {
                                        case integer_list_code:
                                            cur_val = value;
                                            cur_val_level = integer_val_level;
                                            break ;
                                        case dimension_list_code:
                                            cur_val = value;
                                            cur_val_level = dimension_val_level;
                                            break;
                                        case posit_list_code:
                                            cur_val = value;
                                            cur_val_level = posit_val_level;
                                            break;
                                        default: 
                                            /* the penalties */
                                            cur_val = value;
                                            cur_val_level = integer_val_level;
                                            break ;
                                    }
                                }
                        }
                    }
                }
                break;
            default:
                goto NONEWS;
        }
    } else {
      BADNEWS:
        tex_specification_range_error(specification);
      NONEWS:
        cur_val = 0;
        cur_val_level = integer_val_level;
    }
}
