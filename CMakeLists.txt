cmake_minimum_required(VERSION 3.21)
project(CodeForge VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(CODEFORGE_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Concurrent Network Sql)
qt_standard_project_setup()

# Apple removed the legacy AGL framework from recent macOS SDKs, but Qt still
# adds "-framework AGL" to Qt6::Gui's interface link libraries, which fails at
# link time ("framework 'AGL' not found"). Qt's macOS GL backend does not use
# AGL at runtime, so drop it from the imported target's dependencies.
if(APPLE)
    get_target_property(_cf_gui_iface Qt6::Gui INTERFACE_LINK_LIBRARIES)
    if(_cf_gui_iface)
        list(FILTER _cf_gui_iface EXCLUDE REGEX "AGL")
        set_target_properties(Qt6::Gui PROPERTIES INTERFACE_LINK_LIBRARIES "${_cf_gui_iface}")
    endif()
    get_target_property(_cf_gui_link Qt6::Gui IMPORTED_LINK_DEPENDENT_LIBRARIES)
    if(_cf_gui_link)
        list(FILTER _cf_gui_link EXCLUDE REGEX "AGL")
        set_target_properties(Qt6::Gui PROPERTIES IMPORTED_LINK_DEPENDENT_LIBRARIES "${_cf_gui_link}")
    endif()
endif()

if(APPLE)
    # Homebrew's libcrypto is single-arch and would break universal builds,
    # and macOS exposes no OS-provided GCM API from C. Crypto::available()
    # then reports false on macOS (Windows uses CNG, Linux uses OpenSSL).
    set(OPENSSL_FOUND OFF)
else()
    find_package(OpenSSL QUIET)  # Linux AES-256-GCM backend (Windows uses CNG/BCrypt)
endif()

file(GLOB_RECURSE CODEFORGE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

qt_add_library(CodeForgeLib STATIC EXCLUDE_FROM_ALL ${CODEFORGE_SOURCES})
target_include_directories(CodeForgeLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(CodeForgeLib PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::Network Qt6::Sql)

if(WIN32)
    target_link_libraries(CodeForgeLib PUBLIC bcrypt)   # Windows CNG (AES-256-GCM, random)
    target_link_libraries(CodeForgeLib PUBLIC psapi)   # process metrics (profiler)
    target_compile_definitions(CodeForgeLib PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(OPENSSL_FOUND)
    target_link_libraries(CodeForgeLib PUBLIC OpenSSL::Crypto)
    target_compile_definitions(CodeForgeLib PUBLIC CF_HAVE_OPENSSL)
endif()

target_compile_definitions(CodeForgeLib PRIVATE CF_APP_VERSION="1.2.0" QT_MESSAGELOGCONTEXT)

if(MSVC)
    target_compile_options(CodeForgeLib PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
else()
    target_compile_options(CodeForgeLib PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(CodeForge WIN32 MACOSX_BUNDLE src/main.cpp)
target_link_libraries(CodeForge PRIVATE CodeForgeLib)
if(MSVC)
    # The manifest is embedded through src/app/codeforge.rc (asInvoker, UTF-8
    # code page, DPI awareness). The linker's auto-generated manifest would
    # collide with it (CVT1100 duplicate resource), so disable it.
    target_link_options(CodeForge PRIVATE "/MANIFEST:NO")
endif()
if(WIN32)
    # Embedded VERSIONINFO + icon + manifest (SmartScreen / Explorer metadata).
    enable_language(RC)
    target_sources(CodeForge PRIVATE src/app/codeforge.rc)
endif()
set_target_properties(CodeForge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    QT_RESOURCE_PREFIX "/codeforge")

if(APPLE)
    set_target_properties(CodeForge PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.secretarrow.codeforge"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_NAME "CodeForge")
endif()

# Manual qrc with explicit aliases (correct mapping to /themes/<name>.json).
target_sources(CodeForgeLib PRIVATE src/resources.qrc)

if(CODEFORGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS CodeForge
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION .
    LIBRARY DESTINATION lib)
