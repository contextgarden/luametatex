/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/* todo: 

    discretionaries => remove or adapt 
    demerits        => within set (first of pageshape entry) 
    discardables 
    why copy needed
    depth 
    temphead etc 
    hyphenation     => discretionaries     
    overfull        => skip to next 
*/

typedef enum balance_states {
    balance_no_pass,
    balance_first_pass,
    balance_second_pass,
    balance_final_pass,
    balance_specification_pass,
} balance_states;

balance_state_info lmt_balance_state = {
    .just_box            = 0,
    .no_shrink_error_yet = 0,
    .callback_id         = 0,
    .threshold           = 0,
    .passive             = 0,
    .printed_node        = 0,
    .serial_number       = 0,
    .active_height       = { 0 },
    .background          = { 0 },
    .break_height        = { 0 },
    .disc_height         = { 0 },
    .minimal_demerits    = { 0 },
    .minimum_demerits    = 0,
    .easy_page           = 0,
    .last_special_page   = 0,
    .target_height       = 0,
    .best_bet            = 0,
    .fewest_demerits     = 0,
    .best_page           = 0,
    .actual_looseness    = 0,
    .fill_height         = { 0 },
    .warned              = 0,
    .passes              = { 0 },
    .current_page_number = 0,

    .quality                  = 0,
    .force_check_hyphenation  = 0,
    .extra_background_stretch = 0,
    .last_special_page        = 0,
    .emergency_amount         = 0,
    .emergency_percentage     = 0,
    .emergency_factor         = 0,
    .emergency_height_amount  = 0,
    .artificial_encountered   = 0, 

};

typedef enum fill_orders {
    fi_order    = 0,
    fil_order   = 1,
    fill_order  = 2,
    filll_order = 3,
} fill_orders;

static void tex_aux_pre_balance (
    const balance_properties *properties,
    int                       callback_id,
    halfword                  checks,
    int                       state
);

static void tex_aux_post_balance (
    const balance_properties *properties,
    int                       callback_id,
    halfword                  checks,
    int                       state
);

/* */

static scaled tex_aux_checked_shrink(halfword p)
{
    if (glue_shrink(p) && glue_shrink_order(p) != normal_glue_order) {
        if (lmt_balance_state.no_shrink_error_yet) {
            lmt_balance_state.no_shrink_error_yet = 0;
            tex_handle_error(
                normal_error_type,
                "Infinite glue shrinkage found in a page",
                "The page just ended includes some glue that has infinite shrinkability.\n"
            );
        }
        glue_shrink_order(p) = normal_glue_order;
    }
    return glue_shrink(p);
}

static void tex_aux_clean_up_the_memory(void)
{
    halfword q = node_next(active_head);
    while (q != active_head) {
        halfword p = node_next(q);
        tex_flush_node(q);
        q = p;
    }
    q = lmt_balance_state.passive;
    while (q) {
        halfword p = node_next(q);
        tex_flush_node(q);
        q = p;
    }
}

static inline void tex_aux_add_disc_source_to_target(scaled target[], const scaled source[])
{
    target[total_advance_amount] += source[total_advance_amount];
}

static inline void tex_aux_sub_disc_target_from_source(scaled target[], const scaled source[])
{
    target[total_advance_amount] -= source[total_advance_amount];
}

static inline void tex_aux_reset_disc_target(scaled *target)
{
    target[total_advance_amount] = 0;
}

static inline void tex_aux_set_target_to_source(scaled target[], const scaled source[])
{
    for (int i = total_advance_amount; i <= total_shrink_amount; i++) {
        target[i] = source[i];
    }
}

static inline void tex_aux_add_to_target_from_delta(scaled target[], halfword delta)
{
    target[total_advance_amount] += delta_field_total_glue(delta);
    target[total_stretch_amount] += delta_field_total_stretch(delta);
    target[total_fi_amount]      += delta_field_total_fi_amount(delta);
    target[total_fil_amount]     += delta_field_total_fil_amount(delta);
    target[total_fill_amount]    += delta_field_total_fill_amount(delta);
    target[total_filll_amount]   += delta_field_total_filll_amount(delta);
    target[total_shrink_amount]  += delta_field_total_shrink(delta);
}

static inline void tex_aux_sub_delta_from_target(scaled target[], halfword delta)
{
    target[total_advance_amount] -= delta_field_total_glue(delta);
    target[total_stretch_amount] -= delta_field_total_stretch(delta);
    target[total_fi_amount]      -= delta_field_total_fi_amount(delta);
    target[total_fil_amount]     -= delta_field_total_fil_amount(delta);
    target[total_fill_amount]    -= delta_field_total_fill_amount(delta);
    target[total_filll_amount]   -= delta_field_total_filll_amount(delta);
    target[total_shrink_amount]  -= delta_field_total_shrink(delta);
}

static inline void tex_aux_add_to_delta_from_delta(halfword target, halfword source)
{
    delta_field_total_glue(target)         += delta_field_total_glue(source);
    delta_field_total_stretch(target)      += delta_field_total_stretch(source);
    delta_field_total_fi_amount(target)    += delta_field_total_fi_amount(source);
    delta_field_total_fil_amount(target)   += delta_field_total_fil_amount(source);
    delta_field_total_fill_amount(target)  += delta_field_total_fill_amount(source);
    delta_field_total_filll_amount(target) += delta_field_total_filll_amount(source);
    delta_field_total_shrink(target)       += delta_field_total_shrink(source);
}

static inline void tex_aux_set_delta_from_difference(halfword delta, const scaled source_1[], const scaled source_2[])
{
    delta_field_total_glue(delta)         = (source_1[total_advance_amount] - source_2[total_advance_amount]);
    delta_field_total_stretch(delta)      = (source_1[total_stretch_amount] - source_2[total_stretch_amount]);
    delta_field_total_fi_amount(delta)    = (source_1[total_fi_amount]      - source_2[total_fi_amount]);
    delta_field_total_fil_amount(delta)   = (source_1[total_fil_amount]     - source_2[total_fil_amount]);
    delta_field_total_fill_amount(delta)  = (source_1[total_fill_amount]    - source_2[total_fill_amount]);
    delta_field_total_filll_amount(delta) = (source_1[total_filll_amount]   - source_2[total_filll_amount]);
    delta_field_total_shrink(delta)       = (source_1[total_shrink_amount]  - source_2[total_shrink_amount]);
}

static inline void tex_aux_add_delta_from_difference(halfword delta, const scaled source_1[], const scaled source_2[])
{
    delta_field_total_glue(delta)         += (source_1[total_advance_amount] - source_2[total_advance_amount]);
    delta_field_total_stretch(delta)      += (source_1[total_stretch_amount] - source_2[total_stretch_amount]);
    delta_field_total_fi_amount(delta)    += (source_1[total_fi_amount]      - source_2[total_fi_amount]);
    delta_field_total_fil_amount(delta)   += (source_1[total_fil_amount]     - source_2[total_fil_amount]);
    delta_field_total_fill_amount(delta)  += (source_1[total_fill_amount]    - source_2[total_fill_amount]);
    delta_field_total_filll_amount(delta) += (source_1[total_filll_amount]   - source_2[total_filll_amount]);
    delta_field_total_shrink(delta)       += (source_1[total_shrink_amount]  - source_2[total_shrink_amount]);
}

static void tex_aux_add_to_heights(halfword s, scaled heights[])
{
    while (s) {
        switch (node_type(s)) {
            case hlist_node:
            case vlist_node:
                heights[total_advance_amount] += box_height(s);
                heights[total_advance_amount] += box_depth(s);
                break;
            case rule_node:
                heights[total_advance_amount] += rule_height(s);
                heights[total_advance_amount] += rule_depth(s);
                break;
            case glue_node:
                heights[total_advance_amount] += glue_amount(s);
                heights[total_stretch_amount + glue_stretch_order(s)] += glue_stretch(s);
                heights[total_shrink_amount] += glue_shrink(s);
                break;
            case kern_node:
                heights[total_advance_amount] += kern_amount(s);
                break;
         // case disc_node:
         //     break;
            default:
                break;
        }
        s = node_next(s);
    }
}

