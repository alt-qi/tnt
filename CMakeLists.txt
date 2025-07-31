
cmake_minimum_required(VERSION 3.10.0)
project(tnt)

set(CMAKE_CXX_STANDARD 20)

# add_compile_options(-fsanitize=thread)
# add_link_options(-fsanitize=thread)

# |-------------------------------------------------

include(FetchContent)

# hash-library (replacing OpenSSL)
add_library(hash_library
    hash-library/sha1.cpp
    hash-library/sha1_util.cpp
)

# libcpr
if(NOT DEFINED CPR_USE_SYSTEM_CURL)
    set(CPR_USE_SYSTEM_CURL ON)
endif()
FetchContent_Declare(cpr 
    QUIET
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.11.2
)
FetchContent_MakeAvailable(cpr)

# cxxopts (local)
add_library(cxxopts INTERFACE)
target_include_directories(cxxopts INTERFACE cxxopts/include)

# |-------------------------------------------------

add_library(bencode 
    tnt/bencode/decoding.cpp
    tnt/bencode/encoding.cpp
)
add_library(torrent_file
    tnt/torrent_file/parser.cpp
    tnt/torrent_file/types.cpp
)
add_library(torrent_tracker
    tnt/torrent_tracker.cpp
)
if(WIN32)
    # Check if we can use WinHTTP (Windows Server 2003+)
    # For simplicity, we'll assume any modern Windows build has WinHTTP
    target_link_libraries(
        torrent_tracker PRIVATE bencode winhttp ws2_32
    )
    # We still need cpr as a fallback for pre-Server 2003 or if WinHTTP fails
    target_link_libraries(
        torrent_tracker PRIVATE cpr::cpr
    )
else()
    target_link_libraries(
        torrent_tracker PRIVATE cpr::cpr bencode
    )
endif()

add_library(tcp_connection
    tnt/tcp_connection/tcp_connection.cpp
)
if(WIN32)
    target_link_libraries(
        tcp_connection PRIVATE ws2_32
    )
endif()

add_library(peer_connection
    tnt/peer_connection/peer_connection.cpp
    tnt/peer_connection/message.h
    tnt/peer_connection/message.cpp
)
target_link_libraries(
    peer_connection PRIVATE tcp_connection
)
add_library(piece_storage
    tnt/piece_storage/piece.cpp
    tnt/piece_storage/piece_storage.cpp
)
target_link_libraries(
    piece_storage PRIVATE hash_library
)
add_library(peer_downloader
    tnt/peer_downloader/peer_downloader.cpp
)
target_link_libraries(
    peer_downloader PRIVATE peer_connection piece_storage
)
add_library(conductor
    tnt/conductor/conductor.cpp
)
target_link_libraries(
    conductor PRIVATE peer_downloader
)
add_library(visuals
    tnt/visuals/infoboard.cpp
    tnt/visuals/rows/empty_row.cpp
    tnt/visuals/rows/download_progress_row.cpp
    tnt/visuals/rows/connected_peers_row.cpp
    tnt/visuals/rows/current_speed_row.cpp
)

add_executable(${PROJECT_NAME} tnt/main.cpp)

if(WIN32)
    target_link_libraries(
        ${PROJECT_NAME} 
        cpr::cpr
        cxxopts
        winhttp
        ws2_32

        bencode 
        torrent_file 
        torrent_tracker
        tcp_connection
        peer_connection
        piece_storage
        peer_downloader
        conductor
        visuals
    )
else()
    target_link_libraries(
        ${PROJECT_NAME} 
        cpr::cpr 
        cxxopts

        bencode 
        torrent_file 
        torrent_tracker
        tcp_connection
        peer_connection
        piece_storage
        peer_downloader
        conductor
        visuals
    )
endif()
