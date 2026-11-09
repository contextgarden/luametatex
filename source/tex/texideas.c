/*
    See license.txt in the root of this project.
*/

/* 
    This is an placeholder for upcoming code. It is unlikely to happen in 2026 because I first 
    want to make sure that the generalized scanner works as expected. At the \LUA\ end we can 
    be less \TEX\ compliant but at the \TEX\ end we should do what is expected. 
*/

# include "luametatex.h"

# if 0 

/* package.c */

typedef enum full_spec_keys { 
    full_spec_key_no_match = 0,
    /* */ 
    full_spec_key_adapt,
    full_spec_key_alignsplit,
    full_spec_key_anchor,
    full_spec_key_anchors,
    full_spec_key_attr,
    full_spec_key_axis,
    full_spec_key_class,
    full_spec_key_container,
    full_spec_key_delay,
    full_spec_key_dir,
    full_spec_key_direction,
    full_spec_key_discardable,
    full_spec_key_keepspacing,
    full_spec_key_limit,
    full_spec_key_linesnapping,
    full_spec_key_mathtext,
    full_spec_key_nokerning,
    full_spec_key_noprofiling,
    full_spec_key_nosnapping,
    full_spec_key_orientation,
    full_spec_key_prune,
    full_spec_key_retain,
    full_spec_key_reverse,
    full_spec_key_shift,
    full_spec_key_snapping,
    full_spec_key_source,
    full_spec_key_spread,
    full_spec_key_swap,
    full_spec_key_target,
    full_spec_key_to,
    full_spec_key_xmove,
    full_spec_key_xoffset,
    full_spec_key_ymove,
    full_spec_key_yoffset,
    /* */
    full_spec_key_left_brace,
} full_spec_keys;

static const char *full_spec_words[] = { 
    NULL,
    /* */
    "adapt",
    "alignsplit",
    "anchor",
    "anchors",
    "attr",
    "axis",
    "class",
    "container",
    "delay",
    "dir",
    "direction",
    "discardable",
    "keepspacing",
    "limit",
    "linesnapping",
    "mathtext",
    "nokerning",
    "noprofiling",
    "nosnapping",
    "orientation",
    "prune",
    "retain",
    "reverse",
    "shift",
    "snapping",
    "source",
    "spread",
    "swap",
    "target",
    "to",
    "xmove",
    "xoffset",
    "ymove",
    "yoffset",
};

static const matcher_item private_idea_matcher_item = { 
    .words   = full_spec_words, 
    .first   = full_spec_key_adapt, 
    .last    = full_spec_key_yoffset,
    .options = 0,
};

