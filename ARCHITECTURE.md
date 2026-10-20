# bakajiten — Qt frontend: design and functionality

A Qt6 Widgets desktop app: a Japanese dictionary with handwriting
recognition. This document describes the architecture in enough
detail to reimplement it. All paths are relative to
`frontends/qt/` unless noted; the repo root holds the Rust
workspace (`lib/`, `utils/`, `Cargo.toml`).

Conventions used below: `Class::method` = Qt slot/signal where
noted; `OBJECT` = `objectName` used with `findChild`.

## 1. Principles

- **C++ owns windows, Rust owns matching.** Two Rust crates expose
  a UniFFI boundary: `librecognizer` (stroke matcher + template
  bank) and `libdict` (dictionary storage + lookup, deinflection,
  kana/romaji/furigana helpers). C++ never implements matching or
  search ranking; Rust never touches widgets.
- **Batch bridge.** Strokes cross per `recognize` call as nested
  vectors; nothing list-shaped accumulates incrementally.
- **One dictionary at a time.** Scope combo selects
  English (JMdict) / Names (JMnedict) / Kokugo; exactly one binary
  is loaded.
- **No archive validation beyond headers.** `*.bin` artifacts are
  trusted after magic + version + length checks; corrupt files
  yield empty handles, never crashes.
- **Plain enums + byte indices** in storage paths; no bitmasks,
  no heap strings on hot paths.
- **UI scale**: 25% steps (75–200%), fonts follow scale
  everywhere; toolbar buttons stay square with a 40px floor.
- **One global hover delay** (200 ms) for kanji cards and tag tips.
  Hovering kanji anywhere shows the card; double-click opens the
  modeless kanji dialog.

## 2. Process and startup (`src/main.cpp`)

1. `QT_FORCE_STDERR_LOGGING=1` (headless runs lose stderr).
2. `QApplication`; load bundled `:/fonts/NotoSansJP.ttf` (a static
   Regular instantiated at build time — variable-font defaults
   render Thin under Wine) via `addApplicationFont`. Harmless miss
   on other platforms.
3. Window icon, `OrganizationName`/`ApplicationName` =
   `"bakajiten"`, version from `project()` (currently 0.1.1).
4. `AppSettings::noteOsStyle()` then `shared().apply()` — font
   scale + style before any widget exists.
5. Kana fallback: if the app font lacks U+3042, append the
   embedded Noto family, else the first installed family with the
   Japanese writing system, to `QApplication::font`. (Qt's font
   merging relies on OS font linking, broken e.g. under Wine.)
6. Recreate loop: `do { MainWindow w; … } while
   (consumeRecreateRequested())` — a scale change quits and
   recreates the window in-process.

Environment overrides: `BAKAJITEN_SETTINGS` (settings file path),
`BAKAJITEN_DATA_DIR` (data dir), `MODEL` (template bank path),
`BAKAJITEN_UPDATE_ARTIFACT` (empty disables update checks),
`$UI_DIR` (dev `.ui` lookup).

## 3. Build system (`CMakeLists.txt`, `cmake/`)

`cmake -S . -B build && cmake --build build` (`-G Ninja`).
C++ standard is `BAKAJITEN_CXX_STANDARD` (default 26; CI uses 20).
`-freflection` is added directory-wide only for GCC ≥ 16 with
C++26 (dev reflection fill); `DEV_UILOADER` is forced ON for
Debug builds, OFF otherwise.

- `dependencies.cmake` — `find_package(Qt6 …)`, `find_package
  (yaml-cpp)`, `find_package(ZLIB)`, Corrosion via
  `FetchContent` (`corrosion_import_crate`, crates
  `librecognizer libdict`). macOS-only: clears Corrosion's
  hardcoded CLT SDK link dirs (unparsable TBDs).
- `uniffi.cmake` — generates C++ bindings at build time from the
  just-built `librecognizer`/`libdict` cdylibs with
  `uniffi-bindgen-cpp` (pinned `v0.10.0+v0.31.0`, UniFFI 0.31;
  must be on PATH). Generated TUs live in their own static-lib
  targets (`uniffi_recognizer`, `uniffi_dict`), exempt from tidy
  and clang-format.
- `app.cmake` — the `bakajiten` target: explicit source list,
  `BAKAJITEN_UPDATE_ARTIFACT` compile definition (per-artifact
  kind, empty by default), `BAKAJITEN_BUNDLE_CJK_FONT` (embeds
  the instantiated Noto via generated `.qrc`; Windows static
  only), plugin imports for static Qt builds, TLS plugin.
