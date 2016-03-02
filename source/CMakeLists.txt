set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
add_definitions(-D_UNICODE -DUNICODE)

AUX_SOURCE_DIRECTORY(. SOURCE_FILES)
AUX_SOURCE_DIRECTORY(common COMMON_SOURCE_FILES)

add_executable(Internet_Downloader ${SOURCE_FILES} ${COMMON_SOURCE_FILES})

include_directories(common)

find_package(Boost REQUIRED COMPONENTS system filesystem regex thread)
if (Boost_FOUND)
    MESSAGE(STATUS "Boost include path:" ${Boost_INCLUDE_DIRS})
    MESSAGE(STATUS "Boost lib path:" ${Boost_LIBRARY_DIRS})
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(Internet_Downloader ${Boost_LIBRARIES} ws2_32)
endif ()

