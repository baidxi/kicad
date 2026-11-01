#!/bin/bash
# Source this file to set up the environment for running KiCad from ~/bin/kicad
# Usage: source setup-env.sh
#
# Layout notes (matching .github/workflows/build-packages.yml):
#   - The stock data path is compiled in as <prefix>/share/kicad, i.e.
#     ~/bin/kicad/share/kicad for this prefix.  The bundled libraries
#     (symbols/footprints/3dmodels) and the stock library tables in
#     template/ must therefore be installed into that tree, using the same
#     CMake-install method as the workflow's "Bundle stock libraries" step:
#       cmake -S <lib-repo> -B <lib-repo>-build -G Ninja \
#         -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
#         -DCMAKE_INSTALL_PREFIX="$HOME/bin/kicad"
#       cmake --build <lib-repo>-build && cmake --install <lib-repo>-build
#     (kicad-symbols needs the build step: its pack_symbols target generates
#     the packed *.kicad_sym libraries and the matching sym-lib-table.)
#   - With the tables in place, KiCad self-heals its global library tables on
#     first launch; LD_LIBRARY_PATH below covers the non-multiarch lib/
#     layout of a custom prefix.

export LD_LIBRARY_PATH="$HOME/bin/kicad/lib:$LD_LIBRARY_PATH"
export PATH="$HOME/bin/kicad/bin:$PATH"

echo "KiCad environment configured:"
echo "  LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
echo "  PATH: $PATH"
