#pragma once

#pragma warning(push, 0)
#include "EuroScopePlugIn.h"
#pragma warning(pop)

#include "config/PluginConfig.h"

#include <algorithm>
#include <cstring>
#include <list>

#include "core/DataManager.h"
#include "core/Server.h"
#include "log/Logger.h"

namespace vacdm::core {

class StatusPanel : public EuroScopePlugIn::CRadarScreen {
public:
    static inline PluginConfig pluginConfig;
    static void updatePluginConfig(PluginConfig newPluginConfig) { pluginConfig = newPluginConfig; }

    StatusPanel() : m_panelX(pluginConfig.panelX), m_panelY(pluginConfig.panelY), m_isDragging(false) {
        m_dragOffset.x = 0;
        m_dragOffset.y = 0;
    }
    virtual ~StatusPanel() {}
    void OnAsrContentToBeClosed(void) override { delete this; }

    void OnButtonDownScreenObject(int /*ObjectType*/, const char* sObjectId, POINT pt, RECT /*Area*/, int nButton) override {
        if (nButton != 1) return;
        if (strcmp(sObjectId, "StatusPanelClose") == 0) {
            pluginConfig.showPanel = false;
            this->RequestRefresh();
            return;
        }
        if (strcmp(sObjectId, "StatusPanel") == 0) {
            m_isDragging = true;
            m_dragOffset.x = pt.x - m_panelX;
            m_dragOffset.y = pt.y - m_panelY;
        }
    }

