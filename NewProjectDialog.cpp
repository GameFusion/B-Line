#include "NewProjectDialog.h"
#include "ui_NewProjectDialog.h"

#include <QFileDialog>

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NewProjectDialog)
{
    ui->setupUi(this);

    connect(ui->browseButton, &QPushButton::clicked, this, &NewProjectDialog::browseLocation);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
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