static void tex_aux_scan_full_spec(
    halfword    context, 
    quarterword c, 
    quarterword spec_direction, 
    int         just_pack, 
    scaled      shift, 
    halfword    slot, 
    halfword    callback, 
    halfword    leaders
)
{
    quarterword spec_packing = packing_additional;
    int spec_amount = 0;
    halfword attrlist = null;
    halfword orientation = 0;
    scaled   xoffset = 0;
    scaled   yoffset = 0;
    scaled   xmove = 0;
    scaled   ymove = 0;
    halfword source = 0;
    halfword target = 0;
    halfword anchor = 0;
    halfword geometry = 0;
    halfword axis = 0;
    halfword state = 0;
    halfword retain = 0;
    halfword mainclass = unset_noad_class;
    halfword options = 0;
    halfword snapping = null;
    int brace = 0;
    keyword_matcher *matcher = lmt_matchers[full_spec_matcher];
    while (1) {
        /*tex Maybe |migrate <int>| makes sense here. */
        switch (tex_matcher_use(matcher, 0, full_spec_key_left_brace)) {
            case 0:
                goto DONE;
            case full_spec_key_target:
                target = tex_scan_integer(1, NULL, NULL);
                break;
            case full_spec_key_to:
                spec_packing = packing_exactly;
                spec_amount = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                break;
            case full_spec_key_adapt:
                spec_packing = packing_adapted;
                spec_amount = tex_scan_limited_scale(0);
                break;
            case full_spec_key_attr:
                attrlist = tex_scan_attribute(attrlist);
                break;
            case full_spec_key_anchor:
                anchor = tex_scan_anchor(0);
                break;
            case full_spec_key_anchors:
                anchor = tex_scan_anchors(0);
                break;
            case full_spec_key_alignsplit:
                options |= saved_box_align_split_option;
                break;
            case full_spec_key_axis:
                axis = tex_scan_box_axis();
                break;
            case full_spec_key_shift:
                /*tex
                    This is a bonus because we decoupled the shift amount from the context,
                    where it can be somewhat confusing as that is a hybrid amount, kind, or
                    flag field. The keyword overloads an already given |move_cmd|.
                */
                shift = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
            case full_spec_key_spread:
                spec_packing = packing_additional;
                spec_amount = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                break;
            case full_spec_key_source:
                source = tex_scan_integer(1, NULL, NULL);
                break;
            case full_spec_key_swap:
                options |= saved_box_swap_htdp_option;
                break;
            case full_spec_key_snapping:
                options |= saved_box_snapping_option;
                break;
            case full_spec_key_direction:
                spec_direction = tex_scan_direction(0);
                break;
            case full_spec_key_discardable:
                options |= saved_box_discardable_option;
                break;
            case full_spec_key_direction:
                /*tex
                    For the sake of third party code that assumes \LUATEX's old directives
                    we can support |dir TRT| and |dir TLT|; only these. But \unknown\ that
                    would also mean we have to deal with |\textdir| and such. This will be
                    dropped at some point.
                */
                if (tex_scan_character("t", 0, 1, 0)) {
                    switch (tex_scan_character("lr", 0, 0, 0)) {
                        case 'l':
                            spec_direction = 0;
                            break;
                        case 'r':
                            spec_direction = 1;
                            break;
                        default:
                            goto BADDIR;
                    }
                    if (! tex_scan_character("t", 0, 0, 0)) {
                        goto BADDIR;
                    }
                } else {
                  BADDIR:
                    tex_aux_show_keyword_error("tlt|trt");
                    goto DONE;
                }
                break;
            case full_spec_key_delay:
                state |= package_u_leader_delayed;
                break;
            case full_spec_key_orientation:
                orientation = tex_scan_orientation(0);
                break;
            case full_spec_key_xoffset:
                xoffset = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                break;
            case full_spec_key_xmove:
                xmove = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                break;
            case full_spec_key_yoffset:
                yoffset = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
            case full_spec_key_ymove:
                ymove = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                break;
            case full_spec_key_reverse:
                options |= saved_box_reverse_option;
                break;
            case full_spec_key_retain:
                retain = tex_scan_integer(0, NULL, NULL);
                break;
            case full_spec_key_container:
                options |= saved_box_container_option;
                break;
            case full_spec_key_class:
                mainclass = tex_scan_math_class_number(0);
                break;
            case full_spec_key_keepspacing:
                options |= saved_box_keep_spacing_option;
                break;
            case full_spec_key_prune:
                options |= saved_box_prune_option;
                break;
            case full_spec_key_linesnapping:
                snapping = tex_snapping_scan();
                break;
            case full_spec_key_limit:
                options |= saved_box_limit_option;
                break;
            case full_spec_key_mathtext:
                options |= saved_box_mathtext_option;
                break;
            case full_spec_key_nokerning:
                options |= saved_box_no_kerning_option;
                break;
            case full_spec_key_nosnapping:
                options |= saved_box_no_snapping_option;
                break;
            case full_spec_key_noprofiling:
                options |= saved_box_no_profiling_option;
                break;
            case full_spec_key_left_brace:
                brace = 1;
                goto DONE;
            default:
                goto DONE;
        }
    }
  DONE:
    if (anchor || source || target) {
        geometry |= anchor_geometry;
    }
    if (orientation || xmove || ymove) {
        geometry |= orientation_geometry;
    }
    if (xoffset || yoffset) {
        geometry |= offset_geometry;
    }
    /*tex
        We either build one triggered by the |attr| key or we never set it in which case we use the
        default. As we will use it anyway, we also bump the reference, which also makes sure that
        it will stay.
    */
    if (! attrlist) {
        /* this alse sets the reference when not yet set */
        attrlist = tex_current_attribute_list();
    }
    /*tex Now we're referenced. We need to preserve this over the group. */
    add_attribute_reference(attrlist);
    /* */
    saved_box_initialize();
    saved_box_slot = slot;
    saved_box_context = context;
    /*tex Traditionally these two are packed into one record: */
    saved_box_packing = spec_packing;
    saved_box_amount = spec_amount;
    /*tex Adjust |text_dir_ptr| for |scan_spec|: */
    saved_box_direction = spec_direction;
    if (spec_direction != direction_unknown) {
        saved_box_dir_pointer = lmt_dir_state.text_dir_ptr;
        lmt_dir_state.text_dir_ptr = tex_new_dir(normal_dir_subtype, spec_direction);
    } else {
        saved_box_dir_pointer = null;
    }
    saved_box_attr_list = attrlist;
    saved_box_pack = just_pack;
    saved_box_orientation = orientation;
    saved_box_anchor = anchor;
    saved_box_geometry = geometry;
    saved_box_xoffset = xoffset;
    saved_box_yoffset = yoffset;
    saved_box_xmove = xmove;
    saved_box_ymove = ymove;
    saved_box_source = source;
    saved_box_target = target;
    saved_box_state = state;
    saved_box_options = options;
    saved_box_shift = shift;
    saved_box_axis = axis;
    saved_box_class = mainclass;
    saved_box_retain = retain;
    saved_box_callback = callback;
    saved_box_leaders = leaders;
    /* isn't this already a copy: needs checking */
 // saved_box_snapping = tex_copy_specification_node(snapping);
    saved_box_snapping = snapping;
    lmt_save_state.save_stack_data.ptr += saved_box_n_of_records;
    tex_new_save_level(c);
    if (! brace) {
        tex_scan_left_brace();
    }
    update_tex_par_direction(spec_direction);
    update_tex_text_direction(spec_direction);
}

