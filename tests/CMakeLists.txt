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

# Cap wall time so a hang fails the job instead of blocking runners.
add_test(NAME unit_tests COMMAND unit_tests)
set_tests_properties(unit_tests PROPERTIES TIMEOUT 60)

add_executable(benchmarks
    benchmarks.cpp
)
target_include_directories(benchmarks PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(benchmarks PRIVATE dsp_engine)

# Benchmarks are a separate CI step — do not register with ctest.
