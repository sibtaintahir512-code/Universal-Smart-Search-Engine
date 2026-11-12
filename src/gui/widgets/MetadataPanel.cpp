#include "gui/widgets/MetadataPanel.hpp"
#include "gui/adapters/SearchEngineAdapter.hpp"
#include "gui/models/SearchResultModel.hpp"
#include "ranking/RankingResult.hpp"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QDateTime>

namespace sse::gui {

MetadataPanel::MetadataPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void MetadataPanel::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    auto* metaHeader = new QLabel("<b>File Properties</b>", this);
    layout->addWidget(metaHeader);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({"Attribute", "Value"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table, 1);

    auto* explHeader = new QLabel("<b>Relevance Score Breakdown</b>", this);
    layout->addWidget(explHeader);

    m_explanationView = new QTextEdit(this);
    m_explanationView->setReadOnly(true);
    m_explanationView->setFontFamily("JetBrains Mono, Menlo, Courier New, monospace");
    m_explanationView->setFontPointSize(10);
    layout->addWidget(m_explanationView, 1);

    clear();
}

void MetadataPanel::addRow(const QString& property, const QString& value) {
    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(property));
    m_table->setItem(row, 1, new QTableWidgetItem(value));
}

void MetadataPanel::setItem(const search::SearchResultItem* item, const SearchEngineAdapter* adapter) {
    clear();
    if (!item) return;

    m_table->setRowCount(0);

    addRow("Document ID", QString::number(item->docId));
    addRow("File Name", QString::fromStdString(item->filename));
    addRow("Category", SearchResultModel::formatCategory(item->category));
    addRow("Extension", QString::fromStdString(item->extension));
    addRow("File Size", SearchResultModel::formatFileSize(item->fileSize) + QString(" (%1 bytes)").arg(item->fileSize));
    addRow("Last Modified", SearchResultModel::formatTimestamp(item->lastModified));
    addRow("Relevance Score", QString::number(item->relevanceScore, 'f', 4));
    addRow("Rank Position", QString("#%1").arg(item->rankPosition));
    addRow("Canonical Path", QString::fromStdString(item->path));

    if (adapter) {
        const FileMetadata* meta = adapter->getDocumentMetadata(item->docId);
        if (meta) {
            addRow("Active Status", (meta->flags & META_FLAG_ACTIVE) ? "Active" : "Tombstoned");
            addRow("Supports Extraction", (meta->flags & META_FLAG_SUPPORTS_CONTENT_EXTRACT) ? "Yes" : "Metadata Only");
        }
    }

    // Score explanation
    if (item->explanation) {
        const auto& exp = *item->explanation;
        QString text;
        text += QString("<b>Algorithm:</b> %1<br>")
            .arg(exp.mode == ranking::RankingMode::BM25 ? "BM25 Probabilistic Model" : "TF-IDF Vector Space");
        text += QString("<b>Final Score:</b> %1<br>").arg(QString::number(exp.finalScore, 'f', 6));
        text += QString("<b>Filename Contribution:</b> %1 (Weight: %2x)<br>")
            .arg(QString::number(exp.filenameContribution, 'f', 4)).arg(exp.filenameWeight);
        text += QString("<b>Body Contribution:</b> %1 (Weight: %2x)<br>")
            .arg(QString::number(exp.bodyContribution, 'f', 4)).arg(exp.bodyWeight);
        text += QString("<b>Doc Body Length:</b> %1 tokens (Avg: %2)<br>")
            .arg(exp.documentBodyLength).arg(QString::number(exp.averageBodyLength, 'f', 1));
        text += QString("<b>Doc Name Length:</b> %1 tokens (Avg: %2)<br><br>")
            .arg(exp.documentNameLength).arg(QString::number(exp.averageNameLength, 'f', 1));

        if (exp.termDetails.size() > 0) {
            text += "<b>Term Score Breakdown:</b><br>";
            text += "<table border='1' cellspacing='0' cellpadding='4' style='border-collapse:collapse;'>";
            text += "<tr><th>Term</th><th>Name TF</th><th>Body TF</th><th>IDF</th><th>Score</th></tr>";
            for (std::size_t i = 0; i < exp.termDetails.size(); ++i) {
                const auto& td = exp.termDetails[i];
                text += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                    .arg(QString::fromStdString(td.term))
                    .arg(td.nameTf)
                    .arg(td.bodyTf)
                    .arg(QString::number(td.idf, 'f', 3))
                    .arg(QString::number(td.totalTermScore, 'f', 4));
            }
            text += "</table>";
        }

        m_explanationView->setHtml(text);
    } else {
        m_explanationView->setText("No score explanation available (Boolean or default docId ordering).");
    }
}

void MetadataPanel::clear() {
    m_table->setRowCount(0);
    m_explanationView->clear();
}

} // namespace sse::gui
