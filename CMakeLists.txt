cmake_minimum_required (VERSION 2.8)
set(APP_NAME odr-dabmod)
project(${APP_NAME})

message("Current configuration: ${CMAKE_BUILD_TYPE}")

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(CMAKE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/cmake)

### Boost ###
option(BOOST_STATIC "Static linking of boost libraries" OFF)
if (BOOST_STATIC)
	message(STATUS "Static linking of boost libraries from " $ENV{BOOST_ROOT})
endif()

if (BOOST_STATIC)
	# Boost libs to use, must be set before including boost.txt.
	set(project_boost_libs 
		system 
		thread
	)
	include(${CMAKE_INCLUDE_DIR}/boost.txt) 
	set(PROJECT_INCLUDE_DIR
		${PROJECT_INCLUDE_DIR}
		$ENV{BOOST_ROOT}
		)
else()
	set(BOOST_USE_STATIC_LIBS OFF)
	set(BOOST_USE_MULTITHREADED ON)
	set(BOOST_MIN_VERSION 1.55)
	find_package(Boost ${BOOST_MIN_VERSION} REQUIRED 
		thread 
		system 
	)
	set(LIBBOOST_LIBS ${Boost_LIBRARIES})
		set(PROJECT_INCLUDE_DIR
			${PROJECT_INCLUDE_DIR}
			${Boost_INCLUDE_DIRS}
			)
endif()
### Boost ###

message ("")

### ZeroMQ ###
option(ZMQ "Support ZeroMQ" ON)
option(ZMQ_STATIC "Static linking of ZeroMQ" ON)
if (ZMQ)
	message(STATUS "Supports ZeroMQ")
	add_definitions(-DHAVE_ZEROMQ)
	if (ZMQ_STATIC)
		message(STATUS "Static linking of ZeroMQ from " $ENV{ZMQ_ROOT})
		# A trick to force CMAKE to look here first, before any system-path like /usr/lib/ which otherwise would have precedence.
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I$ENV{ZMQ_ROOT}/include -I$ENV{ZMQ_ROOT}/cppzmq")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I$ENV{ZMQ_ROOT}/include -I$ENV{ZMQ_ROOT}/cppzmq")
		include(${CMAKE_INCLUDE_DIR}/zeromq.txt)
		set(PROJECT_INCLUDE_DIR
			${PROJECT_INCLUDE_DIR}
			$ENV{ZMQ_ROOT})
		add_definitions(-DZMQ_STATIC)
	else()
		set(ZEROMQ zmq)
	endif()
endif()
### ZeroMQ ###

message ("")

### FFT ###
# defaults to kiss_fft in case of off
include(${CMAKE_INCLUDE_DIR}/kissfft.txt)
option(FFTW3 "Uses FFTW3 fft library" ON)
if (FFTW3)
	message(STATUS "Uses FFTW3")
	include(${CMAKE_INCLUDE_DIR}/fftw.txt) 
endif()
### FFT ###

message ("")

### UHD ###
option(UHD "Support uhd output" ON)
if (UHD)
	message(STATUS "Supports uhd output")
	include(${CMAKE_INCLUDE_DIR}/uhd.txt) 
endif()
### UHD ###

# get version from git describe
exec_program(
    "git"
    ${CMAKE_CURRENT_SOURCE_DIR}
    ARGS "describe"
    OUTPUT_VARIABLE VERSION )

message ("")
message(STATUS "App name:  ${APP_NAME}")
message(STATUS "Building version:  ${VERSION}")

# some common definitions
add_definitions(
	-D_SCL_SECURE_NO_WARNINGS
	-DHAVE_BZERO
	-DHAVE_GETTIMEOFDAY
	-DPACKAGE="odrdabmod"
	-DGITVERSION="${VERSION}"
	-UNDEBUG
)

include_directories("${PROJECT_INCLUDE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")

add_subdirectory(src)

