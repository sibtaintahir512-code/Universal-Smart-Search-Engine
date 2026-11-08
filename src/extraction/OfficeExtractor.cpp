#include "extraction/OfficeExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <pugixml.hpp>
#include <miniz.h>

namespace sse::extraction {

bool OfficeExtractor::supportsExtension(const std::string& ext) const noexcept {
    return (ext == "docx" || ext == "pptx" || ext == "xlsx");
}

bool OfficeExtractor::supportsCategory(FileCategory category) const noexcept {
    return category == FileCategory::Documents;
}

std::string OfficeExtractor::decodeXmlEntities(const std::string& text) {
    std::string result;
    result.reserve(text.size());

    std::size_t i = 0;
    const std::size_t n = text.size();

    while (i < n) {
        if (text[i] == '&') {
            std::size_t semi = text.find(';', i);
            if (semi != std::string::npos && semi - i <= 10) {
                std::string entity = text.substr(i + 1, semi - i - 1);
                if (entity == "amp") {
                    result.push_back('&');
                    i = semi + 1;
                    continue;
                } else if (entity == "lt") {
                    result.push_back('<');
                    i = semi + 1;
                    continue;
                } else if (entity == "gt") {
                    result.push_back('>');
                    i = semi + 1;
                    continue;
                } else if (entity == "quot") {
                    result.push_back('"');
                    i = semi + 1;
                    continue;
                } else if (entity == "apos") {
                    result.push_back('\'');
                    i = semi + 1;
                    continue;
                } else if (!entity.empty() && entity[0] == '#') {
                    try {
                        uint32_t cp = 0;
                        if (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')) {
                            cp = static_cast<uint32_t>(std::stoul(entity.substr(2), nullptr, 16));
                        } else {
                            cp = static_cast<uint32_t>(std::stoul(entity.substr(1), nullptr, 10));
                        }
                        if (cp <= 0x7F) {
                            result.push_back(static_cast<char>(cp));
                        } else if (cp <= 0x7FF) {
                            result.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
                            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else if (cp <= 0xFFFF) {
                            result.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
                            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else {
                            result.append("\xEF\xBF\xBD");
                        }
                        i = semi + 1;
                        continue;
                    } catch (...) {}
                }
            }
        }
        result.push_back(text[i]);
        ++i;
    }

    return result;
}

std::string OfficeExtractor::stripXmlAndDecode(const std::string& xml) {
    pugi::xml_document doc;
    pugi::xml_parse_result parseRes = doc.load_buffer(xml.data(), xml.size(), pugi::parse_default);
    if (!parseRes) {
        // Fallback for malformed XML fragment: remove tags directly
        std::string text;
        text.reserve(xml.size() / 2);
        bool inside = false;
        for (char c : xml) {
            if (c == '<') inside = true;
            else if (c == '>') inside = false;
            else if (!inside) text.push_back(c);
        }
        return decodeXmlEntities(text);
    }

    std::string text;
    pugi::xpath_node_set paras = doc.select_nodes("//w:p | //a:p | //p | //row");
    if (!paras.empty()) {
        for (pugi::xpath_node pNode : paras) {
            std::string pText;
            for (pugi::xpath_node tNode : pNode.node().select_nodes(".//text()")) {
                pText += tNode.node().value();
            }
            if (!pText.empty()) {
                if (!text.empty()) text.push_back('\n');
                text += pText;
            }
        }
        return decodeXmlEntities(text);
    }

    for (pugi::xpath_node xpNode : doc.select_nodes("//text()")) {
        pugi::xml_node node = xpNode.node();
        std::string val = node.value();
        if (!val.empty()) {
            if (!text.empty() && text.back() != ' ' && text.back() != '\n') {
                text.push_back(' ');
            }
            text += val;
        }
    }
    return decodeXmlEntities(text);
}

namespace {

// Helper to safely extract a zip entry into a string with decompression limits
bool extractZipEntryToMemory(mz_zip_archive& zip,
                            mz_uint fileIndex,
                            std::string& outData,
                            uint64_t& cumulativeDecompressed,
                            uint64_t maxDecompressedEntry = 50ULL * 1024 * 1024,
                            uint64_t maxCumulativeBytes = 100ULL * 1024 * 1024,
                            uint32_t maxRatio = 100) {
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip, fileIndex, &stat)) return false;

    // Reject entries exceeding size limits
    if (stat.m_uncomp_size > maxDecompressedEntry) return false;
    if ((cumulativeDecompressed + stat.m_uncomp_size) > maxCumulativeBytes) return false;

    // Check compression ratio
    if (stat.m_comp_size > 0 && (stat.m_uncomp_size / stat.m_comp_size) > maxRatio) return false;

    outData.resize(static_cast<std::size_t>(stat.m_uncomp_size));
    if (stat.m_uncomp_size > 0) {
        if (!mz_zip_reader_extract_to_mem(&zip, fileIndex, outData.data(), outData.size(), 0)) {
            outData.clear();
            return false;
        }
    }

    cumulativeDecompressed += stat.m_uncomp_size;
    return true;
}

// Find index of entry by exact or case-insensitive name
int findZipEntryIndex(mz_zip_archive& zip, const std::string& name) {
    int idx = mz_zip_reader_locate_file(&zip, name.c_str(), nullptr, 0);
    if (idx >= 0) return idx;

    // Case-insensitive fallback
    mz_uint numFiles = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < numFiles; ++i) {
        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_file_stat(&zip, i, &stat)) {
            std::string entryName = stat.m_filename;
            if (entryName.size() == name.size()) {
                bool match = true;
                for (std::size_t c = 0; c < name.size(); ++c) {
                    if (std::tolower(static_cast<unsigned char>(entryName[c])) !=
                        std::tolower(static_cast<unsigned char>(name[c]))) {
                        match = false;
                        break;
                    }
                }
                if (match) return static_cast<int>(i);
            }
        }
    }
    return -1;
}

