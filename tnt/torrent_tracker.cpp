#include "torrent_tracker.h"
#include "bencode/decoding.h"
#include <cpr/cpr.h>
#include <sstream>
#include <netinet/in.h>
#include <iostream>


TorrentTracker::TorrentTracker(const std::string& url) : url_(url) {}

void TorrentTracker::UpdatePeers(const TorrentFile& tf, std::string peerId, int port) {
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

const std::vector<Peer>& TorrentTracker::GetPeers() const {
    return peers_;
}