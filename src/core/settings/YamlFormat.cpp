#include "core/settings/YamlFormat.h"

#include <QIODevice>
#include <QMetaType>
#include <QSettings>
#include <QSize>
#include <QString>
#include <QStringList>

#include <yaml-cpp/yaml.h>

#include <string>

namespace {
/// QSettings storage: keys like `ui/scale` live nested in YAML.
/// Slashes are nesting delimiters on both sides — the reader
/// flattens maps into slash-keys, the writer nests them back.
/// Scalars cross as strings (QSettings coerces on read:
/// `toDouble`/`toInt` accept them, `@Size(W H)` parses back to
/// QSize); sequences cross as string lists. Anything else is
/// skipped on read, stringified on write.
/// `@Size(W H)` (the QSettings INI convention) back to a QSize.
bool parseSize(const QString &text, QSize &size) {
    if (!text.startsWith(QStringLiteral("@Size(")) || !text.endsWith(QChar(')'))) {
        return false;
    }
    const QStringList parts = text.sliced(6, text.size() - 7).split(QChar(' '));
    if (parts.size() != 2) {
        return false;
    }
    bool okW = false, okH = false;
    const int w = parts.at(0).toInt(&okW);
    const int h = parts.at(1).toInt(&okH);
    if (!okW || !okH) {
        return false;
    }
    size = QSize(w, h);
    return true;
}
void flattenYaml(const YAML::Node &node, const QString &prefix, QSettings::SettingsMap &map) {
    for (auto it = node.begin(); it != node.end(); ++it) {
        const QString key =
            prefix.isEmpty()
                ? QString::fromStdString(it->first.as<std::string>())
                : prefix + QChar('/') + QString::fromStdString(it->first.as<std::string>());
        const YAML::Node &child = it->second;
        switch (child.Type()) {
        case YAML::NodeType::Map:
            flattenYaml(child, key, map);
            break;
        case YAML::NodeType::Sequence: {
            QStringList items;
            for (const YAML::Node &entry : child) {
                if (entry.IsScalar()) {
                    items.push_back(QString::fromStdString(entry.Scalar()));
                }
            }
            map[key] = items;
            break;
        }
        case YAML::NodeType::Scalar: {
            const QString text = QString::fromStdString(child.Scalar());
            QSize size;
            map[key] = parseSize(text, size) ? QVariant::fromValue(size) : QVariant(text);
            break;
        }
        case YAML::NodeType::Null:
        case YAML::NodeType::Undefined:
            break;
        }
    }
}

bool readYamlSettings(QIODevice &device, QSettings::SettingsMap &map) {
    YAML::Node node;
    try {
        node = YAML::Load(device.readAll().toStdString());
    } catch (const YAML::Exception &) {
        return true; // corrupt file: empty map, defaults apply
    }
    if (node.IsMap()) {
        flattenYaml(node, QString(), map);
    }
    return true;
}

YAML::Node toYamlNode(const QVariant &value) {
    switch (value.typeId()) {
    case QMetaType::Bool:
        return YAML::Node(value.toBool());
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return YAML::Node(value.toLongLong());
    case QMetaType::Float:
    case QMetaType::Double:
        return YAML::Node(value.toDouble());
    case QMetaType::QStringList: {
        YAML::Node sequence(YAML::NodeType::Sequence);
        for (const QString &item : value.toStringList()) {
            sequence.push_back(item.toStdString());
        }
        return sequence;
    }
    case QMetaType::QSize: {
        const QSize size = value.toSize();
        return YAML::Node(
            QStringLiteral("@Size(%1 %2)").arg(size.width()).arg(size.height()).toStdString());
    }
    default:
        return YAML::Node(value.toString().toStdString());
    }
}

/// Insert value at a slash-separated path, creating maps along it.
/// NOTE: yaml-cpp handles alias their slot, but `operator=` writes
/// *through* instead of reseating — navigating by reassigning a
/// handle (`current = next`) silently flattens everything into the
/// root. So every level re-resolves from its parent with a fresh
/// lookup; no handle is ever assigned after creation.
void insertNested(YAML::Node parent, QStringList parts, const YAML::Node &value) {
    const std::string head = parts.takeFirst().toStdString();
    if (parts.isEmpty()) {
        parent[head] = value;
        return;
    }
    if (!parent[head].IsMap()) {
        parent[head] = YAML::Node(YAML::NodeType::Map);
    }
    insertNested(parent[head], parts, value);
}

bool writeYamlSettings(QIODevice &device, const QSettings::SettingsMap &map) {
    YAML::Node root(YAML::NodeType::Map);
    for (auto it = map.begin(); it != map.end(); ++it) {
        insertNested(root, it.key().split(QChar('/')), toYamlNode(it.value()));
    }
    device.write(QByteArray::fromStdString(YAML::Dump(root)));
    device.write("\n", 1);
    return true;
}
} // namespace

/// Registered once: `yaml` files under QSettings.
QSettings::Format yamlFormat() {
    static const QSettings::Format format =
        QSettings::registerFormat(QStringLiteral("yaml"), readYamlSettings, writeYamlSettings);
    return format;
}