static void tex_aux_sub_from_heights(halfword s, scaled heights[])
{
    while (s) {
        switch (node_type(s)) {
            case hlist_node:
            case vlist_node:
                heights[total_advance_amount] -= box_height(s);
                heights[total_advance_amount] -= box_depth(s);
                break;
            case rule_node:
                heights[total_advance_amount] -= rule_height(s);
                heights[total_advance_amount] -= rule_depth(s);
                break;
            case glue_node:
                heights[total_advance_amount] -= glue_amount(s);
                heights[total_stretch_amount + glue_stretch_order(s)] -= glue_stretch(s);
                heights[total_shrink_amount] -= glue_shrink(s);
                break;
            case kern_node:
                heights[total_advance_amount] -= kern_amount(s);
                break;
         // case disc_node:
         //     break;
            default:
                break;
        }
        s = node_next(s);
    }
}

static void tex_aux_compute_break_height(int break_type, halfword p)
{
    halfword s = p;
    if (p) {
        switch (break_type) {
            case hyphenated_node:
            case delta_node:
            case passive_node:
                tex_aux_sub_from_heights(disc_no_break_head(p), lmt_balance_state.break_height);
                tex_aux_add_to_heights(disc_post_break_head(p), lmt_balance_state.break_height);
                tex_aux_add_disc_source_to_target(lmt_balance_state.break_height, lmt_balance_state.disc_height);
                if (disc_post_break_head(p)) {
                    s = null;
                } else {
                    /*tex no |post_break|: skip any whitespace following */
                    s = node_next(p);
                }
                break;
        }
    }
    while (s) {
        switch (node_type(s)) {
            case glue_node:
                lmt_balance_state.break_height[total_advance_amount] -= glue_amount(s);
                lmt_balance_state.break_height[total_stretch_amount + glue_stretch_order(s)] -= glue_stretch(s);
                lmt_balance_state.break_height[total_shrink_amount] -= glue_shrink(s);
                break;
            case penalty_node:
                break;
            case kern_node:
                if (node_subtype(s) == explicit_kern_subtype) {
                    lmt_balance_state.break_height[total_advance_amount] -= kern_amount(s);
                    break;
                } else {
                    return;
                }
            default:
                return;
        };
        s = node_next(s);
    }
}

/*tex 
    For now we use the same context values.  
*/

static void tex_aux_balance_callback_initialize(int callback_id, halfword checks, int subpasses)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "ddd->",
        initialize_line_break_context,
        checks,
        subpasses
    );
}

static void tex_aux_balance_callback_start(int callback_id, halfword checks, int pass, int subpass, int classes, int decent)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dddddd->",
        start_line_break_context,
        checks,
        pass,
        subpass,
        classes,
        decent
    );
}

static void tex_aux_balance_callback_stop(int callback_id, halfword checks)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "ddd->",
        stop_line_break_context,
        checks,
        lmt_balance_state.fewest_demerits
    );
}

static void tex_aux_balance_callback_collect(int callback_id, halfword checks)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dd->",
        collect_line_break_context,
        checks
    );
}

static void tex_aux_balance_callback_page(int callback_id, halfword checks, int line, halfword passive)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "ddNdddddd->",
        line_line_break_context,
        checks,
        lmt_balance_state.just_box,
        lmt_packaging_state.last_badness,
        lmt_packaging_state.last_overshoot,
        lmt_packaging_state.total_shrink[normal_glue_order],
        lmt_packaging_state.total_stretch[normal_glue_order],
        line,
        passive_serial(passive)
    );
}

static void tex_aux_balance_callback_delete(int callback_id, halfword checks, halfword passive)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dddd->",
        delete_line_break_context,
        checks,
        passive_serial(passive),
        passive_ref_count(passive)
    );
}

static void tex_aux_balance_callback_wrapup(int callback_id, halfword checks)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dddd->",
        wrapup_line_break_context,
        checks,
        lmt_balance_state.fewest_demerits,
        lmt_balance_state.actual_looseness
    );
}

static halfword tex_aux_balance_callback_report(int callback_id, halfword checks, int pass, halfword subpass, halfword active, halfword passive)
{
    halfword demerits = active_total_demerits(active);
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "ddddddddddddNddd->r",
        report_line_break_context,
        checks,
        pass,
        subpass,
        passive_serial(passive),
        passive_prev_break(passive) ? passive_serial(passive_prev_break(passive)) : 0,
        active_page_number(active) - 1,
        node_type(active),
        active_fitness(active) + 1,            /* we offset the class */
        passive_n_of_fitness_classes(passive), /* also in passive  */
        passive_badness(passive),
        demerits,
        passive_cur_break(passive),
        active_short(active),
        active_glue(active),
        active_page_height(active),
        &demerits  /* optionally changed */
    );
    return demerits;
}

static void tex_aux_balance_callback_list(int callback_id, halfword checks, halfword passive)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dddd->",
        list_line_break_context,
        checks,
        passive_serial(passive),
        passive_ref_count(passive)
    );
}

/* */

static inline halfword tex_max_fitness(halfword fitnessclasses)
{
    return tex_get_specification_count(fitnessclasses);
}

static inline halfword tex_med_fitness(halfword fitnessclasses)
{
    return tex_get_specification_decent(fitnessclasses);
}

static inline halfword tex_get_demerits(const balance_properties *properties, halfword distance, halfword start, halfword stop)
{
    (void) start;
    (void) stop;
    if (distance && distance > 1) {
        return properties->adj_demerits;
    } else { 
        return 0;
    }
}

static inline halfword tex_normalized_loose_badness(halfword b, halfword fitnessclasses)
{
    halfword med = tex_get_specification_decent(fitnessclasses);
    for (halfword c = med - 1; c >= 1; c--) {
        if (b <= tex_get_specification_fitness_class(fitnessclasses, c)) {
            return c;
        }
    }
    return 0;
}

static inline halfword tex_normalized_tight_badness(halfword b, halfword fitnessclasses)
{
    halfword max = tex_get_specification_count(fitnessclasses);
    halfword med = tex_get_specification_decent(fitnessclasses);
    for (halfword c = med + 1; c <= max; c++) {
        if (b <= tex_get_specification_fitness_class(fitnessclasses, c)) {
            return c - 2;
        }
    }
    return max - 1;
}

static void tex_aux_set_quality(halfword active, halfword passive, scaled shrt, scaled glue, scaled height, halfword badness)
{
    halfword quality = 0;
    halfword deficiency = 0;
    active_short(active) = shrt;
    active_glue(active) = glue;
    active_page_height(active) = height;
    if (shrt < 0) {
        shrt = -shrt;
        if (shrt > glue) {
            quality = par_is_overfull;
            deficiency = shrt - glue;
        }
    } else if (shrt > 0) {
        if (shrt > glue) {
            quality = par_is_underfull;
            deficiency = shrt - glue;
        }
    }
    passive_quality(passive) = quality;
    passive_deficiency(passive) = deficiency;
    passive_demerits(passive) = active_total_demerits(active); /* ... */
    passive_badness(passive) = badness;
    active_quality(active) = quality;
    active_deficiency(active) = deficiency;
 }

