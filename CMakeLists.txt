cmake_minimum_required(VERSION 3.10)

project(juno)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(SRC_FILES
	src/prospect.cpp)

add_executable(prospect ${SRC_FILES})

if(NOT MSVC)
	target_compile_options(prospect PUBLIC -Wall -Werror=cast-function-type -Wno-expansion-to-defined -Wunused-parameter -Wtype-limits -Wextra -Wconversion)
	target_link_options(prospect PUBLIC -static -static-libgcc -static-libstdc++)
endif()
