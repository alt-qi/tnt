#include "tcp_connection.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <chrono>

#ifdef _WIN32
    // Ensure we have at least Windows Server 2003 API level (0x0502)
    #if !defined(WINVER)
        #define WINVER 0x0502
    #endif
    #if !defined(_WIN32_WINNT)
        #define _WIN32_WINNT 0x0502
    #endif
    
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    
    // Windows socket initialization helper
    class WSAInitializer {
    public:
        WSAInitializer() {
            WSADATA wsaData;
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
            }
        }
        ~WSAInitializer() {
            WSACleanup();
        }
    };
    
    static WSAInitializer wsaInit; // Global initialization
    
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/poll.h>
    #include <netinet/tcp.h>
#endif


TcpConnection::TcpConnection(std::string ip, int port, std::chrono::milliseconds connectTimeout, std::chrono::milliseconds readTimeout) :
    ip_(ip), port_(port), connectTimeout_(connectTimeout), readTimeout_(readTimeout) {}

TcpConnection::~TcpConnection() {
    Terminate();
}

#ifdef _WIN32
// Windows implementation using Winsock
void TcpConnection::EstablishConnection() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("TCP error while creating socket: " + std::to_string(WSAGetLastError()));
    }

    // Set timeouts
    DWORD timeout = static_cast<DWORD>(readTimeout_.count());
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    // Set TCP_NODELAY
    BOOL flag = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));

    // Set buffer sizes
    int buf = 131072 * 32;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&buf, sizeof(buf));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&buf, sizeof(buf));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    
    int result = inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr);
    if (result != 1) {
        closesocket(sock);
        throw std::runtime_error("Invalid IP address format");
    }

    // Set non-blocking mode for timeout
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        closesocket(sock);
        throw std::runtime_error("Failed to set non-blocking mode");
    }

    // Attempt connection
    result = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            // Connection in progress, wait for completion
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(sock, &writefds);
            
            timeval tv = {};
            tv.tv_sec = static_cast<long>(connectTimeout_.count()) / 1000;
            tv.tv_usec = (static_cast<long>(connectTimeout_.count()) % 1000) * 1000;
            
            result = select(0, nullptr, &writefds, nullptr, &tv);
            if (result != 1) {
                closesocket(sock);
                throw std::runtime_error("Connection timeout or failed");
            }
            
            // Check if connection succeeded
            int sockErr;
            int optLen = sizeof(sockErr);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&sockErr, &optLen) != 0 || sockErr != 0) {
                closesocket(sock);
                throw std::runtime_error("Connection failed: " + std::to_string(sockErr));
            }
        } else {
            closesocket(sock);
            throw std::runtime_error("TCP connect error: " + std::to_string(err));
        }
    }

    // Set back to blocking mode
    mode = 0;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        closesocket(sock);
        throw std::runtime_error("Failed to set blocking mode");
    }

    sock_ = sock;
}

void TcpConnection::SendData(const std::string& data) {
    if (!IsRunning()) {
        throw std::runtime_error("Can't send until connection is established.");
    }

    std::unique_lock lock(sendMtx_);

    int result = send(sock_, data.c_str(), static_cast<int>(data.size()), 0);
    if (result == SOCKET_ERROR) {
        Terminate();
        throw std::runtime_error("TCP send error: " + std::to_string(WSAGetLastError()));
    }
}

std::string TcpConnection::ReceiveData(size_t bufferSize) {
    if (!IsRunning()) {
        throw std::runtime_error("Can't receive until connection is established.");
    }

    std::unique_lock lock(rcvMtx_);

    std::string result;
    if (bufferSize == 0) {
        result.resize(4);

        int sz = recv(sock_, &result[0], 4, MSG_WAITALL);
        if (sz < 4) {
            Terminate();
            throw std::runtime_error("TCP recv error: " + std::to_string(WSAGetLastError()));
        }
        
        bufferSize = ntohl(*reinterpret_cast<uint32_t*>(&result[0]));

        if (bufferSize == 0)
            return result;
    }

    std::string buf;
    buf.resize(bufferSize);

    int totalReceived = 0;
    while (totalReceived < static_cast<int>(bufferSize)) {
        int received = recv(sock_, &buf[totalReceived], static_cast<int>(bufferSize) - totalReceived, 0);
        if (received <= 0) {
            Terminate();
            throw std::runtime_error("TCP recv error: " + std::to_string(WSAGetLastError()));
        }
        totalReceived += received;
    }

    result += buf;
    return result;
}

