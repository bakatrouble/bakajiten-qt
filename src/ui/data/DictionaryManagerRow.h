#pragma once

#include <QWidget>

namespace Ui {
class DictionaryManagerRow;
}

/// One dictionary-manager list row (file name headline, status
/// lines, action/Remove buttons, its own progress bar). Built from
/// its own Designer form; the dialog maps one per artifact file.
class DictionaryManagerRow : public QWidget {
    Q_OBJECT
public:
    explicit DictionaryManagerRow(QWidget *parent = nullptr);

    void setFileName(const QString &name);
    QString fileName() const;
    void setDataFile(const QString &file);
    QString dataFile() const;
    void setStatus(const QString &text);
    void setAction(const QString &text, bool enabled);
    void setActionEnabled(bool enabled);
    bool isActionComplete() const;
    void setRemoveEnabled(bool enabled);
    void setRemoveVisible(bool visible);
    void setProgressVisible(bool visible);
    void setProgress(int permille);

signals:
    void downloadRequested();
    void removeRequested();

private:
    Ui::DictionaryManagerRow *ui = nullptr;
    QWidget *devForm = nullptr;
    QString file;
    QString artifact;
};
