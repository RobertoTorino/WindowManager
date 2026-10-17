#include "GameRegistry.h"

#include "core/Utilities.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace GameRegistry {

QString normalizePath(const QString& path)
{
    if (path.isEmpty())
        return {};
    return QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
}

QString sanitizeFriendlyName(const QString& text)
{
    return QString::fromStdString(Utilities::sanitizeName(text.toStdString()));
}

QString defaultFriendlyName(const QString& selectedPath)
{
    const QFileInfo info(selectedPath);
    QString name = info.dir().dirName();
    if (name.isEmpty())
        name = info.completeBaseName();
    return sanitizeFriendlyName(name);
}

std::optional<QString> promptForFriendlyName(QWidget* parent,
                                             const QString& defaultName,
                                             const QString& initialName)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Game Name");
    dialog.setModal(true);
    dialog.setMinimumWidth(460);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);
    layout->setContentsMargins(18, 16, 18, 16);

    auto* label = new QLabel("Display name for this game:", &dialog);
    auto* hint  = new QLabel("Letters, digits and underscores only; other characters are converted.", &dialog);
    hint->setObjectName("hint");
    auto* nameEdit = new QLineEdit(initialName, &dialog);
    nameEdit->selectAll();

    QObject::connect(nameEdit, &QLineEdit::textEdited, &dialog, [nameEdit](const QString& text) {
        const QString sanitized = sanitizeFriendlyName(text);
        if (sanitized == text)
            return;
        QSignalBlocker blocker(nameEdit);
        nameEdit->setText(sanitized);
        nameEdit->setCursorPosition(nameEdit->text().size());
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         Qt::Horizontal, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    layout->addWidget(label);
    layout->addWidget(nameEdit);
    layout->addWidget(hint);
    layout->addStretch();
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return std::nullopt;

    const QString friendly = sanitizeFriendlyName(nameEdit->text());
    return friendly.isEmpty() ? defaultName : friendly;
}

AppConfig::GameEntry buildEntry(const QString& selectedPath, const QString& friendlyName)
{
    const QString normalizedPath = normalizePath(selectedPath);
    const QFileInfo info(normalizedPath);
    const auto existing = AppConfig::instance().gameByPath(normalizedPath);

    AppConfig::GameEntry game;
    if (existing.has_value())
        game = *existing;

    if (game.id.isEmpty()) {
        std::vector<std::string> existingIds;
        for (const auto& entry : AppConfig::instance().games()) {
            if (!entry.id.isEmpty())
                existingIds.push_back(entry.id.toStdString());
        }
        game.id = QString::fromStdString(
            Utilities::generateUniqueId(friendlyName.toStdString(), existingIds));
    }

    game.applicationPath = normalizedPath;
    if (game.captureDir.isEmpty())
        game.captureDir = QString("captures/") + game.id;
    game.ebootIsoPath.clear();
    game.gameApplication = info.fileName();
    if (game.isPatchable.isEmpty())
        game.isPatchable = "false";
    if (game.launcherType.isEmpty())
        game.launcherType = "STANDARD";
    game.savedName = friendlyName;
    if (game.snapshotDir.isEmpty())
        game.snapshotDir = QString("snapshots/") + game.id;
    return game;
}

QString snapshotDirFor(const QString& exePath)
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString normalized = normalizePath(exePath);
    QString relDir = "snapshots";
    if (!normalized.isEmpty()) {
        if (const auto game = AppConfig::instance().gameByPath(normalized); game.has_value()) {
            if (!game->id.isEmpty())
                relDir = "snapshots/" + game->id;
            else if (!game->snapshotDir.isEmpty())
                relDir = game->snapshotDir;
        }
    }
    return appDir.filePath(relDir);
}

} // namespace GameRegistry
