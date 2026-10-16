file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(Duskhaven STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Duskhaven
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Streaming
        ${CMAKE_CURRENT_SOURCE_DIR}/Patcher
)

target_link_libraries(Duskhaven
    PUBLIC
        winhttp
        bcrypt
)
