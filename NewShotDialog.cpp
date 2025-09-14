
#include "NewShotDialog.h"
#include <QRegularExpression>
#include <QMessageBox>
#include <QMap>
#include <QDateTime>

// Initialize static variables
QString NewShotDialog::lastShotName = "";
QString NewShotDialog::lastDurationPreset = "6 frames"; // Default 2 seconds
int NewShotDialog::lastPanelCount = 1;
QString NewShotDialog::lastPosition = "After Current Shot";

NewShotDialog::NewShotDialog(const double fps, const QString &shotName, QWidget *parent)
    : QDialog(parent), fps(fps), durationMs(2000.0) // Default 2 seconds
{
    setWindowTitle("New Shot");
    mode = Mode::Create;

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout;

    lastShotName = shotName;

    // Shot name input
    shotNameInput = new QLineEdit;
    shotNameInput->setText(lastShotName.isEmpty() ? "SHOT_" + QDateTime::currentDateTime().toString("yyyyMMddhhmmss") : lastShotName);
    shotNameInput->setPlaceholderText("Enter shot name (e.g., SHOT_010)");
    formLayout->addRow("Shot Name:", shotNameInput);

    // Duration preset dropdown
    durationPresetCombo = new QComboBox;
    QStringList presetDurations = {
        "2 frames",        // 2 frames
        "3 frames",        // 3 frames
        "4 frames",        // 4 frames
        "6 frames",        // 6 frames
        "8 frames",        // 8 frames
        "12 frames",       // 12 frames
        "16 frames",       // 16 frames
        "20 frames",       // 20 frames
        "1 second",        // 1 second
        "1 second 12 frames", // 1 second 12 frames
        "1 second 20 frames", // 1 second 20 frames
        "1.5 seconds",     // 1.5 seconds
        "2 seconds",       // 2 seconds
        "2 seconds 12 frames", // 2 seconds 12 frames
        "3 seconds",       // 3 seconds
        "4 seconds"        // 4 seconds
    };
    durationPresetCombo->addItems(presetDurations);
    // Set to last used preset or default
    int index = durationPresetCombo->findText(lastDurationPreset);
    durationPresetCombo->setCurrentIndex(index != -1 ? index : presetDurations.indexOf("2 seconds"));
    formLayout->addRow("Preset Duration:", durationPresetCombo);

    // Duration input (timecode, seconds, or frames)
    durationInput = new QLineEdit;
    durationInput->setPlaceholderText("Enter timecode (HH:MM:SS:FF), seconds, or frames");
    formLayout->addRow("Precise Duration:", durationInput);

    // Panel count input
    panelCountInput = new QSpinBox;
    panelCountInput->setMinimum(1);
    panelCountInput->setValue(lastPanelCount); // Use last used panel count
    formLayout->addRow("Number of Panels:", panelCountInput);

    // Position dropdown
    positionCombo = new QComboBox;
    positionCombo->addItems({"After Current Shot", "Before Current Shot"});
    positionCombo->setCurrentText(lastPosition); // Use last used position
    formLayout->addRow("Insert Position:", positionCombo);

    // OK and Cancel buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    okButton = new QPushButton("OK");
    QPushButton *cancelButton = new QPushButton("Cancel");
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(durationPresetCombo, &QComboBox::currentTextChanged, this, &NewShotDialog::onDurationPresetChanged);
    connect(durationInput, &QLineEdit::textEdited, this, &NewShotDialog::validateInput);
    connect(shotNameInput, &QLineEdit::textEdited, this, &NewShotDialog::validateInput);
    connect(panelCountInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &NewShotDialog::validateInput);
    connect(okButton, &QPushButton::clicked, this, [this]() {
        // Save last used values on OK
        lastShotName = shotNameInput->text().trimmed();
        lastDurationPreset = durationPresetCombo->currentText();
        lastPanelCount = panelCountInput->value();
        lastPosition = positionCombo->currentText();
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    // Initialize durationMs and durationInput from last preset
    onDurationPresetChanged(durationPresetCombo->currentText());
    validateInput(); // Initial validation
}

QMap<QString, double> NewShotDialog::presetMap() const
{
    return {
        {"2 frames", 2.0 / fps}, {"3 frames", 3.0 / fps}, {"4 frames", 4.0 / fps},
        {"6 frames", 6.0 / fps}, {"8 frames", 8.0 / fps}, {"12 frames", 12.0 / fps},
        {"16 frames", 16.0 / fps}, {"20 frames", 20.0 / fps}, {"1 second", 1.0},
        {"1 second 12 frames", 1.0 + 12.0 / fps}, {"1 second 20 frames", 1.0 + 20.0 / fps},
        {"1.5 seconds", 1.5}, {"2 seconds", 2.0}, {"2 seconds 12 frames", 2.0 + 12.0 / fps},
        {"3 seconds", 3.0}, {"4 seconds", 4.0}
    };
}

NewShotDialog::NewShotDialog(double fps, Mode theMode, const QString& existingShotName,
                             double existingDurationMs, int existingPanelCount, QWidget *parent)
    : QDialog(parent), fps(fps), durationMs(existingDurationMs), mode(theMode)
{
    setWindowTitle(mode == Mode::Create ? "New Shot" : "Edit Shot");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout;

    // Shot name input
    shotNameInput = new QLineEdit;
    shotNameInput->setText(mode == Mode::Create ?
                               (lastShotName.isEmpty() ? "SHOT_" + QDateTime::currentDateTime().toString("yyyyMMddhhmmss") : lastShotName) :
                               existingShotName);
    shotNameInput->setPlaceholderText("Enter shot name (e.g., SHOT_010)");
    formLayout->addRow("Shot Name:", shotNameInput);

    // Duration preset dropdown
    durationPresetCombo = new QComboBox;
    QStringList presetDurations = {
        "2 frames", "3 frames", "4 frames", "6 frames", "8 frames", "12 frames", "16 frames", "20 frames",
        "1 second", "1 second 12 frames", "1 second 20 frames", "1.5 seconds", "2 seconds",
        "2 seconds 12 frames", "3 seconds", "4 seconds"
    };
    durationPresetCombo->addItems(presetDurations);
    // Set to matching preset or last used (for Create) or closest match (for Edit)
    QString initialPreset = lastDurationPreset;
    if (mode == Mode::Edit) {
        int closestFrames = qRound(existingDurationMs * fps / 1000.0);
        QString closestTimecode = QString("00:00:%1:%2")
                                      .arg(closestFrames / static_cast<int>(fps), 2, 10, QChar('0'))
                                      .arg(closestFrames % static_cast<int>(fps), 2, 10, QChar('0'));
        for (const auto& preset : presetDurations) {
            int presetFrames = qRound(presetMap()[preset] * fps / 1000.0);
            QString presetTimecode = QString("00:00:%1:%2")
                                         .arg(presetFrames / static_cast<int>(fps), 2, 10, QChar('0'))
                                         .arg(presetFrames % static_cast<int>(fps), 2, 10, QChar('0'));
            if (presetTimecode == closestTimecode) {
                initialPreset = preset;
                break;
            }
        }
    }
    int index = durationPresetCombo->findText(initialPreset);
    durationPresetCombo->setCurrentIndex(index != -1 ? index : presetDurations.indexOf("2 seconds"));
    formLayout->addRow("Preset Duration:", durationPresetCombo);

    // Duration input (timecode, seconds, or frames)
    durationInput = new QLineEdit;
    durationInput->setPlaceholderText("Enter timecode (HH:MM:SS:FF), seconds, or frames");
    formLayout->addRow("Precise Duration:", durationInput);

    // Panel count input
    panelCountInput = new QSpinBox;
    panelCountInput->setMinimum(1);
    panelCountInput->setValue(mode == Mode::Create ? lastPanelCount : existingPanelCount);
    formLayout->addRow("Number of Panels:", panelCountInput);

    // Position dropdown (only for Create mode)
    positionCombo = new QComboBox;
    positionCombo->addItems({"After Current Shot", "Before Current Shot"});
    positionCombo->setCurrentText(lastPosition);
    positionCombo->setVisible(mode == Mode::Create);
    formLayout->addRow("Insert Position:", positionCombo);

    // OK and Cancel buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    okButton = new QPushButton("OK");
    QPushButton *cancelButton = new QPushButton("Cancel");
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(durationPresetCombo, &QComboBox::currentTextChanged, this, &NewShotDialog::onDurationPresetChanged);
    connect(durationInput, &QLineEdit::textEdited, this, &NewShotDialog::validateInput);
    connect(shotNameInput, &QLineEdit::textEdited, this, &NewShotDialog::validateInput);
    connect(panelCountInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &NewShotDialog::validateInput);
    connect(okButton, &QPushButton::clicked, this, [this]() {
        if (mode == Mode::Create) {
            lastShotName = shotNameInput->text().trimmed();
            lastDurationPreset = durationPresetCombo->currentText();
            lastPanelCount = panelCountInput->value();
            lastPosition = positionCombo->currentText();
        }
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    // Initialize durationMs and durationInput
    onDurationPresetChanged(durationPresetCombo->currentText());
    validateInput();
}



void NewShotDialog::onDurationPresetChanged(const QString &text)
{
    durationMs = presetMap().value(text, 2.0) * 1000.0;
    int maxPanels = qRound(durationMs * fps / 1000.0);
    panelCountInput->setMaximum(maxPanels);
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

void NewShotDialog::validateInput()
{
    QString shotName = shotNameInput->text().trimmed();
    bool shotNameValid = !shotName.isEmpty() && !shotName.contains(QRegularExpression("[^a-zA-Z0-9_\\-]"));

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
            int totalFrames = qRound(durationMs * fps / 1000.0);
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

    int maxPanels = qRound(durationMs * fps / 1000.0);
    panelCountInput->setMaximum(maxPanels);
    if (panelCountInput->value() > maxPanels) {
        panelCountInput->setValue(maxPanels);
    }
    bool panelCountValid = panelCountInput->value() <= maxPanels;

    if (mode == Mode::Edit && panelCountInput->value() < panelCountInput->value()) {
        QMessageBox::warning(this, "Warning", "Reducing panel count may delete existing panels. Proceed with caution.");
    }

    okButton->setEnabled(shotNameValid && durationValid && panelCountValid);
}

QString NewShotDialog::getShotName() const
{
    return shotNameInput->text().trimmed();
}

double NewShotDialog::getDurationMs() const
{
    return durationMs;
}

int NewShotDialog::getPanelCount() const
{
    return panelCountInput->value();
}

bool NewShotDialog::isInsertBefore() const
{
    return mode == Mode::Create && positionCombo->currentText() == "Before Current Shot";
}
