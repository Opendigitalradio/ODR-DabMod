cmake_minimum_required (VERSION 2.8)
set(APP_NAME odr-dabmod)
project(${APP_NAME})

message("Current configuration: ${CMAKE_BUILD_TYPE}")

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(CMAKE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/cmake)

### Boost ###
# Boost libs o use, must be set before including boost.txt.
set(project_boost_libs 
	system 
	filesystem 
	thread
	chrono
	program_options
	test
)
include(${CMAKE_INCLUDE_DIR}/boost.txt) 
### Boost ###

# A trick to force CMAKE to look here first, before any system-path like /usr/lib/ which otherwise would have precedence.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I$ENV{ZMQ_ROOT}/include -I$ENV{ZMQ_ROOT}/cppzmq")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I$ENV{ZMQ_ROOT}/include -I$ENV{ZMQ_ROOT}/cppzmq")

include(${CMAKE_INCLUDE_DIR}/zeromq.txt)
include(${CMAKE_INCLUDE_DIR}/kissfft.txt)

set(PROJECT_INCLUDE_DIR
	${PROJECT_INCLUDE_DIR}
	$ENV{ZMQ_ROOT}
	$ENV{BOOST_ROOT}
	)

#list(APPEND CMAKE_MODULE_PATH "${CMAKE_INCLUDE_DIR}/modules/")
#include(GetGitRevisionDescription)
#get_git_head_revision(GIT_REFSPEC GIT_SHA1)
#message(${GIT_SHA1})

exec_program(
    "git"
    ${CMAKE_CURRENT_SOURCE_DIR}
    ARGS "describe"
    OUTPUT_VARIABLE VERSION )
message(${VERSION})
#string( REGEX REPLACE "[.]" "_" VERSION_SHA1 ${VERSION} )
#message("${VERSION_SHA1}")


add_definitions(
	-D_SCL_SECURE_NO_WARNINGS
	-DZMQ_STATIC
	-DHAVE_BZERO
	-DHAVE_GETTIMEOFDAY
	-DHAVE_ZEROMQ
	-DPACKAGE="odrdabmod"
	-DGITVERSION="${VERSION}"
)

option(FFTW3 "Uses FFTW3 fft library" OFF)
if (FFTW3)
	message("Uses FFTW")
	include(${CMAKE_INCLUDE_DIR}/fftw.txt) 
endif()

include_directories("${PROJECT_INCLUDE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")

add_subdirectory(src)



