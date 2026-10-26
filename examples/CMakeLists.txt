# Simple offline processing example

# Prefer the real shared target; fall back to static if only static was built
if(TARGET dsp_engine)
    set(DSP_LINK_TARGET dsp_engine)
elseif(TARGET dsp_engine_static)
    set(DSP_LINK_TARGET dsp_engine_static)
else()
    message(FATAL_ERROR "No dsp_engine library target found")
endif()

add_executable(dsp_example_offline offline_process.cpp)
target_link_libraries(dsp_example_offline PRIVATE ${DSP_LINK_TARGET})
target_include_directories(dsp_example_offline PRIVATE ${CMAKE_SOURCE_DIR}/include)

# (Future) real-time demo that opens a platform backend
# add_executable(dsp_example_realtime realtime_demo.cpp)
# target_link_libraries(dsp_example_realtime PRIVATE ${DSP_LINK_TARGET})
