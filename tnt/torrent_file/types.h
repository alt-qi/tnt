#pragma once

#include <vector>
#include <optional>
#include <variant>
#include <algorithm>
#include <string>

struct TorrentFile {
    using AnnounceListType = std::vector<std::vector<std::string>>;

    struct SingleFileStructure {
        std::string name;
        size_t length;
    };

    struct MultiFileStructure {
        struct File {
            std::vector<std::string> path;
            size_t length;
        };

        std::string name;
        std::vector<File> files;
    };

    struct Info {
        size_t pieceLength;
        std::vector<std::string> pieces;
        std::variant<SingleFileStructure, MultiFileStructure> structure;
    } info;

    /*
     * Calculates size of the data that torrent file represents in bytes.
     */
    size_t CalculateSize() const;
    
    std::string announce;
    std::optional<AnnounceListType> announceList;

    std::optional<std::string> comment;
    std::optional<std::string> author;

    std::string infoHash;
};
