# Simple offline processing example
add_executable(dsp_example_offline offline_process.cpp)
target_link_libraries(dsp_example_offline PRIVATE dsp_engine)
target_include_directories(dsp_example_offline PRIVATE ${CMAKE_SOURCE_DIR}/include)

# (Future) real-time demo that opens a platform backend
# add_executable(dsp_example_realtime realtime_demo.cpp)
# target_link_libraries(dsp_example_realtime PRIVATE dsp_engine)
