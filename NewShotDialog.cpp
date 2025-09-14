
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

void NewShotDialog::onDurationPresetChanged(const QString &text)
{
    // Map preset strings to durations (in seconds for conversion)
    QMap<QString, double> presetMap = {
        {"2 frames", 2.0 / fps},
        {"3 frames", 3.0 / fps},
        {"4 frames", 4.0 / fps},
        {"6 frames", 6.0 / fps},
        {"8 frames", 8.0 / fps},
        {"12 frames", 12.0 / fps},
        {"16 frames", 16.0 / fps},
        {"20 frames", 20.0 / fps},
        {"1 second", 1.0},
        {"1 second 12 frames", 1.0 + 12.0 / fps},
        {"1 second 20 frames", 1.0 + 20.0 / fps},
        {"1.5 seconds", 1.5},
        {"2 seconds", 2.0},
        {"2 seconds 12 frames", 2.0 + 12.0 / fps},
        {"3 seconds", 3.0},
        {"4 seconds", 4.0}
    };

    // Convert to milliseconds
    durationMs = presetMap.value(text, 2.0) * 1000.0;

    // Update panel count maximum
    int maxPanels = qRound(durationMs * fps / 1000.0); // Max panels = frame count
    panelCountInput->setMaximum(maxPanels);

    // Convert to timecode for durationInput
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
    okButton->setEnabled(true);
}

void NewShotDialog::validateInput()
{
    // Validate shot name
    QString shotName = shotNameInput->text().trimmed();
    bool shotNameValid = !shotName.isEmpty() && !shotName.contains(QRegularExpression("[^a-zA-Z0-9_\\-]"));

    // Validate duration
    QString input = durationInput->text().trimmed();
    QRegularExpression timecodeRegex(R"(^(\d+):(\d{2}):(\d{2}):(\d{2})$)");
    QRegularExpressionMatch match = timecodeRegex.match(input);

    // Map preset strings to milliseconds for comparison
    QMap<QString, double> presetMap = {
        {"2 frames", 2.0 / fps * 1000.0},
        {"3 frames", 3.0 / fps * 1000.0},
        {"4 frames", 4.0 / fps * 1000.0},
        {"6 frames", 6.0 / fps * 1000.0},
        {"8 frames", 8.0 / fps * 1000.0},
        {"12 frames", 12.0 / fps * 1000.0},
        {"16 frames", 16.0 / fps * 1000.0},
        {"20 frames", 20.0 / fps * 1000.0},
        {"1 second", 1.0 * 1000.0},
        {"1 second 12 frames", (1.0 + 12.0 / fps) * 1000.0},
        {"1 second 20 frames", (1.0 + 20.0 / fps) * 1000.0},
        {"1.5 seconds", 1.5 * 1000.0},
        {"2 seconds", 2.0 * 1000.0},
        {"2 seconds 12 frames", (2.0 + 12.0 / fps) * 1000.0},
        {"3 seconds", 3.0 * 1000.0},
        {"4 seconds", 4.0 * 1000.0}
    };

    if (match.hasMatch()) {
        // Parse timecode (HH:MM:SS:FF)
        int h = match.captured(1).toInt();
        int m = match.captured(2).toInt();
        int s = match.captured(3).toInt();
        int f = match.captured(4).toInt();
        if (m < 60 && s < 60 && f < fps) {
            durationMs = (h * 3600 + m * 60 + s) * 1000.0 + f * (1000.0 / fps);
            okButton->setEnabled(durationMs > 0);

            // Check if input matches a preset
            int totalFrames = qRound(durationMs * fps / 1000.0);
            QString timecode = QString("%1:%2:%3:%4")
                                   .arg(h, 2, 10, QChar('0'))
                                   .arg(m, 2, 10, QChar('0'))
                                   .arg(s, 2, 10, QChar('0'))
                                   .arg(f, 2, 10, QChar('0'));
            for (const auto& preset : presetMap.keys()) {
                int presetFrames = qRound(presetMap[preset] * fps / 1000.0);
                int presetH = presetFrames / (3600 * fps);
                presetFrames %= static_cast<int>(3600 * fps);
                int presetM = presetFrames / (60 * fps);
                presetFrames %= static_cast<int>(60 * fps);
                int presetS = presetFrames / fps;
                int presetF = presetFrames % static_cast<int>(fps);
                QString presetTimecode = QString("%1:%2:%3:%4")
                                             .arg(presetH, 2, 10, QChar('0'))
                                             .arg(presetM, 2, 10, QChar('0'))
                                             .arg(presetS, 2, 10, QChar('0'))
                                             .arg(presetF, 2, 10, QChar('0'));
                if (presetTimecode == timecode) {
                    durationPresetCombo->setCurrentText(preset);
                    break;
                }
            }
            return;
        }
    } else {
        // Try parsing as seconds or frames
        bool ok;
        double seconds = input.toDouble(&ok);
        if (ok && seconds > 0) {
            durationMs = seconds * 1000.0; // Assume seconds
            okButton->setEnabled(true);
            // Check if input matches a preset
            for (const auto& preset : presetMap.keys()) {
                if (qAbs(presetMap[preset] - durationMs) < 1.0) { // Tolerance for floating-point
                    durationPresetCombo->setCurrentText(preset);
                    break;
                }
            }
            return;
        }
        int frames = input.toInt(&ok);
        if (ok && frames > 0) {
            durationMs = frames * (1000.0 / fps); // Assume frames
            okButton->setEnabled(true);
            // Check if input matches a preset
            for (const auto& preset : presetMap.keys()) {
                if (qAbs(presetMap[preset] - durationMs) < 1.0) { // Tolerance for floating-point
                    durationPresetCombo->setCurrentText(preset);
                    break;
                }
            }
            return;
        }
    }

    // Update panel count maximum
    int maxPanels = qRound(durationMs * fps / 1000.0); // Max panels = frame count
    panelCountInput->setMaximum(maxPanels);
    if (panelCountInput->value() > maxPanels) {
        panelCountInput->setValue(maxPanels);
    }

    // Enable OK button only if all inputs are valid
    okButton->setEnabled(false);
    durationMs = 0;
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
    return positionCombo->currentText() == "Before Current Shot";
}
