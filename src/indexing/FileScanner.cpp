#include "indexing/FileScanner.hpp"
#include <algorithm>
#include <iostream>

namespace sse::indexing {

namespace {
    struct DirQueueItem {
        std::string path;
        uint32_t    depth{0};
    };
}

FileScanner::FileScanner(ScannerConfig config) : m_config(config) {}

void FileScanner::setRoots(const std::vector<std::string>& rootPaths) {
    m_rootPaths = deduplicateRoots(rootPaths);
}

void FileScanner::addRoot(const std::string& rootPath) {
    m_rootPaths.push_back(rootPath);
    m_rootPaths = deduplicateRoots(m_rootPaths);
}

void FileScanner::clearRoots() noexcept {
    m_rootPaths.clear();
}

std::vector<std::string> FileScanner::deduplicateRoots(const std::vector<std::string>& roots) {
    if (roots.empty()) return {};

    std::vector<std::filesystem::path> canonPaths;
    canonPaths.reserve(roots.size());

    for (const auto& r : roots) {
        std::error_code ec;
        auto p = std::filesystem::weakly_canonical(std::filesystem::u8path(r), ec);
        if (!ec) {
            canonPaths.push_back(p);
        } else {
            canonPaths.push_back(std::filesystem::u8path(r));
        }
    }

    std::vector<std::string> uniqueRoots;
    for (std::size_t i = 0; i < canonPaths.size(); ++i) {
        bool isSub = false;
        for (std::size_t j = 0; j < canonPaths.size(); ++j) {
            if (i == j) continue;
            // Check if canonPaths[i] is child of canonPaths[j]
            auto rel = std::filesystem::relative(canonPaths[i], canonPaths[j]);
            if (!rel.empty() && rel.native()[0] != '.' && rel != canonPaths[i]) {
                isSub = true;
                break;
            }
        }
        if (!isSub) {
            std::string s = canonPaths[i].u8string();
            if (std::find(uniqueRoots.begin(), uniqueRoots.end(), s) == uniqueRoots.end()) {
                uniqueRoots.push_back(s);
            }
        }
    }
    return uniqueRoots;
}

bool FileScanner::isExcludedDirectory(const std::filesystem::path& dirPath) noexcept {
    std::string name = dirPath.filename().u8string();
    if (name.empty()) return false;

    // Windows & Unix common exclusions
    if (name == "$Recycle.Bin" || name == "System Volume Information" ||
        name == ".git" || name == ".svn" || name == "node_modules" ||
        name == ".Trash" || name == ".Trash-1000") {
        return true;
    }
    return false;
}

ScanStatistics FileScanner::scan(DocumentCatalog& catalog,
                                FileCallback onFile,
                                ProgressCallback onProgress,
                                std::atomic<bool>& cancelFlag) {
    m_stats.start();

    dsa::CustomQueue<DirQueueItem> dirQueue;
    dsa::CustomHashTable<std::string, bool> visitedDirs;
    dsa::CustomDynamicArray<std::string> discoveredCanonPaths;

    // 1. Seed root directories
    for (const auto& rootStr : m_rootPaths) {
        std::error_code ec;
        auto rootPath = std::filesystem::u8path(rootStr);
        if (std::filesystem::exists(rootPath, ec) && std::filesystem::is_directory(rootPath, ec)) {
            std::string canon = std::filesystem::weakly_canonical(rootPath, ec).u8string();
            if (!visitedDirs.contains(canon)) {
                visitedDirs.insert(canon, true);
                dirQueue.enqueue(DirQueueItem{canon, 0});
                ++m_stats.directoriesDiscovered;
            }
        } else {
            ++m_stats.inaccessibleDirectories;
        }
    }

    uint64_t itemsSinceProgress = 0;

    // 2. Iterative BFS Traversal Loop
    while (!dirQueue.empty() && !cancelFlag.load(std::memory_order_relaxed)) {
        DirQueueItem currItem = dirQueue.popFront();
        ++m_stats.directoriesScanned;

        std::error_code dirEc;
        auto dirPath = std::filesystem::u8path(currItem.path);

        // Directory iterator with non-throwing error handling
        std::filesystem::directory_iterator dirIter(
            dirPath,
            std::filesystem::directory_options::skip_permission_denied,
            dirEc
        );

        if (dirEc) {
            ++m_stats.inaccessibleDirectories;
            continue;
        }

        for (const auto& entry : dirIter) {
            if (cancelFlag.load(std::memory_order_relaxed)) {
                break;
            }

            std::error_code entryEc;
            auto symStatus = entry.symlink_status(entryEc);
            if (entryEc) {
                ++m_stats.metadataErrors;
                continue;
            }

            bool isSymlink = std::filesystem::is_symlink(symStatus);
            if (isSymlink && !m_config.followSymlinks) {
                ++m_stats.skippedEntries;
                continue;
            }

            auto status = entry.status(entryEc);
            if (entryEc) {
                ++m_stats.metadataErrors;
                continue;
            }

            if (std::filesystem::is_directory(status)) {
                if (m_config.detectExclusions && isExcludedDirectory(entry.path())) {
                    ++m_stats.skippedEntries;
                    continue;
                }
                if (currItem.depth + 1 > m_config.maxDepth) {
                    ++m_stats.skippedEntries;
                    continue;
                }

                std::string canonSub = std::filesystem::weakly_canonical(entry.path(), entryEc).u8string();
                if (entryEc) {
                    canonSub = entry.path().u8string();
                }

                // Cycle prevention: only enqueue if not previously visited
                if (!visitedDirs.contains(canonSub)) {
                    visitedDirs.insert(canonSub, true);
                    dirQueue.enqueue(DirQueueItem{canonSub, currItem.depth + 1});
                    ++m_stats.directoriesDiscovered;
                } else {
                    ++m_stats.skippedEntries; // Loop cycle suppressed
                }
            } else if (std::filesystem::is_regular_file(status)) {
                ++m_stats.filesDiscovered;

                ExtendedFileRecord record;
                if (!m_metaManager.extractMetadata(entry.path(), record)) {
                    ++m_stats.metadataErrors;
                    continue;
                }

                // Change detection against catalog
                FileChangeStatus changeStatus = FileChangeDetector::detectChange(
                    catalog,
                    record.canonicalPath,
                    record.metadata.fileSize,
                    record.metadata.lastModifiedTime,
                    true
                );

                switch (changeStatus) {
                    case FileChangeStatus::New:
                        ++m_stats.newFiles;
                        break;
                    case FileChangeStatus::Modified:
                        ++m_stats.modifiedFiles;
                        break;
                    case FileChangeStatus::Unchanged:
                        ++m_stats.unchangedFiles;
                        break;
                    default:
                        break;
                }

                // Register file in DocumentCatalog
                catalog.registerDocument(record.metadata, record.canonicalPath, record.filename);
                ++m_stats.filesCataloged;
                discoveredCanonPaths.push_back(record.canonicalPath);

                if (onFile) {
                    onFile(record, changeStatus);
                }
            }

            ++itemsSinceProgress;
            if (itemsSinceProgress >= 100) {
                itemsSinceProgress = 0;
                if (onProgress) {
                    onProgress(m_stats);
                }
            }
        }
    }

    bool wasCancelled = cancelFlag.load(std::memory_order_relaxed);
    m_stats.stop(wasCancelled);

    // Reconcile missing documents ONLY on full completed scans
    if (!wasCancelled) {
        std::size_t missing = FileChangeDetector::reconcileMissingDocuments(catalog, discoveredCanonPaths);
        m_stats.missingFiles = missing;
    }

    if (onProgress) {
        onProgress(m_stats);
    }

    return m_stats;
}

} // namespace sse::indexing