- `tests.cmake` — `test_mainwindow` (all suites, explicit source
  list), `BUILD_QT_TESTS` option.
- `install.cmake` — `cmake --install` layout: `usr/bin/bakajiten`,
  desktop file, scalable + 256px icons (GNUInstallDirs).
- `analysis.cmake` — clang-tidy (`--warnings-as-errors`,
  `bugprone/clang-analyzer/performance/modernize-*`) per TU via
  launcher script; clang-format normalizes `src/` + `tests/` in
  place on every build.

## 4. Settings (`src/core/settings/`)

- `AppSettings` — plain value type, single home for persisted UI
  state. Keys (under `ui/`, flat-file fallback on read):
  `scale` (double, clamped 0.5–3.0), `style` (empty = OS
  default), `icons` (`IconScheme` Auto/Light/Dark),
  `reading` (`ReadingDisplay` Kana/Hepburn/Kunrei),
  `checkUpdates` (bool, default true). File:
  `<AppConfigLocation>/settings.yaml` (or
  `BAKAJITEN_SETTINGS`); `shared()` is the live instance;
  `operator==` drives dirty checks; `apply()` re-sets style +
  font from the live desktop base (never compounded).
- `YamlFormat` — registers a `yaml` `QSettings` format
  (slash-nested maps, `QStringList`, `@Size(W H)` sizes).
- `MemorizedState` — typed accessors over sibling `state.yaml`
  (window state, never user-edited): results column widths,
  article width (+ rescale on UI-scale change), recognizer panel
  size, kanji strip open states, skipped update version.

## 5. Data layer (`src/core/data/`, `src/ui/data/`)

- `DataPaths` — data lives in the directory holding
  `settings.yaml` (`BAKAJITEN_DATA_DIR` overrides outright).
  `requiredFiles = [jmdict.bin, kanji.bin, templates.bin]`
  (missing → first-run Dictionaries dialog),
  `optionalFiles = [jmnedict.bin, kokugo.bin]`.
- `DataSource` — release `.bin.gz` URLs on the `bakajiten-data`
  repo (`manifest.json` + per-file `name.gz`).
- `DataManifestReader` — parses `manifest.json`
  (`{build_date, files: {name: {size, gz, version, sha256,
  sources}}}`); `status()` compares existence → size →
  sha256 → `Missing/Outdated/Current`. Pure logic, unit-tested.
- `Gzip` — system-zlib gunzip: header parsed/stripped, raw
  deflate, ISIZE checked (CRC left to the sha256 check).
- `DataChecker` / `DataDownloader` (`QObject`, async NAM, no
  threads): check manifest → per-file rows (Missing/Outdated/
  Current + Download/Update/disabled); download streams to
  `.gz.part`, gunzips, sha256-verifies, atomically renames;
  progress 0–1000; required files batch under one button.
- `DataDialog` (code-built, modeless, `WA_DeleteOnClose`):
  rows per file (name + status + action), progress bar hidden
  unless downloading (retained size, no relayout), `dataChanged`
  signal → main window reloads dictionary + recognizer model.

## 6. Rust boundary and dictionary access

### libdict (Rust) — what C++ consumes

- One normalizer everywhere (NFC, katakana→hiragana fold for
  keys, full→half-width; no dakuten folding).
- Artifacts: versioned envelope + string pool + flat tables +
  trigram postings; mmap + lazy decode; pre-ranked rows.
- Deinflection: ~382 rules, BFS over surfaces (visited set,
  depth cap 5), states union verb-class claims; final exact
  lookup requires `entry.classes ∩ state ≠ ∅`. Reason names
  copy zkanji (`conjunctive`, `past`, `potential / passive`,
  `causative`, …). Rules bucketed by final kana of `from`
  (longest-first). Deliberately excluded: prefix honorifics,
  compound-verb boundaries, particles, derivational suffixes,
  dialects, classical rules, empty-surface candidates.
- Katakana→romaji display tables; furigana segmentation
  (greedy outside-in + backtracking); romaji→kana query edge
  (longest-token-first, `n`/geminate lookahead, infallible,
  always hiragana).
- Search tiers: exact (+deinflected, one frequency bucket) >
  prefix > suffix; `is_common` first, `ent_seq` tiebreak.
  Budgets: exact < 100 µs, affix < 1 ms, contains < 10 ms.

