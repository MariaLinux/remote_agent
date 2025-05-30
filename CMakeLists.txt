cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0156 NEW)

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
find_package(zmqpp REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

file(GLOB PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)
set(Protobuf_PROTOC_EXECUTABLE )
if(DEFINED protobuf_PACKAGE_FOLDER_RELEASE)
    set(Protobuf_PROTOC_EXECUTABLE ${protobuf_PACKAGE_FOLDER_RELEASE}/bin/protoc)
elseif(DEFINED protobuf_PACKAGE_FOLDER_DEBUG)
    set(Protobuf_PROTOC_EXECUTABLE ${protobuf_PACKAGE_FOLDER_DEBUG}/bin/protoc)
endif()
# set_property(TARGET protobuf::protoc PROPERTY IMPORTED_LOCATION ${Protobuf_PROTOC_EXECUTABLE})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)
execute_process(
        COMMAND "${Protobuf_PROTOC_EXECUTABLE}" -I=${CMAKE_CURRENT_SOURCE_DIR}/proto --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/proto ${PROTO_FILES}
        OUTPUT_VARIABLE PROTOC_VERSION
        RESULT_VARIABLE PROTOC_RESULT)
    
if(PROTOC_RESULT EQUAL 0)
    message(STATUS "protoc is working: ${PROTOC_VERSION}")
else()
    message(FATAL_ERROR "protoc is not working properly")
endif()

file(GLOB_RECURSE PROTO_SRCS ${CMAKE_CURRENT_BINARY_DIR}/proto/*.pb.cc)
file(GLOB_RECURSE SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${PROTO_SRCS})
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${CMAKE_CURRENT_BINARY_DIR}/proto
    ${mailio_INCLUDE_DIRS}
    ${minizip_INCLUDE_DIRS}
    ${yaml-cpp_INCLUDE_DIRS}
    ${zmqpp_INCLUDE_DIRS})

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
    zmqpp::zmqpp
    protobuf::libprotobuf
    pthread
    )

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.env)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.env ${CMAKE_BINARY_DIR}/.env COPYONLY)
endif()
