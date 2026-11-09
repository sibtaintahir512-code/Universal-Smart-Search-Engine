# tests/CMakeLists.txt

add_executable(UnitTestsRunner
    unit/Test_Main.cpp
    unit/Test_DynamicArray.cpp
    unit/Test_SinglyLinkedList.cpp
    unit/Test_HashTable.cpp
    unit/Test_PostingList.cpp
    unit/Test_Trie.cpp
    unit/Test_MaxHeap.cpp
    unit/Test_MinHeap.cpp
    unit/Test_Stack.cpp
    unit/Test_Queue.cpp
    unit/Test_FileMetadataManager.cpp
    unit/Test_DocumentCatalog.cpp
    unit/Test_FileChangeDetector.cpp
    unit/Test_ScanStatistics.cpp
    unit/Test_FileScanner.cpp
    unit/Test_ScanWorker.cpp
    unit/Test_TextExtractor.cpp
    unit/Test_SourceCodeExtractor.cpp
    unit/Test_OfficeExtractor.cpp
    unit/Test_PDFExtractor.cpp
    unit/Test_ImageMetadataExtractor.cpp
    unit/Test_MediaMetadataExtractor.cpp
    unit/Test_ArchiveMetadataExtractor.cpp
    unit/Test_ExtractionCache.cpp
    unit/Test_ExtractionManager.cpp
    unit/Test_Phase41_Hardening.cpp
    unit/Test_Phase42_Compliance.cpp
    unit/Test_TextNormalizer.cpp
    unit/Test_Tokenizer.cpp
    unit/Test_InvertedIndex.cpp
    unit/Test_IndexingPipeline.cpp
    unit/Test_IndexIntegrity.cpp
    unit/Test_Phase5_Benchmarks.cpp
)

target_include_directories(UnitTestsRunner PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(UnitTestsRunner PRIVATE
    SearchEngineCore
    DsaLib
)

set_project_warnings(UnitTestsRunner)
enable_sanitizers(UnitTestsRunner)

if(APPLE)
    set_target_properties(UnitTestsRunner PROPERTIES
        BUILD_RPATH "${CMAKE_SOURCE_DIR}/third_party/pdfium/lib"
        INSTALL_RPATH "@executable_path/../lib;${CMAKE_SOURCE_DIR}/third_party/pdfium/lib"
    )
endif()

if(WIN32)
    add_custom_command(TARGET UnitTestsRunner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/pdfium/windows-x64/bin/pdfium.dll"
        $<TARGET_FILE_DIR:UnitTestsRunner>
    )
endif()

add_test(NAME UnitTestsRunner COMMAND UnitTestsRunner)
