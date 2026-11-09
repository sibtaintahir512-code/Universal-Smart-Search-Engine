#include "ranking/BM25Scorer.hpp"
#include "ranking/TFIDFScorer.hpp"

namespace sse::ranking {

double BM25Scorer::computeTermScore(double tf,
                                    double idf,
                                    double docLength,
                                    double avgDocLength,
                                    double k1,
                                    double b) noexcept {
    if (tf <= 0.0 || idf <= 0.0) {
        return 0.0;
    }

    double lengthNorm = 1.0;
    if (avgDocLength > 0.0) {
        lengthNorm = 1.0 - b + b * (docLength / avgDocLength);
    }

    double denominator = tf + k1 * lengthNorm;
    if (denominator <= 1e-9) {
        return 0.0;
    }

    double numerator = tf * (k1 + 1.0);
    double score = idf * (numerator / denominator);
    return (std::isfinite(score) && score > 0.0) ? score : 0.0;
}

double BM25Scorer::scoreDocument(DocID docId,
                                const dsa::CustomDynamicArray<ScoringTerm>& queryTerms,
                                const indexing::InvertedIndex& index,
                                const DocumentLengthStatistics& lengthStats,
                                std::size_t totalDocs,
                                const RankingConfig& config,
                                ScoreExplanation* outExplanation) {
    if (docId == INVALID_DOC_ID || totalDocs == 0 || queryTerms.empty()) {
        return 0.0;
    }

    double totalScore = 0.0;
    double filenameContrib = 0.0;
    double bodyContrib = 0.0;

    double nameLen = static_cast<double>(lengthStats.getNameLength(docId));
    double bodyLen = static_cast<double>(lengthStats.getBodyLength(docId));
    double avgNameLen = lengthStats.averageNameLength();
    double avgBodyLen = lengthStats.averageBodyLength();

    if (outExplanation) {
        outExplanation->docId = docId;
        outExplanation->mode = RankingMode::BM25;
        outExplanation->filenameWeight = config.filenameWeight;
        outExplanation->bodyWeight = config.bodyWeight;
        outExplanation->documentNameLength = static_cast<uint32_t>(nameLen);
        outExplanation->documentBodyLength = static_cast<uint32_t>(bodyLen);
        outExplanation->averageNameLength = avgNameLen;
        outExplanation->averageBodyLength = avgBodyLen;
        outExplanation->termDetails.clear();
    }

    for (std::size_t i = 0; i < queryTerms.size(); ++i) {
        const ScoringTerm& qTerm = queryTerms[i];
        const dsa::CustomPostingList* postList = index.getPostingList(qTerm.term);
        if (!postList) continue;

        const PostingNode* node = postList->findNode(docId);
        if (!node) continue;

        uint32_t df = postList->documentFrequency();
        double idf = TFIDFScorer::computeBM25SmoothedIdf(totalDocs, df);
        if (idf <= 0.0) continue;

        uint16_t nameTf = node->nameTermFreq;
        uint16_t bodyTf = node->bodyTermFreq;

        double termNameScore = 0.0;
        double termBodyScore = 0.0;

        if (qTerm.field == search::FieldQualifier::Filename) {
            double rawBm25 = computeTermScore(nameTf, idf, nameLen, avgNameLen, config.bm25K1, config.bm25B);
            termNameScore = config.filenameWeight * rawBm25 * qTerm.weight;
        } else if (qTerm.field == search::FieldQualifier::Body) {
            double rawBm25 = computeTermScore(bodyTf, idf, bodyLen, avgBodyLen, config.bm25K1, config.bm25B);
            termBodyScore = config.bodyWeight * rawBm25 * qTerm.weight;
        } else {
            // Unspecified: score both fields independently
            double rawNameBm25 = computeTermScore(nameTf, idf, nameLen, avgNameLen, config.bm25K1, config.bm25B);
            double rawBodyBm25 = computeTermScore(bodyTf, idf, bodyLen, avgBodyLen, config.bm25K1, config.bm25B);
            termNameScore = config.filenameWeight * rawNameBm25 * qTerm.weight;
            termBodyScore = config.bodyWeight * rawBodyBm25 * qTerm.weight;
        }

        double termTotal = termNameScore + termBodyScore;
        totalScore += termTotal;
        filenameContrib += termNameScore;
        bodyContrib += termBodyScore;

        if (outExplanation) {
            TermExplanation tExp;
            tExp.term = qTerm.term;
            tExp.field = qTerm.field;
            tExp.df = df;
            tExp.idf = idf;
            tExp.nameTf = nameTf;
            tExp.bodyTf = bodyTf;
            tExp.nameScore = termNameScore;
            tExp.bodyScore = termBodyScore;
            tExp.totalTermScore = termTotal;
            outExplanation->termDetails.push_back(std::move(tExp));
        }
    }

    if (!std::isfinite(totalScore) || totalScore < 0.0) {
        totalScore = 0.0;
    }

    if (outExplanation) {
        outExplanation->finalScore = totalScore;
        outExplanation->filenameContribution = filenameContrib;
        outExplanation->bodyContribution = bodyContrib;
    }

    return totalScore;
}

} // namespace sse::ranking
