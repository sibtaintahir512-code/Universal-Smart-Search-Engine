#include "ranking/RankingEngine.hpp"
#include "ranking/TFIDFScorer.hpp"
#include "ranking/BM25Scorer.hpp"
#include "ranking/TopKSelector.hpp"
#include <algorithm>

namespace sse::ranking {

RankingEngine::RankingEngine(const indexing::InvertedIndex* index,
                             const indexing::DocumentCatalog* catalog,
                             const DocumentLengthStatistics* lengthStats,
                             RankingConfig config) noexcept
    : m_index(index), m_catalog(catalog), m_lengthStats(lengthStats), m_config(config) {}

void RankingEngine::collectPositiveTerms(const search::QueryNode* node,
                                         dsa::CustomDynamicArray<ScoringTerm>& outTerms,
                                         bool insideNot) {
    if (!node) return;

    switch (node->type()) {
        case search::QueryNodeType::Term: {
            if (!insideNot) {
                const auto* tNode = static_cast<const search::TermNode*>(node);
                ScoringTerm st(tNode->term, tNode->field);
                bool exists = false;
                for (std::size_t i = 0; i < outTerms.size(); ++i) {
                    if (outTerms[i] == st) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    outTerms.push_back(std::move(st));
                }
            }
            break;
        }

        case search::QueryNodeType::And: {
            const auto* andNode = static_cast<const search::AndNode*>(node);
            collectPositiveTerms(andNode->left.get(), outTerms, insideNot);
            collectPositiveTerms(andNode->right.get(), outTerms, insideNot);
            break;
        }

        case search::QueryNodeType::Or: {
            const auto* orNode = static_cast<const search::OrNode*>(node);
            collectPositiveTerms(orNode->left.get(), outTerms, insideNot);
            collectPositiveTerms(orNode->right.get(), outTerms, insideNot);
            break;
        }

        case search::QueryNodeType::Not: {
            const auto* notNode = static_cast<const search::NotNode*>(node);
            // Invert the negation context: terms inside NOT are excluded from positive scoring
            // Double negation (NOT NOT term) restores positive scoring context
            collectPositiveTerms(notNode->operand.get(), outTerms, !insideNot);
            break;
        }
    }
}

dsa::CustomDynamicArray<ScoringTerm> RankingEngine::extractPositiveQueryTerms(const search::QueryNode* root) {
    dsa::CustomDynamicArray<ScoringTerm> terms;
    collectPositiveTerms(root, terms, false);
    return terms;
}

dsa::CustomDynamicArray<ScoredDocument> RankingEngine::rankCandidates(
    const search::ResultSet& candidates,
    const search::QueryNode* queryAst,
    std::size_t topK) {

    dsa::CustomDynamicArray<ScoredDocument> result;
    if (candidates.empty()) {
        return result;
    }

    std::size_t effectiveK = (topK == 0 || topK > candidates.size()) ? candidates.size() : topK;

    // 1. DocIdOrder Mode: Preserve Phase 6 deterministic ascending DocID baseline
    if (m_config.mode == RankingMode::DocIdOrder) {
        result.reserve(effectiveK);
        for (std::size_t i = 0; i < effectiveK; ++i) {
            result.push_back(ScoredDocument(candidates[i], 0.0, static_cast<uint32_t>(i + 1)));
        }
        m_statistics.recordRanking(RankingMode::DocIdOrder, effectiveK, 0.0, 0.0, false);
        return result;
    }

    // 2. Validate configuration
    if (!m_config.isValid() || !m_index || !m_catalog) {
        m_statistics.recordInvalidConfiguration();
        // Fallback to DocIdOrder on invalid configuration
        result.reserve(effectiveK);
        for (std::size_t i = 0; i < effectiveK; ++i) {
            result.push_back(ScoredDocument(candidates[i], 0.0, static_cast<uint32_t>(i + 1)));
        }
        m_statistics.recordRanking(m_config.mode, effectiveK, 0.0, 0.0, true);
        return result;
    }

    // 3. Extract positive scoring terms from query AST
    dsa::CustomDynamicArray<ScoringTerm> positiveTerms = extractPositiveQueryTerms(queryAst);

    // 4. Pure negation check: if no positive terms exist (e.g. "NOT apple"), use neutral DocID ordering
    if (positiveTerms.empty()) {
        result.reserve(effectiveK);
        for (std::size_t i = 0; i < effectiveK; ++i) {
            result.push_back(ScoredDocument(candidates[i], 0.0, static_cast<uint32_t>(i + 1)));
        }
        m_statistics.recordRanking(m_config.mode, effectiveK, 0.0, 0.0, false);
        return result;
    }

    // 5. Score all matching candidates
    auto tScoringStart = std::chrono::high_resolution_clock::now();

    dsa::CustomDynamicArray<ScoredDocument> scoredCandidates;
    scoredCandidates.reserve(candidates.size());

    std::size_t totalDocs = m_catalog->activeCount();
    if (totalDocs == 0) {
        totalDocs = m_catalog->totalCount();
    }

    struct TermScoringContext {
        const ScoringTerm* term{nullptr};
        const dsa::CustomPostingList* postList{nullptr};
        const PostingNode* cursor{nullptr};
        double idf{0.0};
        uint32_t df{0};
    };

    dsa::CustomDynamicArray<TermScoringContext> termContexts;
    termContexts.reserve(positiveTerms.size());

    const bool useBM25 = (m_config.mode == RankingMode::BM25 && m_lengthStats != nullptr);
    const double avgNameLen = m_lengthStats ? m_lengthStats->averageNameLength() : 0.0;
    const double avgBodyLen = m_lengthStats ? m_lengthStats->averageBodyLength() : 0.0;

    for (std::size_t t = 0; t < positiveTerms.size(); ++t) {
        TermScoringContext ctx;
        ctx.term = &positiveTerms[t];
        ctx.postList = m_index->getPostingList(positiveTerms[t].term);
        if (ctx.postList) {
            ctx.cursor = ctx.postList->head();
            ctx.df = ctx.postList->documentFrequency();
            ctx.idf = TFIDFScorer::computeBM25SmoothedIdf(totalDocs, ctx.df);
        }
        termContexts.push_back(ctx);
    }

    for (std::size_t i = 0; i < candidates.size(); ++i) {
        DocID id = candidates[i];
        std::shared_ptr<ScoreExplanation> expl = nullptr;
        if (m_config.enableExplanation) {
            expl = std::make_shared<ScoreExplanation>();
            expl->docId = id;
            expl->mode = m_config.mode;
            expl->filenameWeight = m_config.filenameWeight;
            expl->bodyWeight = m_config.bodyWeight;
            if (useBM25 && m_lengthStats) {
                expl->documentNameLength = m_lengthStats->getNameLength(id);
                expl->documentBodyLength = m_lengthStats->getBodyLength(id);
                expl->averageNameLength = avgNameLen;
                expl->averageBodyLength = avgBodyLen;
            }
            expl->termDetails.clear();
        }

        double score = 0.0;
        double filenameContrib = 0.0;
        double bodyContrib = 0.0;

        const double nameLen = (useBM25 && m_lengthStats) ? static_cast<double>(m_lengthStats->getNameLength(id)) : 0.0;
        const double bodyLen = (useBM25 && m_lengthStats) ? static_cast<double>(m_lengthStats->getBodyLength(id)) : 0.0;

        for (std::size_t t = 0; t < termContexts.size(); ++t) {
            auto& ctx = termContexts[t];
            if (!ctx.postList || ctx.idf <= 0.0) continue;

            // Fast forward cursor progression
            while (ctx.cursor && ctx.cursor->docId < id) {
                ctx.cursor = ctx.cursor->next;
            }

            const PostingNode* node = nullptr;
            if (ctx.cursor && ctx.cursor->docId == id) {
                node = ctx.cursor;
            } else if (i > 0 && id < candidates[i - 1]) {
                // Fallback in case candidate order is non-monotonic
                node = ctx.postList->findNode(id);
            }

            if (!node) continue;

            uint16_t nameTf = node->nameTermFreq;
            uint16_t bodyTf = node->bodyTermFreq;
            double termNameScore = 0.0;
            double termBodyScore = 0.0;

            if (useBM25) {
                if (ctx.term->field == search::FieldQualifier::Filename) {
                    double raw = BM25Scorer::computeTermScore(nameTf, ctx.idf, nameLen, avgNameLen, m_config.bm25K1, m_config.bm25B);
                    termNameScore = m_config.filenameWeight * raw * ctx.term->weight;
                } else if (ctx.term->field == search::FieldQualifier::Body) {
                    double raw = BM25Scorer::computeTermScore(bodyTf, ctx.idf, bodyLen, avgBodyLen, m_config.bm25K1, m_config.bm25B);
                    termBodyScore = m_config.bodyWeight * raw * ctx.term->weight;
                } else {
                    double rawN = BM25Scorer::computeTermScore(nameTf, ctx.idf, nameLen, avgNameLen, m_config.bm25K1, m_config.bm25B);
                    double rawB = BM25Scorer::computeTermScore(bodyTf, ctx.idf, bodyLen, avgBodyLen, m_config.bm25K1, m_config.bm25B);
                    termNameScore = m_config.filenameWeight * rawN * ctx.term->weight;
                    termBodyScore = m_config.bodyWeight * rawB * ctx.term->weight;
                }
            } else {
                double sublinearBodyTf = (bodyTf > 0) ? (1.0 + std::log(static_cast<double>(bodyTf))) : 0.0;
                double nameVal = static_cast<double>(nameTf);

                if (ctx.term->field == search::FieldQualifier::Filename) {
                    termNameScore = m_config.filenameWeight * nameVal * ctx.idf * ctx.term->weight;
                } else if (ctx.term->field == search::FieldQualifier::Body) {
                    termBodyScore = m_config.bodyWeight * sublinearBodyTf * ctx.idf * ctx.term->weight;
                } else {
                    termNameScore = m_config.filenameWeight * nameVal * ctx.idf * ctx.term->weight;
                    termBodyScore = m_config.bodyWeight * sublinearBodyTf * ctx.idf * ctx.term->weight;
                }
            }

            double termTotal = termNameScore + termBodyScore;
            score += termTotal;
            filenameContrib += termNameScore;
            bodyContrib += termBodyScore;

            if (expl) {
                TermExplanation tExp;
                tExp.term = ctx.term->term;
                tExp.field = ctx.term->field;
                tExp.df = ctx.df;
                tExp.idf = ctx.idf;
                tExp.nameTf = nameTf;
                tExp.bodyTf = bodyTf;
                tExp.nameScore = termNameScore;
                tExp.bodyScore = termBodyScore;
                tExp.totalTermScore = termTotal;
                expl->termDetails.push_back(std::move(tExp));
            }
        }

        if (!std::isfinite(score) || score < 0.0) {
            score = 0.0;
        }

        if (expl) {
            expl->finalScore = score;
            expl->filenameContribution = filenameContrib;
            expl->bodyContribution = bodyContrib;
        }

        scoredCandidates.push_back(ScoredDocument(id, score, 0, std::move(expl)));
    }

    auto tScoringEnd = std::chrono::high_resolution_clock::now();
    double scoringMs = std::chrono::duration<double, std::milli>(tScoringEnd - tScoringStart).count();

    // 6. Select Top-K via Bounded CustomMinHeap
    auto tTopKStart = std::chrono::high_resolution_clock::now();
    result = TopKSelector::selectTopK(scoredCandidates, effectiveK);
    auto tTopKEnd = std::chrono::high_resolution_clock::now();
    double topKMs = std::chrono::duration<double, std::milli>(tTopKEnd - tTopKStart).count();

    // 7. Record Telemetry
    m_statistics.recordRanking(m_config.mode, result.size(), scoringMs, topKMs, false);

    return result;
}

} // namespace sse::ranking
