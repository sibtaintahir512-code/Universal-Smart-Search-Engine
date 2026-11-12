#include "gui/widgets/SearchResultDelegate.hpp"
#include "gui/models/SearchResultModel.hpp"
#include <QApplication>

namespace sse::gui {

SearchResultDelegate::SearchResultDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void SearchResultDelegate::paint(QPainter* painter,
                                 const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
    if (!index.isValid()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    if (index.column() == SearchResultModel::ColCategory) {
        paintCategoryBadge(painter, option, index);
    } else if (index.column() == SearchResultModel::ColScore) {
        paintScorePill(painter, option, index);
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

QSize SearchResultDelegate::sizeHint(const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(std::max(size.height(), 32));
    return size;
}

void SearchResultDelegate::paintCategoryBadge(QPainter* painter,
                                              const QStyleOptionViewItem& option,
                                              const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Draw row background / selection
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    } else {
        painter->fillRect(option.rect, option.palette.base());
    }

    int catVal = index.data(SearchResultModel::CategoryRole).toInt();
    QString text = index.data(Qt::DisplayRole).toString();

    QColor badgeBg;
    QColor badgeFg = Qt::white;

    switch (static_cast<FileCategory>(catVal)) {
        case FileCategory::Documents:
            badgeBg = QColor(37, 99, 235); // Blue
            break;
        case FileCategory::SourceCode:
            badgeBg = QColor(124, 58, 237); // Purple
            break;
        case FileCategory::Images:
            badgeBg = QColor(13, 148, 136); // Teal
            break;
        case FileCategory::Audio:
        case FileCategory::Videos:
            badgeBg = QColor(217, 119, 6); // Amber
            break;
        case FileCategory::Archives:
            badgeBg = QColor(225, 29, 72); // Rose
            break;
        case FileCategory::Executables:
        case FileCategory::Other:
        default:
            badgeBg = QColor(107, 114, 128); // Slate
            break;
    }

    QRect badgeRect = option.rect.adjusted(6, 6, -6, -6);
    painter->setBrush(badgeBg);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(badgeRect, 6, 6);

    painter->setPen(badgeFg);
    QFont font = painter->font();
    font.setPointSize(font.pointSize() > 2 ? font.pointSize() - 1 : 8);
    font.setBold(true);
    painter->setFont(font);
    painter->drawText(badgeRect, Qt::AlignCenter, text);

    painter->restore();
}

void SearchResultDelegate::paintScorePill(QPainter* painter,
                                          const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    } else {
        painter->fillRect(option.rect, option.palette.base());
    }

    double rawScore = index.data(SearchResultModel::RawScoreRole).toDouble();
    QString text = index.data(Qt::DisplayRole).toString();

    QColor pillBg;
    QColor pillFg;

    if (rawScore > 0.0) {
        pillBg = QColor(79, 70, 229, 45); // Indigo translucent
        pillFg = QColor(99, 102, 241);
    } else {
        pillBg = QColor(100, 116, 139, 30);
        pillFg = QColor(148, 163, 184);
    }

    QRect pillRect = option.rect.adjusted(8, 6, -8, -6);
    painter->setBrush(pillBg);
    painter->setPen(QPen(pillFg.lighter(120), 1));
    painter->drawRoundedRect(pillRect, 5, 5);

    painter->setPen(pillFg);
    QFont font = painter->font();
    font.setBold(true);
    painter->setFont(font);
    painter->drawText(pillRect, Qt::AlignCenter, text);

    painter->restore();
}

} // namespace sse::gui
