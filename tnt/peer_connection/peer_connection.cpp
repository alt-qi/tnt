#include "peer_connection.h"
#include "message.h"
#include "../piece_storage/piece_storage.h"
#include "../torrent_file/types.h"
#include <iostream>
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <netinet/in.h>
#endif
#include <optional>
#include <chrono>

using namespace std::chrono_literals;
const std::string proto = "BitTorrent protocol";

PeerConnection::PeerConnection(const Peer& peer, std::string selfId, std::string hash) 
    : peer_(peer), selfId_(selfId), hash_(hash), socket_(peer.ip, peer.port, 3000ms, 10000ms) {}

PeerConnection::~PeerConnection() {
    socket_.Terminate();
}

void PeerConnection::PerformHandshake() {    
    std::string data;
    data.reserve(49 + proto.size());

    data += char(proto.size());
    data += proto;
    data.resize(8 + data.size());

    data += hash_;
    data += selfId_;
    
    socket_.EstablishConnection();
    socket_.SendData(data);

    int peerProtoSize = static_cast<int>(socket_.ReceiveData(1)[0]);
    if (peerProtoSize == 0) {
        socket_.Terminate();
        throw std::runtime_error("Peer connection error: peer protocol size equals zero");
    }
    std::string peerProto = socket_.ReceiveData(peerProtoSize);
    std::string suf = socket_.ReceiveData(48);

    peerId_ = suf.substr(28);
}

bool PeerConnection::IsRunning() const {
    return socket_.IsRunning();
}

void PeerConnection::EstablishConnection() {
    socket_.EstablishConnection();
    PerformHandshake();
}

void PeerConnection::SendMessage(const Message& msg) {
    socket_.SendData(msg.ToString());
}

Message PeerConnection::RecieveMessage() {
    Message msg = Message::Parse(socket_.ReceiveData());
    return msg;
}

void PeerConnection::CloseConnection() {
    socket_.Terminate();
}