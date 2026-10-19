#include "GameRegistry.h"

#include "core/Utilities.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QSet>
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

SteamSync syncSteamGames(const SteamLibrary::Scan& scan)
{
    SteamSync result;
    auto& config = AppConfig::instance();

    QSet<quint32> installed;
    for (const auto& g : scan.games) {
        installed.insert(g.appId);
        const QString exe = normalizePath(g.exePath);

        std::optional<AppConfig::GameEntry> known;
        for (const auto& entry : config.games()) {
            if (entry.steamAppId == g.appId)
                known = entry;
        }
        if (known && QFileInfo::exists(known->applicationPath))
            continue;   // the user's executable, still there

        AppConfig::GameEntry entry = config.gameByPath(exe).value_or(
            buildEntry(exe, known ? known->savedName : sanitizeFriendlyName(g.name)));
        if (known) {
            // the executable moved (game update): keep the name, id and folders under the new path
            const QString path = entry.applicationPath;
            entry = *known;
            entry.applicationPath = path;
            entry.gameApplication = QFileInfo(path).fileName();
            config.removeGame(known->applicationPath);
        } else if (!config.gameByPath(exe)) {
            ++result.added;
        }
        entry.steamAppId = g.appId;
        entry.launcherType = "STEAM";
        config.saveGame(entry);
    }

    // Uninstalled: the AppID is gone from a library that is there, and so is the executable.
    for (const auto& entry : config.games()) {
        if (entry.steamAppId == 0 || installed.contains(entry.steamAppId) || QFileInfo::exists(entry.applicationPath))
            continue;
        const bool inLibrary = std::any_of(scan.libraries.begin(), scan.libraries.end(), [&](const QString& lib) {
            return entry.applicationPath.startsWith(lib + '/', Qt::CaseInsensitive);
        });
        if (inLibrary) {
            config.removeGame(entry.applicationPath);
            ++result.removed;
        }
    }
    return result;
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