static scaled tex_aux_try_balance(
    const balance_properties *properties,
    halfword penalty,
    halfword break_type,
    halfword first_p,
    halfword cur_p,
    int callback_id,
    halfword checks,
    int pass,
    int subpass,
    int artificial
)
{
    halfword previous = active_head;
    halfword before_previous = null;
    scaled current_active_height[n_of_glue_amounts] = { 0 };
    halfword best_place      [max_n_of_fitness_values] = { 0 };
    halfword best_place_page [max_n_of_fitness_values] = { 0 };
    scaled   best_place_short[max_n_of_fitness_values] = { 0 };
    scaled   best_place_glue [max_n_of_fitness_values] = { 0 };
    halfword badness = 0;
    halfword prev_badness = 0;
    int demerits = 0;
    scaled glue = 0;
    scaled shortfall = 0;
    halfword old_page = 0;
    bool no_break_yet = true;
    int current_stays_active;
    halfword fit_class;
    int artificial_demerits;
    scaled page_height = 0;
    halfword page = 0;
    if (penalty >= infinite_penalty) {
        return shortfall;
    } else if (penalty <= -infinite_penalty) {
        penalty = eject_penalty; /* bad name here */
    }
    tex_aux_set_target_to_source(current_active_height, lmt_balance_state.active_height);
    while (1) {
        halfword current = node_next(previous);
        if (node_type(current) == delta_node) {
            tex_aux_add_to_target_from_delta(current_active_height, current);
            before_previous = previous;
            previous = current;
            continue;
        } else {
            /*tex We have an |unhyphenated_node| or |hyphenated_node|. */
        }
        lmt_balance_state.current_page_number = page; /* we could just use this variable */
        page = active_page_number(current);
        if (page > old_page) {
            if ((lmt_balance_state.minimum_demerits < awful_bad) && ((old_page != lmt_balance_state.easy_page) || (current == active_head))) {
                if (properties->page_shape) {
                    page_height = tex_get_specification_height(properties->page_shape, page);
                } else {
                    page_height = lmt_balance_state.target_height;
                }
                if (no_break_yet) {
                    no_break_yet = false;
                    if (lmt_balance_state.emergency_percentage) {
                        scaled stretch = tex_xn_over_d(page_height, lmt_balance_state.emergency_percentage, scaling_factor);
                        lmt_balance_state.background[total_stretch_amount] -= lmt_balance_state.emergency_amount;
                        lmt_balance_state.background[total_stretch_amount] += stretch;
                        lmt_balance_state.emergency_amount = stretch;
                    }
                    tex_aux_set_target_to_source(lmt_balance_state.break_height, lmt_balance_state.background);
                    tex_aux_compute_break_height(break_type, cur_p);
                }
                if (node_type(previous) == delta_node) {
                    tex_aux_add_delta_from_difference(previous, lmt_balance_state.break_height, current_active_height);
                } else if (previous == active_head) {
                    tex_aux_set_target_to_source(lmt_balance_state.active_height, lmt_balance_state.break_height);
                } else {
                    halfword q = tex_new_node(delta_node, default_fitness); /* class and classes zero */
                    node_next(q) = current;
                    tex_aux_set_delta_from_difference(q, lmt_balance_state.break_height, current_active_height);
                    node_next(previous) = q;
                    before_previous = previous;
                    previous = q;
                }
                if (properties->max_adj_demerits >= awful_bad - lmt_balance_state.minimum_demerits) {
                    lmt_balance_state.minimum_demerits = awful_bad - 1;
                } else {
                    lmt_balance_state.minimum_demerits += properties->max_adj_demerits;
                }
                for (halfword fit_class = default_fitness; fit_class <= tex_max_fitness(properties->fitness_classes); fit_class++) {
                    if (lmt_balance_state.minimal_demerits[fit_class] <= lmt_balance_state.minimum_demerits) {
                        halfword passive = tex_new_node(passive_node, (quarterword) fit_class);
                        halfword active = tex_new_node((quarterword) break_type, (quarterword) fit_class);
                        halfword prev_break = best_place[fit_class];
                        /*tex Initialize the passive node: */
                        active_n_of_fitness_classes(active) = tex_max_fitness(properties->fitness_classes);
                        passive_n_of_fitness_classes(passive) = tex_max_fitness(properties->fitness_classes);
                        passive_cur_break(passive) = cur_p;
                        passive_serial(passive) = ++lmt_balance_state.serial_number;
                        passive_ref_count(passive) = 1;
                        passive_prev_break(passive) = prev_break;
                        if (prev_break) {
                            passive_ref_count(prev_break) += 1;
                        }
                        /*tex Initialize the active node: */
                        active_break_node(active) = passive;
                        active_page_number(active) = best_place_page[fit_class] + 1;
                        active_total_demerits(active) = lmt_balance_state.minimal_demerits[fit_class];
                        /*tex Store additional data in the new active node. */
                        tex_aux_set_quality(active, passive, best_place_short[fit_class], best_place_glue[fit_class], page_height, prev_badness);
                        /*tex Append the passive node. */
                        node_next(passive) = lmt_balance_state.passive;
                        lmt_balance_state.passive = passive;
                        /*tex Append the active node. */
                        node_next(active) = current;
                        node_next(previous) = active;
                        previous = active;
                        /* */
                        if (callback_id) {
                            active_total_demerits(active) = tex_aux_balance_callback_report(callback_id, checks, pass, subpass, active, passive);
                        }
                        if (properties->tracing_balancing > 0) {
                            tex_begin_diagnostic();
                            tex_aux_print_break_node(active, passive, 0);
                            tex_end_diagnostic();
                        }
                    }
                    lmt_balance_state.minimal_demerits[fit_class] = awful_bad;
                }
                lmt_balance_state.minimum_demerits = awful_bad;
                if (current != active_head) {
                    halfword delta = tex_new_node(delta_node, default_fitness);
                    node_next(delta) = current;
                    tex_aux_set_delta_from_difference(delta, current_active_height, lmt_balance_state.break_height);
                    node_next(previous) = delta;
                    before_previous = previous;
                    previous = delta;
                }
            }
            /* line_height already has been calculated */
            if (page > lmt_balance_state.easy_page) {
                old_page = max_halfword - 1;
                page_height = lmt_balance_state.target_height;
            } else {
                old_page = page;
                if (properties->page_shape) {
                    page_height = tex_get_specification_height(properties->page_shape, page);
                } else {
                    page_height = lmt_balance_state.target_height;
                }
            }
            if (current == active_head) {
                shortfall = page_height - current_active_height[total_advance_amount];
                return shortfall;
            }
        }
        artificial_demerits = 0;
        shortfall = page_height - current_active_height[total_advance_amount];
        if (shortfall > 0) {
            if (current_active_height[total_fi_amount]   || current_active_height[total_fil_amount] ||
                current_active_height[total_fill_amount] || current_active_height[total_filll_amount]) {
                badness = 0;
                /*tex Infinite stretch. */
                fit_class = tex_get_specification_decent(properties->fitness_classes) - 1;
            } else if (shortfall > large_height_excess && current_active_height[total_stretch_amount] < small_stretchability) {
                badness = infinite_bad;
                fit_class = default_fitness;
            } else {
                badness = tex_badness(shortfall, current_active_height[total_stretch_amount]);
                fit_class = tex_normalized_loose_badness(badness, properties->fitness_classes);
            }
        } else {
            if (-shortfall > current_active_height[total_shrink_amount]) {
                badness = infinite_bad + 1;
            } else {
                badness = tex_badness(-shortfall, current_active_height[total_shrink_amount]);
        }
            fit_class = tex_normalized_tight_badness(badness, properties->fitness_classes);
        }
        if (1) {
            if (! cur_p) {
                shortfall = 0;
                glue = 0;
            } else if (shortfall > 0) {
                glue = current_active_height[total_stretch_amount];
            } else if (shortfall < 0) {
                glue = current_active_height[total_shrink_amount];
            } else {
                glue = 0;
            }
        } else {
            /* Can we get here at all? */
        }
        if ((badness > infinite_bad) || (penalty == eject_penalty)) {
            if (artificial && (lmt_balance_state.minimum_demerits == awful_bad) && (node_next(current) == active_head) && (previous == active_head)) {
                /*tex Set demerits zero, this break is forced. */
                artificial_demerits = 1;
            } else if (badness > lmt_balance_state.threshold) {
                goto DEACTIVATE;
            }
            current_stays_active = 0;
        } else {
            previous = current;
            if (badness > lmt_balance_state.threshold) {
                continue;
            } else {
                current_stays_active = 1;
            }
        }
        if (artificial_demerits) {
            demerits = 0;
        } else {
            /*tex Compute the demerits, |d|, from |r| to |cur_p|. */
            int fit_current = (halfword) active_fitness(current);
            int distance = abs(fit_class - fit_current);
            demerits = badness; /* no line penalty addition equivalent here */
            if (abs(demerits) >= infinite_bad) {
                demerits = extremely_deplorable;
            } else {
                demerits = demerits * demerits;
            }
            if (penalty) {
                if (penalty > 0) {
                    demerits += (penalty * penalty);
                } else if (penalty > eject_penalty) {
                    demerits -= (penalty * penalty);
                }
            }
            demerits += tex_get_demerits(properties, distance, fit_current, fit_class);
        }
        prev_badness = badness;
        if (properties->tracing_balancing > 0) {
            tex_aux_print_feasible_break(cur_p, current, badness, penalty, demerits, artificial_demerits, fit_class, lmt_balance_state.printed_node);
        }
        /*tex This is the minimum total demerits from the beginning to |cur_p| via |r|. */
        demerits += active_total_demerits(current);
        if (demerits <= lmt_balance_state.minimal_demerits[fit_class]) {
            lmt_balance_state.minimal_demerits[fit_class] = demerits;
            best_place[fit_class] = active_break_node(current);
            best_place_page[fit_class] = page;
            best_place_short[fit_class] = shortfall;
            best_place_glue[fit_class] = glue;
            if (demerits < lmt_balance_state.minimum_demerits) {
                lmt_balance_state.minimum_demerits = demerits;
            }
        }
        if (current_stays_active) {
            continue;
        }
      DEACTIVATE:
        {
            halfword passive = active_break_node(current);
            node_next(previous) = node_next(current);
            if (passive) {
                passive_ref_count(passive) -= 1;
                if (callback_id) {
                    /*tex Not that usefull, basically every passive is touched. */
                    switch (node_type(current)) {
                        case unhyphenated_node:
                        case hyphenated_node:
                            tex_aux_balance_callback_delete(callback_id, checks, passive);
                            break;
                    //  case delta_node:
                    //      break;
                    }
                }
            }
            tex_flush_node(current);
        }
        if (previous == active_head) {
            current = node_next(active_head);
            if (node_type(current) == delta_node) {
                tex_aux_add_to_target_from_delta(lmt_balance_state.active_height, current);
                tex_aux_set_target_to_source(current_active_height, lmt_balance_state.active_height);
                node_next(active_head) = node_next(current);
                tex_flush_node(current);
            }
        } else if (node_type(previous) == delta_node) {
            current = node_next(previous);
            if (current == active_head) {
                tex_aux_sub_delta_from_target(current_active_height, previous);
                node_next(before_previous) = active_head;
                tex_flush_node(previous);
                previous = before_previous;
            } else if (node_type(current) == delta_node) {
                tex_aux_add_to_target_from_delta(current_active_height, current);
                tex_aux_add_to_delta_from_delta(previous, current);
                node_next(previous) = node_next(current);
                tex_flush_node(current);
            }
        }
    }
    return shortfall;
}