# endif

typedef enum private_idea_keys { 
    private_idea_key_no_match = 0,
    /* */ 
    private_idea_key_width,
    private_idea_key_height,
    private_idea_key_depth,
    /* */
    private_idea_key_left_brace,
} private_idea_keys;

static const char *private_idea_words[] = { 
    NULL,
    /* */
    "width",
    "height",
    "depth",
};

static const matcher_item private_idea_matcher_item = { 
    .words   = private_idea_words, 
    .first   = private_idea_key_width, 
    .last    = private_idea_key_depth,
    .options = keyword_exact_option | keyword_terminal_option | keyword_ascii_option, 
};

void tex_show_idea(void)
{
 // int brace = 0;
    keyword_matcher *matcher = &lmt_matchers[private_idea_matcher];
    while (1) {
     // switch (tex_matcher_use(matcher, 1, private_idea_key_left_brace)) {
        switch (tex_matcher_use(matcher, 1, 0)) {
            case 0:
                goto DONE;
            case private_idea_key_width:
                scaled width = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                tex_print_format("%<width : %p%>",width);
                break;
            case private_idea_key_height:
                scaled height = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                tex_print_format("%<height : %p%>", height);
                break;
            case private_idea_key_depth:
                scaled depth = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                tex_print_format("%<depth : %p%>",depth);
                break;
            case private_idea_key_left_brace:
             // brace = 1;
                goto DONE;
            default:
                goto DONE;
        }
    }
  DONE:;
    tex_report_matchers();
}

void tex_initialize_ideas(void)
{
    lmt_matcher_items[private_idea_matcher] = private_idea_matcher_item;
}

void tex_cleanup_ideas(void)
{
}
