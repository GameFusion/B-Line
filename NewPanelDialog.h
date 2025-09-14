
#ifndef NEWPANELDIALOG_H
#define NEWPANELDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QMap>

    class NewPanelDialog : public QDialog {
    Q_OBJECT
public:
    enum class Mode { Create, Edit };
    enum class InsertMode { SplitAtCursor, AfterCurrent, BeforeCurrent };

    explicit NewPanelDialog(double fps, Mode mode = Mode::Create, const QString& existingPanelName = "",
                            double existingDurationMs = 1000.0, QWidget *parent = nullptr);
    QString getPanelName() const;
    double getDurationMs() const;
    InsertMode getInsertMode() const;

private slots:
    void onDurationPresetChanged(const QString &text);
    void validateInput();

private:
    QMap<QString, double> presetMap() const;
    QLineEdit *panelNameInput;
    QComboBox *durationPresetCombo;
    QLineEdit *durationInput;
    QComboBox *insertModeCombo;
    QPushButton *okButton;
    double fps;
    double durationMs;
    Mode mode;
    InsertMode insertMode;

    static QString lastPanelName;
    static QString lastDurationPreset;
    static QString lastInsertMode;
};

#endif // NEWPANELDIALOG_H
