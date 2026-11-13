cmake_minimum_required(VERSION 3.20)
project(UniversalSmartSearchEngine VERSION 1.0.0 LANGUAGES CXX C)

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
# PDFium Dependency Setup
# ==============================================================================
if(WIN32)
    set(PDFIUM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/windows-x64/include")
    set(PDFIUM_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/windows-x64/lib/pdfium.dll.lib")
else()
    set(PDFIUM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include")
    set(PDFIUM_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/libpdfium.dylib")
endif()

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
    src/indexing/TextNormalizer.cpp
    src/indexing/Tokenizer.cpp
    src/indexing/InvertedIndex.cpp
    src/indexing/IndexingPipeline.cpp
    src/indexing/IndexingStatistics.cpp
    src/indexing/IndexIntegrityChecker.cpp
    src/search/ResultSet.cpp
    src/search/QueryLexer.cpp
    src/search/QueryParser.cpp
    src/search/QueryEvaluator.cpp
    src/search/SearchStatistics.cpp
    src/search/SearchService.cpp
    src/search/SearchIntegrityChecker.cpp
    src/ranking/DocumentLengthStatistics.cpp
    src/ranking/TFIDFScorer.cpp
    src/ranking/BM25Scorer.cpp
    src/ranking/TopKSelector.cpp
    src/ranking/RankingStatistics.cpp
    src/ranking/RankingEngine.cpp
    src/persistence/CRC32.cpp
    src/persistence/PersistenceStatistics.cpp
    src/persistence/IndexSerializer.cpp
    src/persistence/IndexDeserializer.cpp
    src/persistence/IndexCompactor.cpp
    src/persistence/IndexRecoveryManager.cpp
    src/persistence/IndexPersistenceManager.cpp
    src/extraction/ExtractionResult.cpp
    src/extraction/ExtractionCache.cpp
    src/extraction/TextExtractor.cpp
    src/extraction/SourceCodeExtractor.cpp
    src/extraction/OfficeExtractor.cpp
    src/extraction/PDFExtractor.cpp
    src/extraction/PDFDocumentParser.cpp
    src/extraction/ImageMetadataExtractor.cpp
    src/extraction/MediaMetadataExtractor.cpp
    src/extraction/ArchiveMetadataExtractor.cpp
    src/extraction/ExtractionManager.cpp
    third_party/pugixml/pugixml.cpp
    third_party/miniz/miniz.c
)

target_include_directories(SearchEngineCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pugixml
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz
    ${PDFIUM_INCLUDE_DIR}
)

target_link_libraries(SearchEngineCore PUBLIC
    DsaLib
    ZLIB::ZLIB
    ${PDFIUM_LIBRARY}
)

set_project_warnings(SearchEngineCore)
enable_sanitizers(SearchEngineCore)

# ==============================================================================
# Target 3: GuiLib (Qt 6 Desktop GUI Library & Widgets)
# ==============================================================================
add_library(GuiLib STATIC
    include/gui/adapters/SearchEngineAdapter.hpp
    include/gui/models/SearchResultModel.hpp
    include/gui/widgets/SearchResultDelegate.hpp
    include/gui/widgets/SearchBar.hpp
    include/gui/widgets/FilterPanel.hpp
    include/gui/widgets/PreviewPanel.hpp
    include/gui/widgets/MetadataPanel.hpp
    include/gui/workers/WorkerTypes.hpp
    include/gui/workers/SearchWorker.hpp
    include/gui/workers/IndexingWorker.hpp
    include/gui/workers/PreviewWorker.hpp
    include/gui/workers/PersistenceWorker.hpp
    include/gui/dialogs/IndexManagementDialog.hpp
    include/gui/dialogs/DashboardDialog.hpp
    include/gui/dialogs/SettingsDialog.hpp
    include/gui/ThemeManager.hpp
    include/gui/MainWindow.hpp
    src/gui/adapters/SearchEngineAdapter.cpp
    src/gui/models/SearchResultModel.cpp
    src/gui/widgets/SearchResultDelegate.cpp
    src/gui/widgets/SearchBar.cpp
    src/gui/widgets/FilterPanel.cpp
    src/gui/widgets/PreviewPanel.cpp
    src/gui/widgets/MetadataPanel.cpp
    src/gui/workers/SearchWorker.cpp
    src/gui/workers/IndexingWorker.cpp
    src/gui/workers/PreviewWorker.cpp
    src/gui/workers/PersistenceWorker.cpp
    src/gui/dialogs/IndexManagementDialog.cpp
    src/gui/dialogs/DashboardDialog.cpp
    src/gui/dialogs/SettingsDialog.cpp
    src/gui/ThemeManager.cpp
    src/gui/MainWindow.cpp
)

target_include_directories(GuiLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(GuiLib PUBLIC
    SearchEngineCore
    DsaLib
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

set_project_warnings(GuiLib)
enable_sanitizers(GuiLib)

# ==============================================================================
# Target 4: SearchEngineApp (Qt 6 Desktop Executable)
# ==============================================================================
add_executable(SearchEngineApp
    src/main.cpp
)

target_include_directories(SearchEngineApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SearchEngineApp PRIVATE
    GuiLib
)

set_project_warnings(SearchEngineApp)
enable_sanitizers(SearchEngineApp)

if(APPLE)
    set_target_properties(SearchEngineApp PROPERTIES
        OUTPUT_NAME "Universal Smart Search Engine"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Universal Smart Search Engine"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.sibtain.universalsearchengine"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib"
        INSTALL_RPATH "@executable_path/../Frameworks;@executable_path/../lib;${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib"
    )

    install(TARGETS SearchEngineApp BUNDLE DESTINATION /Applications)
endif()

if(WIN32)
    add_custom_command(TARGET SearchEngineApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/windows-x64/bin/pdfium.dll"
        $<TARGET_FILE_DIR:SearchEngineApp>
    )
endif()

# ==============================================================================
# Target 4: UnitTestsRunner (Test Suite)
# ==============================================================================
enable_testing()
add_subdirectory(tests)
