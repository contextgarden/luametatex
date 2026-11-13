/*
    See license.txt in the root of this project.
*/

/* 
    This is an placeholder for upcoming code. It is unlikely to happen in 2026 because I first 
    want to make sure that the generalized scanner works as expected. At the \LUA\ end we can 
    be less \TEX\ compliant but at the \TEX\ end we should do what is expected. 
*/

# include "luametatex.h"

typedef enum private_idea_keys { 
    private_idea_key_no_match = 0,
    /* */ 
    private_idea_key_depth,
    private_idea_key_height,
    private_idea_key_okay,
    private_idea_key_width,
    /* */
    private_idea_key_left_brace,
} private_idea_keys;

static const char *private_idea_words[] = { 
    NULL,
    /* */
    "depth",
    "height",
    "okay",
    "width",
};

void tex_set_idea_matcher(void)
{
    lmt_matcher_items[private_idea_matcher] = (matcher_item) { 
        .words   = private_idea_words, 
        .name    = "idea",
        .first   = private_idea_key_depth,
        .last    = private_idea_key_width,
        .group   = private_idea_key_left_brace,
        .options = keyword_exact_option 
                 | keyword_terminal_option 
                 | keyword_ascii_option, 
    };
}

void tex_show_idea(void)
{
 // int brace = 0;
    keyword_matcher *matcher = &lmt_matchers[private_idea_matcher];
    while (1) {
        switch (tex_matcher_use(matcher, 1)) {
            case private_idea_key_no_match:
                goto DONE;
            case private_idea_key_depth:
                {
                    scaled depth = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                    tex_print_format("%<idea depth : %p%>",depth);
                    break;
                }
            case private_idea_key_height:
                {
                    scaled height = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                    tex_print_format("%<idea height : %p%>", height);
                    break;
                }
            case private_idea_key_okay:
                {
                    tex_print_format("%<idea okay%>");
                    break;
                }
            case private_idea_key_width:
                {
                    scaled width = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                    tex_print_format("%<idea width : %p%>",width);
                    break;
                }
            case private_idea_key_left_brace:
             // brace = 1;
                goto DONE;
            default:
                goto DONE;
        }
    }
  DONE:
    tex_report_matchers();
}

void tex_initialize_ideas(void)
{
}

void tex_cleanup_ideas(void)
{
}
