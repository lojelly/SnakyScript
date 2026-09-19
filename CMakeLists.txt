cmake_minimum_required(VERSION 4.0)
project(SnakyScript VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build using a shared library." ON)

include(FetchContent)
FetchContent_Declare(vibrant_logs GIT_REPOSITORY https://github.com/lojelly/VibrantLogs.git GIT_TAG main)
FetchContent_Declare(dynamic_map_spellbook GIT_REPOSITORY https://github.com/lojelly/DynamicMapSpellbook.git GIT_TAG main)
FetchContent_MakeAvailable(vibrant_logs dynamic_map_spellbook)

add_library(snaky_script "${CMAKE_CURRENT_SOURCE_DIR}/src/snaky_script.c")

if(BUILD_SHARED_LIBS)
	target_compile_definitions(snaky_script PRIVATE SNAKY_EXPORTS PUBLIC SNAKY_DLL)
endif()

target_include_directories(snaky_script PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(snaky_script PUBLIC vibrant_logs dynamic_map_spellbook)

# testing lib | you can safely remove it:
add_subdirectory(test)
