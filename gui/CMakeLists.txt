cmake_minimum_required(VERSION 3.16)
project(GoofyDeckGUI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/DeckWidget.cpp
    src/ButtonWidget.cpp
    src/GoofyDeckClient.cpp
    src/ConfigEditor.cpp
    src/ConfigEditorDialog.cpp
    src/YamlParser.cpp
    src/IconCache.cpp
    src/ButtonEditDialog.cpp
    src/PageCreateDialog.cpp
    src/PageEditDialog.cpp
    src/YamlHighlighter.cpp
    src/PresetEditDialog.cpp
    src/HaClient.cpp
    src/StateManager.cpp
)

set(HEADERS
    src/MainWindow.h
    src/DeckWidget.h
    src/ButtonWidget.h
    src/GoofyDeckClient.h
    src/ConfigEditor.h
    src/ConfigEditorDialog.h
    src/YamlParser.h
    src/IconCache.h
    src/ButtonEditDialog.h
    src/PageCreateDialog.h
    src/PageEditDialog.h
    src/YamlHighlighter.h
    src/PresetEditDialog.h
    src/HaClient.h
    src/StateManager.h
)

add_executable(goofydeck-gui ${SOURCES} ${HEADERS})

target_link_libraries(goofydeck-gui PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

# Install target
install(TARGETS goofydeck-gui DESTINATION bin)
