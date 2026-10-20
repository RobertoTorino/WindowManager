# Download and install

## Windows

1. Open the [latest release](https://github.com/RobertoTorino/WindowManager/releases/latest) on GitHub and
   download `WindowManager-<version>.zip`.
2. Unzip it into a folder of its own, for example `C:\Tools\WindowManager`. Pick a folder you can write to:
   the app keeps its settings, logs and snapshots next to the program (see [Files and settings](files.md)).
3. Start `WindowManager.exe`.

Each release also has a `.sha256` file with the exe's checksum, and a build provenance record from GitHub.
There is no installer and nothing goes in the registry. To remove the app, close it and delete the folder.

!!! tip "Windows SmartScreen"
    The exe is not code-signed, so Windows may show *Windows protected your PC* the first time. Choose
    *More info > Run anyway* if you downloaded it from the release page above.

**Updating:** close the app, unzip the new release over the old folder, and start it again. Your games and
settings are kept, because they live in files the release does not contain.

### Requirements

- Windows 10 or 11, 64-bit.
- For the FPS counter: administrator rights, or your account in the *Performance Log Users* group (see
  [Game stats](game-stats.md#fps-needs-administrator-rights)).
- For HelixSR: a Direct3D 12 card with shader model 6.6 (see [HelixSR](helixsr.md)).

## Linux (beta)

There is no Linux download yet. The app is built from source; see [Linux](linux.md#getting-it).

## Next

Continue with [First steps](getting-started.md).
