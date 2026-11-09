#pragma once

#include <string>
#include <cstdint>
#include "core/CommonTypes.hpp"

namespace sse::search {

/**
 * @brief Search result item containing resolved document metadata from DocumentCatalog.
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

    SearchResultItem() = default;

    SearchResultItem(DocID id,
                     std::string fname,
                     std::string fpath,
                     std::string ext,
                     FileCategory cat,
                     uint64_t size,
                     uint64_t modTime,
                     bool extracted)
        : docId(id)
        , filename(std::move(fname))
        , path(std::move(fpath))
        , extension(std::move(ext))
        , category(cat)
        , fileSize(size)
        , lastModified(modTime)
        , hasExtractedContent(extracted) {}
};

} // namespace sse::search
