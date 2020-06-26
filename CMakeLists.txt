cmake_minimum_required(VERSION 3.10)

project(juno)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

find_package(LibXml2 REQUIRED)
find_package(CURL REQUIRED)

set(SRC_FILES
	src/prospect.cpp
	src/xml.cpp)

add_executable(prospect ${SRC_FILES})
target_link_libraries(prospect ${LIBXML2_LIBRARIES})
target_link_libraries(prospect ${CURL_LIBRARIES})

target_include_directories(prospect PUBLIC "${LIBXML2_INCLUDE_DIR}")
target_include_directories(prospect PUBLIC "${CURL_INCLUDE_DIRS}")

if(NOT MSVC)
	target_compile_options(prospect PUBLIC -Wall -Werror=cast-function-type -Wno-expansion-to-defined -Wunused-parameter -Wtype-limits -Wextra -Wconversion)
endif()
