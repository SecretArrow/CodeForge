# CodeForge unit tests (QtTest)
# Each test file uses QTEST_GUILESS_MAIN and builds as its own executable.
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test)

function(cf_add_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE CodeForgeLib Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

cf_add_test(TestFuzzyMatch ${TEST_DIR}/test_fuzzymatch.cpp)
cf_add_test(TestEncoding ${TEST_DIR}/test_encoding.cpp)
cf_add_test(TestTextDocument ${TEST_DIR}/test_textdocument.cpp)
cf_add_test(TestCrypto ${TEST_DIR}/test_crypto.cpp)
cf_add_test(TestSettingsTheme ${TEST_DIR}/test_settings_theme.cpp)
cf_add_test(TestSearch ${TEST_DIR}/test_search.cpp)
cf_add_test(TestMultiCursor ${TEST_DIR}/test_multicursor.cpp)
cf_add_test(TestMarkdown ${TEST_DIR}/test_markdown.cpp)
cf_add_test(TestEditorConfig ${TEST_DIR}/test_editorconfig.cpp)
cf_add_test(TestJsonRpc ${TEST_DIR}/test_jsonrpc.cpp)
