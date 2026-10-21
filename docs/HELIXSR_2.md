# HelixSR 2.x support

The app sets up and installs HelixSR 2.x (built 2026-10-20). Until then it held back at 1.x: HelixSR 2.0's
setup script, run unattended, installs HelixSR Scaler into every game it finds. The HelixSR author did not answer
the request for a build-only switch (issue #90), so the app does the setup's steps itself. Facts below are from
the HelixSR 2.0.0 release (2026-10-19, `HelixSR-2.0.0.zip`, SHA-256 `0d60686e...7f52`).

## What 2.0 changed

| Change in 2.0 | What the app does |
|---|---|
| `helixsr-setup.ps1 -Yes` builds, then runs `helixsr_install.py --yes`: HelixSR Scaler (OptiScaler) goes into **every game the setup finds**, with frame generation on. | The `.ps1` is never run. `Helix::setupDownloads` reads its `$Downloads` table (Python, numpy, DXC: URL and SHA-256, which change per release); `HelixSetup::prepareTools` fetches, checks and unpacks what is missing into `%LOCALAPPDATA%\HelixSR`, as the `.ps1` does; then `python -I setup/helixsr_setup.py <package> --dxc <dxc> --yes --no-fg`. Exit 0 or 3 is success; `--status` must then print `network=1`. |
| `helixsr_install.py` lost `install_list` and `uninstall_safe`; `games_from` moved to `helixsr_games.py`. | 2.x packages are installed through HelixSR's JSON bridge, `setup/helixsr_bridge.py` (below). `resources/helixsr_driver.py` stays for 1.x packages still on disk (`Helix::hasBridge`). |
| HelixSR's DLL is in `HelixSR-fsrfg/amd_fidelityfx_dx12.dll`, not in the package root. | `Helix::packageDll`. |
| Frame generation is built by default (DLSS Frame Generation DLLs, NVIDIA's NGX core). | `--no-fg`: only HelixSR Scaler uses it. |
| `helixsr-scaler/` (HelixSR Scaler 1.1.0, GPL) is in the package; the build writes `fsr3-drop-in/` copies of the DLL. | Both are deleted by the clean-up (`Helix::buildLeftovers`). |

Unchanged: shader model 6.6 (`cs_6_6`), Python 3.14.8 and DXC in `%LOCALAPPDATA%\HelixSR`, the network from
NVIDIA's DLSS 310.7.0 DLL, `helixsr.ini` next to the FSR drop-in, the DLL announcing `HelixSR x.y.z` (so
`Helix::scanGame` still recognises installs).

## The bridge

Every command prints one JSON object (`protocol`, `ok`, `error`, `messages`, and for game commands `game`):

```
python -I setup/helixsr_bridge.py <package> install --game <game root> --way fsr
python -I setup/helixsr_bridge.py <package> remove --game <game root>
```

- The app always passes `--way fsr` (`Helix::bridgeArgs`): the bridge's own `auto` picks HelixSR Scaler for most
  games.
- `Helix::parseBridgeReply` reads the last JSON line; a reply whose `protocol` is not `kBridgeProtocol` (1) makes
  the self-test fail, so installing switches off instead of guessing.
- The bridge looks up the game in a scan of all game libraries first (read-only, about 2 s here), then in the
  folder.
- An update of a game that had an older drop-in keeps the previous version in `HelixSR-previous.zip` in the game
  folder (the bridge's `rollback`); `remove` deletes it again.
- Tried on a throw-away game folder with the 2.0 bridge: install replaced the DLL (game's kept as
  `*.original.dll`, plus `helixsr-install.json` and `helixsr.ini`), remove put the game's DLL back byte for byte.

## Status

- Built, 14 test suites pass (`tst_helix`: `.ps1` table, build / status / bridge arguments, bridge replies,
  package layouts, `kSupportedMajor` = 2).
- A full set-up in a scratch copy of the app: see *Where we left off* in `CLAUDE.md`.
- **Not tried yet:** a real game (Horizon Forbidden West has HelixSR 1.9.3: update it to 2.0 from the tab, check
  `helixsr.log` and the DLL's version, then Remove; make sure no other game folder changed). Updating from the
  user's 1.9.3 package to 2.0.0 in the real app.

## Later, separate decisions

- **HelixSR Scaler** (`--way scaler`, plus `--scaler <extracted HelixSR-Scaler zip>`): covers DLSS / XeSS / FSR 2
  games, but is OptiScaler (GPL) as a `dxgi.dll` proxy, with an anti-cheat risk (the bridge refuses games with
  anti-cheat without `--anticheat-ok yes`).
- **Frame generation** (`--fg on`): needs the frame-generation build (drop `--no-fg`, more downloads).
- **Linux / Proton**: 2.0 ships `helixsr-setup.sh`, and the bridge is plain Python, which fits the Linux port.

## OptiScaler-forceenabled (looked at 2026-10-20)

[lonewolf0622/OptiScaler-forceenabled](https://github.com/lonewolf0622/OptiScaler-forceenabled), by the HelixSR
author, marked "testing": one squashed commit of OptiScaler 0.9.4 (upstream `7534ad0`) plus a 15-line patch, no
releases (CI artifacts only). Diffed against upstream, the patch only adds `[DLSS] ForceEnabled` (default false):

- OptiScaler normally switches its DLSS backend off when the GPU is not NVIDIA. With `ForceEnabled=true` it
  keeps it on and loads the library in `[Libraries] NvngxPath`, meant for a DLSS-compatible (NGX) library that
  runs on other GPUs, such as HelixSR's earlier `nvngx.dll` way.
- It also keeps DXGI spoofing on, so the game offers DLSS.
- It does not make NVIDIA's own DLSS run on another GPU, and changes nothing on an NVIDIA GPU (this PC's GTX 1650
  passes the check already).

Not taken up: HelixSR 2.0 no longer builds an `nvngx.dll`, and HelixSR Scaler 1.1.0 (the released successor)
plugs HelixSR into OptiScaler as its FidelityFX upscaler instead. Using the fork would mean building and shipping
a GPL OptiScaler ourselves, installed as a DLL proxy in games. HelixSR Scaler through the bridge does that job
with released, checksummed files.

## Working rules

Same as `CLAUDE.md`: the user does all git operations; never commit, stage or push.
