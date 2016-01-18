cmake_minimum_required(VERSION 2.8)
project (${APP_NAME})
 
include(${CMAKE_INCLUDE_DIR}/find_libs.txt)
include(${CMAKE_INCLUDE_DIR}/compiler_flags.txt)

### Uncomment to distinguish debug builds with a post-fix 'd' #######
#set_target_properties(${APP_NAME} PROPERTIES DEBUG_POSTFIX d)
#####################################################################

# additional source files
set(PROJECT_SRCS 
	${KISSFFT_SRC_DIR}/kiss_fft.c
)

file(GLOB PROJECT_SRCS_AUTO ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

file(GLOB PROJECT_HDRS_AUTO ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

### Uncomment to add debug settings file for Visual Studio, to keep track of debug arguments, etc. ###
#if (WIN32)
#	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../config/Visual Studio 2013/${APP_NAME}.vcxproj.user.in" "${CMAKE_CURRENT_BINARY_DIR}/mux.vcxproj.user" @ONLY)
#endif()
######################################################################################################

add_executable(
	${PROJECT_NAME}
	${PROJECT_SRCS}
	${PROJECT_SRCS_AUTO} 
	${PROJECT_HDRS_AUTO} 
	) 

if(NOT WIN32)
	install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
endif()

if (ZMQ_STATIC)
	add_dependencies(${PROJECT_NAME} zeromq)
endif()

if (BOOST_STATIC)
	add_dependencies(${PROJECT_NAME} boost_libs)
endif()

target_link_libraries(${PROJECT_NAME}
	${ZEROMQ}
	${LIBBOOST_LIBS}
	${OS_LIBS}
	${LIBFFTW3}
	${LIBUHD}
)
