cmake_minimum_required(VERSION 3.15)
include(FeatureSummary)

project(triegen
        VERSION 0.1.0
        LANGUAGES CXX
)

find_package(Boost REQUIRED COMPONENTS Headers)

set(PROJECT_LIB "${PROJECT_NAME}-lib")

add_library(${PROJECT_LIB} INTERFACE)
target_include_directories(${PROJECT_LIB} INTERFACE include)
target_compile_definitions(${PROJECT_LIB} INTERFACE $<$<BOOL:${WIN32}>:BOOST_ALL_NO_LIB>)
target_link_libraries(${PROJECT_LIB} INTERFACE Boost::headers RapidJSON::RapidJSON)

function(make_generator_exe name)
    add_executable(${PROJECT_NAME}-${name} src/${name}.cpp)
    set_target_properties(${PROJECT_NAME}-${name}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
            CXX_STANDARD 20
    )
    target_link_libraries(${PROJECT_NAME}-${name} PRIVATE ${PROJECT_LIB})
endfunction()

make_generator_exe(emoji)

if(TARGET GTest::gtest_main)
    message(STATUS "Tests enabled for ${PROJECT_NAME}")

    include(GoogleTest)

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    enable_testing()

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")

    set(PROJECT_TESTS "${PROJECT_NAME}-test")
    set(TEST_FILES 
        tests/Tree.cpp

        # snapshots
        tests/snapshots/BasicFullMatch.snapshot.cpp
        tests/snapshots/BasicPartialMatch.snapshot.cpp
        tests/snapshots/UnicodeFullMatch.snapshot.cpp
        tests/snapshots/UnicodePartialMatch.snapshot.cpp
    )

    add_executable(${PROJECT_TESTS} ${TEST_FILES})
    target_link_libraries(${PROJECT_TESTS} PRIVATE
        ${PROJECT_LIB}
        GTest::gtest_main
        Qt${MAJOR_QT_VERSION}::Core
    )
    set_target_properties(${PROJECT_TESTS}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            CXX_STANDARD 20
    )
    gtest_discover_tests(${PROJECT_TESTS})
endif()

