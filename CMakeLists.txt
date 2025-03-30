cmake_minimum_required(VERSION 3.16)

project(remote_agent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependecy management with conan
include(FetchContent)
    FetchContent_Declare(conan
    URL https://github.com/conan-io/conan/releases/download/2.14.0/conan-2.14.0-linux-x86_64.tgz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(conan)
if (conan_POPULATED)
    set(CONANEXE ${conan_SOURCE_DIR}/conan)
    execute_process(COMMAND "${CONANEXE}" install conanfile.txt --build=missing --profile:build=conan.profile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_BINARY_DIR}/Release/generators)

    include(conan_toolchain)
endif()

find_package(mailio REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem program_options)
find_package(OpenSSL REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(minizip REQUIRED)

message(STATUS "mailio_LIBRARIES: ${mailio_LIBRARIES}")
message(STATUS "mailio_LIBS_RELEASE: ${mailio_LIBS_RELEASE}")
message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
message(STATUS "OpenSSL_LIBRARIES: ${OpenSSL_LIBRARIES}")

file(GLOB_RECURSE SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${mailio_INCLUDE_DIRS}
    ${minizip_INCLUDE_DIRS})

target_link_directories(${PROJECT_NAME} PRIVATE
    ${CONAN_RUNTIME_LIB_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    mailio::mailio
    Boost::system
    Boost::filesystem
    Boost::program_options
    OpenSSL::SSL
    OpenSSL::Crypto
    yaml-cpp
    MINIZIP::minizip
    pthread
    )

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.env)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.env ${CMAKE_BINARY_DIR}/.env COPYONLY)
endif()
