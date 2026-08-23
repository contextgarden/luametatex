/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex 

    We have a a limited set of codes (normal, empty, strut and virtual) but at the \LUA\ end one
    can have more subtypes, like outline or image. The four common ones have the same numbers but 
    the codes are (as usual) used in the primitive commands. 

    In theory we could win some on also supporting shorter keywords but in the current setup there 
    is little gain (e.g. 50K times nothing 0.004, one 0.007, two 0.010 and three 0.013, and in 
    practice scanning the dimension takes much of that). 

    \starttabulate[|Tl|Tl|]
    \NC wd  \NC dimen \NC \NR
    \NC ht  \NC dimen \NC \NR 
    \NC dp  \NC dimen \NC \NR
    \NC hd  \NC dimen dimen \NC \NR
    \NC whd \NC dimen dimen dimen \NC \NR
    \stoptabulate 

    Virtual rules don't accept |left|, |right|,  |top|, |bottom|, |char|, |font| and |fam| keys
    because they use these fields for other purposes. This has changed.

    The default nullflags also serve as a signal, for instance un strut rules combined with char 
    keys. Because a strut is meant for vertical purposes we set the width to zero. 

    We used to share slots in the node with ameanign dependent on the subtype but in the end it
    mess up the code so now we just have then seperated. Keep in mind that not all subtypes use
    all fields. Some fields gets set by the engine, some are part of the rendering logic. We still
    share left and right with top and bottom.

*/

