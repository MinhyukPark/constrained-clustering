#[[ CONSTANTS ]]
set(LIBRARY_SOURCES
    ${CMAKE_SOURCE_DIR}/libs/library.cpp)
#[[ CONSTANTS END ]]

#[[ Create external library ]]
add_library(external_libs ${LIBRARY_SOURCES})
target_include_directories(external_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
