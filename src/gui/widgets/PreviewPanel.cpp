#include "gui/widgets/PreviewPanel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QHeaderView>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextCursor>

namespace sse::gui {

PreviewPanel::PreviewPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void PreviewPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    m_stackedWidget = new QStackedWidget(this);

    // =========================================================================
    // Page 0: Empty Page
    // =========================================================================
    m_emptyPage = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(m_emptyPage);
    auto* emptyLabel = new QLabel("Select a file from results to preview content", m_emptyPage);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: #6B7280; font-size: 14px;");
    emptyLayout->addStretch();
    emptyLayout->addWidget(emptyLabel);
    emptyLayout->addStretch();
    m_stackedWidget->addWidget(m_emptyPage);

    // =========================================================================
    // Page 1: Text / Code / PDF Page
    // =========================================================================
    m_textPage = new QWidget(this);
    auto* textLayout = new QVBoxLayout(m_textPage);
    textLayout->setContentsMargins(0, 0, 0, 0);

    auto* textHeaderLayout = new QHBoxLayout();
    m_textTitleLabel = new QLabel(m_textPage);
    m_textTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    m_btnOpenTextExternal = new QPushButton("Open in App", m_textPage);
    m_btnOpenTextExternal->setObjectName("secondaryButton");
    textHeaderLayout->addWidget(m_textTitleLabel, 1);
    textHeaderLayout->addWidget(m_btnOpenTextExternal);
    textLayout->addLayout(textHeaderLayout);

    m_textEdit = new QPlainTextEdit(m_textPage);
    m_textEdit->setReadOnly(true);
    m_textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    textLayout->addWidget(m_textEdit);
    m_stackedWidget->addWidget(m_textPage);

    // =========================================================================
    // Page 2: Image Page
    // =========================================================================
    m_imagePage = new QWidget(this);
    auto* imageLayout = new QVBoxLayout(m_imagePage);
    imageLayout->setContentsMargins(0, 0, 0, 0);

    auto* imgHeaderLayout = new QHBoxLayout();
    m_imageTitleLabel = new QLabel(m_imagePage);
    m_imageTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px;");

    auto* btnZoomIn = new QPushButton("+", m_imagePage);
    btnZoomIn->setFixedWidth(30);
    auto* btnZoomOut = new QPushButton("-", m_imagePage);
    btnZoomOut->setFixedWidth(30);
    auto* btnZoomFit = new QPushButton("Fit", m_imagePage);
    btnZoomFit->setFixedWidth(40);

    imgHeaderLayout->addWidget(m_imageTitleLabel, 1);
    imgHeaderLayout->addWidget(btnZoomIn);
    imgHeaderLayout->addWidget(btnZoomOut);
    imgHeaderLayout->addWidget(btnZoomFit);
    imageLayout->addLayout(imgHeaderLayout);