    void OnMoveScreenObject(int /*ObjectType*/, const char* /*sObjectId*/, POINT pt, RECT /*Area*/, bool Released) override {
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

    void OnButtonUpScreenObject(int /*ObjectType*/, const char* /*sObjectId*/, POINT /*pt*/, RECT /*Area*/, int nButton) override {
        if (nButton == 1)
            m_isDragging = false;
    }

    void OnRefresh(HDC hDC, int Phase) override {
        if (Phase != EuroScopePlugIn::REFRESH_PHASE_AFTER_LISTS) return;
        if (!pluginConfig.showPanel) return;

        auto activeAirports = DataManager::instance().getActiveAirports();
        auto masters = com::Server::instance().getMasterAirports();
        std::list<std::string> displayAirports = activeAirports;

        for (const auto& icao : masters) {
            if (std::find(displayAirports.begin(), displayAirports.end(), icao) == displayAirports.end())
                displayAirports.push_back(icao);
        }

        // --- Layout constants (matches StandsPanel) ---
        const int PANEL_W  = 180;
        const int TITLE_H  = 16;
        const int HEADER_H = 16;
        const int ROW_H    = 15;
        const int COL_ICAO = 6;
        const int COL_ROLE = 60;

        // --- Calculate height based on content ---
        int contentH = 0;
        if (displayAirports.empty()) {
            contentH = ROW_H;
        } else {
            for (const auto& icao : displayAirports) {
                contentH += ROW_H;
                auto meta = com::Server::instance().getAirportMetadata(icao);
                contentH += static_cast<int>(meta.activeDelays.size()) * ROW_H;
            }
        }
        int PANEL_H  = TITLE_H + HEADER_H + contentH + 4;

        int x = m_panelX;
        int y = m_panelY;

        // --- Background ---
        RECT panelRect = { x, y, x + PANEL_W, y + PANEL_H };
        HBRUSH bgBrush = CreateSolidBrush(pluginConfig.panelColor);
        FillRect(hDC, &panelRect, bgBrush);
        DeleteObject(bgBrush);

        // --- Title Background ---
        RECT titleBgRect = { x, y, x + PANEL_W, y + TITLE_H };
        HBRUSH titleBrush = CreateSolidBrush(pluginConfig.panelHeaderColor);
        FillRect(hDC, &titleBgRect, titleBrush);
        DeleteObject(titleBrush);

        // --- Column Header Background ---
        RECT colHeaderRect = { x, y + TITLE_H, x + PANEL_W, y + TITLE_H + HEADER_H };
        HBRUSH colHeaderBrush = CreateSolidBrush(pluginConfig.panelColumnHeaderColor);
        FillRect(hDC, &colHeaderRect, colHeaderBrush);
        DeleteObject(colHeaderBrush);

        // Border
        HBRUSH borderBrush = CreateSolidBrush(RGB(32, 32, 32));
        FrameRect(hDC, &panelRect, borderBrush);
        DeleteObject(borderBrush);

        SetBkMode(hDC, TRANSPARENT);

        // Register title bar for dragging
        RECT titleRect = { x, y, x + PANEL_W - 16, y + TITLE_H };
        this->AddScreenObject(1000, "StatusPanel", titleRect, true, "");

        // Register close button
        RECT closeRect = { x + PANEL_W - 16, y, x + PANEL_W, y + TITLE_H };
        this->AddScreenObject(1001, "StatusPanelClose", closeRect, false, "");

        // --- Title ---
        SetTextColor(hDC, pluginConfig.panelTextColor);
        TextOutA(hDC, x + COL_ICAO, y + 2, "ACDM STATUS", 12);

        // --- Close Button ---
        SetTextColor(hDC, pluginConfig.panelTextColor);
        TextOutA(hDC, x + PANEL_W - 12, y + 2, "X", 1);

        // Divider under title
        HPEN divPen = CreatePen(PS_SOLID, 1, RGB(110, 110, 110));
        HPEN oldPen = (HPEN)SelectObject(hDC, divPen);
        MoveToEx(hDC, x, y + TITLE_H, NULL);
        LineTo(hDC, x + PANEL_W, y + TITLE_H);
        y += TITLE_H;

        // --- Column headers ---
        SetTextColor(hDC, pluginConfig.panelTextColor);
        TextOutA(hDC, x + COL_ICAO, y + 2, "ICAO", 4);
        TextOutA(hDC, x + COL_ROLE, y + 2, "POS", 4);

        // Divider under headers
        MoveToEx(hDC, x, y + HEADER_H, NULL);
        LineTo(hDC, x + PANEL_W, y + HEADER_H);
        y += HEADER_H;

        SelectObject(hDC, oldPen);
        DeleteObject(divPen);

        // --- Rows ---
        if (displayAirports.empty()) {
            SetTextColor(hDC, pluginConfig.panelTextColor);
            const char* text = "NO ACTIVE AIRPORT";
            TextOutA(hDC, x + COL_ICAO, y + 2, text, static_cast<int>(strlen(text)));
            return;
        }

        int i = 0;
        for (const auto& icao : displayAirports) {
            auto meta = com::Server::instance().getAirportMetadata(icao);

            // Alternating row shading for the header part
            RECT rowRect = { x + 1, y, x + PANEL_W - 1, y + ROW_H };
            HBRUSH rowBrush = CreateSolidBrush((i % 2 == 0) ? RGB(50, 50, 50) : RGB(60, 60, 60));
            FillRect(hDC, &rowRect, rowBrush);
            DeleteObject(rowBrush);

            // ICAO label
            SetTextColor(hDC, pluginConfig.panelTextColor);
            std::string icaoText = icao;
            if (meta.lvo) icaoText += " (LVO)";
            TextOutA(hDC, x + COL_ICAO, y + 2, icaoText.c_str(), static_cast<int>(icaoText.length()));

            // Role label
            std::string role;
            COLORREF roleColor;
            if (masters.find(icao) != masters.end()) {
                role      = "MASTER";
                roleColor = RGB(255, 200, 60);   // amber
            } else {
                role = "SLAVE";
                if (!meta.master.empty())
                    role += " (" + meta.master + ")";
                roleColor = RGB(80, 160, 255);   // blue
            }

            SetTextColor(hDC, roleColor);
            TextOutA(hDC, x + COL_ROLE, y + 2, role.c_str(), static_cast<int>(role.length()));

            y += ROW_H;

            // Draw active delays if any
            for (const auto& delay : meta.activeDelays) {
                RECT delayRect = { x + 1, y, x + PANEL_W - 1, y + ROW_H };
                HBRUSH delayBrush = CreateSolidBrush((i % 2 == 0) ? RGB(45, 45, 45) : RGB(55, 55, 55));
                FillRect(hDC, &delayRect, delayBrush);
                DeleteObject(delayBrush);

                SetTextColor(hDC, RGB(255, 100, 100)); // Red for delays
                std::string delayText = "- " + delay;
                TextOutA(hDC, x + COL_ICAO + 4, y + 2, delayText.c_str(), static_cast<int>(delayText.length()));
                y += ROW_H;
            }

            ++i;
        }
    }

private:
    int   m_panelX;
    int   m_panelY;
    bool  m_isDragging;
    POINT m_dragOffset;
};

} // namespace vacdm::core