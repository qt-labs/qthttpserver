
cmake_minimum_required(VERSION 2.8)

project(qmake_cmake_files)

enable_testing()

find_package(Qt5Core REQUIRED)

include("${_Qt5CTestMacros}")

add_test(qabstracthttpserver_test ${CMAKE_CTEST_COMMAND}
    --build-and-test
    "${CMAKE_CURRENT_SOURCE_DIR}/abstracthttpserver/"
    "${CMAKE_CURRENT_BINARY_DIR}/abstracthttpserver"
    --build-config "${CMAKE_BUILD_TYPE}"
    --build-generator ${CMAKE_GENERATOR}
    --build-makeprogram ${CMAKE_MAKE_PROGRAM}
    --build-project qabstracthttpserver_test
    --build-options "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" ${BUILD_OPTIONS_LIST}
    --test-command qabstracthttpserver_test
)
