#pragma once

#include <string>
#include <chrono>
#include <atomic>
#include <mutex>

#ifdef _WIN32
    #include <winsock2.h>
    using socket_t = SOCKET;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
    using socket_t = int;
    #define INVALID_SOCKET_VALUE -1
#endif

using namespace std::chrono_literals;

class TcpConnection {
public:
    TcpConnection(std::string ip, int port, std::chrono::milliseconds connectTimeout = 10000ms, std::chrono::milliseconds readTimeout = 10000ms);

    ~TcpConnection();

    // Establish connection.
    void EstablishConnection();

    // Send data to remote host.
    void SendData(const std::string& data);

    // Recieve data from remote host.
    std::string ReceiveData(size_t bufferSize = 0);

    // Close connection.
    void Terminate();
    
    // Returns true if remote host is connected and false otherwise.
    bool IsRunning() const;
private:
    std::string ip_;
    int port_;
    std::chrono::milliseconds connectTimeout_, readTimeout_;

    mutable std::mutex sendMtx_, rcvMtx_;
    socket_t sock_ = INVALID_SOCKET_VALUE;
};
