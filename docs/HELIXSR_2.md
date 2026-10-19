# HelixSR 2.x support (not started)

Working notes for moving the HelixSR tab from HelixSR 1.x to 2.x. Until this is done the app sets up and
updates HelixSR **1.x only** (`Helix::kSupportedMajor` in `src/helix/HelixRelease.h`);
the tab says when a newer major version is out. Facts below are from the HelixSR 2.0.0 release (2026-10-19,
`HelixSR-2.0.0.zip`, SHA-256 `0d60686e...7f52`).

## Why 2.0 is held back

| Problem | What happens with 2.0 | Where in the app |
|---|---|---|
| The setup installs into games | `helixsr-setup.ps1 -Yes` builds, then runs `helixsr_install.py --yes`. That answers every game question with its default: HelixSR Scaler (OptiScaler) goes into **every game the setup finds**, with frame generation on. In 1.x the game step got `--yes` only for downloads and stopped at EOF. | `HelixSetup::buildNetwork` starts the `.ps1` with `-Yes` |
| The install helper no longer fits | `helixsr_install.py` lost `install_list` and `uninstall_safe`; `games_from` moved to `helixsr_games.py`. `resources/helixsr_driver.py` fails with exit code 2, so the self-test switches installing off. | `resources/helixsr_driver.py`, `HelixSetup::testDriver` |
| The DLL moved | HelixSR's DLL is in `HelixSR-fsrfg/amd_fidelityfx_dx12.dll`, not in the package root. | `HelixPaths`, `HelixGame` (package version, outdated games) |
| More is built and downloaded | Frame generation is built by default: NVIDIA's DLSS Frame Generation DLLs (310.3, 310.9.16) and NVIDIA's NGX core from the Linux driver package. | build time, disk check (`kRequiredDiskMB`) |
| The setup keeps more | `helixsr-scaler/` (HelixSR Scaler 1.1.0, GPL) is part of the package. The app's clean-up deletes it, which is fine as long as the app installs only the FSR drop-in. | `Helix::buildLeftovers` |

Unchanged: shader model 6.6 is still required (`DXC_ARGS = ['-T', 'cs_6_6', ...]`), Python and DXC still go in
`%LOCALAPPDATA%\HelixSR`, the network still comes from NVIDIA's DLSS 310.7.0 DLL, and `helixsr.ini` still sits
next to the FSR drop-in.

## The way in: `helixsr_bridge.py`

2.0 ships `setup/helixsr_bridge.py`, written for HelixSR's own desktop app. Every command prints one JSON object,
and the bridge and the text setup call the same functions, so installs made by either can be updated or removed
by the other.

```
python -I setup/helixsr_bridge.py <release dir> status
python -I setup/helixsr_bridge.py <release dir> games [--folder DIR]...
python -I setup/helixsr_bridge.py <release dir> install --game DIR --way fsr
python -I setup/helixsr_bridge.py <release dir> remove --game DIR
python -I setup/helixsr_bridge.py <release dir> settings-get --game DIR
python -I setup/helixsr_bridge.py <release dir> settings-set --game DIR --values '{"Section.Key": "value"}'
python -I setup/helixsr_bridge.py <release dir> log --game DIR
```

- `status`: `version`, `network_ready`, `fg_ready`, `fg_reason`.
- `games`: per game `ways` (`scaler`, `fsr`), `recommended`, `anticheat`, `installed`, `version`,
  `settings_path`, `log_path`, `blocked`. `--folder` adds a game the scan does not find (the app passes the
  saved game's folder).
- `install --way fsr` is the 1.x FSR drop-in: the game's FSR 3.1 DLL is replaced and kept as `*.original.dll`.
  It also updates 1.x installs.

## Plan

1. **Build without the game step.** Do not run `helixsr-setup.ps1`. Run the two things it does before the games:
   - Python with numpy, and DXC, into `%LOCALAPPDATA%\HelixSR`. Their URLs and SHA-256 sums are in the
     `$Downloads` table of the release's `.ps1`; read them from there (they change per release) and check the
     hashes as the `.ps1` does.
   - `python -I setup/helixsr_setup.py <release dir> --dxc <dxc.exe> --yes --no-fg`. `--yes` only agrees to
     NVIDIA's DLSS download here (the user agreed in the app). `--no-fg` skips frame generation, which only
     HelixSR Scaler uses. Exit code 0 (or 3) is success; then `--status` must print `network=1`.
   - Fall-back if parsing the `.ps1` proves fragile: ask the HelixSR author for a `-NoGames` (build only) switch.
2. **Replace `helixsr_driver.py` with the bridge** for install, remove and the self-test. Parse the JSON in a
   pure unit (like `HelixRelease`) and test it in `tst_helix` with recorded outputs. Always pass `--way fsr`,
   never `auto` (auto picks HelixSR Scaler for most games).
3. **Use `games --folder <game folder>`** for the HelixSR list instead of the app's own DLL scan, or next to it:
   it reports anti-cheat, the installed way and version, and the settings and log paths.
4. **Settings:** keep `HelixIni` for `helixsr.ini`, or switch to `settings-get` / `settings-set`.
5. **Paths:** read the package DLL from `HelixSR-fsrfg/`; keep reading the root for 1.x packages.
6. **Version gate:** support both majors during the move (1.x packages stay installed on users' PCs), then raise
   `kSupportedMajor`. Update the help page, README and CHANGELOG.
7. **Test on a real game** (Horizon Forbidden West has HelixSR 1.9.3 installed): update to 2.0 through the
   app, check the game's DLL version, `helixsr.log`, then remove. Make sure no other game folder changed.

Later, separate decisions: HelixSR Scaler (`--way scaler`, covers DLSS/XeSS/FSR 2 games, GPL, OptiScaler,
anti-cheat risk) and frame generation (`--fg on`, needs the frame-generation build). 2.0 also runs on Linux /
Proton (`helixsr-setup.sh`), which fits the Linux port: the bridge is plain Python.

## Working rules

Same as `CLAUDE.md`: the user does all git operations; never commit, stage or push.