static inline int tex_aux_valid_glue_break(halfword p)
{
    halfword prv = node_prev(p);
 // return (prv && prv != temp_head && (precedes_break(prv) || precedes_kern(prv) || precedes_dir(prv)));
    return (prv && prv != temp_head && precedes_break(prv));
}

# define max_prev_graf (max_integer/2)

static inline int tex_aux_emergency(const balance_properties *properties)
{
    if (properties->emergency_stretch > 0) {
        return 1;
    } else {
        return 0;
    }
}

static inline int tex_aux_emergency_skip(halfword s)
{
    return ! tex_glue_is_zero(s) && glue_stretch_order(s) == normal_glue_order && glue_shrink_order(s) == normal_glue_order;
}

static scaled tex_check_balance_quality(scaled shortfall, scaled *overfull, scaled *underfull, halfword *verdict, halfword *classified)
{
    halfword active = active_break_node(lmt_balance_state.best_bet);
    halfword passive = passive_prev_break(active);
    int result = 1;
    /* last page ... */
    switch (active_quality(active)) {
        case par_is_overfull:
            *overfull = active_deficiency(active);
            *underfull = 0;
            break;
        case par_is_underfull:
            *overfull = 0;
            *underfull = active_deficiency(active);
            break;
        default:
            *overfull = 0;
            *underfull = 0;
            break;
    }
    *verdict = active_total_demerits(active);
    *classified |= (1 << active_fitness(active));
    /* previous pages */
    if (passive) {
        while (passive) {
            switch (passive_quality(passive)) {
                case par_is_overfull:
                    if (passive_deficiency(passive) > *overfull) {
                        *overfull = passive_deficiency(passive);
                    }
                    break;
                case par_is_underfull:
                    if (passive_deficiency(passive) > *underfull) {
                        *underfull = passive_deficiency(passive);
                    }
                    break;
                default:
                    /* not in tex */
                    break;
            }
            // *classified |= classification[node_subtype(q)];
            *classified |= (1 << passive_fitness(passive));
            if (passive_demerits(passive) > *verdict) {
                *verdict = passive_demerits(passive);
            }
            passive = passive_prev_break(passive);
        }
    } else {
        if (passive_demerits(active) > *verdict) {
            *verdict = passive_demerits(active);
            result = 2;
        }
        if (-shortfall > *overfull) {
            *overfull = -shortfall;
            result = 2;
        }
    }
    if (*verdict > infinite_bad) {
        *verdict = infinite_bad;
    }
    return result;
}

static inline void tex_aux_set_initial_active(const balance_properties *properties)
{
    halfword initial = tex_new_node(unhyphenated_node, (quarterword) tex_get_specification_decent(properties->fitness_classes) - 1);
    node_next(initial) = active_head;
    active_break_node(initial) = null;
    active_page_number(initial) = 1;
    active_total_demerits(initial) = 0; // default
    active_short(initial) = 0;          // default
    active_glue(initial) = 0;           // default
    active_page_height(initial) = 0;    // default
    node_next(active_head) = initial;
}

static inline void tex_aux_set_looseness(const balance_properties *properties)
{
    lmt_balance_state.actual_looseness = 0;
    if (properties->looseness == 0) {
        lmt_balance_state.easy_page = lmt_balance_state.last_special_page;
    } else {
        lmt_balance_state.easy_page = max_halfword;
    }
}

static inline void tex_aux_set_adjacent_demerits(balance_properties *properties)
{
    properties->max_adj_demerits = properties->adj_demerits;
}

static int tex_aux_set_sub_pass_parameters(
    balance_properties *properties,
    halfword            passes,
    int                 subpass,
    halfword            first,
    int                 details,
    halfword            features,
    halfword            overfull,
    halfword            underfull,
    halfword            verdict,
    halfword            classified,
    halfword            threshold,
    halfword            demerits,
    halfword            classes
) {
    int success = 0;
    uint64_t okay = tex_get_passes_okay(passes, subpass);
    if (okay & passes_tolerance_okay) {
        properties->tolerance = tex_get_passes_tolerance(passes, subpass);
    }
    lmt_balance_state.threshold = properties->tolerance;
    if (okay & passes_basics_okay) {
        if (okay & passes_hyphenation_okay) {
            lmt_balance_state.force_check_hyphenation = tex_get_passes_hyphenation(passes, subpass) > 0 ? 1 : 0;
        }
        if (okay & passes_emergencyfactor_okay) {
            lmt_balance_state.emergency_factor = tex_get_passes_emergencyfactor(passes, subpass);
        }
        if (okay & passes_emergencypercentage_okay) {
            lmt_balance_state.emergency_percentage = tex_get_passes_emergencypercentage(passes, subpass);
        }
    }
    if (okay & passes_emergencystretch_okay) {
        halfword v = tex_get_passes_emergencystretch(passes, subpass);
        if (v) {
            properties->emergency_stretch = v;
            properties->emergency_original = v; /* ! */
        } else {
            properties->emergency_stretch = properties->emergency_original;
        }
    } else {
        properties->emergency_stretch = properties->emergency_original;
    }
    if (lmt_balance_state.emergency_factor) {
        properties->emergency_stretch = tex_xn_over_d(properties->emergency_original, lmt_balance_state.emergency_factor, scaling_factor);
    } else {
        properties->emergency_stretch = 0;
    }
    lmt_balance_state.background[total_stretch_amount] -= lmt_balance_state.extra_background_stretch;
    lmt_balance_state.extra_background_stretch = properties->emergency_stretch;
    lmt_balance_state.background[total_stretch_amount] += properties->emergency_stretch;
    if (okay & passes_looseness_okay) {
        properties->looseness = tex_get_passes_looseness(passes, subpass);
        tex_aux_set_looseness(properties);
    }
    if (details) {

        # define is_okay(a) ((okay & a) == a ? ">" : " ")

        tex_begin_diagnostic();
        tex_print_format("[balance: values used in subpass %i]\n", subpass);
        tex_print_str("  --------------------------------\n");
        tex_print_format("  use criteria          %s\n", subpass >= passes_first_final(passes) ? "true" : "false");
        if (features & passes_test_set) {
            tex_print_str("  --------------------------------\n");
            if (features & passes_if_emergency_stretch) { tex_print_format("  if emergency stretch true\n"); }
            if (features & passes_if_looseness)         { tex_print_format("  if looseness         true\n"); }
        }
        tex_print_str("  --------------------------------\n");
        tex_print_format("%s threshold            %p\n", is_okay(passes_threshold_okay), tex_get_passes_threshold(passes, subpass));
     // tex_print_format("%s demerits             %i\n", is_okay(passes_demerits_okay), tex_get_passes_demerits(passes, subpass));
        tex_print_str("  --------------------------------\n");
        tex_print_format("%s tolerance            %i\n", is_okay(passes_tolerance_okay), properties->tolerance);
        tex_print_format("%s hyphenation          %s\n", is_okay(passes_hyphenation_okay), lmt_balance_state.force_check_hyphenation ? "true": "false");
        tex_print_format("%s looseness            %i\n", is_okay(passes_looseness_okay), properties->looseness);
        tex_print_str("  --------------------------------\n");
        tex_print_format("%s adjdemerits          %i\n", is_okay(passes_adjdemerits_okay), properties->adj_demerits);
        tex_print_str("  --------------------------------\n");
        tex_print_format("%s emergencyoriginal    %p\n", is_okay(passes_emergencystretch_okay), properties->emergency_original);
        tex_print_format("%s emergencystretch     %p\n", is_okay(passes_emergencystretch_okay), properties->emergency_stretch);
        tex_print_format("%s emergencyfactor      %i\n", is_okay(passes_emergencyfactor_okay), tex_get_passes_emergencyfactor(passes, subpass));
        tex_print_format("%s emergencypercentage  %i\n", is_okay(passes_emergencypercentage_okay), lmt_balance_state.emergency_percentage);
        tex_print_str("  --------------------------------\n");
        tex_end_diagnostic();
    }
    return success;
}

