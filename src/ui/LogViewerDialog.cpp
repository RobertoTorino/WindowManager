#include "LogViewerDialog.h"

#include "core/Logger.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString toQString(const std::filesystem::path &p)
{
    return QString::fromStdWString(p.wstring());
}

} // namespace

LogViewerDialog::LogViewerDialog(QWidget *parent)
    : QDialog(parent)
    , m_source(new QComboBox(this))
    , m_text(new QPlainTextEdit(this))
{
    setWindowTitle("WindowManager - Logs");
    resize(900, 560);

    m_text->setReadOnly(true);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_source->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    auto *refresh = new QPushButton("Refresh", this);
    auto *folder  = new QPushButton("Open folder", this);
    auto *close   = new QPushButton("Close", this);

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel("Log:", this));
    top->addWidget(m_source, 1);
    top->addWidget(refresh);
    top->addWidget(folder);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_text, 1);
    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    bottom->addWidget(close);
    layout->addLayout(bottom);

    connect(m_source, &QComboBox::currentIndexChanged, this, &LogViewerDialog::load);
    connect(refresh,  &QPushButton::clicked, this, [this]() { populate(); });
    connect(folder,   &QPushButton::clicked, this, &LogViewerDialog::openFolder);
    connect(close,    &QPushButton::clicked, this, &QDialog::accept);

    populate();
}

void LogViewerDialog::populate()
{
    // keep the selection across a refresh; first open selects the current game
    QString keep = m_source->currentData().toString();
    if (keep.isEmpty()) {
        const std::string game = Logger::instance().currentGame();
        if (!game.empty())
            keep = toQString(Logger::instance().gameLogPath(game));
    }

    const QSignalBlocker block(m_source);
    m_source->clear();

    m_source->addItem("Application log", toQString(Logger::instance().appLogPath()));

    const QDir dir(toQString(Logger::instance().gameLogDirectory()));
    for (const QFileInfo &fi : dir.entryInfoList({"*.log"}, QDir::Files, QDir::Name | QDir::IgnoreCase)) {
        const bool previous = fi.completeBaseName().endsWith("_old");
        QString label = fi.completeBaseName();
        if (previous)
            label = label.chopped(4) + " (previous)";
        m_source->addItem(label, fi.absoluteFilePath());
    }

    const int idx = m_source->findData(keep);
    m_source->setCurrentIndex(idx >= 0 ? idx : 0);
    load();
}

void LogViewerDialog::load()
{
    const QString path = m_source->currentData().toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_text->setPlainText(QString("No log yet (%1).").arg(QDir::toNativeSeparators(path)));
        return;
    }
    m_text->setPlainText(QString::fromUtf8(file.readAll()));
    m_text->moveCursor(QTextCursor::End);   // newest entries are at the bottom
}

void LogViewerDialog::openFolder()
{
    const QString path = m_source->currentData().toString();
    QDir dir = QFileInfo(path).dir();
    if (!dir.exists())
        dir.mkpath(".");
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
}