### `DictStore` — global handle owner (C++, `shared_mutex`)

`loadDictionary` / `loadKanji` swap live UniFFI handles
(reloads never strand); queries copy the `shared_ptr` under a
shared lock then run lock-free: `searchIndices`,
`searchMeaningIndices`, `entryAt`, `searchMiddle` (Words tab),
`lookupKanji`, `kanjiStrokes` (pen space 0–255),
`kanjiContaining` (kradfile order), `similarKanji` (excludes
self). Errors become `false`/empty; never unwind.

### `DictWorker` + `DictionaryModel` (`QAbstractTableModel`, 3 cols)

- Worker lives on its own `QThread`; `load/search/
  searchMeaning/loadParts/loadPartOf` slots echo a `generation`;
  the model drops stale replies.
- `loadAsync(path)` swaps the dictionary (scope switch);
  `search`/`searchMeaning` no-op until ready; `clear()` always
  allowed. Cap is effectively uncapped; layout interpolates.
- Rows decode lazily at paint (`entry_at`): heading/reading
  prefer the form containing the query;
  `articleFor(row)` builds the full `ArticleDisplay` fresh each
  call (heading, reading, JLPT, kanji, readings, senses with
  `stagk/stagr`, priorities, xrefs).
- Roles: `entSeq`, `SensesRole` (pos/misc/field + tips, gloss,
  xrefs, only-kana/reading flags), `DeinflectedRole`,
  `InflectionRole` (reason chain), `MatchRole` (matched
  surface), `KanaOnlyRole` (`uk` marker).
- `MeaningBuilder` turns senses into paint runs (`Infl/Number/
  Pos/Misc/Uk/Gloss/Field/Gap`) with shared elision math;
  `KanjiInfo` serves kanji cards, strokes, parts/part-of,
  similarity (via the recognizer, excluding self), and all
  kanjidic detail fields.

## 7. Recognizer (`librecognizer` + `src/core/recognizer/`)

Classical two-stage matcher (zkanji lineage): per-stroke segment-
angle DTW + 16-bit relative-position layout, fused per candidate
(greedy stroke assignment, ±1 order window, ±2 layout neighbors,
±5% size prior, missing-stroke fines). Trajectory-native, pen
space 0–255 (`u8`); KanjiVG sources scaled at parse; full-bank
match ~8 ms. Bank builds deterministically (rayon map + ordered
sort ⇒ bit-identical at any thread count).

C++ side: `Recognizer` (thin wrapper over generated bindings;
`loadModel` reads the file, strokes cross per call as nested
vectors, hits return best-first with fused distances) and
`RecognizerWorker` on a dedicated match thread with a generation
counter (stale replies dropped). `MODEL` overrides the bank path.

## 8. Dictionary UI (`src/ui/dictionary/`)

`DictionaryWidget` (central widget, `.ui` + 4 `.cpp` by concern)
owns the search row (prefix/suffix toggles, handwriting ✎
button, kana input, meaning/ja toggle, scope combo), the
`QSplitter` (results table + article pane + status label), the
article close chip, and the `kanjiDialog` (lazy).

Search flow (200 ms debounce): raw text → (meaning mode ? raw :
`KanaInput::searchText`) → empty clears; model not ready parks
as `pendingQuery`; else `search` or `searchMeaning` by mode.
Mode switch clears the field. Row select → `articleFor(row)`
into `ArticleView`; first show sizes the splitter from memorized
article width or thirds; splitter moves persist. Close chip
clears selections. Links and kanji double-clicks route to search
or the kanji dialog. Scope switch reverts if the file is
missing, parks the query, and `loadAsync`s the new binary.

## 9. Article pane (`src/widgets/text/`)

`ArticleView` (`QScrollArea`) builds widgets, not HTML: heading
`RubyLabel`, reading `RunLabel`, badge row (Common word / JLPT),
one `RunLabel` per sense, other-forms blocks. `RunLabel` paints
through `QTextLayout` (formats installed first, lines re-placed
after — `setFormats` clears laid-out lines): inks Body/Dim/
Green(pos)/Red(`uk`→"kana")/Link, href underlines, selection
via `QPalette` roles, press-drag selection, tag tooltips after
the shared delay. `RubyLabel` paints furigana spans with
`QPainter` (centered columns, keb-mapped selection so romaji
display never leaks into copies). `ArticleRuns` maps model
structs to runs (`queryFor` drops `・n` tails).