// Extract text runs from a DOCX body node
void extractDocxNode(const pugi::xml_node& node, std::string& outText, uint64_t maxBytes) {
    for (pugi::xml_node child : node.children()) {
        if (outText.size() >= maxBytes) return;

        std::string name = child.name();
        if (name == "w:p") {
            for (pugi::xml_node pChild : child.children()) {
                std::string pName = pChild.name();
                if (pName == "w:r") {
                    for (pugi::xml_node rChild : pChild.children()) {
                        std::string rName = rChild.name();
                        if (rName == "w:t") {
                            outText += rChild.text().get();
                        } else if (rName == "w:tab") {
                            outText.push_back('\t');
                        } else if (rName == "w:br") {
                            outText.push_back('\n');
                        }
                    }
                }
            }
            if (!outText.empty() && outText.back() != '\n') {
                outText.push_back('\n');
            }
        } else if (name == "w:tbl") {
            for (pugi::xml_node row : child.children("w:tr")) {
                for (pugi::xml_node cell : row.children("w:tc")) {
                    extractDocxNode(cell, outText, maxBytes);
                    if (!outText.empty() && outText.back() != '\t' && outText.back() != '\n') {
                        outText.push_back('\t');
                    }
                }
                if (!outText.empty() && outText.back() != '\n') {
                    outText.push_back('\n');
                }
            }
        } else {
            extractDocxNode(child, outText, maxBytes);
        }
    }
}

