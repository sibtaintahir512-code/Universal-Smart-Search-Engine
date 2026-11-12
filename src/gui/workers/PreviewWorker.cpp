#include "gui/workers/PreviewWorker.hpp"
#include <QFileInfo>
#include <QImageReader>

namespace sse::gui {

PreviewWorker::PreviewWorker(SearchEngineAdapter* adapter, QObject* parent)
    : QObject(parent)
    , m_adapter(adapter)
{
}

void PreviewWorker::loadPreview(quint32 docId,
                                const QString& filePath,
                                int categoryInt,
                                const QString& /*searchQuery*/) {
    if (!m_adapter) {
        emit previewFailed(docId, "Backend adapter unavailable");
        return;
    }

    m_cancelToken.store(false, std::memory_order_relaxed);

    PreviewData data;
    data.docId = static_cast<DocID>(docId);
    data.filePath = filePath;
    data.category = static_cast<FileCategory>(categoryInt);

    QFileInfo fi(filePath);
    data.title = fi.fileName();

    if (!fi.exists()) {
        data.success = false;
        data.errorMessage = "File does not exist on disk";
        emit previewReady(data);
        return;
    }

    // Populate basic metadata
    data.metadata["File Name"] = fi.fileName();
    data.metadata["File Path"] = fi.absoluteFilePath();
    data.metadata["File Size"] = QString::number(fi.size()) + " bytes";
    data.metadata["Last Modified"] = fi.lastModified().toString(Qt::ISODate);

    // If it's an image, load thumbnail
    if (data.category == FileCategory::Images) {
        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        QSize imageSize = reader.size();
        if (imageSize.isValid()) {
            data.metadata["Dimensions"] = QString("%1 x %2").arg(imageSize.width()).arg(imageSize.height());
            data.metadata["Format"] = QString::fromLatin1(reader.format()).toUpper();
        }

        // Clamp thumbnail size for snappy memory
        QImage img = reader.read();
        if (!img.isNull()) {
            if (img.width() > 1200 || img.height() > 1200) {
                img = img.scaled(1200, 1200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            data.imagePixmap = QPixmap::fromImage(img);
            data.success = true;
        } else {
            data.errorMessage = "Failed to decode image data";
        }
    }

    // Extract text / metadata content via extraction engine
    try {
        extraction::ExtractionResult result;
        if (docId != INVALID_DOC_ID) {
            result = m_adapter->extractPreview(docId, &m_cancelToken);
        } else {
            result = m_adapter->extractPreviewForPath(filePath.toStdString(), data.category, &m_cancelToken);
        }

        if (m_cancelToken.load(std::memory_order_relaxed)) {
            return;
        }

        if (result.isSuccess() || !result.extractedText.empty()) {
            data.success = true;
            data.isTruncated = result.isTruncated;
            data.textContent = QString::fromUtf8(result.extractedText.c_str(), static_cast<qsizetype>(result.extractedText.size()));
            if (!result.extractorId.empty()) {
                data.metadata["Extractor Engine"] = QString::fromStdString(result.extractorId);
            }
            if (!result.metadataSummary.empty()) {
                data.metadata["Extraction Summary"] = QString::fromStdString(result.metadataSummary);
            }
        } else if (data.category != FileCategory::Images) {
            data.errorMessage = QString::fromStdString(result.errorDescription);
            if (data.errorMessage.isEmpty()) {
                data.errorMessage = "Content extraction returned no readable text";
            }
        }
    } catch (const std::exception& ex) {
        data.errorMessage = QString::fromUtf8(ex.what());
    }

    emit previewReady(data);
}

void PreviewWorker::cancel() {
    m_cancelToken.store(true, std::memory_order_relaxed);
}

} // namespace sse::gui
