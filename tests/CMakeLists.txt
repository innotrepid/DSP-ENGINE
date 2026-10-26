# Unit tests and benchmarks for DSP ENGINE

add_executable(unit_tests
    unit_tests.cpp
)
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(unit_tests PRIVATE dsp_engine)
add_test(NAME unit_tests COMMAND unit_tests)

add_executable(benchmarks
    benchmarks.cpp
)
target_include_directories(benchmarks PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(benchmarks PRIVATE dsp_engine)

# Optional: run benchmarks as a test that always passes (just for CI visibility)
add_test(NAME benchmarks COMMAND benchmarks)
