cmake_minimum_required(VERSION 3.20)
project(Screeni.Core LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Screeni.Core SHARED
    src/api.cpp
    src/store.cpp
    src/tracker.cpp
    third_party/sqlite/sqlite3.c
)

target_include_directories(Screeni.Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite
)

target_compile_definitions(Screeni.Core
    PRIVATE
        SCREENI_CORE_EXPORTS
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        SQLITE_OMIT_LOAD_EXTENSION
        SQLITE_THREADSAFE=1
)

target_link_libraries(Screeni.Core
    PRIVATE
        user32
        kernel32
        advapi32
        version
        shell32
        ole32
        uuid
)

# Faster sqlite compile on CI runners (functional Release build, less optimizing).
if(DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS})
    set_source_files_properties(third_party/sqlite/sqlite3.c PROPERTIES COMPILE_OPTIONS "/O1")
endif()

# /W4 on our C++ only — sqlite3.c is third-party and too noisy under /W4.
if(MSVC)
    target_compile_options(Screeni.Core PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4>)
endif()

set_target_properties(Screeni.Core PROPERTIES
    OUTPUT_NAME "Screeni.Core"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
