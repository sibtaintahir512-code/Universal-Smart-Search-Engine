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

add_test(NAME UnitTestsRunner COMMAND UnitTestsRunner)
