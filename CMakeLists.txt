cmake_minimum_required(VERSION 3.21)
project(CodeForge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(CODEFORGE_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Concurrent Network)
qt_standard_project_setup()

find_package(OpenSSL QUIET)  # Linux AES-256-GCM backend (Windows uses CNG/BCrypt)

file(GLOB_RECURSE CODEFORGE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

qt_add_library(CodeForgeLib STATIC EXCLUDE_FROM_ALL ${CODEFORGE_SOURCES})
target_include_directories(CodeForgeLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(CodeForgeLib PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::Network)

if(WIN32)
    target_link_libraries(CodeForgeLib PUBLIC bcrypt)   # Windows CNG (AES-256-GCM, random)
    target_compile_definitions(CodeForgeLib PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(OPENSSL_FOUND)
    target_link_libraries(CodeForgeLib PUBLIC OpenSSL::Crypto)
    target_compile_definitions(CodeForgeLib PUBLIC CF_HAVE_OPENSSL)
endif()

target_compile_definitions(CodeForgeLib PRIVATE CF_APP_VERSION="1.0.0" QT_MESSAGELOGCONTEXT)

if(MSVC)
    target_compile_options(CodeForgeLib PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
else()
    target_compile_options(CodeForgeLib PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(CodeForge WIN32 MACOSX_BUNDLE src/main.cpp)
target_link_libraries(CodeForge PRIVATE CodeForgeLib)
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
