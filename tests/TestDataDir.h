#pragma once

#include <QDir>
#include <QFile>

/// Point `BAKAJITEN_DATA_DIR` at the repo's packed data for tests:
/// searches the dev-tree candidates for `jmdict.bin` when the
/// override is unset, so `ctest` works from any build nesting
/// (CI downloads `data/` to the repo root; local builds sit under
/// `frontends/qt/`). Explicit overrides always win.
inline void pointDataDirAtRepoData() {
    if (qEnvironmentVariableIsSet("BAKAJITEN_DATA_DIR")) {
        return;
    }
    const QString cwd = QDir::currentPath();
    for (const QString &dir :
         {cwd + QStringLiteral("/data/"), cwd + QStringLiteral("/../data/"),
          cwd + QStringLiteral("/../../data/"), cwd + QStringLiteral("/../../../data/")}) {
        if (QFile::exists(dir + QStringLiteral("jmdict.bin"))) {
            qputenv("BAKAJITEN_DATA_DIR", QDir(dir).absolutePath().toUtf8());
            return;
        }
    }
}
