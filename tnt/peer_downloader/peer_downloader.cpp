#include "peer_downloader.h"
#include "../torrent_file/types.h"
#include <memory>
#include <iostream>
#include <utility>
#include <thread>
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif
#include <random>

using namespace std::chrono_literals;

std::default_random_engine rng;

PeerDownloader::PeerDownloader(Peer peer, TorrentFile file,
        std::function<void(std::shared_ptr<Piece>)> callback) : 
    connection_(peer, "TTST1APP2DONT3WORRY6", file.infoHash),
    pieceAvailability_(file.info.pieces.size(), false),
    downloadCallback_(callback) {}

void PeerDownloader::QueuePiece(std::shared_ptr<Piece> piece) {
    std::unique_lock lock(queueMtx_);
    auto itQueue = std::find_if(queuedPieces_.begin(), queuedPieces_.end(), 
        [&](const std::shared_ptr<Piece>& ptr) { 
            return ptr->GetIndex() == piece->GetIndex();
        }
    );

    if (itQueue != queuedPieces_.end())
        return;

    queuedPieces_.push_back(piece);
}

void PeerDownloader::CancelPiece(std::shared_ptr<Piece> piece) {
    std::unique_lock lock1(queueMtx_);
    auto itQueue = std::find_if(queuedPieces_.begin(), queuedPieces_.end(), 
        [&](const std::shared_ptr<Piece>& ptr) { 
            return ptr->GetIndex() == piece->GetIndex();
        }
    );

    if (itQueue != queuedPieces_.end()) {
        queuedPieces_.erase(itQueue);
        return;
    }

    lock1.unlock();

    std::unique_lock lock2(reqMtx_);
    auto itReq = std::find_if(requestedPieces_.begin(), requestedPieces_.end(), 
        [&](const std::shared_ptr<Piece>& ptr) { 
            return ptr->GetIndex() == piece->GetIndex();
        }
    );

    if (itReq != requestedPieces_.end()) {
        CancelBlocksForPiece(*itReq);
        requestedPieces_.erase(itReq);
    }
}

bool PeerDownloader::IsPieceAvailable(size_t pieceIdx) {
    std::lock_guard lock(availabilityMtx_);
    return pieceAvailability_[pieceIdx];
}

void PeerDownloader::Start() {
    connection_.EstablishConnection();
    connection_.SendMessage(Message::Init(Message::Id::Interested));
}

size_t PeerDownloader::GetQueuedPiecesCount() const {
    std::unique_lock lock(queueMtx_);
    return queuedPieces_.size();
}

bool PeerDownloader::IsRunning() const {
    return connection_.IsRunning();
}

void PeerDownloader::RequestBlocksForPiece(std::shared_ptr<Piece> piece) {
    for (size_t j = 0; j < piece->GetBlocks().size(); j++) {
        auto& block = piece->GetBlocks()[j];
        connection_.SendMessage(Message::InitRequest(piece->GetIndex(), block.offset, block.length));
    }
}

void PeerDownloader::CancelBlocksForPiece(std::shared_ptr<Piece> piece) {
    for (size_t j = 0; j < piece->GetBlocks().size(); j++) {
        auto& block = piece->GetBlocks()[j];
        connection_.SendMessage(Message::InitCancel(piece->GetIndex(), block.offset, block.length));
    }
}

void PeerDownloader::SendLoop() {
    while (IsRunning()) {
        if (!choked_) {
            std::unique_lock lock(reqMtx_);
            while (requestedPieces_.size() < requestedLimit_) {
                auto piece = AcquirePiece();

                if (piece != nullptr) {
                    requestedPieces_.push_back(piece);
                    RequestBlocksForPiece(piece);
                } else break;
            }
        }

        std::this_thread::sleep_for(15ms);
    }
}

void PeerDownloader::ReceiveLoop() {
    while (IsRunning()) {
        Message msg = connection_.RecieveMessage();
        switch (msg.id) {
            case Message::Id::Unchoke:
                ProcessUnchokeMessage(msg);
                break;
            
            case Message::Id::Choke:
                ProcessChokeMessage(msg);
                break;
            
            case Message::Id::Piece:
                ProcessPieceMessage(msg);
                break;
            
            case Message::Id::BitField:
                ProcessBitFieldMessage(msg);
                break;
            
            case Message::Id::Have:
                ProcessHaveMessage(msg);
                break;
            
            default:
                break;
        }
    }
}

std::shared_ptr<Piece> PeerDownloader::AcquirePiece() {
    std::lock_guard lock(queueMtx_);

    if (queuedPieces_.empty())
        return nullptr;

    size_t idx = rng() % queuedPieces_.size();
    std::swap(queuedPieces_[idx], queuedPieces_.back());
    auto piece = queuedPieces_.back();
    queuedPieces_.pop_back();

    return piece;
}

void PeerDownloader::Terminate() {
    connection_.CloseConnection();
    choked_ = true;
    FlushPieces();
}

void PeerDownloader::FlushPieces(bool sendCancel) {
    std::vector<std::shared_ptr<Piece>> toRemove;

    queueMtx_.lock();
    for (auto ptr : queuedPieces_)
        toRemove.push_back(ptr);
    queuedPieces_.clear();
    queueMtx_.unlock();

    reqMtx_.lock();
    for (auto ptr : requestedPieces_) {
        toRemove.push_back(ptr);
        if (sendCancel)
            CancelBlocksForPiece(ptr);
    }
    requestedPieces_.clear();
    reqMtx_.unlock();

    while (!toRemove.empty()) {
        downloadCallback_(toRemove.back());
        toRemove.pop_back();
    }
}

// ---------------------------------------------------

void PeerDownloader::ProcessPieceMessage(Message& msg) {
    uint32_t pieceIndex = ntohl(*reinterpret_cast<const uint32_t*>(&msg.payload[0]));
    uint32_t blockOffset = ntohl(*reinterpret_cast<const uint32_t*>(&msg.payload[4]));
    std::string data = msg.payload.substr(8);

    std::unique_lock lock(reqMtx_);

    auto it = find_if(requestedPieces_.begin(), requestedPieces_.end(), [&](const std::shared_ptr<Piece>& ptr) {
        return (ptr->GetIndex() == pieceIndex);
    });

    if (it == requestedPieces_.end())
        return;

    auto piece = (*it);
    piece->SaveBlock(blockOffset, data);

    if (piece->AllBlocksRetrieved()) {
        requestedPieces_.erase(it);
        lock.unlock();
        downloadCallback_(piece);
    }
}

void PeerDownloader::ProcessHaveMessage(Message& msg) {
    uint32_t pieceIndex = ntohl(*reinterpret_cast<const uint32_t*>(&msg.payload[0]));

    availabilityMtx_.lock();
    pieceAvailability_[pieceIndex] = true;
    availabilityMtx_.unlock();
}

void PeerDownloader::ProcessBitFieldMessage(Message& msg) {
    std::lock_guard lock(availabilityMtx_);

    std::string data = msg.payload;
    for (size_t i = 0; i < pieceAvailability_.size(); i++) {
        pieceAvailability_[i] = (data[i / 8] >> (7 - (i % 8))) & 1;
    }
}

void PeerDownloader::ProcessUnchokeMessage(Message& msg) {
    choked_ = false;
}

void PeerDownloader::ProcessChokeMessage(Message& msg) {
    choked_ = true;
}
