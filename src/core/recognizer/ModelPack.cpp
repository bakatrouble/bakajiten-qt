#include "core/recognizer/ModelPack.h"

#include "core/data/DataPaths.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>

QString ModelPacks::rootDir() {
    if (const QByteArray env = qgetenv("MODEL"); !env.isEmpty()) {
        return QString::fromLocal8Bit(env);
    }
    return QDir(DataPaths::directory()).filePath(QStringLiteral("mlkit-ink"));
}

QString ModelPacks::tag() { return QStringLiteral("ja"); }

std::vector<ModelPacks::Pack> ModelPacks::packs() {
    return {
        {"recospec", "qrnn_ja_reco_20191217_fst_20190322_recospec_zip",
         "https://dl.google.com/handwriting/models/qrnn.ja.reco_20191217.fst_20190322.recospec.zip",
         "0cc70076bfeaee959e558ca55f0ae079f2fe5f25", 46002, ".recospec.local", "recospec", 94195},
        {"tflite", "lstm_japanese_tflite_5x144_tflite_20190523_zip",
         "https://dl.google.com/handwriting/models/lstm.japanese.tflite_5x144.tflite.20190523.zip",
         "fc5c5f0a999635dfabb9bebb7a4834cc1b7f075d", 3132494, ".tflite", "network.tflite", 4186912},
    };
}

QString ModelPacks::artifactPath(const Pack &pack) { return artifactPath(rootDir(), pack); }

QString ModelPacks::artifactPath(const QString &root, const Pack &pack) {
    return QDir(root).filePath(QStringLiteral("ja/%1").arg(QString::fromLatin1(pack.roleFile)));
}

QString ModelPacks::zipPath(const Pack &pack) { return zipPath(rootDir(), pack); }

QString ModelPacks::zipPath(const QString &root, const Pack &pack) {
    return QDir(root).filePath(QStringLiteral(".zips/%1.zip").arg(QString::fromLatin1(pack.pack)));
}

QString ModelPacks::safetensorsPath(const QString &root) {
    return QDir(root).filePath(QStringLiteral("ja/network.safetensors"));
}

QString ModelPacks::safetensorsPath() { return safetensorsPath(rootDir()); }

qint64 ModelPacks::safetensorsSize() { return 16567648; }

qint64 ModelPacks::downloadBytes() {
    qint64 total = 0;
    for (const Pack &pack : packs()) {
        total += pack.zipSize;
    }
    return total;
}

bool ModelPacks::isCached() { return QFileInfo(safetensorsPath()).size() == safetensorsSize(); }
