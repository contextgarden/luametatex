# This setup is used on my laptop where I (cross) compile under wsl.

# When I cross compile under WSL running the profiler works best on the windows
# end so we call cmd but with a setup that sets the path. That also sets the root
# of the development tree that is not organized like TDS.

# We need to set a path in order to pick up mtxrun, context and luametatex.

export LMT_SETUP_CMD=c:/data/setup/console-wsl.cmd

# Here is where my development files are, they are not in a TDS structure and
# overload those in an installed tree (that I never update anyway).

export CONTEXTROOT=/mnt/c/data/develop/context

# We need to run in a tree and pickup texmf-local/web2c/texmfcnf.lua that
# extends the paths so that we pick up the latest greatest.

export LMT_CONTEXT_BIN=/mnt/c/data/develop/tex-context/tex/texmf-win64/bin

# Here is where the latest manuals are.

export LMT_MANUAL_LUAMETATEX=$CONTEXTROOT/manuals/mkiv/external/luametatex/luametatex.tex
export LMT_MANUAL_LUAMETAFUN=$CONTEXTROOT/manuals/mkiv/external/luametafun/luametafun.tex
export LMT_MANUAL_MATHINCONTEXT=$CONTEXTROOT/manuals/mkiv/external/mathincontext/mathincontext.tex

printenv | grep LMT