#ifndef NEWSHOTDIALOG_H
#define NEWSHOTDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>

    class NewShotDialog : public QDialog {
    Q_OBJECT
public:
    enum class Mode { Create, Edit };
    explicit NewShotDialog(const double fps, const QString &shotName, QWidget *parent = nullptr);
    explicit NewShotDialog(double fps, Mode mode = Mode::Create, const QString& existingShotName = "",
                           double existingDurationMs = 2000.0, int existingPanelCount = 1, QWidget *parent = nullptr);
    double getDurationMs() const; // Returns duration in milliseconds
    bool isInsertBefore() const; // Returns true if "Before Current Shot" is selected
    QString getShotName() const; // Returns shot name
    int getPanelCount() const; // Returns number of panels

private slots:
    void onDurationPresetChanged(const QString &text);
    void validateInput();

private:
    QMap<QString, double> presetMap() const; // Declare presetMap
    QLineEdit *shotNameInput;
    QComboBox *durationPresetCombo;
    QLineEdit *durationInput;
    QSpinBox *panelCountInput;
    QComboBox *positionCombo;
    QPushButton *okButton;
    double fps; // Project FPS for timecode conversion
    double durationMs; // Stored duration in milliseconds
    Mode mode; // Create or Edit mode

    // Persistent memory (for Create mode only)
    static QString lastShotName;
    static QString lastDurationPreset;
    static int lastPanelCount;
    static QString lastPosition;
};

#endif // NEWSHOTDIALOG_H
