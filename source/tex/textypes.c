/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

// # ifdef __STDC_IEC_60559_DFP__
//     static _Decimal32 foo = 12; /* just a test to see if we have it */
// # endif 

void tex_dump_constants(dumpstream f)
{
    dump_via_int(f, max_n_of_toks_registers);
    dump_via_int(f, max_n_of_box_registers);
    dump_via_int(f, max_n_of_integer_registers);
    dump_via_int(f, max_n_of_dimension_registers);
    dump_via_int(f, max_n_of_attribute_registers);
    dump_via_int(f, max_n_of_posit_registers);
    dump_via_int(f, max_n_of_glue_registers);
    dump_via_int(f, max_n_of_muglue_registers);
    dump_via_int(f, max_n_of_bytecodes);
    dump_via_int(f, max_n_of_math_families);
    dump_via_int(f, max_n_of_math_classes);
    dump_via_int(f, max_n_of_catcode_tables);
    dump_via_int(f, max_n_of_box_indices);
    dump_via_int(f, max_n_of_marks);
    dump_via_int(f, max_n_of_inserts);
    dump_via_int(f, max_n_of_box_indices);
    dump_via_int(f, max_n_of_bytecodes);
    dump_via_int(f, max_n_of_math_families);
    dump_via_int(f, max_n_of_math_classes);
    /* */
    dump_via_int(f, max_chain_size);
}

static void tex_aux_check_constant(dumpstream f, int c)
{
    int x;
    undump_int(f, x);
    if (x != c) {
        tex_fatal_undump_error("inconsistent constant");
    }
}

void tex_undump_constants(dumpstream f)
{
    tex_aux_check_constant(f, max_n_of_toks_registers);
    tex_aux_check_constant(f, max_n_of_box_registers);
    tex_aux_check_constant(f, max_n_of_integer_registers);
    tex_aux_check_constant(f, max_n_of_dimension_registers);
    tex_aux_check_constant(f, max_n_of_attribute_registers);
    tex_aux_check_constant(f, max_n_of_posit_registers);
    tex_aux_check_constant(f, max_n_of_glue_registers);
    tex_aux_check_constant(f, max_n_of_muglue_registers);
    tex_aux_check_constant(f, max_n_of_bytecodes);
    tex_aux_check_constant(f, max_n_of_math_families);
    tex_aux_check_constant(f, max_n_of_math_classes);
    tex_aux_check_constant(f, max_n_of_catcode_tables);
    tex_aux_check_constant(f, max_n_of_box_indices);
    tex_aux_check_constant(f, max_n_of_marks);
    tex_aux_check_constant(f, max_n_of_inserts);
    tex_aux_check_constant(f, max_n_of_box_indices);
    tex_aux_check_constant(f, max_n_of_bytecodes);
    tex_aux_check_constant(f, max_n_of_math_families);
    tex_aux_check_constant(f, max_n_of_math_classes);
    /* */
    tex_aux_check_constant(f, max_chain_size);
}
