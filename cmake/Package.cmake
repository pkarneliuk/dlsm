set(CPACK_GENERATOR "ZIP")
set(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_INSTALL_PREFIX})

set(CPACK_PACKAGE_NAME "DLSM")
set(CPACK_PACKAGE_VENDOR "DLSM")
set(CPACK_PACKAGE_VERSION ${DLSM_VERSION})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_DESCRIPTION "DLSM
    Version: ${CPACK_PACKAGE_VERSION} ${CMAKE_BUILD_TYPE}
    System: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}
    Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}
    Build: ${BUILD_TIME} ${COMMIT_HASH}")

set(CPACK_PACKAGE_INSTALL_DIRECTORY "dlsm")
