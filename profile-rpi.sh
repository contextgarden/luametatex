# This setup is used on the raspberrypi build machine.

#      LMT_SETUP_CMD=
export LMT_CONTEXT_BIN=~/data/context/tex/texmf-aarch64/bin/luametatex
export LMT_MANUAL_LUAMETATEX=~/data/context/tex/texmf-context/doc/context/sources/general/manuals/luametatex/luametatex.tex
export LMT_MANUAL_LUAMETAFUN=~/data/context/tex/texmf-context/doc/context/sources/general/manuals/luametafun/luametafun.tex
export LMT_MANUAL_MATHINCONTEXT=

printenv | grep LMT