#include "gui/widgets/SearchBar.hpp"
#include <QHBoxLayout>
#include <QShortcut>
#include <QKeySequence>
#include <QRegularExpression>

namespace sse::gui {

SearchBar::SearchBar(SearchEngineAdapter* adapter, QWidget* parent)
    : QWidget(parent)
    , m_adapter(adapter)
{
    setupUi();
}

QString SearchBar::currentQuery() const {
    return m_inputEdit ? m_inputEdit->text().trimmed() : QString();
}

void SearchBar::setQuery(const QString& query) {
    if (m_inputEdit) {
        m_inputEdit->setText(query);
        validateSyntax(query);
    }
}

void SearchBar::setFocusToInput() {
    if (m_inputEdit) {
        m_inputEdit->setFocus();
        m_inputEdit->selectAll();
    }
}

void SearchBar::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_inputEdit = new QLineEdit(this);
    m_inputEdit->setPlaceholderText("Search local files (e.g. dijkstra, (tree OR graph) AND NOT binary, ext:cpp)...");
    m_inputEdit->setClearButtonEnabled(true);
    m_inputEdit->setMinimumHeight(40);
    QFont inputFont = m_inputEdit->font();
    inputFont.setPointSize(14);
    m_inputEdit->setFont(inputFont);

    // Setup Trie Autocomplete Completer
    m_completerModel = new QStringListModel(this);
    m_completer = new QCompleter(m_completerModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setMaxVisibleItems(8);
    m_inputEdit->setCompleter(m_completer);

    m_statusBadge = new QLabel(this);
    m_statusBadge->setFixedWidth(24);
    m_statusBadge->setAlignment(Qt::AlignCenter);

    m_searchBtn = new QPushButton("Search", this);
    m_searchBtn->setMinimumHeight(40);
    m_searchBtn->setCursor(Qt::PointingHandCursor);

    layout->addWidget(m_inputEdit, 1);
    layout->addWidget(m_statusBadge);
    layout->addWidget(m_searchBtn);

    connect(m_inputEdit, &QLineEdit::textEdited, this, &SearchBar::onTextChanged);
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &SearchBar::onSearchClicked);
    connect(m_searchBtn, &QPushButton::clicked, this, &SearchBar::onSearchClicked);
    connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated),
            this, &SearchBar::onCompleterActivated);

    // Global shortcut Ctrl+K to focus search
    auto* shortcut = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(shortcut, &QShortcut::activated, this, &SearchBar::setFocusToInput);
}

void SearchBar::onTextChanged(const QString& text) {
    emit queryChanged(text);
    validateSyntax(text);

    // Query Trie for top autocomplete suggestions on last word
    if (m_adapter && !text.isEmpty()) {
        QStringList tokens = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (!tokens.isEmpty()) {
            QString lastToken = tokens.last();
            // Ignore Boolean operator prefixes
            if (lastToken.toUpper() != "AND" && lastToken.toUpper() != "OR" && lastToken.toUpper() != "NOT") {
                auto suggestions = m_adapter->getAutocompleteSuggestions(lastToken.toStdString(), 6);
                QStringList qSuggestions;
                for (const auto& s : suggestions) {
                    qSuggestions.append(QString::fromStdString(s));
                }
                m_completerModel->setStringList(qSuggestions);
            }
        }
    }
}

void SearchBar::onSearchClicked() {
    QString q = currentQuery();
    if (!q.isEmpty()) {
        emit searchRequested(q);
    }
}

void SearchBar::onCompleterActivated(const QString& text) {
    if (!m_inputEdit) return;

    QString current = m_inputEdit->text();
    int lastSpace = static_cast<int>(current.lastIndexOf(' '));
    if (lastSpace >= 0) {
        m_inputEdit->setText(current.left(lastSpace + 1) + text + " ");
    } else {
        m_inputEdit->setText(text + " ");
    }
    m_inputEdit->setFocus();
}

void SearchBar::validateSyntax(const QString& query) {
    if (query.trimmed().isEmpty()) {
        m_statusBadge->clear();
        m_statusBadge->setToolTip("");
        return;
    }

    if (!m_adapter) return;

    std::string errMsg;
    std::size_t errOffset = 0;
    bool isValid = m_adapter->validateQuery(query.toStdString(), errMsg, errOffset);

    if (isValid) {
        m_statusBadge->setText("✓");
        m_statusBadge->setStyleSheet("color: #10B981; font-weight: bold; font-size: 14px;");
        m_statusBadge->setToolTip("Valid query syntax");
    } else {
        m_statusBadge->setText("⚠");
        m_statusBadge->setStyleSheet("color: #EF4444; font-weight: bold; font-size: 14px;");
        m_statusBadge->setToolTip(QString("Syntax warning: %1 (at offset %2)").arg(QString::fromStdString(errMsg)).arg(errOffset));
    }
}

} // namespace sse::gui
