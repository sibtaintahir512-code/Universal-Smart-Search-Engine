#include "gui/widgets/FilterPanel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTime>

namespace sse::gui {

FilterPanel::FilterPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void FilterPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(12);

    auto* header = new QLabel("<b>Filters & Facets</b>", this);
    mainLayout->addWidget(header);

    // 1. Categories Group
    auto* catGroup = new QGroupBox("File Categories", this);
    auto* catLayout = new QVBoxLayout(catGroup);
    catLayout->setSpacing(6);

    m_chkAll = new QCheckBox("All Categories", catGroup);
    m_chkAll->setChecked(true);
    m_chkDocs = new QCheckBox("Documents (.pdf, .docx, .txt)", catGroup);
    m_chkDocs->setChecked(true);
    m_chkCode = new QCheckBox("Source Code (.cpp, .py, .h)", catGroup);
    m_chkCode->setChecked(true);
    m_chkImages = new QCheckBox("Images (.png, .jpg, .bmp)", catGroup);
    m_chkImages->setChecked(true);
    m_chkMedia = new QCheckBox("Media (.mp3, .mp4, .wav)", catGroup);
    m_chkMedia->setChecked(true);
    m_chkArchives = new QCheckBox("Archives (.zip, .tar, .gz)", catGroup);
    m_chkArchives->setChecked(true);
    m_chkOther = new QCheckBox("Other Files", catGroup);
    m_chkOther->setChecked(true);

    catLayout->addWidget(m_chkAll);
    catLayout->addWidget(m_chkDocs);
    catLayout->addWidget(m_chkCode);
    catLayout->addWidget(m_chkImages);
    catLayout->addWidget(m_chkMedia);
    catLayout->addWidget(m_chkArchives);
    catLayout->addWidget(m_chkOther);

    mainLayout->addWidget(catGroup);

    // 2. File Size Group
    auto* sizeGroup = new QGroupBox("File Size", this);
    auto* sizeLayout = new QVBoxLayout(sizeGroup);

    m_cmbSize = new QComboBox(sizeGroup);
    m_cmbSize->addItem("Any Size", 0);
    m_cmbSize->addItem("Small (< 1 MB)", 1);
    m_cmbSize->addItem("Medium (1 MB – 10 MB)", 2);
    m_cmbSize->addItem("Large (10 MB – 100 MB)", 3);
    m_cmbSize->addItem("Very Large (> 100 MB)", 4);
    sizeLayout->addWidget(m_cmbSize);

    mainLayout->addWidget(sizeGroup);

    // 3. Date Modified Group
    auto* dateGroup = new QGroupBox("Date Modified", this);
    auto* dateLayout = new QVBoxLayout(dateGroup);

    m_cmbDate = new QComboBox(dateGroup);
    m_cmbDate->addItem("Any Time", 0);
    m_cmbDate->addItem("Past 24 Hours", 1);
    m_cmbDate->addItem("Past 7 Days", 2);
    m_cmbDate->addItem("Past 30 Days", 3);
    m_cmbDate->addItem("Past Year", 4);
    dateLayout->addWidget(m_cmbDate);

    mainLayout->addWidget(dateGroup);

    // Reset Button
    m_btnReset = new QPushButton("Reset All Filters", this);
    m_btnReset->setObjectName("secondaryButton");
    mainLayout->addWidget(m_btnReset);

    mainLayout->addStretch();

    // Signal connections
    connect(m_chkAll, &QCheckBox::toggled, this, &FilterPanel::onCategoryAllToggled);
    connect(m_chkDocs, &QCheckBox::toggled, this, &FilterPanel::onIndividualCategoryChanged);
    connect(m_chkCode, &QCheckBox::toggled, this, &FilterPanel::onIndividualCategoryChanged);
    connect(m_chkImages, &QCheckBox::toggled, this, &FilterPanel::onIndividualCategoryChanged);
    connect(m_chkMedia, &QCheckBox::toggled, this, &FilterPanel::onIndividualCategoryChanged);
    connect(m_chkArchives, &QCheckBox::toggled, this, &FilterPanel::onIndividualCategoryChanged);
    connect(m_chkOther, &QCheckBox::toggled, this, &FilterPanel::onIndividualCategoryChanged);

