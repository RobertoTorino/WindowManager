#include "HelpDialog.h"

#include <QFile>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QSignalBlocker>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Reading order of the topics. Titles come from each page's first heading.
const QStringList kTopics = {
    "getting-started", "games-and-launching", "window-controls", "fit-to-monitor",
    "downscaled-view", "display-settings", "game-stats", "helixsr", "linux", "logs-and-troubleshooting", "shortcuts", "reference",
};

QString readPage(const QString &topic)
{
    QFile file(":/help/" + topic + ".md");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QString titleOf(const QString &markdown, const QString &fallback)
{
    for (const QString &line : markdown.split('\n')) {
        if (line.startsWith("# "))
            return line.mid(2).trimmed();
    }
    return fallback;
}

QPointer<HelpDialog> g_open;

} // namespace

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
    , m_search(new QLineEdit(this))
    , m_topics(new QListWidget(this))
    , m_page(new QTextBrowser(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("WindowManager - Help");
    resize(960, 640);

    m_search->setPlaceholderText("Search help...");
    m_search->setClearButtonEnabled(true);
    m_topics->setMinimumWidth(200);
    m_topics->setMaximumWidth(260);
    m_page->setOpenLinks(false);   // .md links switch topic; handled in followLink

    auto *left = new QVBoxLayout;
    left->addWidget(m_search);
    left->addWidget(m_topics, 1);

    auto *layout = new QHBoxLayout(this);
    layout->addLayout(left);
    layout->addWidget(m_page, 1);

    connect(m_search, &QLineEdit::textChanged, this, &HelpDialog::applyFilter);
    connect(m_topics, &QListWidget::currentRowChanged, this, &HelpDialog::showTopic);
    connect(m_page, &QTextBrowser::anchorClicked, this, &HelpDialog::followLink);

    applyFilter();
}

void HelpDialog::applyFilter()
{
    const QString query = m_search->text().trimmed();
    const QString keep  = m_topics->currentItem() ? m_topics->currentItem()->data(Qt::UserRole).toString()
                                                  : kTopics.first();

    const QSignalBlocker block(m_topics);
    m_topics->clear();
    for (const QString &topic : kTopics) {
        const QString text = readPage(topic);
        if (!query.isEmpty() && !text.contains(query, Qt::CaseInsensitive))
            continue;
        auto *item = new QListWidgetItem(titleOf(text, topic), m_topics);
        item->setData(Qt::UserRole, topic);
        if (topic == keep)
            m_topics->setCurrentItem(item);
    }
    if (m_topics->count() == 0) {
        m_page->setPlainText("No help topic mentions \"" + query + "\".");
        return;
    }
    if (!m_topics->currentItem())
        m_topics->setCurrentRow(0);
    showTopic(m_topics->currentRow());
}

void HelpDialog::showTopic(int row)
{
    const QListWidgetItem *item = m_topics->item(row);
    if (!item)
        return;
    m_page->setMarkdown(readPage(item->data(Qt::UserRole).toString()));

    const QString query = m_search->text().trimmed();
    if (!query.isEmpty())
        m_page->find(query);   // jump to the first match
}

void HelpDialog::followLink(const QUrl &url)
{
    // Pages link to each other as "topic.md" so they also work on GitHub and in MkDocs.
    const QString file = url.path();
    if (!file.endsWith(".md"))
        return;
    const QString topic = file.chopped(3);
    for (int i = 0; i < m_topics->count(); ++i) {
        if (m_topics->item(i)->data(Qt::UserRole).toString() == topic) {
            m_topics->setCurrentRow(i);
            return;
        }
    }
    m_search->clear();   // the topic was filtered out
    for (int i = 0; i < m_topics->count(); ++i) {
        if (m_topics->item(i)->data(Qt::UserRole).toString() == topic)
            m_topics->setCurrentRow(i);
    }
}

void HelpDialog::open(QWidget *parent, const QString &topic)
{
    if (!g_open) {
        g_open = new HelpDialog(parent);
        g_open->show();
    }
    if (!topic.isEmpty()) {
        g_open->m_search->clear();
        for (int i = 0; i < g_open->m_topics->count(); ++i) {
            if (g_open->m_topics->item(i)->data(Qt::UserRole).toString() == topic)
                g_open->m_topics->setCurrentRow(i);
        }
    }
    g_open->raise();
    g_open->activateWindow();
}
