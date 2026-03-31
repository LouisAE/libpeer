project(sample)

file(GLOB SRCS "*.c")

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(sample ${SRCS})

target_link_libraries(sample peer pthread opus)

target_compile_options(sample PRIVATE -g)

