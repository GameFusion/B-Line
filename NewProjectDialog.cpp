#include "NewProjectDialog.h"
#include "ui_NewProjectDialog.h"

#include <QFileDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>

namespace {

const QString kCanvasPresetMatchOutput = QStringLiteral("Match Output");
const QString kCanvasPresetOverscan10 = QStringLiteral("Overscan 10%");
const QString kCanvasPresetOverscan20 = QStringLiteral("Overscan 20% (Recommended)");
const QString kCanvasPresetOverscan30 = QStringLiteral("Overscan 30%");
const QString kCanvasPreset1920 = QStringLiteral("Preset 1920x1080");
const QString kCanvasPreset2560 = QStringLiteral("Preset 2560x1440");
const QString kCanvasPreset3840 = QStringLiteral("Preset 3840x2160");
const QString kCanvasPresetCustom = QStringLiteral("Custom");

int parseLeadingInt(const QString& text, bool* okOut = nullptr)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        if (okOut) {
            *okOut = false;
        }
        return 0;
    }

    int index = 0;
    if (trimmed[0] == '+' || trimmed[0] == '-') {
        index = 1;
    }
    while (index < trimmed.size() && trimmed[index].isDigit()) {
        ++index;
    }

    if (index <= 0 || (index == 1 && (trimmed[0] == '+' || trimmed[0] == '-'))) {
        if (okOut) {
            *okOut = false;
        }
        return 0;
    }

    bool ok = false;
    const int value = trimmed.left(index).toInt(&ok);
    if (okOut) {
        *okOut = ok;
    }
    return ok ? value : 0;
}

}

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NewProjectDialog)
{
    ui->setupUi(this);

    connect(ui->browseButton, &QPushButton::clicked, this, &NewProjectDialog::browseLocation);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    ui->labelCanvasMargin->setText(tr("Canvas Size Preset"));
    ui->canvasMarginComboBox->clear();
    ui->canvasMarginComboBox->addItems({
        kCanvasPresetMatchOutput,
        kCanvasPresetOverscan10,
        kCanvasPresetOverscan20,
        kCanvasPresetOverscan30,
        kCanvasPreset1920,
        kCanvasPreset2560,
        kCanvasPreset3840,
        kCanvasPresetCustom
    });
    ui->canvasMarginComboBox->setCurrentText(kCanvasPresetOverscan20);

    // Legacy margin custom input is kept hidden for backward compatibility.
    ui->canvasMarginCustomEdit->setVisible(false);
    ui->canvasMarginCustomEdit->setEnabled(false);

    canvasWidthSpinBox = new QSpinBox(this);
    canvasHeightSpinBox = new QSpinBox(this);
    canvasWidthSpinBox->setRange(1, 16384);
    canvasHeightSpinBox->setRange(1, 16384);
    canvasWidthSpinBox->setSingleStep(16);
    canvasHeightSpinBox->setSingleStep(16);

    QWidget *canvasSizeWidget = new QWidget(this);
    QHBoxLayout *canvasSizeLayout = new QHBoxLayout(canvasSizeWidget);
    canvasSizeLayout->setContentsMargins(0, 0, 0, 0);
    canvasSizeLayout->setSpacing(6);
    canvasSizeLayout->addWidget(canvasWidthSpinBox);
    canvasSizeLayout->addWidget(new QLabel(QStringLiteral("x"), canvasSizeWidget));
    canvasSizeLayout->addWidget(canvasHeightSpinBox);

    ui->formLayout->insertRow(7, tr("Canvas Size (px)"), canvasSizeWidget);

    connect(ui->canvasMarginComboBox, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        applyCanvasPreset(text);
    });

    connect(ui->resolutionComboBox, &QComboBox::currentTextChanged, this, [this](const QString&) {
        const QString preset = ui->canvasMarginComboBox->currentText();
        if (preset == kCanvasPresetMatchOutput ||
            preset == kCanvasPresetOverscan10 ||
            preset == kCanvasPresetOverscan20 ||
            preset == kCanvasPresetOverscan30) {
            applyCanvasPreset(preset);
        }
    });

    connect(canvasWidthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!updatingCanvasControls) {
            syncCanvasPresetFromSize();
        }
    });

    connect(canvasHeightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!updatingCanvasControls) {
            syncCanvasPresetFromSize();
        }
    });

    applyCanvasPreset(ui->canvasMarginComboBox->currentText());
}

NewProjectDialog::~NewProjectDialog()
{
    delete ui;
}

void NewProjectDialog::browseLocation()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Project Folder"), QString(),
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        ui->locationEdit->setText(dir);
    }
}

QString NewProjectDialog::projectName() const
{
    return ui->projectNameEdit->text();
}

