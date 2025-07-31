#include "connected_peers_row.h"
#include "../codes.h"
#include <sstream>

ConnectedPeersStatusRow::ConnectedPeersStatusRow(
    std::function<std::tuple<int, int>()> dataSrc
) : dataSrc_(dataSrc) {}

std::string ConnectedPeersStatusRow::GetValue() {
    auto [connectedCnt, totalCnt] = dataSrc_();

    std::stringstream sstr;
    sstr << CLEAR_LINE << RESET << LIGHT_GRAY << " ~ "
        << "Peers connected: " << YELLOW << BOLD << connectedCnt
        << RESET << DARK_GRAY << " out of " << totalCnt << " found";

    return sstr.str();
}