Span selection: press anchors a sense; drags extend through
intermediate senses (gap positions clamp to the line under the
cursor); heading drags never span; Ctrl+C copies the whole span
once (the view's filter runs before labels' own copy).

## 10. Results table (`src/widgets/table/`)

`DictionaryTable : QTableView` (per-pixel scroll, fixed row
height, single-row select, mouse tracking): press-drag text
selection in columns 0–1 (clamped to the origin cell),
double-click emits raw char (opener filters kana), Ctrl+C /
Esc, hover row + link-color cursor, native tag tooltips after
the shared delay, `pinCurrentToText` keeps the 6000px meaning
column from yanking the scrollbar. `DictionaryTableDelegate`
paints single-line rows with primitives: body text, selection
invert, green query-match span, and the meaning cell from
`MeaningBuilder` runs on a shared baseline. `Colors` resolves
hover wash (Highlight α32), deinflected tint (green α28), and
kana-only fading from the live palette. Column widths persist
in `MemorizedState`.

## 11. Handwriting panel (`src/ui/recognizer/`, `src/widgets/canvas/`, `src/widgets/list/`)

`RecognizerWidget` is a plain child of the central widget
(deliberately no `Qt::Window` flag — a parented native window
would escape to top-level under Wayland). Hidden by default;
opens under the search button; size persists, position always
re-anchors; hides on click-away, focus loss, or its close
button.

- `CanvasWidget`: capture in pen space (`strokeStarted/Moved/
  Ended`, >1.2px moves, right-click clears); paints paper,
  1px grid, finished ink + active stroke (amber), help text.
- `Strokes.cpp`: open-stroke capture, undo/redo as vector
  moves, clear.
- `Candidates.cpp`: `onRecognize` (topK 20, kana toggle live)
  → worker; `onRecognized` drops stale generations →
  `CandidateList::setHits`; status line (templates, strokes,
  ms); click accepts (inserts at cursor + searches),
  second click within the double-click interval opens kanji
  info.
- `CandidateList` (`QListWidget`, horizontal, uniform cells):
  kana hits get vivid container tint (katakana blue / hiragana
  green, inverted when active), scores on hover, scrollable
  edge fades (`EdgeFade`, click-through gradients), smooth
  180 ms wheel scrolling, vertical wheel never scrolls.
- `Drag.cpp` + `PanelSize.h`: corner grip resizes with
  square-canvas snap math (pure, unit-tested); toolbar gaps
  are the drag handle.
- Chrome (`paintEvent`): card fill + 1px border + 8px radius,
  triangular grip glyph; drop shadow; close/grid/kana/
  undo/redo/clear buttons (40px, `ButtonIcon` SVG re-tint on
  theme change).

## 12. Kanji dialog, hover, tooltips

- `KanjiDialog` (modeless): stroke-order player
  (`KanjiStrokeView`: fitted square, 4×4 grid, round caps,
  numbered badges with compass-ring placement, 1–10 speed,
  frame-advance off the screen rate), detail runs (meanings,
  readings, grade/JLPT/frequency, radicals, codes), words tab
  (own `DictionaryModel` + shared delegate), similar/parts/
  part-of strips (worker + generation guard, retarget on
  click). State persists per strip.
- `KanjiHover`: one shared card per top-level window; resolves
  the kanji under the cursor in table / article / candidate
  strip (viewport-coord hit-testing, offscreen-safe), re-hits
  on scroll/selection/model change, suppresses during drags.
- `KanjiTooltip` (400px card: glyph, meanings, readings,
  strokes/radical, grade/JLPT) and `Tooltip` base (200 ms show,
  400 ms hide grace, cursor-clamped placement, palette-aware
  restyle).

## 13. Kana input (`src/widgets/input/KanaInput`)

Event filter holding a raw romaji buffer behind the display:
longest-token-first Hepburn + Kunrei accepted, `n`-lookahead,
geminate lookahead, fallthrough raw; always hiragana out.
Backspace deletes the last display character whole (never
un-converts); a pending partial mora is modal (only backspace
and conversion keystrokes land, mouse presses are swallowed).
`hasPending` gates searches so half-typed morae never query.

## 14. Self-updater (`src/ui/update/`)

