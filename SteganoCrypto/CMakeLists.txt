cmake_minimum_required(VERSION 3.16)
project(SteganoCrypto VERSION 2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Concurrent)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_executable(steganocrypto
    src/main.cpp
    src/core/auth.cpp
    src/core/bmp.cpp
    src/core/files.cpp
    src/core/records.cpp
    src/core/stego.cpp
    src/gui/login_dialog.cpp
    src/gui/main_window.cpp
    src/gui/path_field.cpp
    src/gui/catalog_page.cpp
    src/gui/resources.qrc
)
target_link_libraries(steganocrypto PRIVATE Qt6::Widgets Qt6::Concurrent OpenSSL::Crypto)

target_compile_options(steganocrypto PRIVATE -Wall -Wextra -Wpedantic)