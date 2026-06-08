# This program source code file is part of KiCad, a free EDA CAD application.
#
# Copyright (C) 2026 KiCad Developers
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.

add_executable( qa_ipc_smoke
    ipc_smoke.cpp
)

target_include_directories( qa_ipc_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries( qa_ipc_smoke
    kiapi
    ${NNG_LIBRARY}
)

kicad_add_utils_executable( qa_ipc_smoke )

