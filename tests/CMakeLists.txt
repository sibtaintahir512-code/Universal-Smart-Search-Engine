# tests/CMakeLists.txt

add_executable(UnitTestsRunner
    unit/Test_Main.cpp
)

target_include_directories(UnitTestsRunner PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(UnitTestsRunner PRIVATE
    SearchEngineCore
    DsaLib
)

set_project_warnings(UnitTestsRunner)
enable_sanitizers(UnitTestsRunner)

add_test(NAME UnitTestsRunner COMMAND UnitTestsRunner)
