file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(Duskhaven STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Duskhaven
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)
