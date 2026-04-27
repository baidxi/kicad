find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(_NNG nng)
endif (PKG_CONFIG_FOUND)

FIND_PATH(NNG_INCLUDE_DIR
    NAMES
        nng/nng.h
    PATH_SUFFIXES
        include
    )

FIND_LIBRARY(NNG_LIBRARY
    NAMES
        nng
    PATH_SUFFIXES
        "lib"
        "local/lib"
    )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(nng
        REQUIRED_VARS NNG_INCLUDE_DIR NNG_LIBRARY)

# nng built with mbedTLS (e.g. MacPorts) requires these to be linked explicitly
find_library(MBEDTLS_LIBRARY NAMES mbedtls)
find_library(MBEDCRYPTO_LIBRARY NAMES mbedcrypto)
find_library(MBEDX509_LIBRARY NAMES mbedx509)
if(MBEDTLS_LIBRARY AND MBEDCRYPTO_LIBRARY AND MBEDX509_LIBRARY)
    set(NNG_LIBRARY ${NNG_LIBRARY} ${MBEDTLS_LIBRARY} ${MBEDCRYPTO_LIBRARY} ${MBEDX509_LIBRARY})
endif()

MARK_AS_ADVANCED(NNG_INCLUDE_DIR NNG_LIBRARY)
