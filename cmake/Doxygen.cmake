option(ENABLE_DOXYGEN "Enable generation Doxygen docs" OFF)
find_package(Doxygen REQUIRED dot)
if((ENABLE_DOXYGEN OR DEFINED ENV{ENABLE_DOXYGEN}) AND Doxygen_FOUND)
    set(DOXYGEN_GENERATE_HTML   YES)
    set(DOXYGEN_HTML_OUTPUT     docs)
    set(DOXYGEN_RECURSIVE       YES)
    set(DOXYGEN_QUIET           YES)
    set(DOXYGEN_EXTRACT_ALL     YES)
    set(DOXYGEN_CALL_GRAPH      YES)
    set(DOXYGEN_CALLER_GRAPH    YES)
    set(DOXYGEN_HAVE_DOT        YES)
    set(DOXYGEN_DOT_FONTSIZE    12)
    set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)

    doxygen_add_docs(docs
        include src ${CMAKE_CURRENT_BINARY_DIR}/include
        ALL COMMENT "Generate Doxygen docs in ${CMAKE_CURRENT_BINARY_DIR}/${DOXYGEN_HTML_OUTPUT}")
endif()