#pragma once

#include <functional>
#include <optional>

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

/**
 * Finds the games installed through Steam, from Steam's own text files: `libraryfolders.vdf` lists the
 * library folders, and each game has `steamapps/appmanifest_<appid>.acf` with its name and folder.
 *
 * The parsing and the choice of a game's main executable are pure (no OS calls) and unit-tested;
 * findGames() reads the files.
 */
namespace SteamLibrary {

struct Manifest
{
    quint32 appId{0};
    QString name;
    QString installDir;          // folder name under steamapps/common
    bool    fullyInstalled{false};
};

struct Game
{
    quint32 appId{0};
    QString name;
    QString library;             // the library folder the game is in
    QString installPath;         // <library>/steamapps/common/<installDir>
    QString exePath;             // the main executable; empty when none was found
};

struct Scan
{
    QStringList libraries;       // library folders that exist (a game outside them is not judged)
    QList<Game> games;           // installed games with an executable, tools left out
};

// ----- Pure -----------------------------------------------------------------

// Library folder paths from the text of libraryfolders.vdf.
QStringList parseLibraryFolders(const QString& vdf);

// The AppState of an appmanifest_<appid>.acf; nullopt when it has no AppID.
std::optional<Manifest> parseManifest(const QString& acf);

// Proton, the Steam Linux Runtime, the Steamworks redistributables: installed like games, but not games.
bool isTool(quint32 appId, const QString& name);

struct ExeCandidate
{
    QString relativePath;        // to the install folder, '/' separated
    qint64  size{0};
};

// The game's own executable: the largest one that is not a crash reporter, installer, redistributable,
// anti-cheat or launcher helper. -1 when there is none.
int pickMainExe(const QList<ExeCandidate>& candidates);

// ----- Files ----------------------------------------------------------------

// Steam's folders on this system (the registry on Windows; ~/.local/share/Steam, ~/.steam/steam and the
// Flatpak folder on Linux), only those that exist, without duplicates.
QStringList defaultSteamRoots();

// Every installed game in the libraries of `steamRoots`.
Scan findGames(const QStringList& steamRoots);

// findGames(defaultSteamRoots()) on a worker thread; `done` runs on the thread of `context` (queued),
// and is skipped when `context` has been destroyed by then.
void findGamesAsync(QObject* context, std::function<void(const Scan&)> done);

} // namespace SteamLibrary