// Extract PPTX slide texts using ordered relationships
void extractPptxText(mz_zip_archive& zip,
                     std::string& outText,
                     uint64_t maxBytes,
                     uint64_t& cumulativeBytes) {
    // 1. Read ppt/_rels/presentation.xml.rels
    std::unordered_map<std::string, std::string> relIdToTarget;
    int relIdx = findZipEntryIndex(zip, "ppt/_rels/presentation.xml.rels");
    if (relIdx >= 0) {
        std::string relXml;
        if (extractZipEntryToMemory(zip, static_cast<mz_uint>(relIdx), relXml, cumulativeBytes)) {
            pugi::xml_document doc;
            if (doc.load_buffer(relXml.data(), relXml.size())) {
                for (pugi::xml_node rel : doc.child("Relationships").children("Relationship")) {
                    std::string id = rel.attribute("Id").as_string();
                    std::string target = rel.attribute("Target").as_string();
                    if (!id.empty() && !target.empty()) {
                        // Normalize target relative to ppt/
                        if (target.rfind("ppt/", 0) != 0 && target.rfind("/", 0) != 0) {
                            target = "ppt/" + target;
                        }
                        relIdToTarget[id] = target;
                    }
                }
            }
        }
    }

    // 2. Read ppt/presentation.xml to get true ordered slides
    std::vector<std::string> orderedSlidePaths;
    int presIdx = findZipEntryIndex(zip, "ppt/presentation.xml");
    if (presIdx >= 0) {
        std::string presXml;
        if (extractZipEntryToMemory(zip, static_cast<mz_uint>(presIdx), presXml, cumulativeBytes)) {
            pugi::xml_document doc;
            if (doc.load_buffer(presXml.data(), presXml.size())) {
                pugi::xml_node sldIdLst = doc.select_node("//p:sldIdLst").node();
                for (pugi::xml_node sldId : sldIdLst.children("p:sldId")) {
                    std::string rId = sldId.attribute("r:id").as_string();
                    auto it = relIdToTarget.find(rId);
                    if (it != relIdToTarget.end()) {
                        orderedSlidePaths.push_back(it->second);
                    }
                }
            }
        }
    }

    // Fallback: If presentation manifest missing, collect and numerically sort slide*.xml
    if (orderedSlidePaths.empty()) {
        mz_uint numFiles = mz_zip_reader_get_num_files(&zip);
        std::vector<std::pair<int, std::string>> slideEntries;
        for (mz_uint i = 0; i < numFiles; ++i) {
            mz_zip_archive_file_stat stat;
            if (mz_zip_reader_file_stat(&zip, i, &stat)) {
                std::string name = stat.m_filename;
                if (name.rfind("ppt/slides/slide", 0) == 0 && name.find(".xml") != std::string::npos) {
                    // Extract numeric index
                    std::size_t numStart = 16; // "ppt/slides/slide"
                    std::size_t dot = name.find(".xml", numStart);
                    int sNum = 0;
                    try {
                        sNum = std::stoi(name.substr(numStart, dot - numStart));
                    } catch (...) {}
                    slideEntries.push_back({sNum, name});
                }
            }
        }
        std::sort(slideEntries.begin(), slideEntries.end());
        for (const auto& item : slideEntries) {
            orderedSlidePaths.push_back(item.second);
        }
    }

    // 3. Extract text from each slide in guaranteed order
    for (const auto& slidePath : orderedSlidePaths) {
        if (outText.size() >= maxBytes) break;

        int slideIdx = findZipEntryIndex(zip, slidePath);
        if (slideIdx < 0) continue;

        std::string slideXml;
        if (!extractZipEntryToMemory(zip, static_cast<mz_uint>(slideIdx), slideXml, cumulativeBytes)) continue;

        pugi::xml_document doc;
        if (!doc.load_buffer(slideXml.data(), slideXml.size())) continue;

        for (pugi::xpath_node xpNode : doc.select_nodes("//a:p")) {
            pugi::xml_node node = xpNode.node();
            std::string paraText;
            for (pugi::xpath_node xpTNode : node.select_nodes(".//a:t")) {
                paraText += xpTNode.node().text().get();
            }
            if (!paraText.empty()) {
                if (!outText.empty() && outText.back() != '\n') {
                    outText.push_back('\n');
                }
                outText += paraText;
            }
        }
    }
}

