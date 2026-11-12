#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCompleter>
#include <QStringListModel>
#include "gui/adapters/SearchEngineAdapter.hpp"

namespace sse::gui {

/**
 * @brief Search bar widget featuring live AST syntax validation and Trie-driven autocomplete.
 */
class SearchBar : public QWidget {
    Q_OBJECT

public:
    explicit SearchBar(SearchEngineAdapter* adapter, QWidget* parent = nullptr);
    ~SearchBar() override = default;

    QString currentQuery() const;
    void setQuery(const QString& query);
    void setFocusToInput();

signals:
    void searchRequested(const QString& query);
    void queryChanged(const QString& query);

private slots:
    void onTextChanged(const QString& text);
    void onSearchClicked();
    void onCompleterActivated(const QString& text);

private:
    SearchEngineAdapter* m_adapter{nullptr};
    QLineEdit*           m_inputEdit{nullptr};
    QPushButton*         m_searchBtn{nullptr};
    QLabel*              m_statusBadge{nullptr};
    QCompleter*          m_completer{nullptr};
    QStringListModel*    m_completerModel{nullptr};

    void setupUi();
    void validateSyntax(const QString& query);
};

} // namespace sse::gui
