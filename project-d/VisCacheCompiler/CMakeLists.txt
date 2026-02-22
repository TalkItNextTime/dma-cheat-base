cmake_minimum_required(VERSION 3.22)

project(VisCacheCompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(viscore_core STATIC
    src/bvh.cpp
    src/cache_io.cpp
    src/compiler_pipeline.cpp
    src/hash.cpp
    src/vphys_parser.cpp
)

target_include_directories(viscore_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(viscore_core PRIVATE /W4 /EHsc /permissive- /Zc:__cplusplus)
else()
    target_compile_options(viscore_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(map_compiler
    tools/map_compiler/main.cpp
)

target_link_libraries(map_compiler PRIVATE viscore_core)
target_include_directories(map_compiler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(map_compiler PRIVATE /W4 /EHsc /permissive- /Zc:__cplusplus)
else()
    target_compile_options(map_compiler PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(vis_test EXCLUDE_FROM_ALL
    tmp_vis_test.cpp
    ../VisCheckCS2/VisCheck.cpp
)

target_include_directories(vis_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../VisCheckCS2
)

if(MSVC)
    target_compile_options(vis_test PRIVATE /W4 /EHsc /permissive- /Zc:__cplusplus)
else()
    target_compile_options(vis_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
