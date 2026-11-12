#include "gui/dialogs/SettingsDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSettings>

namespace sse::gui {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi() {
    setWindowTitle("Application Settings & Preferences");
    resize(460, 320);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    // Ranking algorithm
    m_cmbRankingMode = new QComboBox(this);
    m_cmbRankingMode->addItem("BM25 Probabilistic Ranking (Recommended)", static_cast<int>(ranking::RankingMode::BM25));
    m_cmbRankingMode->addItem("TF-IDF Vector Space Ranking", static_cast<int>(ranking::RankingMode::TFIDF));
    m_cmbRankingMode->addItem("DocID Natural Order (Boolean Only)", static_cast<int>(ranking::RankingMode::DocIdOrder));
    formLayout->addRow("Relevance Ranking:", m_cmbRankingMode);

    // UI Theme
    m_cmbTheme = new QComboBox(this);
    m_cmbTheme->addItem("Modern Dark Theme", static_cast<int>(ThemeMode::Dark));
    m_cmbTheme->addItem("Clean Light Theme", static_cast<int>(ThemeMode::Light));
    formLayout->addRow("Application Theme:", m_cmbTheme);

    // Top-K Results limit
    auto* topKLayout = new QHBoxLayout();
    m_sliderTopK = new QSlider(Qt::Horizontal, this);
    m_sliderTopK->setRange(10, 500);
    m_sliderTopK->setValue(50);
    m_spinTopK = new QSpinBox(this);
    m_spinTopK->setRange(10, 500);
    m_spinTopK->setValue(50);
    connect(m_sliderTopK, &QSlider::valueChanged, m_spinTopK, &QSpinBox::setValue);
    connect(m_spinTopK, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderTopK, &QSlider::setValue);
    topKLayout->addWidget(m_sliderTopK, 1);
    topKLayout->addWidget(m_spinTopK);
    formLayout->addRow("Default Result Limit (Top-K):", topKLayout);

    // Max Extraction MB
    auto* extLayout = new QHBoxLayout();
    m_sliderExtractionMb = new QSlider(Qt::Horizontal, this);
    m_sliderExtractionMb->setRange(5, 100);
    m_sliderExtractionMb->setValue(50);
    m_spinExtractionMb = new QSpinBox(this);
    m_spinExtractionMb->setRange(5, 100);
    m_spinExtractionMb->setValue(50);
    m_spinExtractionMb->setSuffix(" MB");
    connect(m_sliderExtractionMb, &QSlider::valueChanged, m_spinExtractionMb, &QSpinBox::setValue);
    connect(m_spinExtractionMb, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderExtractionMb, &QSlider::setValue);
    extLayout->addWidget(m_sliderExtractionMb, 1);
    extLayout->addWidget(m_spinExtractionMb);
    formLayout->addRow("Max File Extraction Cap:", extLayout);

    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    m_btnSave = new QPushButton("Save Preferences", this);
    auto* btnCancel = new QPushButton("Cancel", this);
    btnCancel->setObjectName("secondaryButton");

    btnLayout->addStretch();
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(m_btnSave);
    mainLayout->addLayout(btnLayout);

    connect(m_btnSave, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::loadSettings() {
    QSettings settings("UniversalSearchEngine", "SearchEngineApp");

    int modeInt = settings.value("ranking/mode", static_cast<int>(ranking::RankingMode::BM25)).toInt();
    int idx = m_cmbRankingMode->findData(modeInt);
    if (idx >= 0) m_cmbRankingMode->setCurrentIndex(idx);

    int themeInt = settings.value("ui/theme", static_cast<int>(ThemeMode::Dark)).toInt();
    int tIdx = m_cmbTheme->findData(themeInt);
    if (tIdx >= 0) m_cmbTheme->setCurrentIndex(tIdx);

    int topK = settings.value("search/topK", 50).toInt();
    m_spinTopK->setValue(topK);

    int extMb = settings.value("indexing/maxExtractionMb", 50).toInt();
    m_spinExtractionMb->setValue(extMb);
}

void SettingsDialog::saveSettings() {
    QSettings settings("UniversalSearchEngine", "SearchEngineApp");

    settings.setValue("ranking/mode", m_cmbRankingMode->currentData().toInt());
    settings.setValue("ui/theme", m_cmbTheme->currentData().toInt());
    settings.setValue("search/topK", m_spinTopK->value());
    settings.setValue("indexing/maxExtractionMb", m_spinExtractionMb->value());
}

void SettingsDialog::onSaveClicked() {
    saveSettings();
    emit settingsSaved();
    accept();
}

ranking::RankingMode SettingsDialog::rankingMode() const {
    return static_cast<ranking::RankingMode>(m_cmbRankingMode->currentData().toInt());
}

int SettingsDialog::topKLimit() const {
    return m_spinTopK->value();
}

ThemeMode SettingsDialog::themeMode() const {
    return static_cast<ThemeMode>(m_cmbTheme->currentData().toInt());
}

int SettingsDialog::maxExtractionMb() const {
    return m_spinExtractionMb->value();
}

} // namespace sse::gui
