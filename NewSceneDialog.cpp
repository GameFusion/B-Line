#include "NewSceneDialog.h"
#include <QRegularExpression>
#include <QMessageBox>
#include <QDateTime>
#include <QtGlobal>
#include <QHBoxLayout>
#include <cmath> // Added for fmod

// Initialize static variables
QString NewSceneDialog::lastSceneName = "";
QString NewSceneDialog::lastDurationPreset = "1 minute";
int NewSceneDialog::lastShotCount = 1;
QString NewSceneDialog::lastInsertMode = "Append";
bool NewSceneDialog::lastInitDefaultCamera = false;
bool NewSceneDialog::lastDuplicateCameraSettings = false;

QMap<QString, double> NewSceneDialog::presetMap() const
{
    return {
        {"1 seconds", 1.0}, {"2 seconds", 2.0}, {"5 seconds", 5.0}, {"10 seconds", 10.0},
        {"15 seconds", 1.0}, {"20 seconds", 20.0},
        {"30 seconds", 30.0}, {"30 seconds", 40.0}, {"1 minute", 60.0}, {"1.5 minutes", 90.0},
        {"2 minutes", 120.0}, {"3 minutes", 180.0}, {"4 minutes", 240.0}
    };
}

QList<QPair<QString, double>> NewSceneDialog::presetList() const {
    return {
        {"1 seconds", 1.0}, {"2 seconds", 2.0}, {"5 seconds", 5.0}, {"10 seconds", 10.0},
        {"15 seconds", 1.0}, {"20 seconds", 20.0},
        {"30 seconds", 30.0}, {"30 seconds", 40.0}, {"1 minute", 60.0}, {"1.5 minutes", 90.0},
        {"2 minutes", 120.0}, {"3 minutes", 180.0}, {"4 minutes", 240.0}
    };
}

double NewSceneDialog::parseDuration(const QString& input) const
{
    QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return 0.0;
    }

    // Timecode regex: HH:MM:SS(.ff)?
    QRegularExpression re(R"(\d{2}:\d{2}:\d{2}(\.\d+)?)");
    QRegularExpressionMatch match = re.match(trimmed);
    if (match.hasMatch() && match.capturedEnd() == trimmed.length()) {
        int h = trimmed.mid(0, 2).toInt();
        int m = trimmed.mid(3, 2).toInt();
        QString secStr = trimmed.mid(6);
        double s = secStr.toDouble();
        return (h * 3600.0 + m * 60.0 + s) * 1000.0;
    } else {
        bool ok;
        double sec = trimmed.toDouble(&ok);
        if (ok && sec > 0.0) {
            return sec * 1000.0;
        }
    }
    return 0.0;
}

