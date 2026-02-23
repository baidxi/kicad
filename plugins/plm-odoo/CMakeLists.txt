# PLM Odoo Plugin

set( PLM_ODOO_SRCS
    odoo_error_mapper.cpp
    odoo_rpc_client.cpp
    plm_odoo_plugin.cpp
)

add_library( plm-odoo MODULE ${PLM_ODOO_SRCS} )

target_include_directories( plm-odoo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries( plm-odoo
    kicommon
    ${wxWidgets_LIBRARIES}
)

set_target_properties( plm-odoo PROPERTIES
    PREFIX ""
    OUTPUT_NAME "plm-odoo"
)

if( APPLE )
    set_target_properties( plm-odoo PROPERTIES SUFFIX ".bundle" )
elseif( WIN32 )
    set_target_properties( plm-odoo PROPERTIES SUFFIX ".dll" )
else()
    set_target_properties( plm-odoo PROPERTIES SUFFIX ".so" )
endif()

install( TARGETS plm-odoo
    DESTINATION ${KICAD_PLUGINS}
    COMPONENT plugins
)