QString NewProjectDialog::location() const
{
    return ui->locationEdit->text();
}

int NewProjectDialog::fps() const
{
    return ui->fpsComboBox->currentText().toInt();
}

QString NewProjectDialog::resolution() const
{
    return ui->resolutionComboBox->currentText();
}

QString NewProjectDialog::safeFrame() const
{
    return ui->safeFrameComboBox->currentText();
}

QString NewProjectDialog::estimatedDuration() const
{
    return ui->estimatedDurationEdit->text();
}

QString NewProjectDialog::aspectRatio() const
{
    return ui->aspectRatioComboBox->currentText();
}

QString NewProjectDialog::projectCode() const
{
    return ui->projectCodeEdit->text();
}

QString NewProjectDialog::director() const
{
    return ui->directorEdit->text();
}

QString NewProjectDialog::notes() const
{
    return ui->notesEdit->toPlainText();
}

QString NewProjectDialog::subtitle() const
{
    return ui->subtitleEdit->text();
}

QString NewProjectDialog::episodeFormat() const
{
    return ui->episodeFormatComboBox->currentText();
}

QString NewProjectDialog::copyright() const
{
    return ui->copyrightEdit->text();
}

QString NewProjectDialog::startTC() const
{
    return ui->startTCEdit->text();
}

QString NewProjectDialog::canvasPreset() const
{
    return ui->canvasMarginComboBox->currentText();
}

int NewProjectDialog::canvasWidth() const
{
    const int outputW = outputResolutionFromUi().width();
    const int width = canvasWidthSpinBox ? canvasWidthSpinBox->value() : outputW;
    return qMax(outputW, width);
}

int NewProjectDialog::canvasHeight() const
{
    const int outputH = outputResolutionFromUi().height();
    const int height = canvasHeightSpinBox ? canvasHeightSpinBox->value() : outputH;
    return qMax(outputH, height);
}

QString NewProjectDialog::canvasMargin() const
{
    return ui->canvasMarginComboBox->currentText();
}

QString NewProjectDialog::canvasMarginCustom() const
{
    return ui->canvasMarginCustomEdit->text();
}

void NewProjectDialog::setProjectData(const QJsonObject& projectJson) {
    // Populate fields from projectJson
    ui->projectNameEdit->setText(projectJson["projectName"].toString());
    ui->locationEdit->setText(projectJson["projectPath"].toString());  // Add projectPath to projectJson later
    ui->fpsComboBox->setCurrentText(QString::number(projectJson["fps"].toInt()));
    ui->resolutionComboBox->setCurrentText(
        QString("%1x%2").arg(projectJson["resolution"].toArray()[0].toInt())
            .arg(projectJson["resolution"].toArray()[1].toInt()));
    ui->safeFrameComboBox->setCurrentText(projectJson["safeFrame"].toString());
    ui->estimatedDurationEdit->setText(projectJson["estimatedDuration"].toString());
    ui->aspectRatioComboBox->setCurrentText(projectJson["aspectRatio"].toString());
    ui->projectCodeEdit->setText(projectJson["projectCode"].toString());
    ui->directorEdit->setText(projectJson["director"].toString());
    ui->notesEdit->setPlainText(projectJson["notes"].toString());
    ui->subtitleEdit->setText(projectJson["subtitle"].toString());
    ui->episodeFormatComboBox->setCurrentText(projectJson["episode_format"].toString());
    ui->copyrightEdit->setText(projectJson["copyright"].toString());
    ui->startTCEdit->setText(projectJson["start_tc"].toString());

    const QSize output = outputResolutionFromUi();
    int canvasW = output.width();
    int canvasH = output.height();

    if (projectJson.contains("canvas") && projectJson["canvas"].isArray()) {
        const QJsonArray canvasArray = projectJson["canvas"].toArray();
        if (canvasArray.size() >= 2) {
            canvasW = qMax(output.width(), canvasArray[0].toInt(output.width()));
            canvasH = qMax(output.height(), canvasArray[1].toInt(output.height()));
        }
    } else if (projectJson.contains("canvas_percent") || projectJson.contains("canvas_margin")) {
        int marginPct = 0;
        if (projectJson.contains("canvas_percent")) {
            marginPct = qMax(0, projectJson["canvas_percent"].toInt());
        } else {
            const QString marginText = projectJson["canvas_margin"].toString().trimmed();
            bool ok = false;
            marginPct = qMax(0, parseLeadingInt(marginText, &ok));
            if (!ok) {
                marginPct = 0;
            }
        }

        canvasW = qMax(output.width(), qRound(output.width() * (1.0 + marginPct / 100.0)));
        canvasH = qMax(output.height(), qRound(output.height() * (1.0 + marginPct / 100.0)));
    }

    updatingCanvasControls = true;
    canvasWidthSpinBox->setValue(canvasW);
    canvasHeightSpinBox->setValue(canvasH);
    updatingCanvasControls = false;

    const QString savedPreset = projectJson["canvas_preset"].toString();
    const int presetIndex = ui->canvasMarginComboBox->findText(savedPreset);
    if (presetIndex >= 0) {
        ui->canvasMarginComboBox->setCurrentIndex(presetIndex);
    } else {
        syncCanvasPresetFromSize();
    }
}

