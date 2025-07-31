#pragma once

#include "../torrent_file/types.h"
#include "piece.h"
#include <queue>
#include <string>
#include <set>
#include <mutex>
#include <chrono>
#include <map>
#include <filesystem>
#include <fstream>
#include <atomic>


class PieceStorage {
public:
    explicit PieceStorage(const TorrentFile& torrentFile, std::filesystem::path outputDir);

    /* 
     * Acquires a piece for downloading.
     */
    std::shared_ptr<Piece> AcquirePiece();

    /* 
     * Called when a piece is downloaded.
     */
    bool PieceDownloaded(std::shared_ptr<Piece> piece);

    /* 
     * Returns the number of pieces in torrent file.
     */
    size_t GetTotalCount() const;

    /* 
     * Returns count of pieces that were already validated and saved to file.
     */
    size_t GetFinishedCount() const;

    /* 
     * Returns count of pieces that were acquired but are not saved yet.
     */
    size_t GetPendingCount() const;

    /* 
     * Returns count of pieces that are currently queued for saving.
     */
    size_t GetQueuedCount() const;
private:
    /* 
     * Saves piece to file.
     */
    void SavePieceToFile(std::shared_ptr<Piece> piece);

    /*
     * Reserves space for files on disk (by creating them and filling them with zeros).
     */
    void ReserveSpace(std::ofstream& file, size_t bytesCount);

    /*
     * Initializes file streams for output files.
     */
    void InitFileStreams();

    std::queue<std::shared_ptr<Piece>> piecesQueue_;
    std::vector<bool> savedState_;

    size_t totalCount_ = 0;
    std::atomic<size_t> finishedCount_ = 0;
    std::atomic<size_t> pendingCount_ = 0;

    const TorrentFile& torrentFile_;
    std::filesystem::path outputPath_;
    
    /*
     * In multifile case, holds streams for all files in the same order 
     * as they were stored in torrent file.
     * In single file case, holds one stream for coresponding file.
     */
    std::vector<std::ofstream> outputFiles_;
    std::vector<std::mutex> outputFilesMtxs_;
    std::vector<size_t> outputFilesSizesPref_;

    mutable std::mutex queueMtx_, stateMtx_;
};