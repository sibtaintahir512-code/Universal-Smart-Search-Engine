#include "ranking/TFIDFScorer.hpp"

namespace sse::ranking {

double TFIDFScorer::computeBM25SmoothedIdf(std::size_t totalDocs, uint32_t docFreq) noexcept {
    if (totalDocs == 0 || docFreq == 0 || docFreq > totalDocs) {
        return 0.0;
    }

    double N = static_cast<double>(totalDocs);
    double df = static_cast<double>(docFreq);

    double numerator = N - df + 0.5;
    double denominator = df + 0.5;
    double inner = 1.0 + (numerator / denominator);

    if (inner <= 1e-9) {
        return 0.0;
    }

    double idf = std::log(inner);
    return std::max(0.0, idf);
}

double TFIDFScorer::computeConventionalIdf(std::size_t totalDocs, uint32_t docFreq) noexcept {
    if (totalDocs == 0) {
        return 0.0;
    }

    double N = static_cast<double>(totalDocs);
    double df = static_cast<double>(docFreq);
    if (df > N) {
        df = N;
    }

    double ratio = (N + 1.0) / (df + 1.0);
    double idf = std::log(ratio) + 1.0;
    return std::max(0.0, idf);
}

double TFIDFScorer::scoreDocument(DocID docId,
                                  const dsa::CustomDynamicArray<ScoringTerm>& queryTerms,
                                  const indexing::InvertedIndex& index,
                                  std::size_t totalDocs,
                                  const RankingConfig& config,
                                  ScoreExplanation* outExplanation) {
    if (docId == INVALID_DOC_ID || totalDocs == 0 || queryTerms.empty()) {
        return 0.0;
    }

    double totalScore = 0.0;
    double filenameContrib = 0.0;
    double bodyContrib = 0.0;

    if (outExplanation) {
        outExplanation->docId = docId;
        outExplanation->mode = RankingMode::TFIDF;
        outExplanation->filenameWeight = config.filenameWeight;
        outExplanation->bodyWeight = config.bodyWeight;
        outExplanation->termDetails.clear();
    }

    for (std::size_t i = 0; i < queryTerms.size(); ++i) {
        const ScoringTerm& qTerm = queryTerms[i];
        const dsa::CustomPostingList* postList = index.getPostingList(qTerm.term);
        if (!postList) continue;

        const PostingNode* node = postList->findNode(docId);
        if (!node) continue;

        uint32_t df = postList->documentFrequency();
        double idf = computeBM25SmoothedIdf(totalDocs, df);
        if (idf <= 0.0) continue;

        uint16_t nameTf = node->nameTermFreq;
        uint16_t bodyTf = node->bodyTermFreq;

        double termNameScore = 0.0;
        double termBodyScore = 0.0;
        double sublinearBodyTf = (bodyTf > 0) ? (1.0 + std::log(static_cast<double>(bodyTf))) : 0.0;

        if (qTerm.field == search::FieldQualifier::Filename) {
            termNameScore = config.filenameWeight * static_cast<double>(nameTf) * idf * qTerm.weight;
        } else if (qTerm.field == search::FieldQualifier::Body) {
            termBodyScore = config.bodyWeight * sublinearBodyTf * idf * qTerm.weight;
        } else {
            // Unspecified: score both fields
            termNameScore = config.filenameWeight * static_cast<double>(nameTf) * idf * qTerm.weight;
            termBodyScore = config.bodyWeight * sublinearBodyTf * idf * qTerm.weight;
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
