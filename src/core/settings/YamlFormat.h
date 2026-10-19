#pragma once

#include <QSettings>

/// `QSettings` YAML storage format (registered once): keys like
/// `ui/scale` live nested in the file, slashes as nesting
/// delimiters on both sides. Scalars cross as strings (QSettings
/// coerces on read); sequences as string lists. Tolerant both
/// ways: corrupt files read empty (defaults apply), unknown types
/// stringify on write.
QSettings::Format yamlFormat();
