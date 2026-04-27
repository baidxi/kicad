# PLM QA Tests

set( PLM_TEST_SRCS
    test_module.cpp
    test_plm_conformance.cpp
    test_plm_auth_conformance.cpp
    test_plm_navigation_conformance.cpp
    test_plm_operations_conformance.cpp
    test_plm_integration.cpp
    test_plm_thread_safety.cpp
    test_plm_batch_operations.cpp
    test_plm_error_handling.cpp
    test_plm_parts.cpp
    test_plm_libs.cpp
    test_plm_notifications.cpp
    plm_conformance_framework.cpp
    mock_plm_server.cpp
)

if( WIN32 )
    list( APPEND PLM_TEST_SRCS
        ${CMAKE_SOURCE_DIR}/common/pgm_base.cpp
    )
endif()

add_executable( qa_plm ${PLM_TEST_SRCS} )

target_link_libraries( qa_plm
    qa_utils
    kicommon
    Boost::unit_test_framework
    ${wxWidgets_LIBRARIES}
)

target_include_directories( qa_plm PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/qa/qa_utils
    ${CMAKE_SOURCE_DIR}/qa/qa_utils/include
    ${CMAKE_SOURCE_DIR}/qa/mocks/include
)

kicad_add_boost_test( qa_plm qa_plm )
