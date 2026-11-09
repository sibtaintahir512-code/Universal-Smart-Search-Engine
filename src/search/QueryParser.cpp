#include "search/QueryParser.hpp"

namespace sse::search {

void QueryParser::setError(std::string msg, std::size_t offset) {
    if (m_errorMessage.empty()) {
        m_errorMessage = std::move(msg);
        m_errorOffset = offset;
    }
}

bool QueryParser::canStartUnary(QueryTokenType type) noexcept {
    return type == QueryTokenType::Term ||
           type == QueryTokenType::Not ||
           type == QueryTokenType::LeftParen;
}

bool QueryParser::isAtEnd() const noexcept {
    return m_current >= m_tokens->size() ||
           (*m_tokens)[m_current].type == QueryTokenType::EndOfInput;
}

const QueryToken& QueryParser::peek() const noexcept {
    if (m_current < m_tokens->size()) {
        return (*m_tokens)[m_current];
    }
    return (*m_tokens)[m_tokens->size() - 1];
}

const QueryToken& QueryParser::previous() const noexcept {
    return (*m_tokens)[m_current - 1];
}

bool QueryParser::check(QueryTokenType type) const noexcept {
    if (isAtEnd()) {
        return type == QueryTokenType::EndOfInput;
    }
    return peek().type == type;
}

bool QueryParser::match(QueryTokenType type) noexcept {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

const QueryToken& QueryParser::advance() noexcept {
    if (!isAtEnd()) {
        ++m_current;
    }
    return previous();
}

ParseResult QueryParser::parse(const dsa::CustomDynamicArray<QueryToken>& tokens) {
    ParseResult result;
    m_tokens = &tokens;
    m_current = 0;
    m_errorMessage.clear();
    m_errorOffset = 0;

    if (tokens.empty() || (tokens.size() == 1 && tokens[0].type == QueryTokenType::EndOfInput)) {
        result.success = false;
        result.errorMessage = "Empty query: no search terms provided";
        result.errorOffset = 0;
        return result;
    }

    auto root = parseExpression();
    if (!root) {
        result.success = false;
        result.errorMessage = m_errorMessage.empty() ? "Query syntax error" : m_errorMessage;
        result.errorOffset = m_errorOffset;
        return result;
    }

    if (!isAtEnd()) {
        result.success = false;
        result.errorMessage = "Unexpected trailing input after query expression: '" +
                              peek().raw + "' at offset " + std::to_string(peek().charOffset);
        result.errorOffset = peek().charOffset;
        return result;
    }

    // Safety Bounds Verification
    if (root->depth() > m_config.maxAstDepth) {
        result.success = false;
        result.errorMessage = "Query expression exceeds maximum AST depth of " +
                              std::to_string(m_config.maxAstDepth);
        result.errorOffset = 0;
        return result;
    }

    if (root->nodeCount() > m_config.maxAstNodes) {
        result.success = false;
        result.errorMessage = "Query expression exceeds maximum AST node limit of " +
                              std::to_string(m_config.maxAstNodes);
        result.errorOffset = 0;
        return result;
    }

    result.success = true;
    result.root = std::move(root);
    return result;
}

std::unique_ptr<QueryNode> QueryParser::parseExpression() {
    return parseOrExpression();
}

std::unique_ptr<QueryNode> QueryParser::parseOrExpression() {
    auto left = parseAndExpression();
    if (!left) return nullptr;

    while (match(QueryTokenType::Or)) {
        std::size_t opOffset = previous().charOffset;

        if (check(QueryTokenType::Or) || check(QueryTokenType::And)) {
            setError("Consecutive operators: unexpected '" + peek().value + "' after 'OR'", peek().charOffset);
            return nullptr;
        }

        if (isAtEnd() || check(QueryTokenType::RightParen)) {
            setError("Missing right operand for 'OR' operator", opOffset);
            return nullptr;
        }

        auto right = parseAndExpression();
        if (!right) {
            if (m_errorMessage.empty()) {
                setError("Missing right operand for 'OR' operator", opOffset);
            }
            return nullptr;
        }

        left = std::make_unique<OrNode>(std::move(left), std::move(right));
    }

    return left;
}

std::unique_ptr<QueryNode> QueryParser::parseAndExpression() {
    auto left = parseUnaryExpression();
    if (!left) return nullptr;

    while (true) {
        if (match(QueryTokenType::And)) {
            std::size_t opOffset = previous().charOffset;

            if (check(QueryTokenType::Or) || check(QueryTokenType::And)) {
                setError("Consecutive operators: unexpected '" + peek().value + "' after 'AND'", peek().charOffset);
                return nullptr;
            }

            if (isAtEnd() || check(QueryTokenType::RightParen)) {
                setError("Missing right operand for 'AND' operator", opOffset);
                return nullptr;
            }

            auto right = parseUnaryExpression();
            if (!right) {
                if (m_errorMessage.empty()) {
                    setError("Missing right operand for 'AND' operator", opOffset);
                }
                return nullptr;
            }

            left = std::make_unique<AndNode>(std::move(left), std::move(right), false);
        } else if (canStartUnary(peek().type)) {
            // Implicit AND between adjacent terms/expressions
            auto right = parseUnaryExpression();
            if (!right) return nullptr;

            left = std::make_unique<AndNode>(std::move(left), std::move(right), true);
        } else {
            break;
        }
    }

    return left;
}

std::unique_ptr<QueryNode> QueryParser::parseUnaryExpression() {
    if (match(QueryTokenType::Not)) {
        std::size_t opOffset = previous().charOffset;

        if (check(QueryTokenType::Or) || check(QueryTokenType::And)) {
            setError("Invalid operator placement: unexpected '" + peek().value + "' after 'NOT'", peek().charOffset);
            return nullptr;
        }

        if (isAtEnd() || check(QueryTokenType::RightParen)) {
            setError("Missing operand for 'NOT' operator", opOffset);
            return nullptr;
        }

        auto operand = parseUnaryExpression();
        if (!operand) return nullptr;

        return std::make_unique<NotNode>(std::move(operand));
    }

    return parsePrimary();
}

std::unique_ptr<QueryNode> QueryParser::parsePrimary() {
    if (match(QueryTokenType::Term)) {
        const QueryToken& t = previous();
        return std::make_unique<TermNode>(t.value, t.field, t.charOffset);
    }

    if (match(QueryTokenType::LeftParen)) {
        std::size_t openOffset = previous().charOffset;

        if (check(QueryTokenType::RightParen)) {
            setError("Empty parentheses '()' are not permitted", openOffset);
            return nullptr;
        }

        auto expr = parseExpression();
        if (!expr) return nullptr;

        if (!match(QueryTokenType::RightParen)) {
            setError("Mismatched parentheses: missing closing ')' for opening '(' at offset " +
                     std::to_string(openOffset), peek().charOffset);
            return nullptr;
        }

        return expr;
    }

    if (check(QueryTokenType::RightParen)) {
        setError("Unexpected closing parenthesis ')' at offset " + std::to_string(peek().charOffset),
                 peek().charOffset);
        return nullptr;
    }

    if (check(QueryTokenType::And) || check(QueryTokenType::Or)) {
        setError("Unexpected leading binary operator '" + peek().value + "' at offset " +
                 std::to_string(peek().charOffset), peek().charOffset);
        return nullptr;
    }

    if (isAtEnd()) {
        setError("Unexpected end of query expression", peek().charOffset);
        return nullptr;
    }

    setError("Unexpected token '" + peek().raw + "' at offset " + std::to_string(peek().charOffset),
             peek().charOffset);
    return nullptr;
}

} // namespace sse::search
