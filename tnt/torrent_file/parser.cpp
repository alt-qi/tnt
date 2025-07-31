#include "../bencode/decoding.h"
#include "../bencode/encoding.h"
#include "types.h"
#include <iostream>
#include <sstream>
#include <memory>
#include "../../hash-library/sha1_util.h"

using DataMapType = std::map<std::string, std::shared_ptr<Bencode::Entity>>;

std::string TfGetAnnounce(DataMapType& dataMap) {
    if (!dataMap.count("announce"))
        throw std::runtime_error("Torrent file is invalid (data dictionary is missing \"announce\" key)");

    if (std::dynamic_pointer_cast<Bencode::String>(dataMap["announce"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"announce\" value is not a string)");
    
    return std::static_pointer_cast<Bencode::String>(dataMap["announce"])->value;
}

std::optional<std::string> TfGetComment(DataMapType& dataMap) {
    if (!dataMap.count("comment"))
        return std::nullopt;
    
    if (std::dynamic_pointer_cast<Bencode::String>(dataMap["comment"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"comment\" value is not a string)");
    
    return std::static_pointer_cast<Bencode::String>(dataMap["comment"])->value;
}

std::optional<std::string> TfGetAuthor(DataMapType& dataMap) {
    if (!dataMap.count("created by"))
        return std::nullopt;
    
    if (std::dynamic_pointer_cast<Bencode::String>(dataMap["created by"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"created by\" value is not a string)");
    
    return std::static_pointer_cast<Bencode::String>(dataMap["created by"])->value;
}

std::optional<TorrentFile::AnnounceListType> TfGetAnnounceList(DataMapType& dataMap) {
    if (!dataMap.count("announce-list"))
        return std::nullopt;
    
    if (std::dynamic_pointer_cast<Bencode::List>(dataMap["announce-list"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"announce-list\" value is not a list)");

    TorrentFile::AnnounceListType result;
    for (auto tierPtr : std::static_pointer_cast<Bencode::List>(dataMap["announce-list"])->value) {
        if (std::dynamic_pointer_cast<Bencode::List>(tierPtr) == nullptr)
            throw std::runtime_error("Torrent file is invalid (one of \"announce-list\" tiers is not a list)");
    
        result.emplace_back();
        for (auto elementPtr : std::static_pointer_cast<Bencode::List>(tierPtr)->value) {
            if (std::dynamic_pointer_cast<Bencode::String>(elementPtr) == nullptr)
                throw std::runtime_error("Torrent file is invalid (one of elements in some tier is not a string (\"announce-list\"))");
            result.back().push_back(std::static_pointer_cast<Bencode::String>(elementPtr)->value);
        }
    }
    
    return result;
}

TorrentFile::SingleFileStructure TfGetSingleFileStructure(DataMapType& infoMap) {
    TorrentFile::SingleFileStructure result;

    if (!infoMap.count("name"))
        throw std::runtime_error("Torrent file is invalid (info dictionary is missing \"name\" key)");
    if (std::dynamic_pointer_cast<Bencode::String>(infoMap["name"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"name\" value is not a string)");
    result.name = std::static_pointer_cast<Bencode::String>(infoMap["name"])->value;
    
    if (!infoMap.count("length"))
        throw std::runtime_error("Torrent file is invalid (info dictionary is missing \"length\" key)");
    if (std::dynamic_pointer_cast<Bencode::Integer>(infoMap["length"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"length\" value is not a integer)");
    result.length = std::static_pointer_cast<Bencode::Integer>(infoMap["length"])->value;

    return result;
}

TorrentFile::MultiFileStructure TfGetMultiFileStructure(DataMapType& infoMap) {
    TorrentFile::MultiFileStructure result;

    if (!infoMap.count("name"))
        throw std::runtime_error("Torrent file is invalid (info dictionary is missing \"name\" key)");
    if (std::dynamic_pointer_cast<Bencode::String>(infoMap["name"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"name\" value is not a string)");
    result.name = std::static_pointer_cast<Bencode::String>(infoMap["name"])->value;
    
    if (!infoMap.count("files"))
        throw std::runtime_error("Torrent file is invalid (info dictionary is missing \"files\" key)");
    if (std::dynamic_pointer_cast<Bencode::List>(infoMap["files"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"files\" value is not a list)");

    for (auto filePtr : std::static_pointer_cast<Bencode::List>(infoMap["files"])->value) {
        if (std::dynamic_pointer_cast<Bencode::Dict>(filePtr) == nullptr)
            throw std::runtime_error("Torrent file is invalid (one of \"files\" elements is not a dictionary)");
        
        auto fileMap = std::static_pointer_cast<Bencode::Dict>(filePtr)->value;
        TorrentFile::MultiFileStructure::File file;

        if (!fileMap.count("length"))
            throw std::runtime_error("Torrent file is invalid (\"files\" dictionary is missing \"length\" key)");
        if (std::dynamic_pointer_cast<Bencode::Integer>(fileMap["length"]) == nullptr)
            throw std::runtime_error("Torrent file is invalid (\"files.length\" value is not an integer)");
        
        file.length = std::static_pointer_cast<Bencode::Integer>(fileMap["length"])->value;

        if (!fileMap.count("path"))
            throw std::runtime_error("Torrent file is invalid (\"files\" dictionary is missing \"path\" key)");
        if (std::dynamic_pointer_cast<Bencode::List>(fileMap["path"]) == nullptr)
            throw std::runtime_error("Torrent file is invalid (\"files.path\" value is not a list)");
        
        for (auto pathPtr : std::static_pointer_cast<Bencode::List>(fileMap["path"])->value) {
            if (std::dynamic_pointer_cast<Bencode::String>(pathPtr) == nullptr)
                throw std::runtime_error("Torrent file is invalid (one of \"files.path\" elements is not a string)");
            file.path.push_back(std::static_pointer_cast<Bencode::String>(pathPtr)->value);
        }

        result.files.push_back(std::move(file));
    }

    return result;
}

TorrentFile::Info TfGetInfo(DataMapType& infoMap) {
    TorrentFile::Info result;

    if (!infoMap.count("piece length"))
        throw std::runtime_error("Torrent file is invalid (info dictionary is missing \"piece length\" key)");
    if (std::dynamic_pointer_cast<Bencode::Integer>(infoMap["piece length"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"piece length\" value is not an integer)");
    result.pieceLength = std::static_pointer_cast<Bencode::Integer>(infoMap["piece length"])->value;
    
    if (!infoMap.count("pieces"))
        throw std::runtime_error("Torrent file is invalid (info dictionary is missing \"pieces\" key)");
    if (std::dynamic_pointer_cast<Bencode::String>(infoMap["pieces"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"pieces\" value is not a string)");
    std::string pieces = std::static_pointer_cast<Bencode::String>(infoMap["pieces"])->value;
    for (size_t i = 0; i + 20 <= pieces.size(); i += 20)
        result.pieces.push_back(pieces.substr(i, 20));

    if (!infoMap.count("files")) {
        result.structure = TfGetSingleFileStructure(infoMap);
    } else {
        result.structure = TfGetMultiFileStructure(infoMap);
    }
 
    return result;
}

TorrentFile ParseTorrentFile(std::istream& stream) {
    TorrentFile result;
    
    auto data = std::make_shared<Bencode::Entity>();
    stream >> data;

    if (std::dynamic_pointer_cast<Bencode::Dict>(data) == nullptr)
        throw std::runtime_error("Torrent file is invalid (couldn't parse data dictionary)");
    auto dataMap = std::static_pointer_cast<Bencode::Dict>(data)->value;

    result.announce = TfGetAnnounce(dataMap);
    result.announceList = TfGetAnnounceList(dataMap);
    
    result.comment = TfGetComment(dataMap);
    result.author = TfGetAuthor(dataMap);

    if (!dataMap.count("info"))
        throw std::runtime_error("Torrent file is invalid (data dictionary is missing info dictionary)");
    if (std::dynamic_pointer_cast<Bencode::Dict>(dataMap["info"]) == nullptr)
        throw std::runtime_error("Torrent file is invalid (\"info\" is not a dictionary)");
    auto infoMap = std::static_pointer_cast<Bencode::Dict>(dataMap["info"])->value;

    result.info = TfGetInfo(infoMap);

    std::stringstream encoded;
    Bencode::WriteEntity(encoded, dataMap["info"]);

    result.infoHash.resize(20);
    openssl_compat::SHA1(
        reinterpret_cast<unsigned char*>(&encoded.str()[0]), 
        encoded.str().size(), 
        reinterpret_cast<unsigned char*>(&(result.infoHash[0]))
    );

    return result;
}

std::istream& operator>>(std::istream& stream, TorrentFile& file) {
    file = ParseTorrentFile(stream);
    return stream;
}