NewSceneDialog::NewSceneDialog(qreal fps, QWidget *parent)
    : QDialog(parent), fps(fps), durationMs(60000.0) // Default 1 minute
{
    setWindowTitle(tr("New Scene"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout;

    // Scene Name
    formLayout->addRow(tr("Scene Name:"), nameEdit = new QLineEdit(lastSceneName.isEmpty() ? "SCENE_" + QDateTime::currentDateTime().toString("yyyyMMddhhmmss") : lastSceneName));
    nameEdit->setPlaceholderText(tr("Enter scene name"));

    // Duration preset
    durationPresetCombo = new QComboBox;

    QStringList presetDurations;
    auto presets = presetList();
    for (const auto &p : presets) {
        presetDurations << p.first;
    }
    durationPresetCombo->addItems(presetDurations);

    int index = durationPresetCombo->findText(lastDurationPreset);
    durationPresetCombo->setCurrentIndex(index != -1 ? index : 1); // Default to "1 minute"
    formLayout->addRow(tr("Preset Duration:"), durationPresetCombo);

    // Duration input
    durationInput = new QLineEdit;
    durationInput->setPlaceholderText(tr("Enter duration (seconds or timecode HH:MM:SS)"));
    formLayout->addRow(tr("Precise Duration:"), durationInput);

    // Insert Mode
    insertModeCombo = new QComboBox;
    insertModeCombo->addItems({"Append to End", "Insert Before Current Scene", "Insert After Current Scene"
                               /*, "Split Current Scene Before Current Shot", "Split Current Scene After Current Shot"*/});
    index = insertModeCombo->findText(lastInsertMode);
    insertModeCombo->setCurrentIndex(index != -1 ? index : 0); // Default to "Append to End"
    formLayout->addRow(tr("Insert Mode:"), insertModeCombo);

    formLayout->addRow(tr("Shots Generation"), new QFrame());
    // Shot Count
    shotCountEdit = new QSpinBox;
    shotCountEdit->setRange(1, 50);
    shotCountEdit->setValue(lastShotCount);
    formLayout->addRow(tr("Shots Count:"), shotCountEdit);

    // Base Shot Duration display
    shotDurationLabel = new QLabel;
    formLayout->addRow(tr("Shot Duration:"), shotDurationLabel);

    // Shot note
    shotNoteLabel = new QLabel;
    formLayout->addRow(QString(), shotNoteLabel);

    // Checkboxes
    initDefaultCheck = new QCheckBox();
    initDefaultCheck->setChecked(lastInitDefaultCamera);
    formLayout->addRow(QString("Initialize with default Camera"), initDefaultCheck);

    duplicateCheck = new QCheckBox();
    duplicateCheck->setChecked(lastDuplicateCameraSettings);
    formLayout->addRow(QString("Duplicate current camera settings"), duplicateCheck);

    // Metadata section
    metadataButton = new QPushButton(tr("Metadata ▶"));
    //mainLayout->addWidget(metadataButton);

    metadataFrame = new QFrame;
    metadataFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout *metaVLayout = new QVBoxLayout(metadataFrame);
    metadataForm = new QFormLayout;
    sluglineEdit = new QLineEdit;
    sluglineEdit->setPlaceholderText(tr("Enter slugline"));
    metadataForm->addRow(tr("Slugline:"), sluglineEdit);

    descriptionEdit = new QTextEdit;
    descriptionEdit->setMaximumHeight(100);
    descriptionEdit->setPlaceholderText(tr("Enter description"));
    metadataForm->addRow(tr("Description:"), descriptionEdit);

    tagsEdit = new QLineEdit;
    tagsEdit->setPlaceholderText(tr("comma-separated tags"));
    metadataForm->addRow(tr("Tags:"), tagsEdit);

    notesEdit = new QTextEdit;
    notesEdit->setMaximumHeight(100);
    notesEdit->setPlaceholderText(tr("Enter notes"));
    metadataForm->addRow(tr("Notes:"), notesEdit);

    metaVLayout->addLayout(metadataForm);
    //mainLayout->addWidget(metadataFrame);
    metadataFrame->hide();

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    okButton = new QPushButton(tr("OK"));
    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(metadataButton);
    mainLayout->addWidget(metadataFrame);
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(durationPresetCombo, &QComboBox::currentTextChanged, this, &NewSceneDialog::onDurationPresetChanged);
    connect(durationInput, &QLineEdit::textChanged, this, &NewSceneDialog::onDurationChanged);
    connect(nameEdit, &QLineEdit::textChanged, this, &NewSceneDialog::validateInput);
    connect(shotCountEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, &NewSceneDialog::onShotCountChanged);
    connect(okButton, &QPushButton::clicked, this, [this]() {
        lastSceneName = nameEdit->text().trimmed();
        lastDurationPreset = durationPresetCombo->currentText();
        lastShotCount = shotCountEdit->value();
        lastInsertMode = insertModeCombo->currentText();
        lastInitDefaultCamera = initDefaultCheck->isChecked();
        lastDuplicateCameraSettings = duplicateCheck->isChecked();
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(metadataButton, &QPushButton::clicked, this, [this]() {
        bool visible = !metadataFrame->isVisible();
        metadataFrame->setVisible(visible);
        metadataButton->setText(visible ? tr("Metadata ▼") : tr("Metadata ▶"));
    });

    onDurationPresetChanged(durationPresetCombo->currentText());
    onShotCountChanged(shotCountEdit->value());
    validateInput();
}

void NewSceneDialog::onDurationPresetChanged(const QString &text)
{
    double presetSec = presetMap().value(text, 60.0);
    durationMs = presetSec * 1000.0;
    int totalMin = static_cast<int>(presetSec / 60.0);
    int totalSec = static_cast<int>(std::fmod(presetSec, 60.0));
    QString timecode = QString("00:%1:%2").arg(totalMin, 2, 10, QChar('0')).arg(totalSec, 2, 10, QChar('0'));
    durationInput->setText(timecode);
    // onDurationChanged will be triggered by setText
}

void NewSceneDialog::onDurationChanged(const QString &)
{
    double d = parseDuration(durationInput->text());
    if (d > 0.0) {
        durationMs = d;
        updateShotDurationDisplay();
    }
    validateInput();
}

void NewSceneDialog::onShotCountChanged(int)
{
    updateShotDurationDisplay();
    validateInput();
}

void NewSceneDialog::updateShotDurationDisplay()
{
    double totalSec = durationMs / 1000.0;
    int totalFrames = qRound(totalSec * fps);
    int n = shotCountEdit->value();
    if (n <= 0) {
        shotDurationLabel->setText("Invalid shot count");
        shotNoteLabel->setText("");
        return;
    }
    int baseFrames = totalFrames / n;
    int remainder = totalFrames % n;
    double baseSec = static_cast<double>(baseFrames) / fps;
    int baseMin = static_cast<int>(baseSec / 60);
    int baseSecInt = static_cast<int>(std::fmod(baseSec, 60.0));
    QString baseTimecode = QString("00:%1:%2").arg(baseMin, 2, 10, QChar('0')).arg(baseSecInt, 2, 10, QChar('0'));
    shotDurationLabel->setText(QString("Base Shot Duration: %1 (%2 frames)").arg(baseTimecode).arg(baseFrames));

    if (remainder == 0) {
        shotNoteLabel->setText(QString("%1 shots, equal duration: %2 frames each.").arg(n).arg(baseFrames));
    } else {
        shotNoteLabel->setText(QString("%1 shots, base duration: %1 frames. The last %2 shot(s) %3 frames (base +1).")
                                   .arg(n).arg(baseFrames).arg(remainder).arg(baseFrames + 1));
    }
}

void NewSceneDialog::validateInput()
{
    QString name = nameEdit->text().trimmed();
    bool nameValid = !name.isEmpty() && !name.contains(QRegularExpression("[^a-zA-Z0-9_\\-]"));

    QString input = durationInput->text().trimmed();
    bool durationValid = parseDuration(input) > 0.0;

    bool shotCountValid = shotCountEdit->value() >= 1;

    okButton->setEnabled(nameValid && durationValid && shotCountValid);
}

QString NewSceneDialog::getSceneName() const { return nameEdit->text(); }
double NewSceneDialog::getDurationMs() const {
    double d = parseDuration(durationInput->text());
    return (d > 0.0) ? d : durationMs;
}
int NewSceneDialog::getShotCount() const { return shotCountEdit->value(); }
NewSceneDialog::InsertMode NewSceneDialog::getInsertMode() const
{
    QString modeText = insertModeCombo->currentText();
    if (modeText == "Insert Before Current Scene") return InsertMode::InsertBefore;
    if (modeText == "Insert After Current Scene") return InsertMode::InsertAfter;
    if (modeText == "Split Current Scene Before Current Shot") return InsertMode::SplitBefore;
    if (modeText == "Split Current Scene After Current Shot") return InsertMode::SplitAfter;
    return InsertMode::Append;
}

bool NewSceneDialog::getInitializeDefaultCamera() const { return initDefaultCheck->isChecked(); }
bool NewSceneDialog::getDuplicateCameraSettings() const { return duplicateCheck->isChecked(); }
QString NewSceneDialog::getSlugline() const { return sluglineEdit->text().trimmed(); }
QString NewSceneDialog::getDescription() const { return descriptionEdit->toPlainText().trimmed(); }
QString NewSceneDialog::getTags() const { return tagsEdit->text().trimmed(); }
QString NewSceneDialog::getNotes() const { return notesEdit->toPlainText().trimmed(); }
