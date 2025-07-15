#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>

#include "ScriptBreakdown.h"

class CameraSidePanel : public QWidget {
    Q_OBJECT
public:
    explicit CameraSidePanel(QWidget* parent = nullptr);

    void setCameraList(const QStringList& cameraNames);
    void selectPanel(const QString& panelName);
    void setCameraList(const QString& panelUuid, const std::vector<GameFusion::CameraFrame>& frames);
    QString selectedCamera() const;

public slots:
    void updateCameraFrame(const GameFusion::CameraFrame& frame);

signals:
    void cameraAdded();
    void cameraDeleted(const QString& name);
    void cameraDuplicated(const QString& name);
    void cameraRenamed(const QString& oldName, const QString& newName);
    void cameraReordered(const QStringList& newOrder);
    void cameraSelected(const QString& name);
    void cameraFrameUpdated(const GameFusion::CameraFrame& frame);

private slots:
    void onAddCamera();
    void onDeleteCamera();
    void onDuplicateCamera();
    void onRenameCamera(QListWidgetItem* item);
    void onSelectionChanged();
    void onItemDropped();
    //void onSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous);



private:

    QListWidget* listWidget;
    QPushButton *addButton, *delButton, *dupButton;
    QString currentPanel;

    void setupUI();
    void setupConnections();
    void startInlineRename(QListWidgetItem* item);

    // Camera attribute widgets
    void populateForm(const GameFusion::CameraFrame& frame);
    void clearForm();

    QFormLayout* formLayout = nullptr;
    QWidget* formContainer = nullptr;
    QMap<QString, GameFusion::CameraFrame> frameMap; // uuid → CameraFrame

    // Input fields
    QLineEdit* nameEdit = nullptr;
    QDoubleSpinBox* xEdit = nullptr;
    QDoubleSpinBox* yEdit = nullptr;
    QDoubleSpinBox* zoomEdit = nullptr;
    QDoubleSpinBox* rotationEdit = nullptr;
    QSpinBox* frameOffset = nullptr;
    //QLabel* uuidLabel=nullptr;
    QString uuidSelected;

    void onAttributeChanged();
};
