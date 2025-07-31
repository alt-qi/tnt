#include "torrent_tracker.h"
#include "bencode/decoding.h"

#ifdef _WIN32
    // Define version requirements before any Windows headers
    #if !defined(WINVER)
        #define WINVER 0x0502
    #endif
    #if !defined(_WIN32_WINNT)
        #define _WIN32_WINNT 0x0502
    #endif
    
    // Prevent windows.h from defining min/max macros and including winsock.h
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    
    // Include winsock2.h BEFORE windows.h to avoid conflicts
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
    
    #include <windows.h>
    
    // Check for Windows Server 2003 or later to use WinHTTP
    #if WINVER >= 0x0502
        #define USE_WINHTTP
        #include <winhttp.h>
        #pragma comment(lib, "winhttp.lib")
    #else
        // Fallback to cpr for older Windows versions
        #include <cpr/cpr.h>
    #endif
#else
    #include <cpr/cpr.h>
    #include <netinet/in.h>
#endif

// Standard C++ includes after Windows headers to avoid conflicts
#include <sstream>
#include <iostream>
#include <vector>
#include <string>


TorrentTracker::TorrentTracker(const std::string& announce_url) : url_(announce_url) {}

#if defined(_WIN32) && defined(USE_WINHTTP)
// Windows Server 2003+ implementation using WinHTTP
void TorrentTracker::UpdatePeers(const TorrentFile& tf, const std::string& peerId, int port) {
    // Parse announce URL to extract components
    std::string announce = tf.announce;
    
    // Find protocol separator
    size_t protoPos = announce.find("://");
    if (protoPos == std::string::npos) {
        throw std::runtime_error("Invalid announce URL format");
    }
    
    std::string protocol = announce.substr(0, protoPos);
    std::string remaining = announce.substr(protoPos + 3);
    
    // Find path separator 
    size_t pathPos = remaining.find('/');
    std::string host = remaining.substr(0, pathPos);
    std::string path = (pathPos != std::string::npos) ? remaining.substr(pathPos) : "/";
    
    // Build query string
    std::stringstream queryStream;
    queryStream << "?info_hash=" << tf.infoHash
                << "&peer_id=" << peerId  
                << "&port=" << port
                << "&uploaded=0"
                << "&downloaded=0"
                << "&left=" << tf.CalculateSize()
                << "&compact=1"
                << "&numwant=50";
    
    std::string fullPath = path + queryStream.str();
    
    // Convert strings to wide chars for WinHTTP
    int hostLen = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wHost(hostLen);
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, wHost.data(), hostLen);
    
    int pathLen = MultiByteToWideChar(CP_UTF8, 0, fullPath.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wPath(pathLen);
    MultiByteToWideChar(CP_UTF8, 0, fullPath.c_str(), -1, wPath.data(), pathLen);
    
    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(L"TNT Torrent Client/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS,
                                     0);
    if (!hSession) {
        throw std::runtime_error("Failed to initialize WinHTTP session");
    }
    
    // Connect to server - use WinHTTP port type and standard ports
    INTERNET_PORT port_num = (protocol == "https") ? 443 : 80;
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.data(), port_num, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("Failed to connect to tracker");
    }
    
    // Create request
    DWORD flags = (protocol == "https") ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.data(),
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("Failed to create HTTP request");
    }
    
    // Send request
    BOOL result = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!result) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("Failed to send HTTP request");
    }
    
    // Receive response
    result = WinHttpReceiveResponse(hRequest, NULL);
    if (!result) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);  
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("Failed to receive HTTP response");
    }
    
    // Read response data
    std::string responseData;
    DWORD bytesRead = 0;
    char buffer[4096];
    
    do {
        result = WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead);
        if (result && bytesRead > 0) {
            responseData.append(buffer, bytesRead);
        }
    } while (result && bytesRead > 0);
    
    // Cleanup WinHTTP handles
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    if (!result) {
        throw std::runtime_error("Failed to read HTTP response data");
    }
    
    // Parse response using existing logic
    std::stringstream stream;
    stream << responseData;

    auto dataDict = std::static_pointer_cast<Bencode::Dict>(Bencode::ReadEntity(stream))->value;
    auto rawPeers = std::static_pointer_cast<Bencode::String>(dataDict["peers"])->value;

    peers_.clear();

    for (size_t i = 0; i < rawPeers.size(); i += 6) {
        peers_.push_back(Peer {
            .ip = std::to_string(static_cast<uint8_t>(rawPeers[i])) + "."
                + std::to_string(static_cast<uint8_t>(rawPeers[i + 1])) + "."
                + std::to_string(static_cast<uint8_t>(rawPeers[i + 2])) + "."
                + std::to_string(static_cast<uint8_t>(rawPeers[i + 3])),
            
            .port = (static_cast<uint8_t>(rawPeers[i + 4]) << 8) 
                + static_cast<uint8_t>(rawPeers[i + 5])
        });
    }
}

#else
// Unix/Linux implementation or Windows pre-Server 2003 using cpr
void TorrentTracker::UpdatePeers(const TorrentFile& tf, const std::string& peerId, int port) {
    cpr::Response response = cpr::Get(
        cpr::Url { tf.announce },
        cpr::Parameters {
            { "info_hash", tf.infoHash },
            { "peer_id", peerId },
            { "port", std::to_string(port) },
            { "uploaded", std::to_string(0) },
            { "downloaded", std::to_string(0) },
            { "left", std::to_string(tf.CalculateSize()) },
            { "compact", std::to_string(1) },
            { "numwant", std::to_string(50) }
        }
    );

    std::stringstream stream;
    stream << response.text;

    auto dataDict = std::static_pointer_cast<Bencode::Dict>(Bencode::ReadEntity(stream))->value;
    auto rawPeers = std::static_pointer_cast<Bencode::String>(dataDict["peers"])->value;

    peers_.clear();

    for (size_t i = 0; i < rawPeers.size(); i += 6) {
        peers_.push_back(Peer {
            .ip = std::to_string(static_cast<uint8_t>(rawPeers[i])) + "."
                + std::to_string(static_cast<uint8_t>(rawPeers[i + 1])) + "."
                + std::to_string(static_cast<uint8_t>(rawPeers[i + 2])) + "."
                + std::to_string(static_cast<uint8_t>(rawPeers[i + 3])),
            
            .port = (static_cast<uint8_t>(rawPeers[i + 4]) << 8) 
                + static_cast<uint8_t>(rawPeers[i + 5])
        });
    }
}
#endif

const std::vector<Peer>& TorrentTracker::GetPeers() const {
    return peers_;
}