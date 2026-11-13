#include "gui/ThemeManager.hpp"
#include <QApplication>
#include <QStyleFactory>

namespace sse::gui {

ThemeMode ThemeManager::s_currentTheme = ThemeMode::Dark;

void ThemeManager::applyTheme(ThemeMode mode) {
    s_currentTheme = mode;
    if (qApp) {
        if (mode == ThemeMode::Dark) {
            qApp->setStyleSheet(getDarkStyleSheet());
        } else {
            qApp->setStyleSheet(getLightStyleSheet());
        }
    }
}

void ThemeManager::toggleTheme() {
    if (s_currentTheme == ThemeMode::Dark) {
        applyTheme(ThemeMode::Light);
    } else {
        applyTheme(ThemeMode::Dark);
    }
}

QString ThemeManager::getDarkStyleSheet() {
    return QString(R"(
        /* Global & Base */
        QMainWindow, QDialog, QWidget#centralWidget {
            background-color: #1E1E24;
            color: #F3F4F6;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            font-size: 13px;
        }

        /* Menu Bar */
        QMenuBar {
            background-color: #1E1E24;
            color: #F3F4F6;
            border-bottom: 1px solid #3F4050;
            padding: 2px 4px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 4px 10px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #2A2B36;
        }

        /* Menu */
        QMenu {
            background-color: #2A2B36;
            color: #F3F4F6;
            border: 1px solid #3F4050;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #4F46E5;
            color: #FFFFFF;
        }
        QMenu::separator {
            height: 1px;
            background-color: #3F4050;
            margin: 4px 8px;
        }

        /* Tool Bar */
        QToolBar {
            background-color: #1E1E24;
            border-bottom: 1px solid #3F4050;
            padding: 4px;
            spacing: 6px;
        }
        QToolButton {
            background-color: #2A2B36;
            color: #F3F4F6;
            border: 1px solid #3F4050;
            border-radius: 5px;
            padding: 5px 12px;
            font-weight: 500;
        }
        QToolButton:hover {
            background-color: #383948;
            border-color: #6366F1;
        }
        QToolButton:pressed {
            background-color: #4F46E5;
        }

        /* Inputs & LineEdits */
        QLineEdit {
            background-color: #2A2B36;
            color: #FFFFFF;
            border: 1px solid #3F4050;
            border-radius: 6px;
            padding: 8px 12px;
            selection-background-color: #4F46E5;
            selection-color: #FFFFFF;
        }
        QLineEdit:focus {
            border: 1px solid #6366F1;
            background-color: #323340;
        }

        /* Buttons */
        QPushButton {
            background-color: #4F46E5;
            color: #FFFFFF;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #6366F1;
        }
        QPushButton:pressed {
            background-color: #4338CA;
        }
        QPushButton:disabled {
            background-color: #323340;
            color: #6B7280;
        }
        QPushButton#secondaryButton {
            background-color: #2A2B36;
            color: #F3F4F6;
            border: 1px solid #3F4050;
        }
        QPushButton#secondaryButton:hover {
            background-color: #383948;
        }

        /* Table & Lists */
        QTableView {
            background-color: #1E1E24;
            alternate-background-color: #24252F;
            color: #F3F4F6;
            gridline-color: #2A2B36;
            border: 1px solid #3F4050;
            border-radius: 6px;
            selection-background-color: #3730A3;
            selection-color: #FFFFFF;
            outline: none;
        }
        QTableView::item {
            padding: 6px;
            border-bottom: 1px solid #2A2B36;
        }
        QTableView::item:selected {
            background-color: #3730A3;
            color: #FFFFFF;
        }

        QHeaderView::section {
            background-color: #2A2B36;
            color: #9CA3AF;
            border: none;
            border-right: 1px solid #3F4050;
            border-bottom: 1px solid #3F4050;
            padding: 8px;
            font-weight: 600;
        }

        /* Group Boxes */
        QGroupBox {
            border: 1px solid #3F4050;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 14px;
            font-weight: 600;
            color: #F3F4F6;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 6px;
            color: #818CF8;
        }

        /* Checkboxes & Radios */
        QCheckBox, QRadioButton {
            color: #D1D5DB;
            spacing: 8px;
        }
        QCheckBox::indicator, QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            border: 1px solid #4B5563;
            background-color: #2A2B36;
        }
        QRadioButton::indicator {
            border-radius: 8px;
        }
        QCheckBox::indicator:checked, QRadioButton::indicator:checked {
            background-color: #4F46E5;
            border-color: #6366F1;
        }

        /* Combo Boxes */
        QComboBox {
            background-color: #2A2B36;
            color: #F3F4F6;
            border: 1px solid #3F4050;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #2A2B36;
            color: #F3F4F6;
            selection-background-color: #4F46E5;
            border: 1px solid #3F4050;
        }

        /* Sliders */
        QSlider::groove:horizontal {
            height: 6px;
            background: #3F4050;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #4F46E5;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #FFFFFF;
            border: 2px solid #6366F1;
            width: 14px;
            margin-top: -5px;
            margin-bottom: -5px;
            border-radius: 7px;
        }

        /* Progress Bar */
        QProgressBar {
            background-color: #2A2B36;
            border: 1px solid #3F4050;
            border-radius: 5px;
            text-align: center;
            color: #FFFFFF;
            font-weight: bold;
        }
        QProgressBar::chunk {
            background-color: #4F46E5;
            border-radius: 4px;
        }

        /* Scrollbars */
        QScrollBar:vertical {
            background: #1E1E24;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #3F4050;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background: #6366F1;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background: #1E1E24;
            height: 10px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #3F4050;
            min-width: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #6366F1;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }

        /* Tabs */
        QTabWidget::pane {
            border: 1px solid #3F4050;
            background-color: #2A2B36;
            border-radius: 6px;
        }
        QTabBar::tab {
            background-color: #1E1E24;
            color: #9CA3AF;
            padding: 8px 16px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-weight: 500;
        }
        QTabBar::tab:selected {
            background-color: #2A2B36;
            color: #FFFFFF;
            border-bottom: 2px solid #6366F1;
        }

        /* Status Bar */
        QStatusBar {
            background-color: #1E1E24;
            border-top: 1px solid #3F4050;
            color: #9CA3AF;
        }
        QStatusBar QLabel {
            color: #9CA3AF;
        }

        /* PlainTextEdit (Code / Text viewer) */
        QPlainTextEdit {
            background-color: #18181B;
            color: #E4E4E7;
            border: 1px solid #3F4050;
            border-radius: 6px;
            font-family: "JetBrains Mono", Menlo, Monaco, Consolas, "Courier New", monospace;
            font-size: 12px;
        }
    )");
}

