cmake_minimum_required(VERSION 3.10)
project(storage_streamer CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(storage_streamer SHARED
    StorageStreamer.cpp
)

target_include_directories(storage_streamer
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(storage_streamer
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

target_link_libraries(storage_streamer
    PRIVATE
        winhttp
        bcrypt
        Storm
        Base
        BlizzardCore
)

set_target_properties(storage_streamer PROPERTIES
    OUTPUT_NAME storage_streamer
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/Win/plugins
)

foreach (config ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
    string(TOUPPER ${config} config)
    set_target_properties(storage_streamer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config} ${CMAKE_SOURCE_DIR}/bin/Win/plugins
    )
endforeach ()
