#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include "search/QueryToken.hpp"

namespace sse::search {

/**
 * @brief Node type identifier for Abstract Syntax Tree (AST) nodes.
 */
enum class QueryNodeType : uint8_t {
    Term = 0,
    And,
    Or,
    Not
};

/**
 * @brief Abstract base class for all Query AST nodes.
 *
 * Utilizes strict RAII and std::unique_ptr for recursive child ownership,
 * guaranteeing exception safety, zero leaks, and no double deletion.
 */
class QueryNode {
public:
    virtual ~QueryNode() = default;

    virtual QueryNodeType type() const noexcept = 0;
    virtual std::size_t depth() const noexcept = 0;
    virtual std::size_t nodeCount() const noexcept = 0;
    virtual std::string toString() const = 0;
};

/**
 * @brief Leaf AST node representing a single search term with optional field restriction.
 */
class TermNode : public QueryNode {
public:
    std::string    term;
    FieldQualifier field;
    std::size_t    charOffset;

    TermNode(std::string t, FieldQualifier f, std::size_t offset)
        : term(std::move(t)), field(f), charOffset(offset) {}

    QueryNodeType type() const noexcept override { return QueryNodeType::Term; }
    std::size_t depth() const noexcept override { return 1; }
    std::size_t nodeCount() const noexcept override { return 1; }

    std::string toString() const override {
        std::string prefix;
        if (field == FieldQualifier::Filename) prefix = "name:";
        else if (field == FieldQualifier::Body) prefix = "body:";
        return prefix + term;
    }
};

/**
 * @brief Binary AST node representing a logical AND conjunction.
 */
class AndNode : public QueryNode {
public:
    std::unique_ptr<QueryNode> left;
    std::unique_ptr<QueryNode> right;
    bool                       isImplicit{false};

    AndNode(std::unique_ptr<QueryNode> l, std::unique_ptr<QueryNode> r, bool implicit = false)
        : left(std::move(l)), right(std::move(r)), isImplicit(implicit) {}

    QueryNodeType type() const noexcept override { return QueryNodeType::And; }

    std::size_t depth() const noexcept override {
        std::size_t ld = left ? left->depth() : 0;
        std::size_t rd = right ? right->depth() : 0;
        return 1 + std::max(ld, rd);
    }

    std::size_t nodeCount() const noexcept override {
        std::size_t lc = left ? left->nodeCount() : 0;
        std::size_t rc = right ? right->nodeCount() : 0;
        return 1 + lc + rc;
    }

    std::string toString() const override {
        std::string lStr = left ? left->toString() : "<null>";
        std::string rStr = right ? right->toString() : "<null>";
        return "(" + lStr + (isImplicit ? " [AND] " : " AND ") + rStr + ")";
    }
};

/**
 * @brief Binary AST node representing a logical OR disjunction.
 */
class OrNode : public QueryNode {
public:
    std::unique_ptr<QueryNode> left;
    std::unique_ptr<QueryNode> right;

    OrNode(std::unique_ptr<QueryNode> l, std::unique_ptr<QueryNode> r)
        : left(std::move(l)), right(std::move(r)) {}

    QueryNodeType type() const noexcept override { return QueryNodeType::Or; }

    std::size_t depth() const noexcept override {
        std::size_t ld = left ? left->depth() : 0;
        std::size_t rd = right ? right->depth() : 0;
        return 1 + std::max(ld, rd);
    }

    std::size_t nodeCount() const noexcept override {
        std::size_t lc = left ? left->nodeCount() : 0;
        std::size_t rc = right ? right->nodeCount() : 0;
        return 1 + lc + rc;
    }

    std::string toString() const override {
        std::string lStr = left ? left->toString() : "<null>";
        std::string rStr = right ? right->toString() : "<null>";
        return "(" + lStr + " OR " + rStr + ")";
    }
};

/**
 * @brief Unary AST node representing a logical NOT negation.
 */
class NotNode : public QueryNode {
public:
    std::unique_ptr<QueryNode> operand;

    explicit NotNode(std::unique_ptr<QueryNode> op)
        : operand(std::move(op)) {}

    QueryNodeType type() const noexcept override { return QueryNodeType::Not; }

    std::size_t depth() const noexcept override {
        return 1 + (operand ? operand->depth() : 0);
    }

    std::size_t nodeCount() const noexcept override {
        return 1 + (operand ? operand->nodeCount() : 0);
    }

    std::string toString() const override {
        return "NOT (" + (operand ? operand->toString() : "<null>") + ")";
    }
};

} // namespace sse::search
