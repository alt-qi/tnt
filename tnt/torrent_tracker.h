#pragma once

#include "torrent_file/types.h"
#include "peer.h"
#include <string>
#include <vector>


class TorrentTracker {
public:
    TorrentTracker(const std::string& announce_url);
    void UpdatePeers(const TorrentFile& tf, const std::string& peerId, int port);
    const std::vector<Peer>& GetPeers() const;

private:
    std::string url_;
    std::vector<Peer> peers_;
};