QString ThemeManager::getLightStyleSheet() {
    return QString(R"(
        /* Global & Base */
        QMainWindow, QDialog, QWidget#centralWidget {
            background-color: #F8FAFC;
            color: #0F172A;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            font-size: 13px;
        }

        /* Menu Bar */
        QMenuBar {
            background-color: #F8FAFC;
            color: #0F172A;
            border-bottom: 1px solid #E2E8F0;
            padding: 2px 4px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 4px 10px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #E2E8F0;
        }

        /* Menu */
        QMenu {
            background-color: #FFFFFF;
            color: #0F172A;
            border: 1px solid #CBD5E1;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #4F46E5;
            color: #FFFFFF;
        }
        QMenu::separator {
            height: 1px;
            background-color: #E2E8F0;
            margin: 4px 8px;
        }

        /* Tool Bar */
        QToolBar {
            background-color: #FFFFFF;
            border-bottom: 1px solid #E2E8F0;
            padding: 4px;
            spacing: 6px;
        }
        QToolButton {
            background-color: #F1F5F9;
            color: #0F172A;
            border: 1px solid #CBD5E1;
            border-radius: 5px;
            padding: 5px 12px;
            font-weight: 500;
        }
        QToolButton:hover {
            background-color: #E2E8F0;
            border-color: #4F46E5;
        }
        QToolButton:pressed {
            background-color: #4F46E5;
            color: #FFFFFF;
        }

        /* Inputs & LineEdits */
        QLineEdit {
            background-color: #FFFFFF;
            color: #0F172A;
            border: 1px solid #CBD5E1;
            border-radius: 6px;
            padding: 8px 12px;
            selection-background-color: #4F46E5;
            selection-color: #FFFFFF;
        }
        QLineEdit:focus {
            border: 1px solid #4F46E5;
            background-color: #FFFFFF;
        }

        /* Buttons */
        QPushButton {
            background-color: #4F46E5;
            color: #FFFFFF;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #4338CA;
        }
        QPushButton:pressed {
            background-color: #3730A3;
        }
        QPushButton:disabled {
            background-color: #E2E8F0;
            color: #94A3B8;
        }
        QPushButton#secondaryButton {
            background-color: #F1F5F9;
            color: #0F172A;
            border: 1px solid #CBD5E1;
        }
        QPushButton#secondaryButton:hover {
            background-color: #E2E8F0;
        }

        /* Table & Lists */
        QTableView {
            background-color: #FFFFFF;
            alternate-background-color: #F8FAFC;
            color: #0F172A;
            gridline-color: #E2E8F0;
            border: 1px solid #CBD5E1;
            border-radius: 6px;
            selection-background-color: #EEF2FF;
            selection-color: #312E81;
            outline: none;
        }
        QTableView::item {
            padding: 6px;
            border-bottom: 1px solid #F1F5F9;
        }
        QTableView::item:selected {
            background-color: #EEF2FF;
            color: #312E81;
        }

        QHeaderView::section {
            background-color: #F1F5F9;
            color: #475569;
            border: none;
            border-right: 1px solid #E2E8F0;
            border-bottom: 1px solid #CBD5E1;
            padding: 8px;
            font-weight: 600;
        }

        /* Group Boxes */
        QGroupBox {
            border: 1px solid #CBD5E1;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 14px;
            font-weight: 600;
            color: #0F172A;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 6px;
            color: #4F46E5;
        }

        /* Checkboxes & Radios */
        QCheckBox, QRadioButton {
            color: #334155;
            spacing: 8px;
        }
        QCheckBox::indicator, QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            border: 1px solid #94A3B8;
            background-color: #FFFFFF;
        }
        QRadioButton::indicator {
            border-radius: 8px;
        }
        QCheckBox::indicator:checked, QRadioButton::indicator:checked {
            background-color: #4F46E5;
            border-color: #4F46E5;
        }

        /* Combo Boxes */
        QComboBox {
            background-color: #FFFFFF;
            color: #0F172A;
            border: 1px solid #CBD5E1;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #FFFFFF;
            color: #0F172A;
            selection-background-color: #4F46E5;
            border: 1px solid #CBD5E1;
        }

        /* Sliders */
        QSlider::groove:horizontal {
            height: 6px;
            background: #E2E8F0;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #4F46E5;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #FFFFFF;
            border: 2px solid #4F46E5;
            width: 14px;
            margin-top: -5px;
            margin-bottom: -5px;
            border-radius: 7px;
        }

        /* Progress Bar */
        QProgressBar {
            background-color: #F1F5F9;
            border: 1px solid #CBD5E1;
            border-radius: 5px;
            text-align: center;
            color: #0F172A;
            font-weight: bold;
        }
        QProgressBar::chunk {
            background-color: #4F46E5;
            border-radius: 4px;
        }

        /* Scrollbars */
        QScrollBar:vertical {
            background: #F8FAFC;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #CBD5E1;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background: #4F46E5;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background: #F8FAFC;
            height: 10px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #CBD5E1;
            min-width: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #4F46E5;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }

        /* Tabs */
        QTabWidget::pane {
            border: 1px solid #CBD5E1;
            background-color: #FFFFFF;
            border-radius: 6px;
        }
        QTabBar::tab {
            background-color: #F1F5F9;
            color: #64748B;
            padding: 8px 16px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-weight: 500;
        }
        QTabBar::tab:selected {
            background-color: #FFFFFF;
            color: #0F172A;
            border-bottom: 2px solid #4F46E5;
        }

        /* Status Bar */
        QStatusBar {
            background-color: #FFFFFF;
            border-top: 1px solid #E2E8F0;
            color: #64748B;
        }
        QStatusBar QLabel {
            color: #64748B;
        }

        /* PlainTextEdit (Code / Text viewer) */
        QPlainTextEdit {
            background-color: #F8FAFC;
            color: #0F172A;
            border: 1px solid #CBD5E1;
            border-radius: 6px;
            font-family: "JetBrains Mono", Menlo, Monaco, Consolas, "Courier New", monospace;
            font-size: 12px;
        }
    )");
}

} // namespace sse::gui
