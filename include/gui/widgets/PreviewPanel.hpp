#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include "gui/workers/WorkerTypes.hpp"

namespace sse::gui {

/**
 * @brief Multi-format preview panel rendering text snippets, high-DPI images,
 * and metadata summaries.
 */
class PreviewPanel : public QWidget {
    Q_OBJECT

public:
    explicit PreviewPanel(QWidget* parent = nullptr);
    ~PreviewPanel() override = default;

    void setPreviewData(const PreviewData& data, const QString& searchQuery = QString());
    void clear();

private slots:
    void onOpenExternalClicked();
    void onZoomIn();
    void onZoomOut();
    void onZoomFit();

private:
    QStackedWidget* m_stackedWidget{nullptr};

    // Page 0: Empty
    QWidget*        m_emptyPage{nullptr};

    // Page 1: Text / Code / PDF View
    QWidget*        m_textPage{nullptr};
    QLabel*         m_textTitleLabel{nullptr};
    QPlainTextEdit* m_textEdit{nullptr};
    QPushButton*    m_btnOpenTextExternal{nullptr};

    // Page 2: Image View
    QWidget*        m_imagePage{nullptr};
    QLabel*         m_imageTitleLabel{nullptr};
    QLabel*         m_imageLabel{nullptr};
    QScrollArea*    m_imageScrollArea{nullptr};
    QPixmap         m_originalPixmap;
    double          m_imageZoomScale{1.0};

    // Page 3: Multimedia / Archive View
    QWidget*        m_mediaPage{nullptr};
    QLabel*         m_mediaTitleLabel{nullptr};
    QTableWidget*   m_mediaMetaTable{nullptr};
    QPushButton*    m_btnOpenMediaExternal{nullptr};

    QString         m_currentFilePath;

    void setupUi();
    void highlightTerms(const QString& query);
    void updateImageZoom();
};

} // namespace sse::gui
