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

add_executable(meshproc meshproc.cpp)
target_compile_features(meshproc PUBLIC cxx_std_20)
set_target_properties(meshproc PROPERTIES CXX_EXTENSIONS OFF)
enable_asan(meshproc PRIVATE)