halfword tex_aux_scan_rule_spec(rule_types type, halfword code, int maysnap)
{
    /*tex |width|, |depth|, and |height| all equal |null_flag| now */
    halfword rule     = tex_new_rule_node((quarterword) code); /* here code == subtype */
    halfword attrlist = null;
    halfword snapping = null;
    if (code == strut_rule_code) {
        rule_width(rule) = 0;
        switch (type) {
            case h_rule_type:
                rule_options(rule) |= rule_option_horizontal;
                break;
            case v_rule_type:
                rule_options(rule) |= rule_option_vertical;
                break;
            default:
                break;
        }
        rule_options(rule) |= rule_option_keep_spacing;
    } else {
        switch (type) {
            case h_rule_type:
                rule_height(rule) = default_rule;
                rule_depth(rule) = 0;
                rule_options(rule) |= rule_option_horizontal;
                break;
            case v_rule_type:
                rule_options(rule) |= rule_option_vertical;
                FALLTHROUGH
            case m_rule_type:
                rule_width(rule) = default_rule;
                break;
        }
    }
    while (1) {
        /* maybe also three in one go but what keyword */
        switch (tex_scan_character("awhdkpxylrtbcfons", 0, 1, 0)) {
            case 0:
                goto DONE;
            case 'a':
                if (tex_scan_mandate_keyword("attr", 1)) {
                    attrlist = tex_scan_attribute(attrlist);
                }
                break;
            case 'w':
                if (tex_scan_mandate_keyword("width", 1)) {
                    rule_width(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                }
                break;
            case 'h':
                if (tex_scan_mandate_keyword("height", 1)) {
                    rule_height(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                }
                break;
            case 'n':
                if (tex_scan_mandate_keyword("nosnapping", 1)) {
                    rule_options(rule) |= rule_option_no_snapping;
                    maysnap = 0; /* so no need to test the option later */
                }
                break;
            case 's':
                if (tex_scan_mandate_keyword("snapping", 1)) {
                    rule_options(rule) |= rule_option_snapping;
                }
                break;
            case 'd':
                switch (tex_scan_character("ei", 0, 0, 0)) {
                    case 'e':
                        if (tex_scan_mandate_keyword("depth", 2)) {
                            rule_depth(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                        }
                        break;
                    case 'i':
                        if (tex_scan_mandate_keyword("discardable", 2)) {
                            rule_options(rule) |= rule_option_discardable;
                        }
                        break;
                    default:
                        tex_aux_show_keyword_error("depth|discardable");
                        goto DONE;
                }
                break;
            case 'p':
                if (tex_scan_mandate_keyword("pair", 1)) {
                    rule_height(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                    rule_depth(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                }
                break;
            case 'l':
                switch (tex_scan_character("ei", 0, 0, 0)) {
                    case 'i':
                        if (tex_scan_mandate_keyword("linesnapping", 2)) {
                            snapping = tex_snapping_scan();
                        }
                        break;
                    case 'e':
                        if (tex_scan_mandate_keyword("left", 2)) {
                            rule_left(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                        }
                        break;
                    default:
                        tex_aux_show_keyword_error("left|linesnapping");
                        goto DONE;
                }
                break;
            case 'r':
                switch (tex_scan_character("uie", 0, 0, 0)) {
                    case 'u':
                        if (tex_scan_mandate_keyword("running", 2)) {
                            rule_width (rule) = null_flag;
                            rule_height(rule) = null_flag;
                            rule_depth (rule) = null_flag;
                        }
                        break;
                    case 'i':
                        if (tex_scan_mandate_keyword("right", 2)) {
                            rule_right(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                        }
                        break;
                    case 'e':
                        if (tex_scan_mandate_keyword("resetspacing", 2)) {
                            tex_remove_rule_option(rule, rule_option_keep_spacing);
                        }
                        break;
                    default:
                        tex_aux_show_keyword_error("right|running|resetspacing");
                        goto DONE;
                }
                break;
            case 't': /* just because it's nicer */
                if (tex_scan_mandate_keyword("top", 1)) {
                    rule_left(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                }
                break;
            case 'b': /* just because it's nicer */
                if (tex_scan_mandate_keyword("bottom", 1)) {
                    rule_right(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                }
                break;
            case 'x':
                if (tex_scan_mandate_keyword("xoffset", 1)) {
                    rule_x_offset(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                }
                break;
            case 'y':
                if (tex_scan_mandate_keyword("yoffset", 1)) {
                    rule_y_offset(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                }
                break;
            case 'k':
                if (tex_scan_mandate_keyword("keepspacing", 1)) {
                    rule_options(rule) |= rule_option_keep_spacing;
                }
                break;
            case 'f':
                switch (tex_scan_character("ao", 0, 0, 0)) {
                    case 'o':
                        if (tex_scan_mandate_keyword("font", 2)) {
                            tex_set_rule_font(rule, tex_scan_font_identifier(NULL));
                        }
                        break;
                    case 'a':
                        if (tex_scan_mandate_keyword("fam", 2)) {
                            tex_set_rule_family(rule, tex_scan_math_family_number());
                        }
                        break;
                    default:
                        tex_aux_show_keyword_error("font|fam");
                        goto DONE;
                }
                break;
            case 'o':
                switch (tex_scan_character("nf", 0, 0, 0)) {
                    case 'n':
                        rule_line_on(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                        break;
                    case 'f':
                        if (tex_scan_mandate_keyword("off", 2)) {
                            rule_line_off(rule) = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                        }
                        break;
                    default:
                        tex_aux_show_keyword_error("on|off");
                        goto DONE;
                }
                break;
            case 'c':
                if (tex_scan_mandate_keyword("char", 1)) {
                    rule_strut_character(rule) = tex_scan_char_number(0);
                }
                break;
            default:
                goto DONE;
        }
    }
  DONE:
    if (attrlist) {
        tex_attach_attribute_list_attribute(rule, attrlist);
    }
    switch (code) {
        case strut_rule_code:
            if (type == v_rule_type) {
                tex_aux_check_text_strut_rule(rule, text_style);
            }
            break;
        case virtual_rule_code:
            rule_virtual_width(rule) = rule_width(rule);
            rule_virtual_height(rule) = rule_height(rule);
            rule_virtual_depth(rule) = rule_depth(rule);
            rule_width(rule) = 0;
            rule_height(rule) = 0;
            rule_depth(rule) = 0;
            break;
    }
    if (type == h_rule_type) {
        if (rule_width(rule) == null_flag) {
            rule_options(rule) |= rule_option_running;
        }
    } else {
        if (rule_height(rule) == null_flag && rule_width(rule) == null_flag) {
            rule_options(rule) |= rule_option_running;
        }
    }
    /* snapping */
    if (snapping) {
        if (maysnap && type == h_rule_type) {
            scaled oldht = rule_height(rule) == null_flag ? 0 : rule_height(rule);
            scaled olddp = rule_depth (rule) == null_flag ? 0 : rule_depth (rule);
            scaled newht = oldht;
            scaled newdp = olddp;
            tex_snapping_done(&newht, &newdp, snapping);
            newht -= oldht;
            newdp -= olddp;
            if (newht) {
                halfword kern = tex_new_kern_node(newht, line_snapping_kern_subtype);
                tex_attach_attribute_list_copy(kern, rule);
                tex_tail_append(kern);
                rule_snapping(rule) = newht;
            }
            if (newdp) {
                halfword kern = tex_new_kern_node(newdp, line_snapping_kern_subtype);
                tex_attach_attribute_list_copy(kern, rule);
                tex_tail_append(rule);
                rule = kern;
            }
        }
        tex_flush_specification_node(snapping);
    }
    return rule;
}

void tex_aux_run_vrule(void)
{
    tex_tail_append(tex_aux_scan_rule_spec(v_rule_type, cur_chr, 0));
    if (! (rule_options(cur_list.tail) & rule_option_keep_spacing)) {
        cur_list.space_factor = default_space_factor;
    }
}

void tex_aux_run_hrule(void)
{
# if (delayed_glue_supported == 1)
    tex_delayed_glue_check(delayed_glue_target_current, delayed_glue_location_rule);
# endif
    tex_tail_append(tex_aux_scan_rule_spec(h_rule_type, cur_chr, 1));
    cur_list.prev_depth = ignore_depth_criterion_par;
}

void tex_aux_run_mrule(void)
{
    tex_tail_append(tex_aux_scan_rule_spec(m_rule_type, cur_chr, 0));
}

void tex_aux_check_math_strut_rule(halfword rule, halfword style)
{
    if (node_subtype(rule) == strut_rule_subtype) {
        scaled ht = rule_height(rule);
        scaled dp = rule_depth(rule);
        if (ht == null_flag || dp == null_flag) {
            halfword fnt = tex_get_rule_font(rule, style);
            halfword chr = rule_strut_character(rule);
            if (fnt > 0 && chr && tex_char_exists(fnt, chr)) {
                if (ht == null_flag) {
                    ht = tex_math_font_char_ht(fnt, chr, style);
                }
                if (dp == null_flag) {
                    dp = tex_math_font_char_dp(fnt, chr, style);
                }
            } else {
                if (ht == null_flag) {
                    ht = tex_get_math_y_parameter(style, math_parameter_rule_height);
                }
                if (dp == null_flag) {
                    dp = tex_get_math_y_parameter(style, math_parameter_rule_depth);
                }
            }
            rule_height(rule) = ht;
            rule_depth(rule) = dp;
        }
    }
}

void tex_aux_check_text_strut_rule(halfword rule, halfword style)
{
    if (node_subtype(rule) == strut_rule_subtype) {
        scaled ht = rule_height(rule);
        scaled dp = rule_depth(rule);
        if (ht == null_flag || dp == null_flag) {
            halfword fnt = tex_get_rule_font(rule, style);
            halfword chr = rule_strut_character(rule);
            if (fnt > 0 && chr && tex_char_exists(fnt, chr)) {
                scaledwhd whd = tex_char_whd_from_font(fnt, chr);
                if (ht == null_flag) {
                    rule_height(rule) = whd.ht;
                }
                if (dp == null_flag) {
                    rule_depth(rule) = whd.dp;
                }
            }
        }
    }
}

halfword tex_get_rule_font(halfword n, halfword style)
{
    halfword fnt = rule_strut_font(n);
    if (fnt >= rule_font_fam_offset) {
        halfword fam = fnt - rule_font_fam_offset;
        if (fam_par_in_range(fam)) {
            fnt = tex_fam_fnt(fam, tex_size_of_style(style));
        }
    }
    if (fnt < 0 || fnt >= max_n_of_fonts) {
        return null_font;
    } else {
        return fnt;
    }
}

halfword tex_get_rule_family(halfword n)
{
    halfword fnt = rule_strut_font(n);
    if (fnt >= rule_font_fam_offset) {
        halfword fam = fnt - rule_font_fam_offset;
        if (fam_par_in_range(fam)) {
            return fam;
        }
    }
    return 0;
}

void tex_set_rule_font(halfword n, halfword fnt)
{
    if (fnt < 0 || fnt >= rule_font_fam_offset) {
        rule_strut_font(n) = 0;
    } else {
        rule_strut_font(n) = fnt;
    }
}

void tex_set_rule_family(halfword n, halfword fam)
{
    if (fam < 0 || fam >= max_n_of_math_families) {
        rule_strut_font(n) = rule_font_fam_offset;
    } else {
        rule_strut_font(n) = rule_font_fam_offset + fam;
    }
}

halfword tex_get_rule_left(halfword n)
{
    return rule_left(n);
}

halfword tex_get_rule_right(halfword n)
{
    return rule_right(n);
}

void tex_set_rule_left(halfword n, halfword value)
{
    rule_left(n) = value;
}

void tex_set_rule_right(halfword n, halfword value)
{
    rule_right(n) = value;
}

halfword tex_get_rule_on(halfword n)
{
    return rule_line_on(n);
}

halfword tex_get_rule_off(halfword n)
{
    return rule_line_off(n);
}

void tex_set_rule_on(halfword n, halfword value)
{
    rule_line_on(n) = value;
}

void tex_set_rule_off(halfword n, halfword value)
{
    rule_line_off(n) = value;
}
