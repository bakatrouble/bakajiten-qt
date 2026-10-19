// Dev-only runtime .ui loading. Compiled solely with
// `-DDEV_UILOADER=ON` (see CMakeLists); release builds never
// see QUiLoader.

#include "UiDev.h"

#include "widgets/canvas/CanvasWidget.h"
#include "widgets/list/CandidateList.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/text/ArticleView.h"

#include <QUiLoader>

#include <QDir>
#include <QDirIterator>
#include <QFile>

Q_LOGGING_CATEGORY(uiLoaderLog, "ui.loader");

namespace {

/// QUiLoader that builds our forms at runtime: the behavior-owning
/// `CanvasWidget`, `CandidateList`, `DictionaryTable`, and `ArticleView`
/// for real,
/// every other custom class (the `RecognizerWidget`/`DictionaryWidget`
/// roots) as a plain QWidget. Layouts, properties, and object names
/// all still apply, so the caller can findChild by the same names
/// uic generates members for.
class DevLoader : public QUiLoader {
public:
    QWidget *createWidget(const QString &className, QWidget *parent, const QString &name) override {
        if (className == QLatin1String("CanvasWidget")) {
            auto *w = new CanvasWidget(parent);
            w->setObjectName(name);
            return w;
        }
        if (className == QLatin1String("CandidateList")) {
            auto *w = new CandidateList(parent);
            w->setObjectName(name);
            return w;
        }
        if (className == QLatin1String("DictionaryTable")) {
            auto *w = new DictionaryTable(parent);
            w->setObjectName(name);
            return w;
        }
        if (className == QLatin1String("ArticleView")) {
            auto *w = new ArticleView(parent);
            w->setObjectName(name);
            return w;
        }
        if (className == QLatin1String("RecognizerWidget") ||
            className == QLatin1String("DictionaryWidget")) {
            return new QWidget(parent);
        }
        return QUiLoader::createWidget(className, parent, name);
    }
};

} // namespace

QString uiDir() {
    const QByteArray env = qgetenv("UI_DIR");
    if (!env.isEmpty()) {
        return QString::fromLocal8Bit(env);
    }
#ifdef UI_DIR
    return QStringLiteral(UI_DIR);
#else
    return QDir::currentPath() + QStringLiteral("/src");
#endif
}

QWidget *loadUiForm(const QString &uiFileName, QWidget *parent) {
    const QString path = uiDir() + QLatin1Char('/') + uiFileName;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(uiLoaderLog) << "cannot open" << path << "(set $UI_DIR?)";
        return nullptr;
    }
    DevLoader loader;
    QWidget *form = loader.load(&file, parent);
    if (form == nullptr) {
        qCWarning(uiLoaderLog) << path << "failed:" << loader.errorString();
        return nullptr;
    }
    qCInfo(uiLoaderLog) << "loaded" << path;
    return form;
}

QStringList uiFormFiles() {
    QStringList forms;
    QDirIterator it(uiDir(), {QStringLiteral("*.ui")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        forms << it.next();
    }
    return forms;
}
