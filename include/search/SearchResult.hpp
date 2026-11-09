#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "core/CommonTypes.hpp"

namespace sse::ranking {
    struct ScoreExplanation;
}

namespace sse::search {

/**
 * @brief Search result item containing resolved document metadata from DocumentCatalog
 * and optional relevance score and explanation.
 */
struct SearchResultItem {
    DocID        docId{INVALID_DOC_ID};
    std::string  filename;
    std::string  path;
    std::string  extension;
    FileCategory category{FileCategory::Other};
    uint64_t     fileSize{0};
    uint64_t     lastModified{0};
    bool         hasExtractedContent{false};
    double       relevanceScore{0.0};
    uint32_t     rankPosition{0};
    std::shared_ptr<ranking::ScoreExplanation> explanation{nullptr};

    SearchResultItem() = default;

    SearchResultItem(DocID id,
                     std::string fname,
                     std::string fpath,
                     std::string ext,
                     FileCategory cat,
                     uint64_t size,
                     uint64_t modTime,
                     bool extracted,
                     double score = 0.0,
                     uint32_t rank = 0,
                     std::shared_ptr<ranking::ScoreExplanation> expl = nullptr)
        : docId(id)
        , filename(std::move(fname))
        , path(std::move(fpath))
        , extension(std::move(ext))
        , category(cat)
        , fileSize(size)
        , lastModified(modTime)
        , hasExtractedContent(extracted)
        , relevanceScore(score)
        , rankPosition(rank)
        , explanation(std::move(expl)) {}
};

} // namespace sse::search
