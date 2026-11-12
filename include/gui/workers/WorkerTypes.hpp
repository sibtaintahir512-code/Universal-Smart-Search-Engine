#pragma once

#include <QString>
#include <QPixmap>
#include <QMap>
#include <QMetaType>
#include "core/CommonTypes.hpp"
#include "search/SearchResponse.hpp"

namespace sse::gui {

/**
 * @brief Structured container holding file preview and extraction details.
 */
struct PreviewData {
    DocID                   docId{INVALID_DOC_ID};
    QString                 filePath;
    FileCategory            category{FileCategory::Other};
    QString                 title;
    QString                 textContent;
    QPixmap                 imagePixmap;
    QMap<QString, QString>  metadata;
    bool                    isTruncated{false};
    bool                    success{false};
    QString                 errorMessage;
};

} // namespace sse::gui

Q_DECLARE_METATYPE(sse::search::SearchResponse)
Q_DECLARE_METATYPE(sse::gui::PreviewData)
