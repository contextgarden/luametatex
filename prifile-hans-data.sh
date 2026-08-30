# This setup is a generic one used on my linux boxes.

export LMT_SETUP_CMD=

export CONTEXTROOT=/data/context

export          LMT_CONTEXT_BIN=$CONTEXTROOT/tex/texmf-linux-64/bin
export    LMT_MANUAL_LUAMETATEX=$CONTEXTROOT/tex/texmf-context/doc/context/sources/general/manuals/luametatex/luametatex.tex
export    LMT_MANUAL_LUAMETAFUN=$CONTEXTROOT/tex/texmf-context/doc/context/sources/general/manuals/luametafun/luametafun.tex
export LMT_MANUAL_MATHINCONTEXT=$CONTEXTROOT/tex/texmf-context/doc/context/sources/general/manuals/mathincontext/mathincontext.tex
export  LMT_PROFILE_RUNTIME_DIR=

printenv | grep LMT