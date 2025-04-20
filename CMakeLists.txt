cmake_minimum_required(VERSION 3.16)

project(remote_agent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Set default build type to Release." FORCE)
endif()
# Dependecy management with conan
include(FetchContent)
FetchContent_Declare(conan
    URL https://github.com/conan-io/conan/releases/download/2.14.0/conan-2.14.0-linux-x86_64.tgz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(conan)
if (conan_POPULATED)
    set(CONANEXE ${conan_SOURCE_DIR}/conan)
    execute_process(
        COMMAND "${CONANEXE}" profile list
        OUTPUT_VARIABLE PROFILES_OUTPUT
        RESULT_VARIABLE RESULT
        ERROR_QUIET
    )
    string(FIND "${PROFILES_OUTPUT}" "default" DEFAULT_PROFILE_POS)
    if(DEFAULT_PROFILE_POS EQUAL -1)
        execute_process(
        COMMAND "${CONANEXE}" profile detect
    )
    endif()
    execute_process(COMMAND "${CONANEXE}" install conanfile.txt --build=missing -s build_type=${CMAKE_BUILD_TYPE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_BINARY_DIR}/generators)
    include(conan_toolchain)
endif()

find_package(mailio REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem program_options process log log_setup)
find_package(OpenSSL REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(minizip REQUIRED)

file(GLOB_RECURSE SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${mailio_INCLUDE_DIRS}
    ${minizip_INCLUDE_DIRS}
    ${yaml-cpp_INCLUDE_DIRS})

target_link_directories(${PROJECT_NAME} PRIVATE
    ${CONAN_RUNTIME_LIB_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    mailio::mailio
    Boost::system
    Boost::filesystem
    Boost::program_options
    Boost::process
    Boost::log
    Boost::log_setup
    OpenSSL::SSL
    OpenSSL::Crypto
    yaml-cpp::yaml-cpp
    MINIZIP::minizip
    pthread
    )

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.env)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.env ${CMAKE_BINARY_DIR}/.env COPYONLY)
endif()
