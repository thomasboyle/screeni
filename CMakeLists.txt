cmake_minimum_required(VERSION 3.21)

# Allow CI to override the version: -DSCREENI_VERSION=x.y.z
set(SCREENI_VERSION "1.0.0" CACHE STRING "Screeni version")
project(Screeni VERSION ${SCREENI_VERSION} LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network)

find_package(SQLite3 QUIET)
set(SCREENI_SQLITE_SOURCES "")
set(SCREENI_SQLITE_LIBS "")
if(SQLite3_FOUND)
  set(SCREENI_SQLITE_LIBS SQLite::SQLite3)
else()
  if(EXISTS "${CMAKE_SOURCE_DIR}/src/Screeni.Core/third_party/sqlite/sqlite3.c")
    list(APPEND SCREENI_SQLITE_SOURCES src/Screeni.Core/third_party/sqlite/sqlite3.c)
    include_directories(src/Screeni.Core/third_party/sqlite)
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c")
    list(APPEND SCREENI_SQLITE_SOURCES third_party/sqlite/sqlite3.c)
    include_directories(third_party/sqlite)
  else()
    message(FATAL_ERROR "SQLite3 not found")
  endif()
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/core/store.cpp")
  set(SCREENI_CORE_SOURCES src/core/store.cpp src/core/tracker.cpp)
  set(SCREENI_CORE_INCLUDE src/core)
else()
  set(SCREENI_CORE_SOURCES src/Screeni.Core/src/store.cpp src/Screeni.Core/src/tracker.cpp)
  set(SCREENI_CORE_INCLUDE src/Screeni.Core/src)
endif()

add_executable(Screeni WIN32
  src/main.cpp
  ${SCREENI_CORE_SOURCES}
  src/ui/MainWindow.cpp
  src/ui/OverviewPage.cpp
  src/ui/InsightsPage.cpp
  src/ui/SettingsPanel.cpp
  src/ui/BarChart.cpp
  src/services/TrayService.cpp
  src/services/Autostart.cpp
  src/services/UpdateService.cpp
  resources/resources.qrc
  ${SCREENI_SQLITE_SOURCES}
)

target_include_directories(Screeni PRIVATE src ${SCREENI_CORE_INCLUDE} src/ui src/services)
target_link_libraries(Screeni PRIVATE Qt6::Widgets Qt6::Network ${SCREENI_SQLITE_LIBS})
target_compile_definitions(Screeni PRIVATE SCREENI_VERSION="${PROJECT_VERSION}")

if(WIN32)
  target_link_libraries(Screeni PRIVATE user32 shell32 ole32 version advapi32 gdi32 dwmapi)
  target_compile_definitions(Screeni PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  if(MSVC)
    target_compile_options(Screeni PRIVATE /O1 /GL)
    target_link_options(Screeni PRIVATE /LTCG /OPT:REF /OPT:ICF)
  else()
    target_compile_options(Screeni PRIVATE -Os -ffunction-sections -fdata-sections)
    target_link_options(Screeni PRIVATE -Wl,--gc-sections -s)
  endif()
endif()

install(TARGETS Screeni RUNTIME DESTINATION bin)
message(STATUS "Screeni ${PROJECT_VERSION} — Qt ${Qt6_VERSION}")
