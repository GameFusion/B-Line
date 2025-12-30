#include "NewProjectDialog.h"
#include "ui_NewProjectDialog.h"

#include <QFileDialog>
#include <QJsonObject>
#include <QJsonArray>

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NewProjectDialog)
{
    ui->setupUi(this);

    connect(ui->browseButton, &QPushButton::clicked, this, &NewProjectDialog::browseLocation);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    ui->canvasMarginCustomEdit->setVisible(false);

    connect(ui->canvasMarginComboBox, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        bool isCustom = (text == "Custom");
        ui->canvasMarginCustomEdit->setVisible(isCustom);
        //ui->canvasMarginCustomEdit
        if (isCustom) ui->canvasMarginCustomEdit->setFocus();
    });
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

    // Canvas Margin
    QString marginPct = QString::number(projectJson["canvas_percent"].toInt());
    int idx = ui->canvasMarginComboBox->findText(marginPct + "%");
    if (idx >= 0) {
        ui->canvasMarginComboBox->setCurrentIndex(idx);
    } else {
        ui->canvasMarginComboBox->setCurrentText("Custom");
        ui->canvasMarginCustomEdit->setText(marginPct);
        ui->canvasMarginCustomEdit->setVisible(true);
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
