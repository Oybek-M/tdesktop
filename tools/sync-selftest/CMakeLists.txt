cmake_minimum_required(VERSION 3.16)
project(sync_selftest CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Qt6 COMPONENTS Core REQUIRED)
find_package(OpenSSL REQUIRED)

# Ataylab faqat tdesktop'ga bog'liq bo'lmagan fayllar kompilyatsiya qilinadi.
add_executable(sync_selftest
    main.cpp
    ../../Telegram/SourceFiles/custom_sync_record.cpp
    ../../Telegram/SourceFiles/custom_sync_crypto.cpp
)

target_include_directories(sync_selftest PRIVATE
    ../../Telegram/SourceFiles)

target_link_libraries(sync_selftest PRIVATE
    Qt6::Core OpenSSL::Crypto crypt32 ws2_32)
