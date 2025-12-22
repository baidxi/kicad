# PLM Git Plugin

set( PLM_GIT_SRCS
    plm_git_auth.cpp
    plm_git_forge.cpp
    plm_git_plugin.cpp
    plm_git_schema.cpp
)

add_library( plm-git MODULE ${PLM_GIT_SRCS} )

target_include_directories( plm-git PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries( plm-git
    kicommon
    ${wxWidgets_LIBRARIES}
)

set_target_properties( plm-git PROPERTIES
    PREFIX ""
    OUTPUT_NAME "plm-git"
)

if( APPLE )
    set_target_properties( plm-git PROPERTIES SUFFIX ".bundle" )
elseif( WIN32 )
    set_target_properties( plm-git PROPERTIES SUFFIX ".dll" )
else()
    set_target_properties( plm-git PROPERTIES SUFFIX ".so" )
endif()

install( TARGETS plm-git
    DESTINATION ${KICAD_PLUGINS}
    COMPONENT plugins
)