static void tex_aux_skip_message(halfword passes, int subpass, int nofsubpasses, const char *str)
{
    tex_begin_diagnostic();
    tex_print_format("[balance: id %i, subpass %i of %i, skip %s]\n",
        passes_identifier(passes), subpass, nofsubpasses, str
    );
    tex_end_diagnostic();
}

static inline int tex_aux_next_subpass(const balance_properties *properties, halfword passes, int subpass, int nofsubpasses, int tracing)
{
    while (++subpass <= nofsubpasses) {
        halfword features = tex_get_passes_features(passes, subpass);
        if (features & passes_test_set) {
            if (features & passes_if_emergency_stretch) {
                if (! ( (properties->emergency_original || tex_get_passes_emergencystretch(passes, subpass)) && tex_get_passes_emergencyfactor(passes, subpass) ) ) {
                    if (tracing) {
                        tex_aux_skip_message(passes, subpass, nofsubpasses, "emergency stretch");
                    }
                    continue;
                }
            }
            if (features & passes_if_looseness) {
                if (! properties->looseness) {
                    if (tracing) {
                        tex_aux_skip_message(passes, subpass, nofsubpasses, "no looseness");
                    }
                    continue;
                }
            }
        }
        return subpass;
    }
    return nofsubpasses + 1;
}

static inline int tex_aux_check_sub_pass(balance_properties *properties, scaled shortfall, halfword passes, int subpass, int nofsubpasses, halfword first)
{
    scaled overfull = 0;
    scaled underfull = 0;
    halfword verdict = 0;
    halfword classified = 0;
    int tracing = properties->tracing_balancing > 0 || properties->tracing_passes > 0;
    int result = tex_check_balance_quality(shortfall, &overfull, &underfull, &verdict, &classified);
    if (result) {
        if (tracing && result > 1) {
            tex_begin_diagnostic();
            tex_print_format("[balance: id %i, subpass %i of %i, overfull %p, verdict %i, special case, entering subpasses]\n",
                passes_identifier(passes), subpass, nofsubpasses, overfull, verdict
            );
            tex_end_diagnostic();
        }
        while (subpass < nofsubpasses) {
            subpass = tex_aux_next_subpass(properties, passes, subpass, nofsubpasses, tracing);
            if (subpass > nofsubpasses) {
                return subpass;
            } else {
                halfword features = tex_get_passes_features(passes, subpass);
                if (features & passes_quit_pass) {
                    return -1;
                } else if (features & passes_skip_pass) {
                    continue;
                } else {
                    scaled threshold = tex_get_passes_threshold(passes, subpass);
                    halfword demerits = tex_get_passes_demerits(passes, subpass); /* here we just use defaults */
                    halfword classes = tex_get_passes_classes(passes, subpass);   /* here we just use defaults */  
                    int callback = features & passes_callback_set;
                    int success = 0;
                    int details = properties->tracing_passes > 1;
                    int retry = callback ? 1 : overfull > threshold || verdict > demerits || (classes && (classes & classified) != 0);
                    if (tracing) {
                        int id = passes_identifier(passes);
                        tex_begin_diagnostic();
                        if (callback) {
                            tex_print_format("[balance: id %i, subpass %i of %i, overfull %p, underfull %p, verdict %i, classified %x, %s]\n",
                                id, subpass, nofsubpasses, overfull, underfull, verdict, classified, "callback"
                            );
                        } else {
                            const char *action = retry ? "retry" : "skipped";
                            if (id < 0) {
                                id = -id; /* nicer for our purpose */
                            }
                            if (threshold == max_dimension) {
                                if (demerits == max_dimension) {
                                    tex_print_format("[balance: id %i, subpass %i of %i, overfull %p, underfull %p, verdict %i, classified %x, %s]\n",
                                        id, subpass, nofsubpasses, overfull, underfull, verdict, classified, action
                                    );
                                } else {
                                    tex_print_format("[balance: id %i, subpass %i of %i, overfull %p, underfull %p, verdict %i, demerits %i, classified %x, %s]\n",
                                        id, subpass, nofsubpasses, overfull, underfull, verdict, demerits, classified, action
                                    );
                                }
                            } else {
                                if (demerits == max_dimension) {
                                    tex_print_format("[balance: id %i, subpass %i of %i, overfull %p, underfull %p, verdict %i, threshold %p, classified %x, %s]\n",
                                        id, subpass, nofsubpasses, overfull, underfull, verdict, threshold, classified, action
                                    );
                                } else {
                                    tex_print_format("[balance: id %i, subpass %i of %i, overfull %p, underfull %p, verdict %i, threshold %p, demerits %i, classified %x, %s]\n",
                                        id, subpass, nofsubpasses, overfull, underfull, verdict, threshold, demerits, classified, action
                                    );
                                }
                            }
                        }
                    }
                    if (retry) {
                        success = tex_aux_set_sub_pass_parameters(
                            properties, passes, subpass, first,
                            details,
                            features, overfull, underfull, verdict, classified, threshold, demerits, classes
                        );
                    }
                    if (tracing) {
                        tex_end_diagnostic();
                    }
                    if (success) {
                        return subpass;
                    }
                }
            }
        }
    } else {
        /*tex We have a few hits in our test files. */
    }
    return 0;
}