    m_imageScrollArea = new QScrollArea(m_imagePage);
    m_imageScrollArea->setAlignment(Qt::AlignCenter);
    m_imageLabel = new QLabel(m_imageScrollArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageScrollArea->setWidget(m_imageLabel);
    m_imageScrollArea->setWidgetResizable(false);
    imageLayout->addWidget(m_imageScrollArea);
    m_stackedWidget->addWidget(m_imagePage);

    // =========================================================================
    // Page 3: Media / Archive Metadata Page
    // =========================================================================
    m_mediaPage = new QWidget(this);
    auto* mediaLayout = new QVBoxLayout(m_mediaPage);
    mediaLayout->setContentsMargins(0, 0, 0, 0);

    auto* mediaHeaderLayout = new QHBoxLayout();
    m_mediaTitleLabel = new QLabel(m_mediaPage);
    m_mediaTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    m_btnOpenMediaExternal = new QPushButton("Open in Player", m_mediaPage);
    m_btnOpenMediaExternal->setObjectName("secondaryButton");
    mediaHeaderLayout->addWidget(m_mediaTitleLabel, 1);
    mediaHeaderLayout->addWidget(m_btnOpenMediaExternal);
    mediaLayout->addLayout(mediaHeaderLayout);

    m_mediaMetaTable = new QTableWidget(m_mediaPage);
    m_mediaMetaTable->setColumnCount(2);
    m_mediaMetaTable->setHorizontalHeaderLabels({"Property", "Value"});
    m_mediaMetaTable->horizontalHeader()->setStretchLastSection(true);
    m_mediaMetaTable->verticalHeader()->setVisible(false);
    m_mediaMetaTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_mediaMetaTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mediaLayout->addWidget(m_mediaMetaTable);
    m_stackedWidget->addWidget(m_mediaPage);

    mainLayout->addWidget(m_stackedWidget);

    // Connections
    connect(m_btnOpenTextExternal, &QPushButton::clicked, this, &PreviewPanel::onOpenExternalClicked);
    connect(m_btnOpenMediaExternal, &QPushButton::clicked, this, &PreviewPanel::onOpenExternalClicked);
    connect(btnZoomIn, &QPushButton::clicked, this, &PreviewPanel::onZoomIn);
    connect(btnZoomOut, &QPushButton::clicked, this, &PreviewPanel::onZoomOut);
    connect(btnZoomFit, &QPushButton::clicked, this, &PreviewPanel::onZoomFit);

    clear();
}

void PreviewPanel::setPreviewData(const PreviewData& data, const QString& searchQuery) {
    m_currentFilePath = data.filePath;

    if (data.category == FileCategory::Images && !data.imagePixmap.isNull()) {
        m_originalPixmap = data.imagePixmap;
        m_imageZoomScale = 1.0;
        m_imageTitleLabel->setText(data.title);
        updateImageZoom();
        m_stackedWidget->setCurrentWidget(m_imagePage);
    } else if (data.category == FileCategory::Audio ||
               data.category == FileCategory::Videos ||
               data.category == FileCategory::Archives) {
        m_mediaTitleLabel->setText(data.title);
        m_mediaMetaTable->setRowCount(static_cast<int>(data.metadata.size()));
        int row = 0;
        for (auto it = data.metadata.begin(); it != data.metadata.end(); ++it, ++row) {
            m_mediaMetaTable->setItem(row, 0, new QTableWidgetItem(it.key()));
            m_mediaMetaTable->setItem(row, 1, new QTableWidgetItem(it.value()));
        }
        m_stackedWidget->setCurrentWidget(m_mediaPage);
    } else {
        // Text / Code / PDF or general fallback
        m_textTitleLabel->setText(data.title);
        if (!data.textContent.isEmpty()) {
            m_textEdit->setPlainText(data.textContent);
            highlightTerms(searchQuery);
        } else if (!data.errorMessage.isEmpty()) {
            m_textEdit->setPlainText(QString("Unable to preview file content:\n%1").arg(data.errorMessage));
        } else {
            m_textEdit->setPlainText("(Empty document or binary format)");
        }
        m_stackedWidget->setCurrentWidget(m_textPage);
    }
}

void PreviewPanel::clear() {
    m_currentFilePath.clear();
    m_textEdit->clear();
    m_imageLabel->clear();
    m_mediaMetaTable->setRowCount(0);
    m_stackedWidget->setCurrentWidget(m_emptyPage);
}

void PreviewPanel::onOpenExternalClicked() {
    if (!m_currentFilePath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentFilePath));
    }
}

void PreviewPanel::onZoomIn() {
    m_imageZoomScale = std::min(m_imageZoomScale * 1.25, 5.0);
    updateImageZoom();
}

void PreviewPanel::onZoomOut() {
    m_imageZoomScale = std::max(m_imageZoomScale / 1.25, 0.2);
    updateImageZoom();
}

void PreviewPanel::onZoomFit() {
    if (m_originalPixmap.isNull()) return;

    QSize viewportSize = m_imageScrollArea->viewport()->size();
    double scaleW = static_cast<double>(viewportSize.width() - 20) / m_originalPixmap.width();
    double scaleH = static_cast<double>(viewportSize.height() - 20) / m_originalPixmap.height();
    m_imageZoomScale = std::min(scaleW, scaleH);
    if (m_imageZoomScale <= 0.05) m_imageZoomScale = 1.0;
    updateImageZoom();
}

void PreviewPanel::updateImageZoom() {
    if (m_originalPixmap.isNull()) return;

    QSize newSize = m_originalPixmap.size() * m_imageZoomScale;
    QPixmap scaled = m_originalPixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(scaled);
    m_imageLabel->resize(newSize);
}

void PreviewPanel::highlightTerms(const QString& query) {
    if (query.trimmed().isEmpty()) return;

    // Highlight search query tokens with bright yellow background
    QStringList tokens = query.split(QRegularExpression("[^a-zA-Z0-9]+"), Qt::SkipEmptyParts);

    QTextCharFormat fmt;
    fmt.setBackground(QColor(245, 158, 11, 100)); // Amber transparent
    fmt.setFontWeight(QFont::Bold);

    QTextDocument* doc = m_textEdit->document();

    for (const QString& token : tokens) {
        if (token.length() < 2) continue;
        if (token.compare("AND", Qt::CaseInsensitive) == 0 ||
            token.compare("OR", Qt::CaseInsensitive) == 0 ||
            token.compare("NOT", Qt::CaseInsensitive) == 0) {
            continue;
        }

        QRegularExpression regex(QRegularExpression::escape(token), QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator iter = regex.globalMatch(m_textEdit->toPlainText());
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            QTextCursor cursor(doc);
            cursor.setPosition(static_cast<int>(match.capturedStart()));
            cursor.setPosition(static_cast<int>(match.capturedEnd()), QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(fmt);
        }
    }
}

} // namespace sse::gui
