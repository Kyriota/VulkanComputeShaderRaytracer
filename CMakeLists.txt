cmake_minimum_required(VERSION 3.5.0)
project(vulkan-cpp-engine)

# Engine
add_subdirectory(${CMAKE_SOURCE_DIR}/src/lve)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Add source files
file(GLOB_RECURSE APP_SRC ${CMAKE_SOURCE_DIR}/src/app/*.cpp)
file(GLOB_RECURSE APP_HDR ${CMAKE_SOURCE_DIR}/src/app/*.hpp)
set(SOURCE ${APP_SRC} ${CMAKE_SOURCE_DIR}/src/main.cpp)
set(HEADER ${APP_HDR})

# Add executable
add_executable(${PROJECT_NAME} ${SOURCE} ${HEADER})

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/build/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/build/Release
)

# Get the current configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CONFIGURATION "DEBUG")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CONFIGURATION "RELEASE")
else()
    message(FATAL_ERROR "Unsupported configuration")
endif()

# Set RUNTIME_OUTPUT_DIRECTORY
set(RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/${CONFIGURATION})

# Add include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Link Engine
target_link_libraries(${PROJECT_NAME} Engine)



# ==================== Other scripts ====================
if(WIN32)
    set(SCRIPT_SUFFIX .bat)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

add_custom_target(make_scripts
    COMMAND ${CMAKE_COMMAND} -E echo "Running make scripts..."
    COMMAND ${CMAKE_SOURCE_DIR}/run_make_scripts${SCRIPT_SUFFIX} ${RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_SOURCE_DIR}/assets/shaders ${CMAKE_SOURCE_DIR}/assets ${CMAKE_SOURCE_DIR}/config
    RESULT_VARIABLE MAKE_SCRIPTS_RESULT
    VERBATIM
)

add_dependencies(${PROJECT_NAME} make_scripts)