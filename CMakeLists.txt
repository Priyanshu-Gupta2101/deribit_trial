cmake_minimum_required(VERSION 3.14)
project(deribit_trading_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(cpprestsdk CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(jsoncpp CONFIG REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

file(GLOB SOURCES "src/*.cpp")

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)

add_executable(${PROJECT_NAME} ${SOURCES})

add_executable(websocket_client_test tests/websocket_client_test.cpp)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    cpprestsdk::cpprest
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    JsonCpp::JsonCpp
)

target_link_libraries(websocket_client_test
    PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    JsonCpp::JsonCpp
)

configure_file(
    ${CMAKE_SOURCE_DIR}/config/config.json.example
    ${CMAKE_BINARY_DIR}/config/config.json.example
    COPYONLY
)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/config)