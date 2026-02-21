#ifndef NEWPROJECTDIALOG_H
#define NEWPROJECTDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class NewProjectDialog; }
QT_END_NAMESPACE

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget *parent = nullptr);
    ~NewProjectDialog();

    void setProjectData(const QJsonObject& projectJson);
    void setEditMode(bool isEdit);

    QString projectName() const;
    QString location() const;
    int fps() const;
    QString resolution() const;
    QString safeFrame() const;
    QString estimatedDuration() const;
    QString aspectRatio() const;
    QString projectCode() const;
    QString director() const;
    QString notes() const;


    QString subtitle() const;
    QString episodeFormat() const;
    QString copyright() const;
    QString startTC() const;
    QString canvasMargin() const;
    QString canvasMarginCustom() const;

private slots:
    void browseLocation();

private:
    Ui::NewProjectDialog *ui;
};

#endif // NEWPROJECTDIALOG_H