// Extract XLSX shared strings and worksheets
void extractXlsxText(mz_zip_archive& zip,
                     std::string& outText,
                     uint64_t maxBytes,
                     uint64_t& cumulativeBytes) {
    // 1. Extract shared strings table: xl/sharedStrings.xml
    std::vector<std::string> sharedStrings;
    int ssIdx = findZipEntryIndex(zip, "xl/sharedStrings.xml");
    if (ssIdx >= 0) {
        std::string ssXml;
        if (extractZipEntryToMemory(zip, static_cast<mz_uint>(ssIdx), ssXml, cumulativeBytes)) {
            pugi::xml_document doc;
            if (doc.load_buffer(ssXml.data(), ssXml.size())) {
                for (pugi::xpath_node xpSi : doc.select_nodes("//si")) {
                    pugi::xml_node si = xpSi.node();
                    std::string itemText;
                    pugi::xml_node tDirect = si.child("t");
                    if (tDirect) {
                        itemText = tDirect.text().get();
                    } else {
                        for (pugi::xml_node rNode : si.children("r")) {
                            itemText += rNode.child("t").text().get();
                        }
                    }
                    sharedStrings.push_back(std::move(itemText));
                }
            }
        }
    }

    // 2. Locate worksheet entries
    mz_uint numFiles = mz_zip_reader_get_num_files(&zip);
    std::vector<std::string> sheetPaths;
    for (mz_uint i = 0; i < numFiles; ++i) {
        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_file_stat(&zip, i, &stat)) {
            std::string name = stat.m_filename;
            if (name.rfind("xl/worksheets/sheet", 0) == 0 && name.find(".xml") != std::string::npos) {
                sheetPaths.push_back(name);
            }
        }
    }
    std::sort(sheetPaths.begin(), sheetPaths.end());

    // 3. Extract cell values from worksheets
    for (const auto& sheetPath : sheetPaths) {
        if (outText.size() >= maxBytes) break;

        int sheetIdx = findZipEntryIndex(zip, sheetPath);
        if (sheetIdx < 0) continue;

        std::string sheetXml;
        if (!extractZipEntryToMemory(zip, static_cast<mz_uint>(sheetIdx), sheetXml, cumulativeBytes)) continue;

        pugi::xml_document doc;
        if (!doc.load_buffer(sheetXml.data(), sheetXml.size())) continue;

        for (pugi::xpath_node xpRow : doc.select_nodes("//row")) {
            pugi::xml_node row = xpRow.node();
            std::string rowText;
            for (pugi::xml_node c : row.children("c")) {
                std::string type = c.attribute("t").as_string();
                std::string cellVal;

                if (type == "s") {
                    // Shared string index
                    std::string idxStr = c.child("v").text().get();
                    try {
                        std::size_t idx = static_cast<std::size_t>(std::stoul(idxStr));
                        if (idx < sharedStrings.size()) {
                            cellVal = sharedStrings[idx];
                        }
                    } catch (...) {}
                } else if (type == "inlineStr") {
                    cellVal = c.select_node(".//t").node().text().get();
                } else if (type == "b") {
                    std::string bVal = c.child("v").text().get();
                    cellVal = (bVal == "1" ? "TRUE" : "FALSE");
                } else {
                    // Numeric value or formula value
                    cellVal = c.child("v").text().get();
                }

                if (!cellVal.empty()) {
                    if (!rowText.empty()) rowText.push_back('\t');
                    rowText += cellVal;
                }
            }

            if (!rowText.empty()) {
                if (!outText.empty() && outText.back() != '\n') {
                    outText.push_back('\n');
                }
                outText += rowText;
            }
        }
    }

    // Fallback: If no worksheet cells were found, output shared strings table directly
    if (outText.empty() && !sharedStrings.empty()) {
        for (const auto& s : sharedStrings) {
            if (!s.empty()) {
                if (!outText.empty() && outText.back() != '\n') {
                    outText.push_back('\n');
                }
                outText += s;
            }
        }
    }
}

} // anonymous namespace

