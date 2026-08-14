cmake_minimum_required(VERSION 3.21)
project(Screeni VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(SCREENI_STATIC_QT "Link Qt statically" OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network)

find_package(SQLite3 QUIET)
set(SCREENI_SQLITE_SOURCES "")
set(SCREENI_SQLITE_LIBS "")
if(SQLite3_FOUND)
  set(SCREENI_SQLITE_LIBS SQLite::SQLite3)
else()
  if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c")
    list(APPEND SCREENI_SQLITE_SOURCES third_party/sqlite/sqlite3.c)
    include_directories(third_party/sqlite)
  else()
    message(FATAL_ERROR "SQLite3 not found. Install libsqlite3-dev or place amalgamation in third_party/sqlite/")
  endif()
endif()

add_executable(Screeni WIN32
  src/main.cpp
  src/core/store.cpp
  src/core/tracker.cpp
  src/ui/MainWindow.cpp
  src/ui/OverviewPage.cpp
  src/ui/InsightsPage.cpp
  src/ui/SettingsPanel.cpp
  src/ui/BarChart.cpp
  src/services/TrayService.cpp
  src/services/Autostart.cpp
  resources/resources.qrc
  ${SCREENI_SQLITE_SOURCES}
)

target_include_directories(Screeni PRIVATE src src/core src/ui src/services)

target_link_libraries(Screeni PRIVATE Qt6::Widgets Qt6::Network ${SCREENI_SQLITE_LIBS})

if(WIN32)
  target_link_libraries(Screeni PRIVATE user32 shell32 ole32 version advapi32 gdi32)
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
install(DIRECTORY assets/ DESTINATION share/screeni/assets OPTIONAL)

message(STATUS "Screeni ${PROJECT_VERSION} — Qt ${Qt6_VERSION}")
