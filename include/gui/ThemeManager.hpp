#pragma once

#include <QApplication>
#include <QString>

namespace sse::gui {

enum class ThemeMode {
    Dark,
    Light
};

/**
 * @brief Central theme and styling manager providing modern dark/light QSS sheets.
 */
class ThemeManager {
public:
    static void applyTheme(ThemeMode mode);
    static ThemeMode currentTheme() noexcept { return s_currentTheme; }
    static void toggleTheme();

    static QString getDarkStyleSheet();
    static QString getLightStyleSheet();

private:
    static ThemeMode s_currentTheme;
};

} // namespace sse::gui