ExtractionResult OfficeExtractor::extract(const std::string& filePath,
                                         uint64_t maxBytes,
                                         const std::atomic<bool>* cancelToken) {
    auto tStart = std::chrono::steady_clock::now();

    ExtractionResult result;
    result.sourcePath = filePath;
    result.extractorId = EXTRACTOR_ID;
    result.extractorVersion = "1.1.0";

    std::error_code ec;
    auto fsPath = std::filesystem::u8path(filePath);
    if (!std::filesystem::exists(fsPath, ec) || ec) {
        result.status = ExtractionStatus::Failed;
        result.errorDescription = "File not found";
        return result;
    }

    uint64_t fileSize = std::filesystem::file_size(fsPath, ec);
    if (ec) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Cannot access file size";
        return result;
    }
    result.originalFileSize = fileSize;

    if (fileSize < 22) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "File too small to be a valid Office OOXML package";
        return result;
    }

    // Check for OLE encrypted container (D0 CF 11 E0 A1 B1 1A E1)
    {
        std::ifstream f(fsPath, std::ios::binary);
        if (!f.is_open()) {
            result.status = ExtractionStatus::PermissionDenied;
            result.errorDescription = "Failed to open file for read access";
            return result;
        }
        uint8_t magic[8];
        f.read(reinterpret_cast<char*>(magic), 8);
        if (f.gcount() == 8 && magic[0] == 0xD0 && magic[1] == 0xCF && magic[2] == 0x11 && magic[3] == 0xE0) {
            result.status = ExtractionStatus::Encrypted;
            result.errorDescription = "Office document is password-protected or encrypted (OLE container)";
            return result;
        }
    }

    // Open ZIP archive using miniz reader (streams central directory without loading entire file)
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, filePath.c_str(), 0)) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "Failed to parse ZIP central directory: " + std::string(mz_zip_get_error_string(zip.m_last_error));
        return result;
    }

    // Entry count safety limit
    mz_uint numFiles = mz_zip_reader_get_num_files(&zip);
    if (numFiles > 10000) {
        mz_zip_reader_end(&zip);
        result.status = ExtractionStatus::FileTooLarge;
        result.errorDescription = "ZIP package exceeds maximum entry count limit (10,000 entries)";
        return result;
    }

    // Check for encrypted entries
    for (mz_uint i = 0; i < numFiles; ++i) {
        if (mz_zip_reader_is_file_encrypted(&zip, i)) {
            mz_zip_reader_end(&zip);
            result.status = ExtractionStatus::Encrypted;
            result.errorDescription = "ZIP package contains encrypted or password-protected entries";
            return result;
        }
    }

    std::string ext = fsPath.extension().u8string();
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::string accumulatedText;
    uint64_t cumulativeDecompressed = 0;

    if (ext == "docx") {
        // Extract word/document.xml
        int docIdx = findZipEntryIndex(zip, "word/document.xml");
        if (docIdx >= 0) {
            std::string docXml;
            if (extractZipEntryToMemory(zip, static_cast<mz_uint>(docIdx), docXml, cumulativeDecompressed)) {
                pugi::xml_document doc;
                if (doc.load_buffer(docXml.data(), docXml.size())) {
                    extractDocxNode(doc.child("w:document").child("w:body"), accumulatedText, maxBytes);
                }
            } else {
                mz_zip_reader_end(&zip);
                result.status = ExtractionStatus::FileTooLarge;
                result.errorDescription = "Document XML exceeds safe expansion limit (Zip bomb protection)";
                return result;
            }
        }
    } else if (ext == "pptx") {
        extractPptxText(zip, accumulatedText, maxBytes, cumulativeDecompressed);
    } else if (ext == "xlsx") {
        extractXlsxText(zip, accumulatedText, maxBytes, cumulativeDecompressed);
    }

    mz_zip_reader_end(&zip);

    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        result.status = ExtractionStatus::Cancelled;
        result.errorDescription = "Extraction cancelled by user";
        return result;
    }

    if (accumulatedText.empty()) {
        result.status = ExtractionStatus::MetadataOnly;
        result.errorDescription = "No extractable text XML streams found in Office container";
    } else {
        bool truncated = (accumulatedText.size() > maxBytes);
        if (truncated) {
            accumulatedText.resize(static_cast<std::size_t>(maxBytes));
        }
        result.extractedText = std::move(accumulatedText);
        result.extractedByteCount = result.extractedText.size();
        result.isTruncated = truncated;
        result.status = truncated ? ExtractionStatus::Partial : ExtractionStatus::Success;
    }

    auto tEnd = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    return result;
}

} // namespace sse::extraction
