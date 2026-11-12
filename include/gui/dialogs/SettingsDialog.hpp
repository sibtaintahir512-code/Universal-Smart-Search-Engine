#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include "ranking/RankingConfig.hpp"
#include "gui/ThemeManager.hpp"

namespace sse::gui {

/**
 * @brief Application preferences dialog backed by QSettings.
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    ranking::RankingMode rankingMode() const;
    int topKLimit() const;
    ThemeMode themeMode() const;
    int maxExtractionMb() const;

signals:
    void settingsSaved();

private slots:
    void onSaveClicked();

private:
    QComboBox*   m_cmbRankingMode{nullptr};
    QComboBox*   m_cmbTheme{nullptr};
    QSlider*     m_sliderTopK{nullptr};
    QSpinBox*    m_spinTopK{nullptr};
    QSlider*     m_sliderExtractionMb{nullptr};
    QSpinBox*    m_spinExtractionMb{nullptr};
    QPushButton* m_btnSave{nullptr};

    void setupUi();
    void loadSettings();
    void saveSettings();
};

} // namespace sse::gui
