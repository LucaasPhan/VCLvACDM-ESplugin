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
    StatusPanel() : m_panelX(10), m_panelY(50), m_isDragging(false) {
        m_dragOffset.x = 0;
        m_dragOffset.y = 0;
    }
    virtual ~StatusPanel() {}
    void OnAsrContentToBeClosed(void) override { delete this; }

    void OnButtonDownScreenObject(int ObjectType, const char* sObjectId, POINT pt, RECT Area, int nButton) override {
        if (nButton != 1) return; // Left click only

        auto activeAirports = DataManager::instance().getActiveAirports();
        if (activeAirports.empty()) return;

        m_isDragging = true;
        m_dragOffset.x = pt.x - m_panelX;
        m_dragOffset.y = pt.y - m_panelY;
    }

    void OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT pt, RECT Area, bool Released) override {
        if (m_isDragging) {
            if (Released) {
                m_isDragging = false;
            } else {
                m_panelX = pt.x - m_dragOffset.x;
                m_panelY = pt.y - m_dragOffset.y;
                this->RequestRefresh();
            }
        }
    }

    void OnButtonUpScreenObject(int ObjectType, const char* sObjectId, POINT pt, RECT Area, int nButton) override {
        if (nButton == 1) {
            m_isDragging = false;
        }
    }

    void OnRefresh(HDC hDC, int Phase) override {
        if (Phase != EuroScopePlugIn::REFRESH_PHASE_AFTER_LISTS) return;

        auto activeAirports = DataManager::instance().getActiveAirports();
        if (activeAirports.empty()) return;

        // Draw status panel at its current position
        int x = m_panelX;
        int y = m_panelY;

        RECT r;
        r.left = x;
        r.top = y;
        r.right = x + 120;
        r.bottom = y + (static_cast<int>(activeAirports.size()) * 15) + 20;

        // Register screen object for interaction
        this->AddScreenObject(1000, "StatusPanel", r, true, "");

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
            auto meta = com::Server::instance().getAirportMetadata(icao);
            
            if (masters.find(icao) != masters.end()) {
                text += "MASTER";
                SetTextColor(hDC, RGB(0, 255, 0)); // Green
            } else if (meta.readOnly) {
                text += "READONLY";
                SetTextColor(hDC, RGB(128, 128, 128)); // Grey
            } else {
                text += "SLAVE"; 
                if (!meta.master.empty()) {
                    text += " (" + meta.master + ")";
                }
                SetTextColor(hDC, RGB(255, 100, 100)); // Red-ish for unmanaged/slave
            }

            TextOutA(hDC, x + 5, y, text.c_str(), static_cast<int>(text.length()));
            y += 15;
        }
    }

private:
    int m_panelX;
    int m_panelY;
    bool m_isDragging;
    POINT m_dragOffset;
};

} // namespace vacdm::core
