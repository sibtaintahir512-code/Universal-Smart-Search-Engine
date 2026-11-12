#pragma once

#include <QStyledItemDelegate>
#include <QPainter>

namespace sse::gui {

/**
 * @brief Custom item delegate rendering category badges, score pills, and typography.
 */
class SearchResultDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit SearchResultDelegate(QObject* parent = nullptr);
    ~SearchResultDelegate() override = default;

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    void paintCategoryBadge(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void paintScorePill(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

} // namespace sse::gui
