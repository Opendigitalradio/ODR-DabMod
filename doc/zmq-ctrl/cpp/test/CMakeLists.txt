cmake_minimum_required(VERSION 2.6)
project (ctrl_test)
 
ADD_DEFINITIONS(-DBOOST_TEST_DYN_LINK -D_SCL_SECURE_NO_WARNINGS)

set(BOOST_LIBRARYDIR)
set(BOOST_INCLUDEDIR)
set(BOOST_USE_MULTITHREADED ON)
set(BOOST_USE_STATIC_LIBS ON)
set(BOOST_MIN_VERSION 1.55)

find_package( Boost ${BOOST_MIN_VERSION} REQUIRED 
            unit_test_framework 
            system 
            )

set(PROJECT_TEST_SRCS 
${CMAKE_CURRENT_SOURCE_DIR}/ctrl_test.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../OdrModCtrl.cpp
)

include_directories( ${PROJECT_SOURCE_DIR}/../ )
link_directories (/usr/local/lib)
add_executable(${PROJECT_NAME} ${PROJECT_TEST_SRCS})

target_link_libraries(${PROJECT_NAME}
	zmq 
	${Boost_LIBRARIES}
	)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") 
