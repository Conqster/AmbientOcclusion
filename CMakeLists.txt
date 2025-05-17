cmake_minimum_required(VERSION 3.15)
project(AO_PROGRAM)
set(PROJECT_NAME AO_PROGRAM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


set(AS_MODULE ON CACHE BOOL "Making PreglRenderer as a library")
add_subdirectory(${CMAKE_SOURCE_DIR}/PreglRenderer EXCLUDE_FROM_ALL)


#include_directories(${PROJECT_SOURCE_DIR})

file(GLOB_RECURSE source_files "src/*.h" "src/*.cpp")
add_executable(${PROJECT_NAME} ${source_files})

target_link_libraries(${PROJECT_NAME} PUBLIC PreglRenderer)

#Additional Include Directories
target_include_directories(${PROJECT_NAME} PUBLIC 
												 ${PROJECT_SOURCE_DIR}
												 ${PROJECT_SOURCE_DIR}/src
												 )

#set project working directory
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
#set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

#Copy require dll files to output directory (glew32.dll, assimp-vc143-mt.dll)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
				   COMMAND ${CMAKE_COMMAND} -E copy_if_different
						   #copy glew dll file
						   ${PROJECT_SOURCE_DIR}/PreglRenderer/libs/glew/bin/Release/x64/glew32.dll  
						   #copy assimp dll file
						   ${PROJECT_SOURCE_DIR}/PreglRenderer/libs/assimp/bin/assimp-vc143-mtd.dll
						   $<TARGET_FILE_DIR:${PROJECT_NAME}>)