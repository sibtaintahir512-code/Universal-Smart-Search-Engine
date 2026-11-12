#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include "gui/models/SearchResultModel.hpp"

namespace sse::gui {

/**
 * @brief Filter panel enabling real-time facet filtering by category, size, and date.
 */
class FilterPanel : public QWidget {
    Q_OBJECT

public:
    explicit FilterPanel(QWidget* parent = nullptr);
    ~FilterPanel() override = default;

    ResultFilterCriteria criteria() const;
    void resetFilters();

signals:
    void filterChanged(const ResultFilterCriteria& criteria);

private slots:
    void onCategoryAllToggled(bool checked);
    void onIndividualCategoryChanged();
    void onFilterControlChanged();

private:
    QCheckBox*   m_chkAll{nullptr};
    QCheckBox*   m_chkDocs{nullptr};
    QCheckBox*   m_chkCode{nullptr};
    QCheckBox*   m_chkImages{nullptr};
    QCheckBox*   m_chkMedia{nullptr};
    QCheckBox*   m_chkArchives{nullptr};
    QCheckBox*   m_chkOther{nullptr};

    QComboBox*   m_cmbSize{nullptr};
    QComboBox*   m_cmbDate{nullptr};
    QPushButton* m_btnReset{nullptr};

    bool         m_updatingGroup{false};

    void setupUi();
    void emitCriteria();
};

} // namespace sse::gui