void TcpConnection::Terminate() {
    if (sock_ != INVALID_SOCKET) {
        shutdown(sock_, SD_BOTH);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
}

bool TcpConnection::IsRunning() const {
    return sock_ != INVALID_SOCKET;
}

#else
// Unix/Linux implementation using standard sockets
void TcpConnection::EstablishConnection() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        throw std::runtime_error(
            "TCP error while using socket(): " + std::string(strerror(errno))
        );
    }

    timeval tvRead = {};
    tvRead.tv_sec = static_cast<int>(readTimeout_.count()) / 1000;
    tvRead.tv_usec = static_cast<int>(readTimeout_.count()) % 1000 * 1000;

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tvRead, sizeof(tvRead));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tvRead, sizeof(tvRead));

    char flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    int buf = 131072 * 32;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));

    sockaddr_in addr {
        .sin_family = AF_INET,
        .sin_port = htons(static_cast<uint16_t>(port_)),
        .sin_addr = in_addr { inet_addr(ip_.c_str()) }
    };

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);

    timeval tvConnect = {};
    tvConnect.tv_sec = static_cast<int>(connectTimeout_.count()) / 1000;
    tvConnect.tv_usec = static_cast<int>(connectTimeout_.count()) % 1000 * 1000;

    int flags = fcntl(sock, F_GETFL);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        if (errno == EINPROGRESS) {
            if (select(sock + 1, nullptr, &fds, nullptr, &tvConnect) != 1) {
                close(sock);
                throw std::runtime_error(
                    "TCP error while using select(): " + std::string(strerror(errno))
                );
            }

            int sockErrCode;
            socklen_t optLen = sizeof(sockErrCode);

            getsockopt(sock, SOL_SOCKET, SO_ERROR, &sockErrCode, &optLen);

            if (sockErrCode != 0) {
                close(sock);
                throw std::runtime_error(
                    "TCP socket error: " + std::string(strerror(errno))
                );
            }
        } else {
            close(sock);
            throw std::runtime_error(
                "TCP error while using connect(): " + std::string(strerror(errno))
            );
        }
    }

    fcntl(sock, F_SETFL, flags & (~O_NONBLOCK));

    sock_ = sock;
}

void TcpConnection::SendData(const std::string& data) {
    if (!IsRunning()) {
        throw std::runtime_error("Can't send until connection is established.");
    }

    std::unique_lock lock(sendMtx_);

    if (send(sock_, &data[0], data.size(), 0) == -1) {
        Terminate();
        throw std::runtime_error(
            "TCP error while using send(): " + std::string(strerror(errno))
        );
    }
}

std::string TcpConnection::ReceiveData(size_t bufferSize) {
    if (!IsRunning()) {
        throw std::runtime_error("Can't receive until connection is established.");
    }

    std::unique_lock lock(rcvMtx_);

    std::string result;
    if (bufferSize == 0) {
        result.resize(4);

        int sz = recv(sock_, &result[0], 4, MSG_WAITALL);
        if (sz < 4) {
            Terminate();
            throw std::runtime_error(
                "TCP error while using recv(): " + std::string(strerror(errno))
            );
        }
        
        bufferSize = ntohl(*reinterpret_cast<uint32_t*>(&result[0]));

        if (bufferSize == 0)
            return result;
    }

    std::string buf;
    buf.resize(bufferSize);

    if (recv(sock_, &buf[0], bufferSize, MSG_WAITALL) < bufferSize) {
        Terminate();
        throw std::runtime_error(
            "TCP error while using recv(): " + std::string(strerror(errno))
        );
    }

    result += buf;

    return result;
}

void TcpConnection::Terminate() {
    if (sock_ != INVALID_SOCKET_VALUE) {
        shutdown(sock_, SHUT_RDWR);
        close(sock_);
        sock_ = INVALID_SOCKET_VALUE;
    }
}

bool TcpConnection::IsRunning() const {
    return sock_ != INVALID_SOCKET_VALUE;
}
#endif