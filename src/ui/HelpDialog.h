#pragma once

#include <QDialog>

class QLineEdit;
class QListWidget;
class QTextBrowser;
class QUrl;

/**
 * Topic-based help window. The pages are the markdown files in docs/help, embedded
 * in the executable (see resources/resources.qrc). Links between pages work, and the
 * search box filters the topics by content and highlights the match in the page.
 * The pages hold the highlights; web links (the online wiki) open in the browser.
 */
class HelpDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HelpDialog(QWidget *parent = nullptr);

    // Opens (or raises) the help window, optionally on a topic such as "shortcuts".
    static void open(QWidget *parent, const QString &topic = {});

    // The online wiki (the MkDocs site), optionally on a page such as "wiki/fit-to-monitor/".
    static QUrl wikiUrl(const QString &page = {});

private:
    void applyFilter();
    void showTopic(int row);
    void followLink(const QUrl &url);

    QLineEdit    *m_search{nullptr};
    QListWidget  *m_topics{nullptr};
    QTextBrowser *m_page{nullptr};
};
