# The docs site

This wiki is an [MkDocs](https://www.mkdocs.org/) site with the
[Material](https://squidfunk.github.io/mkdocs-material/) theme, built from the `docs/` folder and published to
GitHub Pages at <https://robertotorino.github.io/WindowManager/>.

## Running it locally

From the repo root:

```
python -m mkdocs serve
```

Then open <http://127.0.0.1:8000>. The page reloads by itself when you edit files in `docs/`. On another
machine, install the tools first:

```
pip install mkdocs-material
```

`python -m mkdocs build --strict` builds the site into `site/` (git-ignored) and fails on broken links, the
same check the publish workflow runs.

## What is where

| Path | What it is |
|---|---|
| `mkdocs.yml` | Site settings: name, URL, theme, colours, the navigation. A new page needs an entry under `nav`. |
| `docs/index.md` | The home page (hero, feature cards, FAQ). |
| `docs/wiki/` | The user documentation, in full. |
| `docs/development/`, `docs/*.md` | The *Development* tab: building, code layout, port notes. |
| `docs/assets/icon.png`, `favicon.png` | The logo and favicon, made from `resources/window-manager-icon.png`. |
| `docs/assets/extra.css` | The blue theme: colours for light and dark mode, header, hero, cards, footer. |
| `docs/assets/screenshots/` | Screenshots used in the pages (placeholders until replaced, see below). |
| `docs/assets/video/` | The demo video on the home page (`demo.mp4`, 720p) and its poster frame (see below). |
| `overrides/partials/header.html` | Theme override: the header (big icon and name, search except on the home page, *GitHub* to the repo, *Wiki* to the install page). Light / dark starts from the system's setting; the sun / moon button switches it. |
| `overrides/main.html` | Theme override: no tabs on the home page, the link columns in the footer. |
| `docs/help/` | The in-app help (**Help > Help topics**). Not part of the site, see below. |

## The wiki, the in-app help and the README

- **The wiki** has everything, in full.
- **The in-app help** (`docs/help/*.md`, embedded in the exe through `resources/resources.qrc`) has the
  highlights only, so it stays short and works offline. Each page ends with a link to its wiki page, and the
  help window and *Help > Online wiki* link to the site. A new help page needs an entry in the qrc **and** in
  `kTopics` in `src/ui/HelpDialog.cpp`. `mkdocs.yml` excludes `docs/help/` from the site.
- **The README** says what the app is for and how to get started, then points to the wiki.

When a feature changes, update its wiki page, and its help page if the highlights change.

## Screenshots

The images in `docs/assets/screenshots/` are placeholders: a blue card that names the file to replace. To add a
real screenshot, save it over the placeholder with the same name (PNG, about 1600 px wide); no page needs to
change.

| File | Used on |
|---|---|
| `main-window.png` | Home, First steps, The main window |
| `fit-to-monitor.png` | Fit to monitor |
| `downscaled-view.png` | Downscaled view |
| `display-tab.png` | Display settings |
| `game-config-tab.png` | Game config |
| `game-overlay.png`, `floating-bar.png` | Game info overlay and floating bar |
| `reports-session.png`, `reports-setups.png`, `reports-pictures.png` | Reports (taken with the stand-in game) |
| `gpu-power-tab.png` | GPU and power |
| `helixsr-tab.png` | HelixSR |
| `log-viewer.png` | Logs and troubleshooting |
| `linux.png` | Linux (beta) |

In a page a screenshot is written as `![Caption](../assets/screenshots/<file>.png){ .wm-shot loading=lazy }`;
`.wm-shot` gives it the rounded border and shadow.

## Demo video

`docs/assets/video/demo.mp4` (1280x720, about 1.6 MB) is the home page's video; `demo-poster.jpg` is the frame
shown before it plays. Keep it small: it is served by GitHub Pages and stored in git. The 1080p version for
YouTube is not in the repo.

To record it again after the UI changed (build the app first; needs ffmpeg, `winget install Gyan.FFmpeg`):

```powershell
pwsh scripts/demo-video/record-demo.ps1 -UpdateDocs
```

The script copies the built app to `C:\WMDemo` with its own `windowmanager.json` (your own setup and games are
not touched or shown), builds the stand-in game (`scripts/demo-video/DemoGame.cs`: a resizable window that
prints its own size), and plays the tour on the second monitor while ffmpeg records it. It moves the real mouse
for about a minute: leave the mouse and keyboard alone until it says it is done. Then it adds the title card, a
caption per step and the end card, and writes `build/demo-video/windowmanager-demo-1080p.mp4` (YouTube) and
`-720p.mp4` (wiki); `-UpdateDocs` copies the 720p video and the poster here.

- `-EditOnly` redoes only the captions and cards from the last recording (`raw.mp4`, `steps.csv`).
- `-Monitor <n>` picks another monitor (`-Monitor 1` records the main one, with everything on it).
- The tour is laid out for a 1920x1080 monitor at 100 % scale. Each step's clicks and caption are in the
  script's *Record* section; when a button is renamed, the script stops and names the control it can't find.

The stand-in game draws with GDI, so it cannot show the downscaled view: Windows stops painting a GDI window
that is parked off every monitor. Record that with a real (Direct3D) game.

## Publishing

`.github/workflows/docs.yml` builds the site with `mkdocs build --strict` and publishes it to GitHub Pages. It
runs on every push to `main` that touches `docs/`, `overrides/` or `mkdocs.yml`, and by hand from the
*Actions* tab (*Docs > Run workflow*).

One-time setup on GitHub:

1. **Settings > Pages > Build and deployment > Source: GitHub Actions.**
2. On the repo's main page, the **gear next to About**: tick **Use your GitHub Pages website**, so the link
   shows in the repository details.
