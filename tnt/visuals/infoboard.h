#pragma once

#include "info_row.h"
#include <string>
#include <functional>
#include <tuple>
#include <thread>
#include <memory>


class InfoBoard {
public:
    InfoBoard(int rowCount, std::chrono::milliseconds checkInterval);
    ~InfoBoard();

    /*
     * Starts the infoboard updater thread.
     */
    void Start();

    /*
     * Stops the infoboard updater thread.
     */
    void Stop();

    void SetRow(int rowIdx, std::shared_ptr<InfoRow> row, std::chrono::milliseconds updateInterval);
private:
    void DisplayRow(int rowIdx);
    void MoveUp(int cnt);
    void MoveDown(int cnt);

    std::thread thr_;
    std::atomic<int> running_;

    std::chrono::milliseconds checkInterval_;
    std::vector<std::chrono::milliseconds> updateIntervals_;
    std::vector<std::shared_ptr<InfoRow>> rows_;
    std::vector<std::chrono::time_point<std::chrono::steady_clock>> lastDisplayed_;
};