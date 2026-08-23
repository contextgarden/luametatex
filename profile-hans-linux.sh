# This setup is used on my laptop where I compile under wsl.

# I always have my data under /data so that is where we find the context tree
# too.

# There is no need to set a path etc:

export LMT_SETUP_CMD=

# Here is where my development files are, they are not in a TDS structure and
# overload those in an installed tree (that I never update anyway).

export CONTEXTROOT=/data/context

# We need to run in a tree and pickup texmf-local/web2c/texmfcnf.lua that
# extends the paths so that we pick up the latest greatest.

export LMT_CONTEXT_BIN=$CONTEXTROOT/tex-context/tex/texmf-linux-64/bin

# Here is where the latest manuals are.

export LMT_MANUAL_LUAMETATEX=$CONTEXTROOT/manuals/mkiv/external/luametatex/luametatex.tex
export LMT_MANUAL_LUAMETAFUN=$CONTEXTROOT/manuals/mkiv/external/luametafun/luametafun.tex
export LMT_MANUAL_MATHINCONTEXT=$CONTEXTROOT/manuals/mkiv/external/mathincontext/mathincontext.tex

printenv | grep LMT