static inline halfword tex_aux_balance_list(const balance_properties *properties, halfword pass, halfword subpass, halfword current, halfword first, int artificial)
{
    halfword callback_id = lmt_balance_state.callback_id;
    halfword checks = properties->balance_checks;
    while (current && (node_next(active_head) != active_head)) { /* we check the cycle */
        switch (node_type(current)) {
            case hlist_node:
            case vlist_node:
                lmt_balance_state.active_height[total_advance_amount] += box_height(current);
                lmt_balance_state.active_height[total_advance_amount] += box_depth(current);
                break;
            case rule_node:
                lmt_balance_state.active_height[total_advance_amount] += rule_height(current);
                lmt_balance_state.active_height[total_advance_amount] += rule_depth(current);
                break;
         // case par_node:
         //     /*tex Advance past a |par| node. */
         //     break;
            case glue_node:
                /*tex Checks for temp_head! */
                if (tex_aux_valid_glue_break(current)) {
                    tex_aux_try_balance(properties, 0, unhyphenated_node, first, current, callback_id, checks, pass, subpass, artificial);
                }
                lmt_balance_state.active_height[total_advance_amount] += glue_amount(current);
                lmt_balance_state.active_height[total_stretch_amount + glue_stretch_order(current)] += glue_stretch(current);
                lmt_balance_state.active_height[total_shrink_amount] += tex_aux_checked_shrink(current);
                break;
            case kern_node:
                /*tex there are not many vertical kerns that can occur in vmode */
                if (node_subtype(current) == explicit_kern_subtype) { 
                    halfword nxt = node_next(current);
                    if (nxt && node_type(nxt) == glue_node) {
                        tex_aux_try_balance(properties, 0, unhyphenated_node, first, current, callback_id, checks, pass, subpass, artificial);
                    }
                }
                lmt_balance_state.active_height[total_advance_amount] += kern_amount(current);
                break;
            case disc_node:
                {
                    halfword replace = disc_no_break_head(current);
                    if (lmt_balance_state.force_check_hyphenation || (node_subtype(current) != syllable_discretionary_code)) {
                        halfword actual_penalty = disc_penalty(current);
                        halfword pre = disc_pre_break_head(current);
                        tex_aux_reset_disc_target(lmt_balance_state.disc_height);
                        if (pre) {
                            if (replace && node_subtype(current) != mathematics_discretionary_code) {
                                if (tex_has_disc_option(current, disc_option_prefer_break) || tex_has_disc_option(current, disc_option_prefer_nobreak)) {
                                    switch (node_type(node_next(current))) {
                                        case glue_node:
                                        case penalty_node:
                                        case boundary_node:
                                            {
                                                scaled hpre = tex_natural_vsize(pre);
                                                scaled hreplace = tex_natural_vsize(replace);
                                                if (tex_has_disc_option(current, disc_option_prefer_break)) {
                                                    halfword post = disc_post_break_head(current);
                                                    scaled hpost = post ? tex_natural_vsize(post) : 0;
                                                    if (hpost > 0) {
                                                        if (properties->tracing_balancing > 1) {
                                                            tex_begin_diagnostic();
                                                            tex_print_format("[balance: favour final prepost over replace, heights %p %p]", hpre + hpost, hreplace);
                                                            tex_short_display(node_next(temp_head));
                                                            tex_end_diagnostic();
                                                        }
                                                    } else {
                                                        goto REPLACEONLY;
                                                    }
                                                } else {
                                                    if (hreplace < hpre) {
                                                        if (properties->tracing_balancing > 1) {
                                                            tex_begin_diagnostic();
                                                            tex_print_format("[balance: favour final replace over pre, heights %p %p]", hreplace, hpre);
                                                            tex_short_display(node_next(temp_head));
                                                            tex_end_diagnostic();
                                                        }
                                                        goto REPLACEONLY;
                                                    }
                                                }
                                            }
                                    }
                                }
                            }
                            tex_aux_add_to_heights(pre, lmt_balance_state.disc_height);
                            tex_aux_add_disc_source_to_target(lmt_balance_state.active_height, lmt_balance_state.disc_height);
                            tex_aux_try_balance(properties, actual_penalty, hyphenated_node, first, current, callback_id, checks, pass, subpass, artificial);
                            tex_aux_sub_disc_target_from_source(lmt_balance_state.active_height, lmt_balance_state.disc_height);
                        } else {
                            /*tex trivial pre-break */
                            tex_aux_try_balance(properties, actual_penalty, hyphenated_node, first, current, callback_id, checks, pass, subpass, artificial);
                        }
                    }
                  REPLACEONLY:
                    if (replace) {
                        tex_aux_add_to_heights(replace, lmt_balance_state.active_height);
                    }
                    break;
                }
            case penalty_node:
                {
                    halfword penalty = penalty_amount(current);
                    tex_aux_try_balance(properties, penalty, unhyphenated_node, first, current, callback_id, checks, pass, subpass, artificial);
                    break;
                }
            case whatsit_node:
            case mark_node:
            case insert_node:
            case adjust_node:
                /*tex Advance past these nodes in the |page_break| loop. Maybe trace them. */
                break;
            default:
                tex_formatted_error("balancer", "weird node %d in page", node_type(current));
        }
        current = node_next(current);
    }
    return current;
}

static void tex_aux_set_height(const balance_properties *properties)
{
    if (properties->page_shape) {
        int n = specification_count(properties->page_shape);
        if (n > 0) {
            if (specification_repeat(properties->page_shape)) {
                lmt_balance_state.last_special_page = max_halfword;
            } else {
                lmt_balance_state.last_special_page = n - 1;
            }
            lmt_balance_state.target_height = tex_get_specification_height(properties->page_shape, n);
        } else {
            lmt_balance_state.last_special_page = 0;
            lmt_balance_state.target_height = properties->vsize;
        }
    } else { 
        lmt_balance_state.last_special_page = 0;
        lmt_balance_state.target_height = properties->vsize;
    }
    /* check: if target_height is zero then make it ... */
}

static int tex_aux_quit_balance(const balance_properties *properties, int pass)
{
    /*tex Find an active node with fewest demerits. */
    if (properties->looseness == 0) {
        return 1;
    } else {
        halfword r = node_next(active_head);
        halfword actual_looseness = 0;
        halfword best_page = lmt_balance_state.best_page;
        int verdict = 0;
        int tracing = tracing_looseness_par;
        if (tracing) {
            tex_begin_diagnostic();
            tex_print_format("[looseness: pass %i, pages %i, looseness %i]\n", pass, best_page - 1, properties->looseness);
        }
        do {
            if (node_type(r) != delta_node) {
                halfword page_number = active_page_number(r);
                halfword page_difference = page_number - best_page;
                halfword total_demerits = active_total_demerits(r);
                if ((page_difference < actual_looseness && properties->looseness <= page_difference) || (page_difference > actual_looseness && properties->looseness >= page_difference)) {
                    lmt_balance_state.best_bet = r;
                    actual_looseness = page_difference;
                    lmt_balance_state.fewest_demerits = total_demerits;
                    if (tracing) {
                        tex_print_format("%l[looseness: pass %i, page %i, difference %i, demerits %i, %s optimal]", pass, page_number - 1, page_difference, total_demerits, "sub");
                    }
                } else if (page_difference == actual_looseness && total_demerits < lmt_balance_state.fewest_demerits) {
                    lmt_balance_state.best_bet = r;
                    lmt_balance_state.fewest_demerits = total_demerits;
                    if (tracing) {
                        tex_print_format("%l[looseness: pass %i, page %i, difference %i, demerits %i, %s optimal]", pass, page_number - 1, page_difference, total_demerits, "more");
                    }
                } else {
                    if (tracing) {
                        tex_print_format("%l[looseness: pass %i, page %i, difference %i, demerits %i, %s optimal]", pass, page_number - 1, page_difference, total_demerits, "not");
                    }
                }
            }
            r = node_next(r);
        } while (r != active_head);
        lmt_balance_state.actual_looseness = actual_looseness;
        lmt_balance_state.best_page = active_page_number(lmt_balance_state.best_bet);
        verdict = actual_looseness == properties->looseness;
        if (tracing) {
            tex_print_format("%l[looseness: pass %i, looseness %i, page %i, demerits %i, %s]\n", pass, actual_looseness, lmt_balance_state.best_page - 1, lmt_balance_state.fewest_demerits, verdict ? "success" : "failure");
            tex_end_diagnostic();
        }
        return verdict || pass >= balance_final_pass;
    }
}

static void tex_aux_find_best_bet(void)
{
    halfword r = node_next(active_head);
    lmt_balance_state.fewest_demerits = awful_bad;
    do {
        if ((node_type(r) != delta_node) && (active_total_demerits(r) < lmt_balance_state.fewest_demerits)) {
            lmt_balance_state.fewest_demerits = active_total_demerits(r);
            lmt_balance_state.best_bet = r;
        }
        r = node_next(r);
    } while (r != active_head);
    lmt_balance_state.best_page = active_page_number(lmt_balance_state.best_bet);
}

void tex_balance_preset(balance_properties *properties)
{
    properties->tracing_balancing  = tracing_balancing_par;
    properties->tracing_fitness    = tracing_fitness_par;
    properties->tracing_passes     = tracing_passes_par;
    properties->tolerance          = 200;
    properties->pretolerance       = 100;
    properties->vsize              = 0; 
    properties->emergency_stretch  = 0;
    properties->emergency_original = 0;
    properties->looseness          = 0;
    properties->adj_demerits       = 0;
    properties->page_shape         = null; 
    properties->fitness_classes    = tex_default_fitness_classes();
    properties->hyphen_penalty     = 0;
    properties->hyphenation_mode   = 1;
    properties->page_passes        = 0;   
    properties->max_adj_demerits   = 0;
    properties->balance_checks     = balance_checks_par;
    properties->packing            = packing_exactly;
}