- `UpdateChecker`: skipped entirely when
  `BAKAJITEN_UPDATE_ARTIFACT` is empty (menu item hidden too).
  Otherwise polls `releases/latest`, compares with
  `QVersionNumber`, honors the skipped version, matches the
  per-kind asset (`appimage→.AppImage`, `win64→.exe` +
  `win-x86_64.zip`, `linux-static→.tar.gz`,
  `macos-arm64→macos-aarch64.zip`, `macos-x64→macos-x86_64.zip`).
- `UpdatePrompt` (has-artifact Download/Later vs notice
  Close-only) with per-version opt-out; `UpdateProgress`
  streams to `.part`, verifies size, atomically renames,
  chmods +x, reports speed/ETA.
- `UpdateInstaller` replaces the running app in place: Unix
  swaps the file/bundle (macOS with rollback) and relaunches;
  Windows stages + a `tasklist`-gated `update.cmd` finishes
  after exit; unwritable installs run the staged copy.
- `Archive` (`src/util/`): in-house zip (stored + raw-deflate
  via zlib, CRC-checked, symlink-aware, `__MACOSX` skipped)
  and tar (ustar, pax skipped, `..`/absolute refused) reader
  plus materializer — no system `tar`/`ditto` needed.

## 15. Palette and icons

`MaterialPalette` (header-only) is the single color authority:
text/dimmed/link/warning inks (fixed blues/red per theme —
style Link roles go near-white on dark themes), selection,
hover wash, deinflected tint, kana spots, badges (blue→green
HSL ramp), canvas paper/grid/ink (paper flag, not palette),
edge-fade ends. `ButtonIcon` re-tints bundled SVGs
(`currentColor` replaced at pixmap time) on theme switch.

## 16. Packaging and install layout

`cmake --install` → `usr/bin/bakajiten` + desktop file +
icons; data is never bundled (resolved at runtime from
`/usr/share/bakajiten` or CWD fallbacks). Per-platform static
builds live in `packaging/` (win64 MinGW + embedded CJK font,
linux-static with vendored OpenSSL, AppImage via linuxdeploy,
macOS `.app` via macdeployqt). CI pulls prebuilt GHCR images
and caches ccache dirs per image; release artifacts carry the
per-kind update asset.

## 17. Tests (`ctest`, offscreen, `BAKAJITEN_DATA_DIR` → repo `data/`)

- `data_manifest` (manifest parse/status), `panel_size_math`
  (snap arithmetic), `panel_square_canvas` (real grip drag).
- `main_window_flows`: open/click-away/fades, real strokes,
  table paint/selection/tooltips, article spans/hover/inks,
  settings/persist/scale/romaji/kana/candidates/kanji/update
  suites. GUI rules: Xvfb `:99`, Qt xcb/offscreen as marked,
  kill by exact PID, one test-function arg per invocation,
  shared `QSettings` YAML state cleaned up.
- Rust: engine suites (deinflection chains, normalizer,
  converters, pack roundtrips) run under `cargo test`.

## 18. Dev iteration (`src/dev/`)

Debug builds load Designer `.ui` files at runtime (`QUiLoader`)
instead of compiled uic; a `QFileSystemWatcher` reloads both
content forms ~300 ms after any save (F5 does it manually);
strokes, toggles, and geometry survive. Static reflection
(`UiForm.h`, needs `-freflection`) fills the same `Ui::`
structs release builds use — renaming a widget Just Works on
next F5. Release builds never touch UiTools.

## 19. Replication checklist

1. UniFFI: generate C++ bindings at build time from the
   cdylibs (never check them in); thin wrappers own the bank
   handles behind mutexes; workers on dedicated threads with
   generation counters.
2. Data first: manifest + `.bin.gz` download with sha256 +
   atomic rename; versioned artifacts resolved from the
   settings-adjacent dir with env overrides.
3. Model/view split: tiered search + deinflection in the
   engine, lazy row decode at paint, role-based delegate
   painting, memorized geometry.
4. Article as widgets (ruby + run labels), not HTML; selection
   and hover resolved in viewport coordinates.
5. Panel as a plain child widget with snap math; candidates as
   a horizontal list with script-tinted cells.
6. One shared hover card + delay; modeless kanji dialog with
   its own models and workers.
7. Updater: kind-gated check → prompt → verified download →
   in-place replace → restart.
8. Static analysis as build gates (tidy errors fail, format
   autofixes), tests green before finishing.
