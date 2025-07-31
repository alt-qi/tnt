#include "piece.h"
#include <iostream>
#include <algorithm>
#include "../../hash-library/sha1_util.h"
#include <chrono>

using namespace std::chrono_literals;

static constexpr size_t BLOCK_SIZE = 1 << 14;

Piece::Piece(size_t index, size_t length, std::string hash) : 
        index_(index), length_(length), hash_(hash), blocks_((length + BLOCK_SIZE - 1) / BLOCK_SIZE), isRetrieved_(blocks_.size()) {
    for (int i = 0; i < blocks_.size(); i++) {
        blocks_[i].piece = index_;
        blocks_[i].offset = i * BLOCK_SIZE;
        blocks_[i].length = std::min<size_t>(length_, (i + 1) * BLOCK_SIZE) - i * BLOCK_SIZE;
    }
}

const std::vector<Block>& Piece::GetBlocks() {
    return blocks_;
}

size_t Piece::GetIndex() const {
    return index_;
}

void Piece::SaveBlock(size_t blockOffset, std::string data) {
    std::lock_guard lock(mtx_);
    blocks_[blockOffset / BLOCK_SIZE].data = data;
    if (!isRetrieved_[blockOffset / BLOCK_SIZE]) {
        isRetrieved_[blockOffset / BLOCK_SIZE] = true;
        retrievedCount_++;
    }
}

bool Piece::AllBlocksRetrieved() const {
    std::lock_guard lock(mtx_);
    return retrievedCount_ == blocks_.size();
}

std::string Piece::GetData() const {
    std::lock_guard lock(mtx_);

    std::string result;
    for (const Block& block : blocks_) {
        auto c = block.data;
        result += c;
    }
    return result;
}


std::string Piece::GetRetrievedDataHash() const {
    std::string result(20, 0);
    std::string data = GetData();
    openssl_compat::SHA1(
        reinterpret_cast<unsigned char*>(&data[0]), 
        data.size(), 
        reinterpret_cast<unsigned char*>(&(result[0]))
    );
    return result;
}

void Piece::Reset() {
    std::lock_guard lock(mtx_);

    for (Block& block : blocks_)
        block.data = "";
    retrievedCount_ = 0;
    isRetrieved_.assign(isRetrieved_.size(), false);
}

bool Piece::HashMatches() const {
    return GetRetrievedDataHash() == hash_;
}