void tex_balance_reset(balance_properties *properties)
{
    tex_flush_node(properties->page_shape);
    tex_flush_node(properties->fitness_classes);
}

/* 
    todo: we could push_nest/pop_nest and use temp_head  
    todo: check for par_shape or vsize 
*/

void tex_balance(balance_properties *properties, halfword head)
{
    halfword passes = properties->page_passes;
    int subpasses = passes ? tex_get_specification_count(passes) : 0;
    int subpass = -2;
    int pass = balance_no_pass;
    halfword first = node_next(temp_head);
    /*tex Some helpers use temp_head so we need to use that! */
    lmt_balance_state.passes.n_of_break_calls++;
    properties->emergency_original = properties->emergency_stretch;
    lmt_balance_state.force_check_hyphenation = hyphenation_permitted(properties->hyphenation_mode, force_check_hyphenation_mode);
    lmt_balance_state.callback_id = properties->balance_checks ? lmt_callback_defined(balance_callback) : 0;
    lmt_balance_state.fewest_demerits = 0;
    lmt_balance_state.no_shrink_error_yet = 1;
    lmt_balance_state.minimum_demerits = awful_bad;
    lmt_balance_state.extra_background_stretch = 0;
    lmt_balance_state.emergency_amount = 0;
    lmt_balance_state.emergency_factor = scaling_factor;
    lmt_balance_state.emergency_percentage = 0;
    lmt_balance_state.emergency_height_amount = 0;
    for (int i = 0; i < max_n_of_fitness_values; i++) {
        lmt_balance_state.minimal_demerits[i] = awful_bad;
    }
    tex_aux_pre_balance(properties, lmt_balance_state.callback_id, properties->balance_checks, 0);
    tex_aux_set_adjacent_demerits(properties);
    tex_aux_set_height(properties);
    tex_aux_set_looseness(properties);
    lmt_balance_state.threshold = properties->pretolerance;
    if (properties->tracing_balancing > 1) {
        tex_begin_diagnostic();
        tex_print_str("[balance: original]");
        tex_short_display(first);
        tex_end_diagnostic();
    }
    if (subpasses) {
        pass = balance_specification_pass;
        lmt_balance_state.threshold = properties->pretolerance; /* or tolerance */
        if (properties->tracing_balancing > 0 || properties->tracing_passes > 0) {
            if (specification_presets(passes)) {
                tex_begin_diagnostic();
                tex_print_str("[balance: specification presets]");
                tex_end_diagnostic();
            }
        }
        if (specification_presets(passes)) {
            subpass = 1;
        }
    } else if (properties->pretolerance >= 0) {
        pass = balance_first_pass;
        lmt_balance_state.threshold = properties->pretolerance;
    } else {
        pass = balance_second_pass;
        lmt_balance_state.threshold = properties->tolerance;
    }
    if (lmt_balance_state.callback_id) {
        tex_aux_balance_callback_initialize(lmt_balance_state.callback_id, properties->balance_checks, subpasses);
    }
    while (1) {
        halfword current = first;
        int artificial = 0;
        switch (pass) {
            case balance_no_pass:
                goto DONE;
            case balance_first_pass:
                if (properties->tracing_balancing > 0 || properties->tracing_passes > 0) {
                    tex_begin_diagnostic();
                    tex_print_format("[balance: first pass, used tolerance %i]", lmt_balance_state.threshold);
                    // tex_end_diagnostic();
                }
                lmt_balance_state.passes.n_of_first_passes++;
                break;
            case balance_second_pass:
                if (tex_aux_emergency(properties)) {
                    lmt_balance_state.passes.n_of_second_passes++;
                    if (properties->tracing_balancing > 0 || properties->tracing_passes > 0) {
                        tex_begin_diagnostic();
                        tex_print_format("[balance: second pass, used tolerance %i]", lmt_balance_state.threshold);
                        // tex_end_diagnostic();
                    }
                    lmt_balance_state.force_check_hyphenation = 1;
                    break;
                } else {
                    pass = balance_final_pass;
                    /* fall through */
                }
            case balance_final_pass:
                lmt_balance_state.passes.n_of_final_passes++;
                if (properties->tracing_balancing > 0 || properties->tracing_passes > 0) {
                    tex_begin_diagnostic();
                    tex_print_format("[balance: final pass, used tolerance %i, used emergency stretch %p]", lmt_balance_state.threshold, properties->emergency_stretch);
                    // tex_end_diagnostic();
                }
                lmt_balance_state.force_check_hyphenation = 1;
                lmt_balance_state.background[total_stretch_amount] += properties->emergency_stretch;
                break;
            case balance_specification_pass:
                if (specification_presets(passes)) {
                    if (subpass <= passes_first_final(passes)) {
                        tex_aux_set_sub_pass_parameters(
                            properties, passes, subpass,
                            first,
                            properties->tracing_passes > 1,
                            tex_get_passes_features(passes,subpass),
                            0, 0, 0, 0, 0, 0, 0
                        );
                        lmt_balance_state.passes.n_of_specification_passes++;
                    }
                } else {
                    switch (subpass) {
                        case -2:
                            lmt_balance_state.threshold = properties->pretolerance;
                            lmt_balance_state.force_check_hyphenation = 0;
                            subpass = -1;
                            break;
                        case -1:
                            lmt_balance_state.threshold = properties->tolerance;
                            lmt_balance_state.force_check_hyphenation = 1;
                            subpass = 0;
                            break;
                        default:
                            lmt_balance_state.force_check_hyphenation = 1;
                            break;
                    }
                }
                if (properties->tracing_balancing > 0 || properties->tracing_passes > 0) {
                    tex_begin_diagnostic();
                    tex_print_format("[balance: specification subpass %i]\n", subpass);
                }
                lmt_balance_state.passes.n_of_sub_passes++;
                break;
        }
        if (lmt_balance_state.threshold > infinite_bad) {
            lmt_balance_state.threshold = infinite_bad; /* we can move this check to where threshold is set */
        }
        if (lmt_balance_state.callback_id) {
            tex_aux_balance_callback_start(lmt_balance_state.callback_id, properties->balance_checks, pass, subpass,
                tex_max_fitness(properties->fitness_classes), tex_med_fitness(properties->fitness_classes));
        }
        tex_aux_set_initial_active(properties);
        {
            halfword page = 1;
            scaled page_height;
            lmt_balance_state.current_page_number = page; /* we could just use this variable */
            if (properties->page_shape) {
                page_height = tex_get_specification_height(properties->page_shape, page);
            } else {
                page_height = lmt_balance_state.target_height;
            }
            lmt_balance_state.background[total_stretch_amount] -= lmt_balance_state.emergency_amount;
            if (lmt_balance_state.emergency_percentage) {
                scaled stretch = tex_xn_over_d(page_height, lmt_balance_state.emergency_percentage, scaling_factor);
                lmt_balance_state.background[total_stretch_amount] += stretch;
                lmt_balance_state.emergency_amount = stretch;
            } else {
                lmt_balance_state.emergency_amount = 0;
            }
            lmt_balance_state.background[total_advance_amount] -= lmt_balance_state.emergency_height_amount;
        }
        tex_aux_set_target_to_source(lmt_balance_state.active_height, lmt_balance_state.background);
        lmt_balance_state.passive = null;
        lmt_balance_state.printed_node = temp_head;
        lmt_balance_state.serial_number = 0;
        lmt_print_state.font_in_short_display = null_font;
        lmt_packaging_state.previous_char_ptr = null;
        switch (pass) {
            case balance_final_pass:
                artificial = 1;
                break;
            case balance_specification_pass:
                artificial = (subpass >= passes_first_final(passes)) || (subpass == subpasses);
                break;
            default:
                artificial = 0;
                break;
        }
        current = tex_aux_balance_list(properties, pass, subpass, current, first, artificial);
        if (! current) {
            scaled shortfall = tex_aux_try_balance(properties, eject_penalty, hyphenated_node, first, current, lmt_balance_state.callback_id, properties->balance_checks, pass, subpass, artificial);
            if (node_next(active_head) != active_head) {
                /*tex Find an active node with fewest demerits. */
                tex_aux_find_best_bet();
                if (pass == balance_specification_pass) {
                    /*tex This is where sub passes differ: we do a check. */
                    if (subpass < 0) {
                        goto HERE;
                    } else if (subpass < passes_first_final(passes)) {
                        goto DONE;
                    } else if (subpass < subpasses) {
                        int found = tex_aux_check_sub_pass(properties, shortfall, passes, subpass, subpasses, first);
                        if (found > 0) {
                            subpass = found;
                            goto HERE;
                        } else if (found < 0) {
                            goto DONE;
                        }
                    } else {
                        /* continue */
                    }
                }
                if (tex_aux_quit_balance(properties, pass)) {
                    goto DONE;
                }
            }
        }
        if (subpass <= passes_first_final(passes)) {
            ++subpass;
        }
      HERE:
        if (properties->tracing_balancing > 0 || properties->tracing_passes > 0) {
            tex_end_diagnostic(); // see above
        }
        tex_aux_clean_up_the_memory();
        switch (pass) {
            case balance_no_pass:
                /* just to be sure */
                goto DONE;
            case balance_first_pass:
                lmt_balance_state.threshold = properties->tolerance;
                pass = balance_second_pass;
                break;
            case balance_second_pass:
                pass = balance_final_pass;
                break;
            case balance_final_pass:
                pass = balance_no_pass;
                break;
            case balance_specification_pass:
                break;
        }
        if (lmt_balance_state.callback_id) {
            tex_aux_balance_callback_stop(lmt_balance_state.callback_id, properties->balance_checks);
        }
    }
    goto INDEED;
  DONE:
    if (lmt_balance_state.callback_id) {
        tex_aux_balance_callback_stop(lmt_balance_state.callback_id, properties->balance_checks);
    }
    if (properties->tracing_balancing > 0 || properties->tracing_passes > 0) {
        tex_end_diagnostic(); // see above
    }
  INDEED:
    if (properties->looseness && (! tracing_looseness_par) && (properties->looseness != lmt_balance_state.actual_looseness)) {
        tex_print_nlp();
        tex_print_format("%l[looseness: page %i, requested %i, actual %i]\n", lmt_balance_state.best_page - 1, properties->looseness, lmt_balance_state.actual_looseness);
    }
    tex_aux_post_balance(properties, lmt_balance_state.callback_id, properties->balance_checks, 0);
    tex_aux_clean_up_the_memory();
    if (lmt_balance_state.callback_id) {
        tex_aux_balance_callback_wrapup(lmt_balance_state.callback_id, properties->balance_checks);
    }
}

