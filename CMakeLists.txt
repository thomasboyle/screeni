cmake_minimum_required(VERSION 3.21)

# Allow CI to override the version: -DSCREENI_VERSION=x.y.z
set(SCREENI_VERSION "1.0.0" CACHE STRING "Screeni version")
project(Screeni VERSION ${SCREENI_VERSION} LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Size-optimize MSVC Release builds: replace the default /O2 with /O1 up front
# so the compiler doesn't emit a D9025 "/O2 overriding /O1" warning per file.
if(MSVC AND (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
  string(REPLACE "/O2" "/O1" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
  string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network)

find_package(SQLite3 QUIET)
set(SCREENI_SQLITE_SOURCES "")
set(SCREENI_SQLITE_LIBS "")
set(SCREENI_SQLITE_INCLUDE "")
if(SQLite3_FOUND)
  set(SCREENI_SQLITE_LIBS SQLite::SQLite3)
else()
  if(EXISTS "${CMAKE_SOURCE_DIR}/src/Screeni.Core/third_party/sqlite/sqlite3.c")
    list(APPEND SCREENI_SQLITE_SOURCES src/Screeni.Core/third_party/sqlite/sqlite3.c)
    list(APPEND SCREENI_SQLITE_INCLUDE src/Screeni.Core/third_party/sqlite)
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c")
    list(APPEND SCREENI_SQLITE_SOURCES third_party/sqlite/sqlite3.c)
    list(APPEND SCREENI_SQLITE_INCLUDE third_party/sqlite)
  else()
    message(FATAL_ERROR "SQLite3 not found")
  endif()
endif()

set(SCREENI_CORE_SOURCES src/Screeni.Core/src/store.cpp src/Screeni.Core/src/tracker.cpp)
set(SCREENI_CORE_INCLUDE src/Screeni.Core/src)

# Core (store + tracker + sqlite) is plain C/C++ — no Qt. Building it as a
# static library keeps the heavy Qt precompiled headers off these translation
# units, which is the biggest win for compile time.
add_library(ScreeniCore STATIC
  ${SCREENI_CORE_SOURCES}
  ${SCREENI_SQLITE_SOURCES}
)
target_include_directories(ScreeniCore PUBLIC ${SCREENI_CORE_INCLUDE} PRIVATE ${SCREENI_SQLITE_INCLUDE})

target_link_libraries(ScreeniCore PRIVATE ${SCREENI_SQLITE_LIBS})

if(WIN32)
  target_compile_definitions(ScreeniCore PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(ScreeniCore PRIVATE user32 shell32 ole32 version advapi32)
endif()

if(SCREENI_SQLITE_SOURCES)
  # Smaller/faster SQLite build. The app only uses sqlite3_open/close, prepare/
  # step/finalize, bind/column, exec, reset, clear_bindings and free (verified
  # in store.cpp/tracker.cpp); its schema also uses WAL, FK, AUTOINCREMENT,
  # UPSERT and PRAGMA, which are deliberately kept. Every other SQLITE_OMIT_*
  # here was verified to compile against the bundled 3.46.1 amalgamation
  # (SQLITE_OMIT_ANALYZE/REINDEX/VIEW/VACUUM/VIRTUALTABLE/WINDOWFUNC leave
  # unguarded declarations in this amalgamation, so they are not used).
  target_compile_definitions(ScreeniCore PRIVATE
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_BLOB_LITERAL
    SQLITE_OMIT_COMPILEOPTION_DIAGS
    SQLITE_OMIT_COMPLETE
    SQLITE_OMIT_DECLTYPE
    SQLITE_OMIT_EXPLAIN
    SQLITE_OMIT_GET_TABLE
    SQLITE_OMIT_INCRBLOB
    SQLITE_OMIT_INTEGRITY_CHECK
    SQLITE_OMIT_LOOKASIDE
    SQLITE_OMIT_MEMORYDB
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_OMIT_TRACE
    SQLITE_OMIT_UTF16
    SQLITE_OMIT_XFER_OPT
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_THREADSAFE=1
    SQLITE_TEMP_STORE=2
  )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" AND NOT MSVC)
  # MSVC Release /O1 is set globally above; non-MSVC needs per-target flags.
  target_compile_options(ScreeniCore PRIVATE -Os -ffunction-sections -fdata-sections)
endif()

add_executable(Screeni WIN32
  src/main.cpp
  src/ui/MainWindow.cpp
  src/ui/OverviewPage.cpp
  src/ui/InsightsPage.cpp
  src/ui/SettingsPanel.cpp
  src/ui/BarChart.cpp
  src/services/TrayService.cpp
  src/services/Autostart.cpp
  src/services/UpdateService.cpp
)

# Embed the app icon (and other Windows resources) into the exe so the
# taskbar / Alt-Tab show the Screeni icon instead of the generic default.
if(WIN32)
  target_sources(Screeni PRIVATE src/app.rc)
endif()

target_include_directories(Screeni PRIVATE src ${SCREENI_CORE_INCLUDE} src/ui src/services)
target_link_libraries(Screeni PRIVATE Qt6::Widgets Qt6::Network ScreeniCore)
target_compile_definitions(Screeni PRIVATE SCREENI_VERSION="${PROJECT_VERSION}")

if(WIN32)
  target_link_libraries(Screeni PRIVATE user32 shell32 ole32 version advapi32 gdi32 dwmapi)
  target_compile_definitions(Screeni PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  if(MSVC)
    # No /GL + /LTCG: whole-program optimization costs ~10s of link time for a
    # handful of KB of savings; /OPT:REF + /OPT:ICF keep the exe small without it.
    target_link_options(Screeni PRIVATE /OPT:REF /OPT:ICF)
  else()
    target_compile_options(Screeni PRIVATE -Os -ffunction-sections -fdata-sections)
    target_link_options(Screeni PRIVATE -Wl,--gc-sections -s)
  endif()
endif()

# Precompile the Qt headers — every UI translation unit includes them, so this
# cuts the bulk of compile time on each source file. Disable only if a caching
# compiler launcher (ccache) is used, which cannot reuse MSVC precompiled
# headers.
option(SCREENI_USE_PCH "Precompile Qt headers to speed up compilation" ON)
if(SCREENI_USE_PCH)
  target_precompile_headers(Screeni PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h)
endif()

install(TARGETS Screeni RUNTIME DESTINATION bin)
message(STATUS "Screeni ${PROJECT_VERSION} — Qt ${Qt6_VERSION}")