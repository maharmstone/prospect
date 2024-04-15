cmake_minimum_required(VERSION 3.12)

project(prospect
	LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(BUILD_SAMPLE "Build sample program" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

find_package(LibXml2 REQUIRED)
find_package(CURL REQUIRED)
find_package(Iconv REQUIRED) # for libxml2

add_definitions(-DPROSPECT_EXPORT)

set(SRC_FILES
	src/prospect.cpp
	src/xml.cpp
	src/soap.cpp
	src/b64.cpp)

add_library(prospect SHARED ${SRC_FILES})

target_link_libraries(prospect LibXml2::LibXml2)
target_link_libraries(prospect CURL::libcurl)
target_link_libraries(prospect Iconv::Iconv)

target_compile_options(prospect PRIVATE
	$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
		-Wall -Werror=cast-function-type -Wno-expansion-to-defined -Wunused-parameter -Wtype-limits -Wextra -Wconversion>
	$<$<CXX_COMPILER_ID:MSVC>:
		/W4>)

set_target_properties(prospect PROPERTIES PUBLIC_HEADER src/prospect.h)

if(WIN32 AND NOT MSVC)
	target_link_options(prospect PUBLIC -static -static-libgcc -static-libstdc++)
endif()

install(TARGETS prospect
	EXPORT prospect-targets
	RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
	PRIVATE_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

if(BUILD_SAMPLE)
	add_executable(prospect-test src/prospect-test.cpp)
	target_include_directories(prospect-test PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
	target_link_libraries(prospect-test prospect)
	install(TARGETS prospect-test DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

install(EXPORT prospect-targets DESTINATION lib/cmake/prospect)

configure_package_config_file(
	"src/prospectConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/prospectConfig.cmake"
	INSTALL_DESTINATION "lib/cmake/prospect"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/prospectConfig.cmake" DESTINATION "lib/cmake/prospect")
