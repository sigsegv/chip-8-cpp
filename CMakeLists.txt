cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0074 OLD)

set(PROJECT_NAME chip8)

project(${PROJECT_NAME} VERSION 0.0.0.1 LANGUAGES CXX)

include(local_settings.cmake OPTIONAL)

if(${MSVC})
  option(SFML_STATIC "Whether to link statically or not" ON)
else()
  option(SFML_STATIC "Whether to link statically or not" OFF)
endif()

if(${SFML_STATIC})
  set(SFML_STATIC_LIBRARIES TRUE)
endif()
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})
find_package(SFML 2 REQUIRED graphics window system)

file(GLOB HDR *.hpp;*.h)
file(GLOB SRC *.cpp)

add_executable(${PROJECT_NAME} ${SRC} ${HDR})
target_link_libraries(${PROJECT_NAME} ${SFML_LIBRARIES} ${SFML_DEPENDENCIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${SFML_INCLUDE_DIR})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)
#if(${MSVC})
#  set(CWD_INTERMEDIATE "")
#else()
#  set(CWD_INTERMEDIATE "/${CMAKE_CFG_INTDIR}")
#endif()

#add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}${CWD_INTERMEDIATE}/assets)
