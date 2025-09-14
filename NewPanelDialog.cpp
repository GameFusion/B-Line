
#include "NewPanelDialog.h"
#include <QRegularExpression>
#include <QMessageBox>
#include <QDateTime>

        QString NewPanelDialog::lastPanelName = "";
QString NewPanelDialog::lastDurationPreset = "1 second";
QString NewPanelDialog::lastInsertMode = "After Current Panel";

QMap<QString, double> NewPanelDialog::presetMap() const
{
    return {
        {"2 frames", 2.0 / fps}, {"3 frames", 3.0 / fps}, {"4 frames", 4.0 / fps},
        {"6 frames", 6.0 / fps}, {"8 frames", 8.0 / fps}, {"12 frames", 12.0 / fps},
        {"1 second", 1.0}, {"1.5 seconds", 1.5}, {"2 seconds", 2.0}
    };
}

NewPanelDialog::NewPanelDialog(double fps, Mode themode, const QString& existingPanelName,
                               double existingDurationMs, QWidget *parent)
    : QDialog(parent), fps(fps), durationMs(existingDurationMs), mode(themode)
{
    setWindowTitle(mode == Mode::Create ? "New Panel" : "Edit Panel");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout;

    // Panel name input
    panelNameInput = new QLineEdit;
    panelNameInput->setText(mode == Mode::Create ?
                                (lastPanelName.isEmpty() ? "PANEL_" + QDateTime::currentDateTime().toString("yyyyMMddhhmmss") : lastPanelName) :
                                existingPanelName);
    panelNameInput->setPlaceholderText("Enter panel name (e.g., PANEL_001)");
    formLayout->addRow("Panel Name:", panelNameInput);

    // Duration preset dropdown
    durationPresetCombo = new QComboBox;
    QStringList presetDurations = {"2 frames", "3 frames", "4 frames", "6 frames", "8 frames",
                                   "12 frames", "1 second", "1.5 seconds", "2 seconds"};
    durationPresetCombo->addItems(presetDurations);
    QString initialPreset = lastDurationPreset;
    if (mode == Mode::Edit) {
        double existingSeconds = existingDurationMs / 1000.0;
        double minDiff = std::numeric_limits<double>::max();
        for (const auto& preset : presetDurations) {
            double presetSeconds = presetMap().value(preset, 1.0);
            double diff = std::abs(presetSeconds - existingSeconds);
            if (diff < minDiff) {
                minDiff = diff;
                initialPreset = preset;
            }
        }
    }
    int index = durationPresetCombo->findText(initialPreset);
    durationPresetCombo->setCurrentIndex(index != -1 ? index : presetDurations.indexOf("1 second"));
    formLayout->addRow("Preset Duration:", durationPresetCombo);

    // Duration input
    durationInput = new QLineEdit;
    durationInput->setPlaceholderText("Enter timecode (HH:MM:SS:FF), seconds, or frames");
    formLayout->addRow("Precise Duration:", durationInput);

    // Insert mode dropdown (only for Create mode)
    insertModeCombo = new QComboBox;
    insertModeCombo->addItems({"Split at Cursor", "After Current Panel", "Before Current Panel"});
    insertModeCombo->setCurrentText(lastInsertMode);
    insertModeCombo->setVisible(mode == Mode::Create);
    formLayout->addRow("Insert Mode:", insertModeCombo);

    // OK and Cancel buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    okButton = new QPushButton("OK");
    QPushButton *cancelButton = new QPushButton("Cancel");
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(durationPresetCombo, &QComboBox::currentTextChanged, this, &NewPanelDialog::onDurationPresetChanged);
    connect(durationInput, &QLineEdit::textEdited, this, &NewPanelDialog::validateInput);
    connect(panelNameInput, &QLineEdit::textEdited, this, &NewPanelDialog::validateInput);
    connect(okButton, &QPushButton::clicked, this, [this]() {
        if (mode == Mode::Create) {
            lastPanelName = panelNameInput->text().trimmed();
            lastDurationPreset = durationPresetCombo->currentText();
            lastInsertMode = insertModeCombo->currentText();
        }
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    onDurationPresetChanged(durationPresetCombo->currentText());
    validateInput();
}

void NewPanelDialog::onDurationPresetChanged(const QString &text)
{
    durationMs = presetMap().value(text, 1.0) * 1000.0;
    int totalFrames = qRound(durationMs * fps / 1000.0);
    int h = totalFrames / (3600 * fps);
    totalFrames %= static_cast<int>(3600 * fps);
    int m = totalFrames / (60 * fps);
    totalFrames %= static_cast<int>(60 * fps);
    int s = totalFrames / fps;
    int f = totalFrames % static_cast<int>(fps);
    QString timecode = QString("%1:%2:%3:%4")
                           .arg(h, 2, 10, QChar('0'))
                           .arg(m, 2, 10, QChar('0'))
                           .arg(s, 2, 10, QChar('0'))
                           .arg(f, 2, 10, QChar('0'));
    durationInput->setText(timecode);
    validateInput();
}

void NewPanelDialog::validateInput()
{
    QString panelName = panelNameInput->text().trimmed();
    bool panelNameValid = !panelName.isEmpty() && !panelName.contains(QRegularExpression("[^a-zA-Z0-9_\\-]"));

    QString input = durationInput->text().trimmed();
    QRegularExpression timecodeRegex(R"(^(\d+):(\d{2}):(\d{2}):(\d{2})$)");
    QRegularExpressionMatch match = timecodeRegex.match(input);

    bool durationValid = false;
    if (match.hasMatch()) {
        int h = match.captured(1).toInt();
        int m = match.captured(2).toInt();
        int s = match.captured(3).toInt();
        int f = match.captured(4).toInt();
        if (m < 60 && s < 60 && f < fps) {
            durationMs = (h * 3600 + m * 60 + s) * 1000.0 + f * (1000.0 / fps);
            durationValid = durationMs > 0;
            QString timecode = QString("%1:%2:%3:%4")
                                   .arg(h, 2, 10, QChar('0'))
                                   .arg(m, 2, 10, QChar('0'))
                                   .arg(s, 2, 10, QChar('0'))
                                   .arg(f, 2, 10, QChar('0'));
            for (const auto& preset : presetMap().keys()) {
                int presetFrames = qRound(presetMap()[preset] * fps / 1000.0);
                QString presetTimecode = QString("00:00:%1:%2")
                                             .arg(presetFrames / static_cast<int>(fps), 2, 10, QChar('0'))
                                             .arg(presetFrames % static_cast<int>(fps), 2, 10, QChar('0'));
                if (presetTimecode == timecode) {
                    durationPresetCombo->setCurrentText(preset);
                    break;
                }
            }
        }
    } else {
        bool ok;
        double seconds = input.toDouble(&ok);
        if (ok && seconds > 0) {
            durationMs = seconds * 1000.0;
            durationValid = true;
            for (const auto& preset : presetMap().keys()) {
                if (qAbs(presetMap()[preset] * 1000.0 - durationMs) < 1.0) {
                    durationPresetCombo->setCurrentText(preset);
                    break;
                }
            }
        } else {
            int frames = input.toInt(&ok);
            if (ok && frames > 0) {
                durationMs = frames * (1000.0 / fps);
                durationValid = true;
                for (const auto& preset : presetMap().keys()) {
                    if (qAbs(presetMap()[preset] * 1000.0 - durationMs) < 1.0) {
                        durationPresetCombo->setCurrentText(preset);
                        break;
                    }
                }
            }
        }
    }

    okButton->setEnabled(panelNameValid && durationValid);
}

QString NewPanelDialog::getPanelName() const
{
    return panelNameInput->text().trimmed();
}

double NewPanelDialog::getDurationMs() const
{
    return durationMs;
}

NewPanelDialog::InsertMode NewPanelDialog::getInsertMode() const
{
    if (mode == Mode::Edit) return InsertMode::AfterCurrent; // Default for edit mode
    QString modeText = insertModeCombo->currentText();
    if (modeText == "Split at Cursor") return InsertMode::SplitAtCursor;
    if (modeText == "Before Current Panel") return InsertMode::BeforeCurrent;
    return InsertMode::AfterCurrent;
}
