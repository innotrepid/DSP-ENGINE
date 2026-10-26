# Unit tests and benchmarks for DSP ENGINE

# Resolve which library target to link against
if(TARGET dsp_engine)
    set(DSP_TEST_LINK dsp_engine)
elseif(TARGET dsp_engine_static)
    set(DSP_TEST_LINK dsp_engine_static)
else()
    message(FATAL_ERROR "No dsp_engine library target available for tests")
endif()

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
add_executable(dsp_unit_tests unit_tests.cpp)
target_link_libraries(dsp_unit_tests PRIVATE ${DSP_TEST_LINK})
target_include_directories(dsp_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(dsp_unit_tests PRIVATE cxx_std_17)

add_test(NAME unit_tests COMMAND dsp_unit_tests)

# ---------------------------------------------------------------------------
# Benchmarks (not run as part of ctest by default)
# ---------------------------------------------------------------------------
add_executable(dsp_benchmarks benchmarks.cpp)
target_link_libraries(dsp_benchmarks PRIVATE ${DSP_TEST_LINK})
target_include_directories(dsp_benchmarks PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_features(dsp_benchmarks PRIVATE cxx_std_17)

# Convenience custom target
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS dsp_unit_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(run_benchmarks
    COMMAND dsp_benchmarks
    DEPENDS dsp_benchmarks
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
