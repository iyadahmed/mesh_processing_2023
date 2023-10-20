cmake_minimum_required(VERSION 3.16)
project(mesh_processing)

# Enable ASAN if config is Debug or RelDeb, use generator expressions, support MSVC and non MSVC
macro(enable_asan target scope)
    if(MSVC)
        target_compile_options(${target} ${scope} $<$<CONFIG:Debug,RelDeb>:/fsanitize=address>)
        target_link_options(${target} ${scope} $<$<CONFIG:Debug,RelDeb>:/fsanitize=address>)
    else()
        target_compile_options(${target} ${scope} $<$<CONFIG:Debug,RelDeb>:-fsanitize=address>)
        target_link_options(${target} ${scope} $<$<CONFIG:Debug,RelDeb>:-fsanitize=address>)
    endif()
endmacro()

add_executable(read_stl read_stl.cpp)
target_compile_features(read_stl PUBLIC cxx_std_17)
set_target_properties(read_stl PROPERTIES CXX_EXTENSIONS OFF)
enable_asan(read_stl PRIVATE)

add_executable(read_ply read_ply.cpp)
target_compile_features(read_ply PUBLIC cxx_std_17)
set_target_properties(read_ply PROPERTIES CXX_EXTENSIONS OFF)
enable_asan(read_ply PRIVATE)
