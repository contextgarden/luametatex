/*
    See license.txt in the root of this project.
*/

# ifndef LMT_BALANCE_H
# define LMT_BALANCE_H

typedef struct balance_state_info {
    halfword     just_box;
    int          no_shrink_error_yet;
    int          threshold;
    halfword     quality;
    int          callback_id;
 // int          force_check_hyphenation;
    scaled       extra_background_stretch;
    scaled       extra_background_shrink;
    halfword     passive;
    halfword     printed_node;
    halfword     serial_number;
    scaled       active_height[n_of_glue_amounts];
    scaled       background[n_of_glue_amounts];
    scaled       break_height[n_of_glue_amounts];
 // scaled       disc_height[n_of_glue_amounts];
    scaled       fill_height[4];
    fitcriterion minimal_demerits;
    halfword     minimum_demerits;
    halfword     easy_page;
    halfword     last_special_page;
    scaled       target_height; 
    scaled       emergency_amount;
    halfword     emergency_percentage;
    halfword     emergency_factor;
    scaled       emergency_height_amount;
    halfword     best_bet;
    halfword     fewest_demerits;
    halfword     best_page;
    halfword     actual_looseness;
    halfword     warned;
    break_passes passes;
    int          artificial_encountered; 
    int          current_page_number; /* check if we can use something else */
} balance_state_info;

extern balance_state_info lmt_balance_state; /* can be private */

typedef enum balance_quality_states { 
    page_is_overfull  = 0x0200, /*tex We use the same values |par_is_overfull|. */
    page_is_underfull = 0x0400, /*tex We use the same values |par_is_underfull|. */
} balance_quality_states;

extern void tex_balance_preset (
    balance_properties *properties
);

extern void tex_balance_reset (
    balance_properties *properties
);

extern void tex_balance (
    balance_properties *properties,
    halfword head
);

# endif
