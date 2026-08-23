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
