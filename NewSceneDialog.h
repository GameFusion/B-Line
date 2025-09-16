#ifndef NEWSCENEDIALOG_H
#define NEWSCENEDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QMap>

#pragma once
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

class NewSceneDialog : public QDialog {
    Q_OBJECT
public:
    enum class InsertMode { Append, InsertBefore, InsertAfter, SplitBefore, SplitAfter };

    explicit NewSceneDialog(qreal fps, QWidget* parent = nullptr);
    QString getSceneName() const;
    double getDurationMs() const;
    int getShotCount() const;
    InsertMode getInsertMode() const;
    bool getInitializeDefaultCamera() const;
    bool getDuplicateCameraSettings() const;
    QString getSlugline() const;
    QString getDescription() const;
    QString getTags() const;
    QString getNotes() const;

private slots:
    void onDurationPresetChanged(const QString &text);
    void onDurationChanged(const QString &text);
    void onShotCountChanged(int value);
    void validateInput();

private:
    double parseDuration(const QString& input) const;
    QMap<QString, double> presetMap() const;
    QList<QPair<QString, double>> presetList() const;
    void updateShotDurationDisplay();
    QLineEdit *nameEdit;
    QComboBox *durationPresetCombo;
    QLineEdit *durationInput;
    QSpinBox *shotCountEdit;
    QComboBox *insertModeCombo;
    QPushButton *okButton;
    QLabel *shotDurationLabel;
    QLabel *shotNoteLabel;
    QCheckBox *initDefaultCheck;
    QCheckBox *duplicateCheck;
    QPushButton *metadataButton;
    QFrame *metadataFrame;
    QFormLayout *metadataForm;
    QLineEdit *sluglineEdit;
    QTextEdit *descriptionEdit;
    QLineEdit *tagsEdit;
    QTextEdit *notesEdit;
    double fps;
    double durationMs;

    static QString lastSceneName;
    static QString lastDurationPreset;
    static int lastShotCount;
    static QString lastInsertMode;
    static bool lastInitDefaultCamera;
    static bool lastDuplicateCameraSettings;
};

#endif // NEWSCENEDIALOG_H
