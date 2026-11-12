#include "gui/dialogs/DashboardDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

namespace sse::gui {

DashboardDialog::DashboardDialog(SearchEngineAdapter* adapter, QWidget* parent)
    : QDialog(parent)
    , m_adapter(adapter)
{
    setupUi();
    refreshMetrics();
}

void DashboardDialog::setupUi() {
    setWindowTitle("Search Engine Telemetry & DSA Health Dashboard");
    resize(560, 420);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto* gridLayout = new QGridLayout();
    gridLayout->setSpacing(12);

    // Card 1: Corpus & Catalog
    auto* cardCorpus = new QGroupBox("Corpus & Document Catalog", this);
    auto* l1 = new QVBoxLayout(cardCorpus);
    m_lblCatalogTotal = new QLabel("Total Files: -", cardCorpus);
    m_lblCatalogActive = new QLabel("Active Documents: -", cardCorpus);
    l1->addWidget(m_lblCatalogTotal);
    l1->addWidget(m_lblCatalogActive);
    gridLayout->addWidget(cardCorpus, 0, 0);

    // Card 2: Inverted Index Metrics
    auto* cardIndex = new QGroupBox("Inverted Index & Vocabulary", this);
    auto* l2 = new QVBoxLayout(cardIndex);
    m_lblVocabTerms = new QLabel("Vocabulary Terms: -", cardIndex);
    m_lblTrieTerms = new QLabel("Trie Terms: -", cardIndex);
    m_lblTotalPostings = new QLabel("Total Postings: -", cardIndex);
    l2->addWidget(m_lblVocabTerms);
    l2->addWidget(m_lblTrieTerms);
    l2->addWidget(m_lblTotalPostings);
    gridLayout->addWidget(cardIndex, 0, 1);

    // Card 3: Custom DSA Performance Health
    auto* cardDsa = new QGroupBox("Custom DSA Library Health", this);
    auto* l3 = new QVBoxLayout(cardDsa);
    m_lblTrieNodes = new QLabel("Trie Allocated Nodes: -", cardDsa);
    auto* lblDsaNote = new QLabel("Zero STL containers in core indexing.", cardDsa);
    lblDsaNote->setStyleSheet("color: #10B981; font-weight: 500; font-size: 11px;");
    l3->addWidget(m_lblTrieNodes);
    l3->addWidget(lblDsaNote);
    gridLayout->addWidget(cardDsa, 1, 0);

    // Card 4: Search & Persistence Telemetry
    auto* cardTelemetry = new QGroupBox("Runtime Search & Persistence", this);
    auto* l4 = new QVBoxLayout(cardTelemetry);
    m_lblPersistenceGen = new QLabel("Index Generation: -", cardTelemetry);
    m_lblQueriesRun = new QLabel("Queries Executed: -", cardTelemetry);
    m_lblAvgLatency = new QLabel("Average Latency: -", cardTelemetry);
    l4->addWidget(m_lblPersistenceGen);
    l4->addWidget(m_lblQueriesRun);
    l4->addWidget(m_lblAvgLatency);
    gridLayout->addWidget(cardTelemetry, 1, 1);

    mainLayout->addLayout(gridLayout);

    // Bottom controls
    auto* btnLayout = new QHBoxLayout();
    m_btnRefresh = new QPushButton("Refresh Metrics", this);
    auto* btnClose = new QPushButton("Close", this);
    btnClose->setObjectName("secondaryButton");

    btnLayout->addWidget(m_btnRefresh);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    mainLayout->addLayout(btnLayout);

    connect(m_btnRefresh, &QPushButton::clicked, this, &DashboardDialog::refreshMetrics);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void DashboardDialog::refreshMetrics() {
    if (!m_adapter) return;

    auto t = m_adapter->getTelemetry();

    m_lblCatalogTotal->setText(QString("Total Files Cataloged: %1").arg(t.totalCatalogDocuments));
    m_lblCatalogActive->setText(QString("Active Indexed Documents: %1").arg(t.activeCatalogDocuments));

    m_lblVocabTerms->setText(QString("Unique Vocabulary Terms: %1").arg(t.vocabularyTerms));
    m_lblTrieTerms->setText(QString("Trie Latin Terms: %1").arg(t.trieVocabularyTerms));
    m_lblTotalPostings->setText(QString("Total Posting Nodes: %1").arg(t.totalPostings));

    m_lblTrieNodes->setText(QString("Trie Allocated Nodes: %1").arg(t.trieNodeCount));

    m_lblPersistenceGen->setText(QString("Index Generation: #%1").arg(t.persistenceGeneration));
    m_lblQueriesRun->setText(QString("Queries Executed: %1").arg(t.totalQueriesExecuted));
    m_lblAvgLatency->setText(QString("Average Latency: %1 ms").arg(QString::number(t.avgQueryTimeMs, 'f', 2)));
}

} // namespace sse::gui
