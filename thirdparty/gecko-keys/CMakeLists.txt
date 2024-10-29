if( APPLE )
elseif( UNIX )
    set( PLATFORM_SRCS
        x11/keysym2ucs.c
    )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

    find_package(X11 REQUIRED)

    # Detect GTK3 and configure it
    set( PLATFORM_LIBS
        ${GTK3_LIBRARIES}
        ${X11_LIBRARIES}
    )

    include_directories( SYSTEM ${GTK3_INCLUDE_DIRS} )
    link_directories( ${GTK3_LIBRARY_DIRS} )
    add_definitions( ${GTK3_CFLAGS_OTHER} )
endif()

add_library( gecko-keys OBJECT
    gecko_keys.cpp
    ${PLATFORM_SRCS}
)

target_link_libraries( gecko-keys
    ${PLATFORM_LIBS}
)

target_include_directories( gecko-keys
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)