    connect(m_cmbSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilterPanel::onFilterControlChanged);
    connect(m_cmbDate, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilterPanel::onFilterControlChanged);
    connect(m_btnReset, &QPushButton::clicked, this, &FilterPanel::resetFilters);
}

void FilterPanel::onCategoryAllToggled(bool checked) {
    if (m_updatingGroup) return;
    m_updatingGroup = true;

    m_chkDocs->setChecked(checked);
    m_chkCode->setChecked(checked);
    m_chkImages->setChecked(checked);
    m_chkMedia->setChecked(checked);
    m_chkArchives->setChecked(checked);
    m_chkOther->setChecked(checked);

    m_updatingGroup = false;
    emitCriteria();
}

void FilterPanel::onIndividualCategoryChanged() {
    if (m_updatingGroup) return;
    m_updatingGroup = true;

    bool allChecked = m_chkDocs->isChecked() && m_chkCode->isChecked() &&
                      m_chkImages->isChecked() && m_chkMedia->isChecked() &&
                      m_chkArchives->isChecked() && m_chkOther->isChecked();
    m_chkAll->setChecked(allChecked);

    m_updatingGroup = false;
    emitCriteria();
}

void FilterPanel::onFilterControlChanged() {
    emitCriteria();
}

ResultFilterCriteria FilterPanel::criteria() const {
    ResultFilterCriteria c;
    c.includeDocuments = m_chkDocs->isChecked();
    c.includeCode = m_chkCode->isChecked();
    c.includeImages = m_chkImages->isChecked();
    c.includeMedia = m_chkMedia->isChecked();
    c.includeArchives = m_chkArchives->isChecked();
    c.includeOther = m_chkOther->isChecked();

    // Size criteria
    int sizeIdx = m_cmbSize->currentIndex();
    switch (sizeIdx) {
        case 1: // < 1 MB
            c.minSizeBytes = 0;
            c.maxSizeBytes = 1024ULL * 1024ULL;
            break;
        case 2: // 1 MB - 10 MB
            c.minSizeBytes = 1024ULL * 1024ULL;
            c.maxSizeBytes = 10ULL * 1024ULL * 1024ULL;
            break;
        case 3: // 10 MB - 100 MB
            c.minSizeBytes = 10ULL * 1024ULL * 1024ULL;
            c.maxSizeBytes = 100ULL * 1024ULL * 1024ULL;
            break;
        case 4: // > 100 MB
            c.minSizeBytes = 100ULL * 1024ULL * 1024ULL;
            c.maxSizeBytes = UINT64_MAX;
            break;
        default: // Any size
            c.minSizeBytes = 0;
            c.maxSizeBytes = UINT64_MAX;
            break;
    }

    // Date criteria
    int dateIdx = m_cmbDate->currentIndex();
    uint64_t nowSecs = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
    switch (dateIdx) {
        case 1: // Past 24 hours
            c.minModifiedTimestamp = (nowSecs > 86400) ? (nowSecs - 86400) : 0;
            break;
        case 2: // Past 7 days
            c.minModifiedTimestamp = (nowSecs > 7 * 86400) ? (nowSecs - 7 * 86400) : 0;
            break;
        case 3: // Past 30 days
            c.minModifiedTimestamp = (nowSecs > 30 * 86400) ? (nowSecs - 30 * 86400) : 0;
            break;
        case 4: // Past year
            c.minModifiedTimestamp = (nowSecs > 365 * 86400) ? (nowSecs - 365 * 86400) : 0;
            break;
        default:
            c.minModifiedTimestamp = 0;
            break;
    }

    return c;
}

void FilterPanel::resetFilters() {
    m_updatingGroup = true;
    m_chkAll->setChecked(true);
    m_chkDocs->setChecked(true);
    m_chkCode->setChecked(true);
    m_chkImages->setChecked(true);
    m_chkMedia->setChecked(true);
    m_chkArchives->setChecked(true);
    m_chkOther->setChecked(true);
    m_cmbSize->setCurrentIndex(0);
    m_cmbDate->setCurrentIndex(0);
    m_updatingGroup = false;

    emitCriteria();
}

void FilterPanel::emitCriteria() {
    emit filterChanged(criteria());
}

} // namespace sse::gui
