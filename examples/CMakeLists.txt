# Simple offline processing example

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

if(WIN32 AND TARGET dsp_engine)
    add_custom_command(TARGET dsp_example_offline POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:dsp_engine>
            $<TARGET_FILE_DIR:dsp_example_offline>
        COMMENT "Copy dsp_engine.dll next to example"
    )
endif()
