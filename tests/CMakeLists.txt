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
