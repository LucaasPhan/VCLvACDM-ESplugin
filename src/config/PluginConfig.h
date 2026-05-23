#pragma once

#include <string>

#pragma warning(push, 0)
#include "EuroScopePlugIn.h"
#pragma warning(pop)

namespace vacdm {
struct PluginConfig {
    bool valid = true;
    std::string serverUrl = "https://app.vacdm.net";
    std::string apiKey = "";
    COLORREF lightgreen = RGB(127, 252, 73);
    COLORREF lightblue = RGB(53, 218, 235);
    COLORREF darkgreen = RGB(0, 128, 0);
    COLORREF green = RGB(0, 181, 27);
    COLORREF blue = RGB(0, 0, 255);
    COLORREF lightyellow = RGB(255, 255, 191);
    COLORREF yellow = RGB(255, 255, 0);
    COLORREF orange = RGB(255, 153, 0);
    COLORREF red = RGB(255, 0, 0);
    COLORREF grey = RGB(153, 153, 153);
    COLORREF white = RGB(255, 255, 255);
    COLORREF debug = RGB(255, 0, 255);
    int panelX = 10;
    int panelY = 50;
    COLORREF panelColor = RGB(50, 50, 50);
    COLORREF panelHeaderColor = RGB(60, 60, 60);
    COLORREF panelColumnHeaderColor = RGB(45, 45, 45);
    COLORREF panelTextColor = RGB(220, 220, 220);
    bool showPanel = true;
};
}  // namespace vacdm