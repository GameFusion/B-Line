#ifndef NEWSHOTDIALOG_H
#define NEWSHOTDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>


class NewShotDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewShotDialog(const double fps, const QString &shotName, QWidget *parent = nullptr);
    double getDurationMs() const; // Returns duration in milliseconds
    bool isInsertBefore() const; // Returns true if "Before Current Shot" is selected
    QString getShotName() const; // Returns shot name
    int getPanelCount() const; // Returns number of panels

private slots:
    void onDurationPresetChanged(const QString &text);
    void validateInput();

private:
    QLineEdit *shotNameInput;
    QComboBox *durationPresetCombo;
    QLineEdit *durationInput;
    QSpinBox *panelCountInput;
    QComboBox *positionCombo;
    QPushButton *okButton;
    double fps; // Project FPS for timecode conversion
    double durationMs; // Stored duration in milliseconds

    // Persistent memory
    static QString lastShotName; // Last used shot name
    static QString lastDurationPreset; // Last selected preset (timecode string)
    static int lastPanelCount; // Last used panel count
    static QString lastPosition; // Last selected position ("Before" or "After")
};

#endif // NEWSHOTDIALOG_H
