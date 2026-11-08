cmake_minimum_required(VERSION 3.20)
project(UniversalSmartSearchEngine VERSION 1.0.0 LANGUAGES CXX)

# Enforce standard C++17 across all targets
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for VS Code IntelliSense / clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

# Windows / MSVC Specific Settings
if(MSVC)
    add_compile_options(/MP) # Multi-processor compilation
endif()

# Qt 6 Setup (Automoc, Autorcc, Autouic)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Core Gui)
find_package(ZLIB REQUIRED)

# ==============================================================================
# Target 1: DsaLib (Pure custom algorithmic data structures - Zero STL)
# ==============================================================================
add_library(DsaLib STATIC
    src/dsa/CustomPostingList.cpp
    src/dsa/CustomTrie.cpp
)

target_include_directories(DsaLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_project_warnings(DsaLib)
enable_sanitizers(DsaLib)

# ==============================================================================
# Target 2: SearchEngineCore (Catalog, Indexing, Parsing, Ranking, Persistence)
# ==============================================================================
add_library(SearchEngineCore STATIC
    src/core/CommonTypes.cpp
    src/indexing/FileMetadataManager.cpp
    src/indexing/DocumentCatalog.cpp
    src/indexing/FileChangeDetector.cpp
    src/indexing/ScanStatistics.cpp
    src/indexing/FileScanner.cpp
    src/indexing/ScanWorker.cpp
    src/extraction/ExtractionResult.cpp
    src/extraction/ExtractionCache.cpp
    src/extraction/TextExtractor.cpp
    src/extraction/SourceCodeExtractor.cpp
    src/extraction/OfficeExtractor.cpp
    src/extraction/PDFExtractor.cpp
    src/extraction/ImageMetadataExtractor.cpp
    src/extraction/MediaMetadataExtractor.cpp
    src/extraction/ArchiveMetadataExtractor.cpp
    src/extraction/ExtractionManager.cpp
)

target_include_directories(SearchEngineCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SearchEngineCore PUBLIC
    DsaLib
    ZLIB::ZLIB
)

set_project_warnings(SearchEngineCore)
enable_sanitizers(SearchEngineCore)

# ==============================================================================
# Target 3: SearchEngineApp (Qt 6 Desktop Executable)
# ==============================================================================
add_executable(SearchEngineApp
    src/main.cpp
)

target_include_directories(SearchEngineApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SearchEngineApp PRIVATE
    SearchEngineCore
    DsaLib
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

set_project_warnings(SearchEngineApp)
enable_sanitizers(SearchEngineApp)

# ==============================================================================
# Target 4: UnitTestsRunner (Test Suite)
# ==============================================================================
enable_testing()
add_subdirectory(tests)