void NewProjectDialog::setEditMode(bool isEdit) {
    if (isEdit) {
        setWindowTitle(tr("Edit Project"));
        ui->locationEdit->setEnabled(false);  // Disable location editing
        ui->browseButton->setEnabled(false);
    } else {
        setWindowTitle(tr("Create New Project"));
        ui->locationEdit->setEnabled(true);
        ui->browseButton->setEnabled(true);
    }
}

QSize NewProjectDialog::outputResolutionFromUi() const
{
    const QString resString = ui->resolutionComboBox->currentText().trimmed().toLower();
    const QStringList resParts = resString.split('x');
    if (resParts.size() != 2) {
        return QSize(1920, 1080);
    }

    const int width = qMax(1, resParts[0].toInt());
    const int height = qMax(1, resParts[1].toInt());
    return QSize(width, height);
}

void NewProjectDialog::applyCanvasPreset(const QString& preset)
{
    if (!canvasWidthSpinBox || !canvasHeightSpinBox) {
        return;
    }

    const QSize output = outputResolutionFromUi();
    int targetWidth = canvasWidthSpinBox->value();
    int targetHeight = canvasHeightSpinBox->value();

    if (preset == kCanvasPresetMatchOutput) {
        targetWidth = output.width();
        targetHeight = output.height();
    } else if (preset == kCanvasPresetOverscan10) {
        targetWidth = qRound(output.width() * 1.10);
        targetHeight = qRound(output.height() * 1.10);
    } else if (preset == kCanvasPresetOverscan20) {
        targetWidth = qRound(output.width() * 1.20);
        targetHeight = qRound(output.height() * 1.20);
    } else if (preset == kCanvasPresetOverscan30) {
        targetWidth = qRound(output.width() * 1.30);
        targetHeight = qRound(output.height() * 1.30);
    } else if (preset == kCanvasPreset1920) {
        targetWidth = 1920;
        targetHeight = 1080;
    } else if (preset == kCanvasPreset2560) {
        targetWidth = 2560;
        targetHeight = 1440;
    } else if (preset == kCanvasPreset3840) {
        targetWidth = 3840;
        targetHeight = 2160;
    } else if (preset == kCanvasPresetCustom) {
        return;
    }

    targetWidth = qMax(output.width(), targetWidth);
    targetHeight = qMax(output.height(), targetHeight);

    updatingCanvasControls = true;
    canvasWidthSpinBox->setValue(targetWidth);
    canvasHeightSpinBox->setValue(targetHeight);
    updatingCanvasControls = false;
}

void NewProjectDialog::syncCanvasPresetFromSize()
{
    if (!canvasWidthSpinBox || !canvasHeightSpinBox) {
        return;
    }

    const QSize output = outputResolutionFromUi();
    const QSize canvas(canvasWidth(), canvasHeight());

    QString matchedPreset = kCanvasPresetCustom;

    if (canvas == output) {
        matchedPreset = kCanvasPresetMatchOutput;
    } else if (canvas.width() == qRound(output.width() * 1.10) &&
               canvas.height() == qRound(output.height() * 1.10)) {
        matchedPreset = kCanvasPresetOverscan10;
    } else if (canvas.width() == qRound(output.width() * 1.20) &&
               canvas.height() == qRound(output.height() * 1.20)) {
        matchedPreset = kCanvasPresetOverscan20;
    } else if (canvas.width() == qRound(output.width() * 1.30) &&
               canvas.height() == qRound(output.height() * 1.30)) {
        matchedPreset = kCanvasPresetOverscan30;
    } else if (canvas.width() == 1920 && canvas.height() == 1080) {
        matchedPreset = kCanvasPreset1920;
    } else if (canvas.width() == 2560 && canvas.height() == 1440) {
        matchedPreset = kCanvasPreset2560;
    } else if (canvas.width() == 3840 && canvas.height() == 2160) {
        matchedPreset = kCanvasPreset3840;
    }

    if (ui->canvasMarginComboBox->currentText() == matchedPreset) {
        return;
    }

    updatingCanvasControls = true;
    ui->canvasMarginComboBox->setCurrentText(matchedPreset);
    updatingCanvasControls = false;
}
