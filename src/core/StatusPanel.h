#pragma once

#pragma warning(push, 0)
#include "EuroScopePlugIn.h"
#pragma warning(pop)

#include "core/DataManager.h"
#include "core/Server.h"
#include "log/Logger.h"

namespace vacdm::core {

class StatusPanel : public EuroScopePlugIn::CRadarScreen {
public:
    StatusPanel() {}
    virtual ~StatusPanel() {}
    void OnAsrContentToBeClosed(void) override {}

    void OnRefresh(HDC hDC, int Phase) override {
        if (Phase != EuroScopePlugIn::REFRESH_PHASE_AFTER_LISTS) return;

        auto activeAirports = DataManager::instance().getActiveAirports();
        if (activeAirports.empty()) return;

        // Draw basic status panel in top-left
        int x = 10;
        int y = 50;

        RECT r;
        r.left = x;
        r.top = y;
        r.right = x + 120;
        r.bottom = y + (activeAirports.size() * 15) + 20;

        // Draw background
        HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hDC, &r, bgBrush);
        DeleteObject(bgBrush);

        SetBkMode(hDC, TRANSPARENT);

        // Draw Title
        SetTextColor(hDC, RGB(200, 200, 200));
        TextOutA(hDC, x + 5, y + 2, "vACDM STATUS", 12);
        y += 18;

        auto masters = com::Server::instance().getMasterAirports();

        for (const auto& icao : activeAirports) {
            std::string text = icao + ": ";
            
            if (masters.find(icao) != masters.end()) {
                text += "MASTER";
                SetTextColor(hDC, RGB(0, 255, 0)); // Green
            } else if (com::Server::instance().isReadOnlyAirport(icao)) {
                text += "READONLY";
                SetTextColor(hDC, RGB(128, 128, 128)); // Grey
            } else {
                text += "SLAVE"; // In future, check if someone else is master vs nobody (from backend)
                SetTextColor(hDC, RGB(255, 100, 100)); // Red-ish for unmanaged/slave
            }

            TextOutA(hDC, x + 5, y, text.c_str(), text.length());
            y += 15;
        }
    }
};

} // namespace vacdm::core
