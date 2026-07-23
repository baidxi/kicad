option( KICAD_SCRIPTING_EESCHEMA
    "Build the eeschema Python module for headless schematic scripting (requires pybind11)"
    OFF )

if( KICAD_SCRIPTING_EESCHEMA )
    find_package( pybind11 REQUIRED )

    set( EESCHEMA_PYTHON_SRCS
        eeschema_module.cpp
    )

    pybind11_add_module( _eeschema MODULE
        ${EESCHEMA_PYTHON_SRCS}
    )

    target_include_directories( _eeschema PRIVATE
        ${CMAKE_SOURCE_DIR}/eeschema
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}
    )

    target_link_libraries( _eeschema PRIVATE
        common
        eeschema_kiface_objects
        pads_common
        pcm
        markdown_lib
        core
        Boost::headers
        ${wxWidgets_LIBRARIES}
        ${NGSPICE_LIBRARY}
        kiapi
    )

    if( UNIX AND NOT APPLE )
        set( KICAD_PYTHON_MODULES "${KICAD_PLUGINS}" CACHE PATH
             "Directory for KiCad Python modules" )
    else()
        set( KICAD_PYTHON_MODULES "${KICAD_BIN}" CACHE PATH
             "Directory for KiCad Python modules" )
    endif()

    install( TARGETS _eeschema
        LIBRARY DESTINATION ${KICAD_PYTHON_MODULES}
    )
endif()