# define passive_next_break passive_prev_break

static void tex_aux_pre_balance(const balance_properties *properties, int callback_id, halfword checks, int state)
{
    /* todo */
}

static void tex_aux_post_balance(const balance_properties *properties, int callback_id, halfword checks, int state)
{
    int post_disc_break = 0;
    scaled cur_height = 0;
    halfword cur_p = null;
    halfword cur_page = 1;
    halfword q = active_break_node(lmt_balance_state.best_bet);
    halfword r;
    if (callback_id) {
        tex_aux_balance_callback_collect(callback_id, checks);
    }
    do {
        r = q;
        q = passive_prev_break(q);
        passive_next_break(r) = cur_p;
        cur_p = r;
    } while (q);
    if (callback_id) {
        halfword p = cur_p;
        while (p) {
            tex_aux_balance_callback_list(callback_id, checks, p);
            p = passive_next_break(p);
        }
    }
    do {
        halfword cur_disc = null;
        q = temp_head;
        /* prune */
 //     while (q) {
 //         switch (node_type(q)) {
 //             case hlist_node:
 //                 goto DONE;
 //             case glue_node:
 //                 goto DONE;
 //             case kern_node:
 //                 if (node_subtype(q) == explicit_kern_subtype) {
 //                     break;
 //                 } else {
 //                     goto DONE;
 //                 }
 //             default:
 //                 if (non_discardable(q)) {
 //                     goto DONE;
 //                 } else {
 //                     break;
 //                 }
 //         }
 //         q = node_next(q);
 //     }
 //   DONE:
        r = passive_cur_break(cur_p);
        q = temp_head; 
        q = node_next(temp_head); 
        post_disc_break = 0;
        if (r) {
         // switch (node_type(r)) {
         //     case glue_node:
         //      // tex_copy_glue_values(r, properties->right_skip);
         //      // node_subtype(r) = right_skip_glue;
         //      // glue_break = 1;
         //         /*tex |q| refers to the last node of the page */
         //         q = r;
         //      // rs = q;
         //         r = node_prev(r);
         //         /*tex |r| refers to the node after which the dir nodes should be closed */
         //         break;
         //     case disc_node:
         //         {
         //             halfword prv = node_prev(r);
         //             halfword nxt = node_next(r);
         //             halfword h = disc_no_break_head(r);
         //             if (h) {
         //                 tex_flush_node_list(h);
         //                 disc_no_break_head(r) = null;
         //                 disc_no_break_tail(r) = null;
         //             }
         //             h = disc_pre_break_head(r);
         //             if (h) {
         //                 halfword t = disc_pre_break_tail(r);
         //                 tex_set_discpart(r, h, t, glyph_discpart_pre);
         //                 tex_couple_nodes(prv, h);
         //                 tex_couple_nodes(t, r);
         //                 disc_pre_break_head(r) = null;
         //                 disc_pre_break_tail(r) = null;
         //             }
         //             h = disc_post_break_head(r);
         //             if (h) {
         //                 halfword t = disc_post_break_tail(r);
         //                 tex_set_discpart(r, h, t, glyph_discpart_post);
         //                 tex_couple_nodes(r, h);
         //                 tex_couple_nodes(t, nxt);
         //                 disc_post_break_head(r) = null;
         //                 disc_post_break_tail(r) = null;
         //                 post_disc_break = 1;
         //             }
         //             cur_disc = r;
         //          //  disc_break = 1;
         //         }
         //         break;
         //     case kern_node:
         //         kern_amount(r) = 0;
         //         break;
         // }
        } else {
            r = tex_tail_of_node_list(temp_head);
        }
node_next(temp_head) = node_next(r);
//        r = node_next(q);
//         node_next(q) = null;
         node_next(r) = null;
//        q = node_next(temp_head);
//        tex_try_couple_nodes(temp_head, r);
        if (properties->page_shape) {
            if (specification_count(properties->page_shape)) {
                cur_height = tex_get_specification_height(properties->page_shape, cur_page);
            } else {
                cur_height = lmt_balance_state.target_height;
            }
        } else {
            cur_height = lmt_balance_state.target_height;
        }
        if (properties->packing == packing_additional) {
            lmt_balance_state.just_box = tex_vpack(q, 0, packing_additional, 0, 0, holding_none_option, NULL);
        } else {
            lmt_balance_state.just_box = tex_vpack(q, cur_height, packing_exactly, 0, 0, holding_none_option, NULL);
        if (callback_id) {
            tex_aux_balance_callback_page(callback_id, checks, cur_page, cur_p);
        }
        }
        tex_tail_append(lmt_balance_state.just_box);
        ++cur_page;
        cur_p = passive_next_break(cur_p);
        if (cur_p && ! post_disc_break) {
            r = temp_head;
            while (1) {
                q = node_next(r);
                if (q == passive_cur_break(cur_p)) {
                    break;
                } else if (non_discardable(q)) {
                    break;
                } else if (node_type(q) == kern_node && ! (node_subtype(q) == explicit_kern_subtype)) {
                    break;
                }
                r = q;
            }
            if (r != temp_head) {
                node_next(r) = null;
                tex_flush_node_list(node_next(temp_head));
                tex_try_couple_nodes(temp_head, q);
            }
        }
        if (cur_disc) {
            tex_try_couple_nodes(node_prev(cur_disc),node_next(cur_disc));
            tex_flush_node(cur_disc);
        }
    } while (cur_p);
    if (cur_page != lmt_balance_state.best_page) {
     // tex_begin_diagnostic();
     // tex_print_format("[balance: dubious situation, current page %i is not best page %i]", cur_page, lmt_balance_state.best_page);
     // tex_end_diagnostic();
     // tex_confusion("balancing 1");
    } else if (node_next(temp_head)) {
        tex_confusion("balancing 2");
    }
}
