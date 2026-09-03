cmake_minimum_required(VERSION 3.21)
project(shinto-app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets Network Sql)

qt_add_executable(shinto-bin
  src/main.cpp
  src/WebProfile.cpp
  src/ThemeLoader.cpp
  src/HistoryStore.cpp
  src/OmniboxOverlay.cpp
  src/BrowserWindow.cpp
  src/SingletonServer.cpp
  src/SingletonClient.cpp
  resources/shinto.qrc
)

target_link_libraries(shinto-bin PRIVATE
  Qt6::Widgets
  Qt6::WebEngineWidgets
  Qt6::Network
  Qt6::Sql
)

include(GNUInstallDirs)
install(TARGETS shinto-bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
