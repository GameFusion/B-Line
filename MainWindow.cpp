


#include "MainWindow.h"
//#include "Database.h"
#include "BezierPath.h"
#include "SceneNode.h"
#include "SceneGraph.h"
#include "AttributeEditor.h"

#include <QTimer>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QMessageBox>
#include <QFileDialog>
#include <QThread>
#include <QButtonGroup>
#include <QInputDialog>

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QUndoStack>

#include <QSettings>

#include "QtUtils.h"

#include "ShotPanelWidget.h"
#include "MainWindowPaint.h"
#include "paintarea.h"
#include "LlamaModel.h"
#include "PromptLogger.h"
#include "BreakdownWorker.h"
#include "ErrorDialog.h"
#include "CameraSidePanel.h"
#include "PerfectScriptWidget.h"
#include "OptionsDialog.h"
#include "ProjectContext.h"
#include "NewShotDialog.h"
#include "NewPanelDialog.h"
#include "NewSceneDialog.h"
#include "ColorPaletteWidget.h"

#include "GameCore.h" // for GameContext->gameTime()
#include "SoundServer.h"
#include "Log.h"

// BEGIN TimeLine related includes
#include <QCheckBox>
#include <QScrollBar>
#include "../TimeLineProject/TimeLineWidget.h"
#include "../TimeLineProject/TimeLineView.h"
#include "../TimeLineProject/CustomSlider.h"
#include "../TimeLineProject/ScrollbarView.h"
#include "../TimeLineProject/Track.h"
#include "../TimeLineProject/Segment.h"
#include "../TimeLineProject/MarkerItem.h"
#include "../TimeLineProject/ShotSegment.h"
#include "../TimeLineProject/AudioSegment.h"
#include "../TimeLineProject/PanelMarker.h"
#include "../TimeLineProject/CursorItem.h"
#include "../TimeLineProject/TimelineOptionsDialog.h"
#include "../TimeLineProject/AudioMeterWidget.h"

// END TimeLine related includes

using namespace GameFusion;


//#include "TimeLineWidget.h"

#include <QUndoCommand>


class InsertSegmentCommand : public QUndoCommand {
public:
    InsertSegmentCommand(MainWindow* mainWindow, ShotIndices shotIndices, const GameFusion::Shot& shot, const GameFusion::Scene& scene, const QString& sceneUuid, double cursorTime)
        : mainWindow_(mainWindow), shot_(shot), sceneUuid_(sceneUuid), cursorTime_(cursorTime), scene_(scene), shotIndices_(shotIndices) {
        setText("Insert Shot Segment");
    }

    void undo() override {
        ShotContext shotContext = mainWindow_->findShotByUuid(shot_.uuid);
        if(shotContext.isValid()) {
            mainWindow_->deleteShotSegment(shotContext, cursorTime_);
        }
    }

    void redo() override {
        CursorItem* sceneMarker=nullptr;
        mainWindow_->insertShotSegment(shot_, shotIndices_, scene_, cursorTime_, sceneMarker);
    }

private:
    MainWindow* mainWindow_;
    GameFusion::Shot shot_; // shot for restore
    GameFusion::Scene scene_; // state of scene for restore
    QString sceneUuid_;
    double cursorTime_;
    ShotIndices shotIndices_;
};

class DeleteSegmentCommand : public QUndoCommand {
public:
    DeleteSegmentCommand(MainWindow* mainWindow, const GameFusion::Shot& shot, const QString& sceneUuid, const double cursorTime)
        : mainWindow_(mainWindow), shot_(shot), sceneUuid_(sceneUuid), cursorTime_(cursorTime) {
        setText("Delete Shot Segment");
    }

    void undo() override {
        CursorItem* sceneMarker=nullptr;
        mainWindow_->insertShotSegment(shot_, shotIndices_, scene_, cursorTime_, sceneMarker);
    }

    void redo() override {
        ShotContext shotContext = mainWindow_->findShotByUuid(shot_.uuid);
        if(shotContext.isValid()){
            scene_ = *shotContext.scene;
            shotIndices_ = mainWindow_->deleteShotSegment(shotContext, cursorTime_);
        }
    }

private:
    MainWindow* mainWindow_;
    GameFusion::Shot shot_; // shot for restore
    GameFusion::Scene scene_; // state of scene for restore
    QString sceneUuid_;
    double cursorTime_;
    ShotIndices shotIndices_;
};

class EditShotCommand : public QUndoCommand {
public:
    EditShotCommand(MainWindow* mainWindow, const GameFusion::Shot& oldShot, const GameFusion::Shot& newShot, double cursorTime)
        : mainWindow_(mainWindow), oldShot_(oldShot), newShot_(newShot), cursorTime_(cursorTime)
    {
        setText("Edit Shot");
    }
    void undo() override {
        mainWindow_->editShotSegment(oldShot_, cursorTime_);
    }

    void redo() override {
        mainWindow_->editShotSegment(newShot_, cursorTime_);
    }

private:
    MainWindow* mainWindow_;
    GameFusion::Shot oldShot_;
    GameFusion::Shot newShot_;
    double cursorTime_;
};

class DuplicateShotCommand : public QUndoCommand {
public:
    DuplicateShotCommand(MainWindow* mainWindow, const GameFusion::Shot& shot, ShotIndices shotIndices,
                         const GameFusion::Scene& scene, double cursorTime)
        : mainWindow_(mainWindow), shot_(shot), shotIndices_(shotIndices), scene_(scene),
        cursorTime_(cursorTime)
    {
        setText("Duplicate Shot");
    }
    void undo() override {
        ShotContext shotContext = mainWindow_->findShotByUuid(shot_.uuid);
        if (shotContext.isValid()) {
            mainWindow_->deleteShotSegment(shotContext, cursorTime_);
            GameFusion::Log().info() << "Undid duplicate of shot " << shot_.uuid.c_str();
        } else {
            GameFusion::Log().warning() << "Shot not found for UUID " << shot_.uuid.c_str();
        }
    }

    void redo() override {
        CursorItem *sceneMarker = nullptr;
        mainWindow_->insertShotSegment(shot_, shotIndices_, scene_, cursorTime_, sceneMarker);
        GameFusion::Log().info() << "Duplicated shot " << shot_.uuid.c_str() << " at segment index " << shotIndices_.segmentIndex;
    }

private:
    MainWindow* mainWindow_;
    GameFusion::Shot shot_;
    ShotIndices shotIndices_;
    GameFusion::Scene scene_;
    double cursorTime_;
};


class RenameShotCommand : public QUndoCommand {
public:
    RenameShotCommand(MainWindow* mainWindow, const QString& shotUuid, const QString& sceneUuid,
                      const QString& oldName, const QString& newName)
        : mainWindow_(mainWindow), shotUuid_(shotUuid), sceneUuid_(sceneUuid),
        oldName_(oldName), newName_(newName)
    {
        setText("Rename Shot");
    }
    void undo() override{
        mainWindow_->renameShotSegment(shotUuid_, oldName_);
    }

    void redo() override{
        mainWindow_->renameShotSegment(shotUuid_, newName_);
    }

private:
    MainWindow* mainWindow_;
    QString shotUuid_;
    QString sceneUuid_;
    QString oldName_;
    QString newName_;
    double cursorTime_;
};

class PanelCommand : public QUndoCommand {
public:
    PanelCommand(MainWindow* mainWindow, const GameFusion::Shot& oldShot, const GameFusion::Shot& newShot,
                               double cursorTime, const QString& commandName)
        : mainWindow_(mainWindow), oldShot_(oldShot), newShot_(newShot), cursorTime_(cursorTime)
    {
        setText(commandName);
    }

    void undo() override
    {
        bool updateTime = text() == "Insert Panel";
        mainWindow_->editShotSegment(oldShot_, cursorTime_, updateTime);
    }

    void redo() override
    {
        bool updateTime = text() == "Insert Panel";
        mainWindow_->editShotSegment(newShot_, cursorTime_, updateTime);
    }

private:
    MainWindow* mainWindow_;
    GameFusion::Shot oldShot_;
    GameFusion::Shot newShot_;
    double cursorTime_;
};

class InsertSceneCommand : public QUndoCommand {
public:
    InsertSceneCommand(MainWindow* mainWindow, GameFusion::Scene& newScene,
                 const QString sceneRefUuid, bool insertAfter, double cursorTime, const QString& commandName)
        : mainWindow_(mainWindow), newScene_(newScene), sceneRefUuid_(sceneRefUuid), insertAfter_(insertAfter), cursorTime_(cursorTime){
        setText(commandName);
    }
    void undo() override {
        mainWindow_->deleteScene(newScene_.uuid.c_str(), cursorTime_);
    }
    void redo() override {
        mainWindow_->insertScene(newScene_, sceneRefUuid_, insertAfter_, cursorTime_);
    }

private:
    MainWindow* mainWindow_;
    GameFusion::Scene newScene_;
    QString sceneRefUuid_;
    bool insertAfter_;
    double cursorTime_;
};

class DeleteSceneCommand : public QUndoCommand {
public:
    DeleteSceneCommand(MainWindow* mainWindow, GameFusion::Scene& deleteScene,
                 const QString sceneRefUuid, bool insertAfter, double cursorTime, const QString& commandName)
        : mainWindow_(mainWindow), deleteScene_(deleteScene), sceneRefUuid_(sceneRefUuid), insertAfter_(insertAfter), cursorTime_(cursorTime)
    {
        setText(commandName);
    }
    void undo() override {
        mainWindow_->insertScene(deleteScene_, sceneRefUuid_, insertAfter_, cursorTime_);
    }
    void redo() override {


        mainWindow_->deleteScene(deleteScene_.uuid.c_str(), cursorTime_);
    }

private:
    MainWindow* mainWindow_;
    GameFusion::Scene deleteScene_;
    QString sceneRefUuid_;
    bool insertAfter_;
    double cursorTime_;
};

class RenameSceneCommand : public QUndoCommand {
public:
    RenameSceneCommand(MainWindow* mainWindow, QString uuid, QString oldName, QString newName, double cursorTime, QString commandName="Rename Scene")
        : mainWindow_(mainWindow), uuid_(uuid), oldName_(oldName), newName_(newName), cursorTime_(cursorTime)
    {
        setText(commandName);
    }
    void undo() override {
        mainWindow_->renameScene(uuid_, newName_, oldName_, cursorTime_);
    }
    void redo() override {


        mainWindow_->renameScene(uuid_, oldName_, newName_, cursorTime_);
    }

private:
    MainWindow* mainWindow_;
    QString uuid_;
    QString oldName_;
    QString newName_;
    double cursorTime_;
};

class SceneCommand : public QUndoCommand {
public:
    SceneCommand(MainWindow* mainWindow, const std::vector<GameFusion::Scene>& oldScenes,
                 const std::vector<GameFusion::Scene>& newScenes, const QString& commandName);
    void undo() override;
    void redo() override;

private:
    MainWindow* mainWindow_;
    GameFusion::Scene oldScenes_;
    GameFusion::Scene newScenes_;
    bool insertAfter_;
    double cursorTime_;
};

class DuplicateSceneCommand : public QUndoCommand {
public:
    DuplicateSceneCommand(MainWindow* mainWindow, GameFusion::Scene& duplicatedScene,
                          const QString& sceneRefUuid, bool insertAfter, double cursorTime, const QString& commandName)
        : mainWindow_(mainWindow), duplicatedScene_(duplicatedScene), sceneRefUuid_(sceneRefUuid),
          insertAfter_(insertAfter), cursorTime_(cursorTime)
    {
        setText(commandName);
    }

    void undo() override {
        // Undo: Delete the duplicated scene (using the existing delete mechanism)
        mainWindow_->deleteScene(duplicatedScene_.uuid.c_str(), cursorTime_);
    }

    void redo() override {
        // Redo: Insert the duplicated scene
        mainWindow_->insertScene(duplicatedScene_, sceneRefUuid_, insertAfter_, cursorTime_);
    }

private:
    MainWindow* mainWindow_;
    GameFusion::Scene duplicatedScene_;  // Copy of the scene to duplicate
    QString sceneRefUuid_;
    bool insertAfter_;
    double cursorTime_;
};

class AddCameraCommand : public QUndoCommand {
public:
    AddCameraCommand(MainWindow* mainWindow, const GameFusion::CameraFrame& cameraframe, double cursorTime, QString commandName="New Camera Frame")
        : mainWindow_(mainWindow), cameraframe_(cameraframe), cursorTime_(cursorTime) {
        setText(commandName);
    }

    void redo() override {
        mainWindow_->newCamera(cameraframe_, cursorTime_);
    }

    void undo() override {
        mainWindow_->deleteCamera(cameraframe_.uuid.c_str(), cursorTime_);
    }

private:
    MainWindow* mainWindow_;
    //PanelContext panelContext_;
    GameFusion::CameraFrame cameraframe_;
    double cursorTime_;
};

class RenameCameraCommand : public QUndoCommand {
public:
    RenameCameraCommand(MainWindow* mainWindow, const QString& cameraUuid,
                       const QString& oldName, const QString& newName, double cursorTime,
                       const QString& commandName = "Rename Camera")
        : mainWindow_(mainWindow), cameraUuid_(cameraUuid),
          oldName_(oldName), newName_(newName), cursorTime_(cursorTime) {
        setText(commandName);
    }

    void redo() override {
        mainWindow_->renameCamera(cameraUuid_, newName_, cursorTime_);
    }

    void undo() override {
        mainWindow_->renameCamera(cameraUuid_, oldName_, cursorTime_);
    }

private:
    MainWindow* mainWindow_;
    QString cameraUuid_;
    QString oldName_;
    QString newName_;
    double cursorTime_;
};

// Undo camera edits like position change
class UpdateCameraCommand : public QUndoCommand {
public:
    UpdateCameraCommand(MainWindow* mainWindow, ScriptBreakdown* breakdown, const GameFusion::CameraFrame& oldFrame,
                       const GameFusion::CameraFrame& newFrame, const QString& uuid, QUndoCommand* parent = nullptr)
        : QUndoCommand(parent), m_mainWindow(mainWindow), m_breakdown(breakdown), m_oldFrame(oldFrame), m_newFrame(newFrame), m_uuid(uuid) {
        setText("Update Camera Frame");
    }

    void undo() override {
        m_mainWindow->setCamera(m_oldFrame, m_oldFrame.uuid.c_str(), false);
    }

    void redo() override {
        m_mainWindow->setCamera(m_newFrame, m_newFrame.uuid.c_str(), false);
    }

private:
    MainWindow* m_mainWindow;
    ScriptBreakdown* m_breakdown;
    GameFusion::CameraFrame m_oldFrame;
    GameFusion::CameraFrame m_newFrame;
    QString m_uuid;
};


class SplitSceneCommand : public QUndoCommand {
public:
    SplitSceneCommand(MainWindow* mainWindow, const QString& sceneUuid,
                      const QString& shotUuid, bool splitBefore,
                      const QString& newSceneName, GameFusion::Scene mergedScene, double cursorTime)
        : mainWindow_(mainWindow),
          sceneUuid_(sceneUuid),
          shotUuid_(shotUuid),
          splitBefore_(splitBefore),
          newSceneName_(newSceneName),
          mergedScene_(mergedScene),
          cursorTime_(cursorTime)
    {
        setText("Split Scene");
    }

    void undo() override {
        // Merge scenes back together
        mainWindow_->mergeScenes(sceneUuid_, newSceneUuid_, splitBefore_, mergedScene_, cursorTime_);
    }

    void redo() override {
        // Actually perform the split
        newSceneUuid_ = mainWindow_->splitScene(sceneUuid_, shotUuid_, splitBefore_, newSceneName_, cursorTime_);
    }

private:
    MainWindow* mainWindow_;
    QString sceneUuid_;
    QString shotUuid_;
    bool splitBefore_;
    QString newSceneName_;
    GameFusion::Scene mergedScene_;
    double cursorTime_;

    QString newSceneUuid_; // stored for undo
};

// Custom undo command for keyframe updates
class KeyframeUpdateCommand : public QUndoCommand {
public:
    KeyframeUpdateCommand(const QString& kfUuid, double oldTimeMs, const QVariantMap& oldValue, double newTimeMs, const QVariantMap& newValue, const QString& layerUuid, const QString& panelUuid, const QString& shotUuid, MainWindow* mainWindow)
        : m_kfUuid(kfUuid), m_oldTimeMs(oldTimeMs), m_oldValue(oldValue), m_newTimeMs(newTimeMs), m_newValue(newValue), m_layerUuid(layerUuid), m_panelUuid(panelUuid), m_shotUuid(shotUuid), m_mainWindow(mainWindow) {
        setText("Update Keyframe Edit");
    }

    void undo() override {
        m_mainWindow->updateKeyframe(m_kfUuid, m_oldTimeMs, m_oldValue, m_layerUuid, m_panelUuid, m_shotUuid);
    }

    void redo() override {
        m_mainWindow->updateKeyframe(m_kfUuid, m_newTimeMs, m_newValue, m_layerUuid, m_panelUuid, m_shotUuid);
    }

private:
    QString m_kfUuid;
    double m_oldTimeMs;
    QVariantMap m_oldValue;
    double m_newTimeMs;
    QVariantMap m_newValue;
    QString m_layerUuid;
    QString m_panelUuid;
    QString m_shotUuid;
    MainWindow* m_mainWindow;
};

// --- undo command for layer keyframe edits from paint area (a la Preston Blair)
class LayerKeyFrameCommand : public QUndoCommand {
public:
    LayerKeyFrameCommand(const QString& layerUuid, const QString& panelUuid, int keyFrameIndex,
                         double oldX, double oldY, double newX, double newY, MainWindow* mainWindow)
        : m_layerUuid(layerUuid), m_panelUuid(panelUuid), m_keyFrameIndex(keyFrameIndex),
          m_oldX(oldX), m_oldY(oldY), m_newX(newX), m_newY(newY), m_mainWindow(mainWindow) {
        setText("Change Keyframe Position");
    }

    void undo() override {
        m_mainWindow->setKeyFramePosition(m_layerUuid, m_panelUuid, m_keyFrameIndex, m_oldX, m_oldY);
    }

    void redo() override {
        m_mainWindow->setKeyFramePosition(m_layerUuid, m_panelUuid, m_keyFrameIndex, m_newX, m_newY);
    }

private:
    QString m_layerUuid;
    QString m_panelUuid;
    int m_keyFrameIndex;
    double m_oldX, m_oldY;
    double m_newX, m_newY;
    MainWindow* m_mainWindow;
};

class LayerPositionCommand : public QUndoCommand {
public:
    LayerPositionCommand(const QString& layerUuid, const QString& panelUuid,
                         double oldX, double oldY, double newX, double newY, MainWindow* mainWindow)
        : m_layerUuid(layerUuid), m_panelUuid(panelUuid),
          m_oldX(oldX), m_oldY(oldY), m_newX(newX), m_newY(newY), m_mainWindow(mainWindow) {
        setText("Change Layer Position");
    }

    void undo() override {
        m_mainWindow->setLayerPosition(m_layerUuid, m_panelUuid, m_oldX, m_oldY);
    }

    void redo() override {
        m_mainWindow->setLayerPosition(m_layerUuid, m_panelUuid, m_newX, m_newY);
    }

private:
    QString m_layerUuid;
    QString m_panelUuid;
    double m_oldX, m_oldY;
    double m_newX, m_newY;
    MainWindow* m_mainWindow;
};

// Custom undo command for keyframe add
class KeyframeAddCommand : public QUndoCommand {
public:
    KeyframeAddCommand(const QString& kfUuid, double time, const QVariantMap& value, const QString& layerUuid, const QString& panelUuid, const QString& shotUuid, MainWindow* mainWindow, double cursorTime)
        : m_kfUuid(kfUuid), m_time(time), m_value(value), m_layerUuid(layerUuid), m_panelUuid(panelUuid), m_shotUuid(shotUuid), m_mainWindow(mainWindow), m_corsorTime(cursorTime) {
        setText("Add Keyframe");
    }

    void undo() override {
        m_mainWindow->deleteKeyframe(m_kfUuid, m_time, m_value, m_layerUuid, m_panelUuid, m_shotUuid, m_corsorTime); // todo implement in MainWindow
    }

    void redo() override {
        m_mainWindow->addKeyframe(m_kfUuid, m_time, m_value, m_layerUuid, m_panelUuid, m_shotUuid, m_corsorTime); // todo implement in MainWindow
    }

private:
    QString m_kfUuid;
    double m_time;
    QVariantMap m_value;
    QString m_layerUuid;
    QString m_panelUuid;
    QString m_shotUuid;
    MainWindow* m_mainWindow;
    double m_corsorTime;
};

class LayerReorderCommand : public QUndoCommand {
public:
    LayerReorderCommand(const std::vector<QString>& oldUuidOrder,
                        const std::vector<QString>& newUuidOrder,
                        const QString& panelUuid, MainWindow* mainWindow)
        : m_oldUuidOrder(oldUuidOrder), m_newUuidOrder(newUuidOrder), m_panelUuid(panelUuid), m_mainWindow(mainWindow) {
        setText("Reorder Layers");
    }

    void undo() override {
        m_mainWindow->applyLayerOrder(m_oldUuidOrder, m_panelUuid);
    }

    void redo() override {
        m_mainWindow->applyLayerOrder(m_newUuidOrder, m_panelUuid);
    }

private:
    std::vector<QString> m_oldUuidOrder;
    std::vector<QString> m_newUuidOrder;
    QString m_panelUuid;
    MainWindow* m_mainWindow;
};

class LayerAddCommand : public QUndoCommand {
public:
    LayerAddCommand(const GameFusion::Layer& layer, const QString& panelUuid, MainWindow* mainWindow)
        : m_layer(layer), m_panelUuid(panelUuid), m_mainWindow(mainWindow) {
        setText("Add Layer");
    }

    void undo() override {
        m_mainWindow->removeLayer(QString::fromStdString(m_layer.uuid), m_panelUuid);
    }

    void redo() override {
        m_mainWindow->addLayer(m_layer, m_panelUuid);
    }

private:
    GameFusion::Layer m_layer;
    QString m_panelUuid;
    MainWindow* m_mainWindow;
};

class LayerDeleteCommand : public QUndoCommand {
public:
    LayerDeleteCommand(const GameFusion::Layer& layer, int index, const QString& panelUuid, MainWindow* mainWindow)
        : m_layer(layer), m_index(index), m_panelUuid(panelUuid), m_mainWindow(mainWindow) {
        setText("Delete Layer");
    }

    void undo() override {
        m_mainWindow->addLayer(m_layer, m_panelUuid);
    }

    void redo() override {
        m_mainWindow->removeLayer(QString::fromStdString(m_layer.uuid), m_panelUuid);
    }

private:
    GameFusion::Layer m_layer;
    int m_index;
    QString m_panelUuid;
    MainWindow* m_mainWindow;
};


class InstanceLayerCommand : public QUndoCommand {
public:
    InstanceLayerCommand(const QString& originalLayerUuid, const QString& instanceLayerUuid,
                         const QString& panelUuid, MainWindow* mainWindow)
        : m_originalLayerUuid(originalLayerUuid), m_instanceLayerUuid(instanceLayerUuid),
          m_panelUuid(panelUuid), m_mainWindow(mainWindow) {
        setText("Instance Layer");
    }

    void undo() override {
        m_mainWindow->removeLayer(m_instanceLayerUuid, m_panelUuid);
    }

    void redo() override {
        LayerContext layerContext = m_mainWindow->findLayerByUuid(m_originalLayerUuid.toStdString().c_str());
        if (!layerContext.isValid()) return;

        GameFusion::Layer instanceLayer = *layerContext.layer;
        instanceLayer.uuid = m_instanceLayerUuid.toStdString();
        instanceLayer.name = layerContext.layer->name + " (Instance)";
        instanceLayer.sourceUuid = m_originalLayerUuid.toStdString();
        instanceLayer.layers.clear(); // Instances don't have nested layers

        m_mainWindow->addLayer(instanceLayer, m_panelUuid);
    }

private:
    QString m_originalLayerUuid;
    QString m_instanceLayerUuid;
    QString m_panelUuid;
    MainWindow* m_mainWindow;
};

class CopyPasteLayerCommand : public QUndoCommand {
public:
    CopyPasteLayerCommand(const GameFusion::Layer& layer, const QString& panelUuid, bool asAlias, MainWindow* mainWindow)
        : m_layer(layer), m_panelUuid(panelUuid), m_asAlias(asAlias), m_mainWindow(mainWindow) {
        setText(asAlias ? "Paste Layer as Alias" : "Paste Layer as Duplicate");
    }

    void undo() override {
        m_mainWindow->removeLayer(QString::fromStdString(m_layer.uuid), m_panelUuid);
    }

    void redo() override {
        m_mainWindow->addLayer(m_layer, m_panelUuid);
    }

private:
    GameFusion::Layer m_layer;
    QString m_panelUuid;
    bool m_asAlias;
    MainWindow* m_mainWindow;
};

class RenameLayerCommand : public QUndoCommand {
public:
    RenameLayerCommand(const QString& layerUuid, const QString& oldName, const QString& newName,
                       bool propagateToAliases, const QString& panelUuid, MainWindow* mainWindow)
        : m_layerUuid(layerUuid), m_oldName(oldName), m_newName(newName),
          m_propagateToAliases(propagateToAliases), m_panelUuid(panelUuid), m_mainWindow(mainWindow) {
        setText("Rename Layer");
    }

    void undo() override {
        m_mainWindow->renameLayer(m_layerUuid, m_oldName, m_propagateToAliases);
    }

    void redo() override {
        m_mainWindow->renameLayer(m_layerUuid, m_newName, m_propagateToAliases);
    }

private:
    QString m_layerUuid;
    QString m_oldName;
    QString m_newName;
    bool m_propagateToAliases;
    QString m_panelUuid;
    MainWindow* m_mainWindow;
};

class GroupLayersCommand : public QUndoCommand {
public:
    GroupLayersCommand(const std::vector<QString>& layerUuids, const QString& groupUuid,
                       const QString& panelUuid, MainWindow* mainWindow)
        : m_layerUuids(layerUuids), m_groupUuid(groupUuid), m_panelUuid(panelUuid), m_mainWindow(mainWindow) {
        setText("Group Layers");
    }

    void undo() override {
        m_mainWindow->ungroupLayers(m_groupUuid, m_panelUuid);
    }

    void redo() override {
        m_mainWindow->groupLayers(m_layerUuids, m_panelUuid);
    }

private:
    std::vector<QString> m_layerUuids;
    QString m_groupUuid;
    QString m_panelUuid;
    MainWindow* m_mainWindow;
};

class LayerFXCommand : public QUndoCommand {
public:
    LayerFXCommand(const QString& layerUuid, const QString& panelUuid, const std::string& oldFX, const std::string& newFX, MainWindow* mainWindow)
        : m_layerUuid(layerUuid), m_panelUuid(panelUuid), m_oldFX(oldFX), m_newFX(newFX), m_mainWindow(mainWindow) {
        setText("Change Layer FX");
    }

    void undo() override {
        m_mainWindow->setLayerFX(m_layerUuid, m_panelUuid, m_oldFX);
    }

    void redo() override {
        m_mainWindow->setLayerFX(m_layerUuid, m_panelUuid, m_newFX);
    }

private:
    QString m_layerUuid;
    QString m_panelUuid;
    std::string m_oldFX;
    std::string m_newFX;
    MainWindow* m_mainWindow;
};


class LayerLoadImageCommand : public QUndoCommand {
public:
    LayerLoadImageCommand(const QString& layerUuid, const QString& panelUuid, const std::string& oldImageFilePath, const std::string& newImageFilePath, MainWindow* mainWindow)
        : m_layerUuid(layerUuid), m_panelUuid(panelUuid), m_oldImageFilePath(oldImageFilePath), m_newImageFilePath(newImageFilePath), m_mainWindow(mainWindow) {
        setText("Load Layer Image");
    }

    void undo() override {
        m_mainWindow->setLayerImage(m_layerUuid, m_panelUuid, m_oldImageFilePath);
    }

    void redo() override {
        m_mainWindow->setLayerImage(m_layerUuid, m_panelUuid, m_newImageFilePath);
    }

private:
    QString m_layerUuid;
    QString m_panelUuid;
    std::string m_oldImageFilePath;
    std::string m_newImageFilePath;
    MainWindow* m_mainWindow;
};


class LayerAttributeCommand : public QUndoCommand {
public:

    LayerAttributeCommand(const QString& layerUuid, const QString& panelUuid,
                         MainWindow::LayerAttributeType attrType,
                         const std::variant<int, double, GameFusion::BlendMode>& oldValue,
                         const std::variant<int, double, GameFusion::BlendMode>& newValue,
                         MainWindow* mainWindow)
        : m_layerUuid(layerUuid), m_panelUuid(panelUuid), m_attrType(attrType),
          m_oldValue(oldValue), m_newValue(newValue), m_mainWindow(mainWindow) {
        switch (attrType) {
            case MainWindow::LayerAttributeType::Rotation: setText("Change Layer Rotation"); break;
            case MainWindow::LayerAttributeType::PosX: setText("Change Layer X Position"); break;
            case MainWindow::LayerAttributeType::PosY: setText("Change Layer Y Position"); break;
            case MainWindow::LayerAttributeType::Scale: setText("Change Layer Scale"); break;
            case MainWindow::LayerAttributeType::BlendMode: setText("Change Layer Blend Mode"); break;
        }
    }

    void undo() override {
        m_mainWindow->setLayerAttribute(m_layerUuid, m_panelUuid, m_attrType, m_oldValue);
    }

    void redo() override {
        m_mainWindow->setLayerAttribute(m_layerUuid, m_panelUuid, m_attrType, m_newValue);
    }

private:
    QString m_layerUuid;
    QString m_panelUuid;
    MainWindow::LayerAttributeType m_attrType;
    std::variant<int, double, GameFusion::BlendMode> m_oldValue;
    std::variant<int, double, GameFusion::BlendMode> m_newValue;
    MainWindow* m_mainWindow;
};

class LayerDuplicateCommand : public QUndoCommand {
public:
    LayerDuplicateCommand(const QString& originalLayerUuid, const QString& duplicatedLayerUuid,
                          const QString& panelUuid, MainWindow* mainWindow)
        : m_originalLayerUuid(originalLayerUuid), m_duplicatedLayerUuid(duplicatedLayerUuid),
          m_panelUuid(panelUuid), m_mainWindow(mainWindow) {
        setText("Duplicate Layer");
    }

    void undo() override {
        m_mainWindow->removeLayer(m_duplicatedLayerUuid, m_panelUuid);
    }

    void redo() override {
        m_mainWindow->duplicateLayer(m_originalLayerUuid, m_duplicatedLayerUuid);
    }

private:
    QString m_originalLayerUuid;
    QString m_duplicatedLayerUuid;
    QString m_panelUuid;
    MainWindow* m_mainWindow;
};


class LayerPaintCommand : public QUndoCommand {
public:
    LayerPaintCommand(const GameFusion::Layer& originalLayer, const GameFusion::Layer& modifiedLayer,
                     const QString& layerUuid, const QString& panelUuid, MainWindow* mainWindow)
        : m_originalLayer(originalLayer), m_modifiedLayer(modifiedLayer),
          m_layerUuid(layerUuid), m_panelUuid(panelUuid), m_mainWindow(mainWindow) {
        setText("Paint Stroke");
    }

    void undo() override {
        m_mainWindow->updateLayer(m_layerUuid, m_panelUuid, m_originalLayer);
    }

    void redo() override {
        m_mainWindow->updateLayer(m_layerUuid, m_panelUuid, m_modifiedLayer);
    }

private:
    GameFusion::Layer m_originalLayer;
    GameFusion::Layer m_modifiedLayer;
    QString m_layerUuid;
    QString m_panelUuid;
    MainWindow* m_mainWindow;
};

class LayerPaintErasedStrokes : public QUndoCommand {
public:
    LayerPaintErasedStrokes(const GameFusion::Layer& originalLayer, const GameFusion::Layer& modifiedLayer,
                     const QString& layerUuid, const QString& panelUuid, MainWindow* mainWindow)
        : m_originalLayer(originalLayer), m_modifiedLayer(modifiedLayer),
          m_layerUuid(layerUuid), m_panelUuid(panelUuid), m_mainWindow(mainWindow) {
        setText("Erase Stroke");
    }

    void undo() override {
        m_mainWindow->updateLayer(m_layerUuid, m_panelUuid, m_originalLayer);
    }

    void redo() override {
        m_mainWindow->updateLayer(m_layerUuid, m_panelUuid, m_modifiedLayer);
    }

private:
    GameFusion::Layer m_originalLayer;
    GameFusion::Layer m_modifiedLayer;
    QString m_layerUuid;
    QString m_panelUuid;
    MainWindow* m_mainWindow;
};

class ClearLayerCommand : public QUndoCommand {
public:
    ClearLayerCommand(const GameFusion::Layer& originalLayer, const GameFusion::Layer& clearedLayer,
                      const QString& layerUuid, const QString& panelUuid, MainWindow* mainWindow)
        : m_originalLayer(originalLayer), m_clearedLayer(clearedLayer),
          m_layerUuid(layerUuid), m_panelUuid(panelUuid), m_mainWindow(mainWindow) {
        setText("Clear Layer");
    }

    void undo() override {
        m_mainWindow->updateLayer(m_layerUuid, m_panelUuid, m_originalLayer);
    }

    void redo() override {
        m_mainWindow->updateLayer(m_layerUuid, m_panelUuid, m_clearedLayer);
    }

private:
    GameFusion::Layer m_originalLayer;
    GameFusion::Layer m_clearedLayer;
    QString m_layerUuid;
    QString m_panelUuid;
    MainWindow* m_mainWindow;
};

class CompositeUndoCommand : public QUndoCommand {
public:
    CompositeUndoCommand(const QString& text) {
        setText(text);
    }

    void addCommand(QUndoCommand* command) {
        m_commands.append(command);
    }

    void undo() override {
        for (int i = m_commands.size() - 1; i >= 0; --i) {
            m_commands[i]->undo();
        }
    }

    void redo() override {
        for (QUndoCommand* cmd : m_commands) {
            cmd->redo();
        }
    }

private:
    QList<QUndoCommand*> m_commands;
};

class FontAwesomeViewer : public QWidget
{
public:

    static QFont fontAwesomeSolid;

    FontAwesomeViewer(QWidget *parent = nullptr) : QWidget(parent)
    {
        // Load Font Awesome font
        int fontId = QFontDatabase::addApplicationFont(":/fa-solid-900.ttf");
        //int fontId = QFontDatabase::addApplicationFont(":/fa-brands-400.ttf");
        if (fontId == -1) {
            qWarning() << "Failed to load Font Awesome font.";
            return;
        }

        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        if (fontFamilies.isEmpty()) {
            qWarning() << "No font families found.";
            return;
        }

        fontAwesomeSolid = fontFamilies.at(0);
        fontAwesomeSolid.setPointSize(12);

        QWidget *containerWidget = new QWidget(this);  // Container widget for the scroll area
        QVBoxLayout *layout = new QVBoxLayout(containerWidget);

        // Iterate through a range of Unicode values
        for (int codePoint = 0xf000; codePoint <= 0xf3ff; ++codePoint) {
            QString iconText = QString(QChar(codePoint));
            QLabel *iconLabel = new QLabel(this);
            iconLabel->setFont(fontAwesomeSolid);
            iconLabel->setText(iconText + " " + QString::number(codePoint, 16).toUpper());
            layout->addWidget(iconLabel);
        }

        containerWidget->setLayout(layout);

        QScrollArea *scrollArea = new QScrollArea(this);
        scrollArea->setWidget(containerWidget);
        scrollArea->setWidgetResizable(true);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(scrollArea);
        setLayout(mainLayout);

        setWindowTitle("Font Awesome Icons Viewer");
        resize(400, 600);
    }
};

QFont FontAwesomeViewer::fontAwesomeSolid;

TimeLineView* createTimeLine(QWidget &parent, MainWindow *myMainWindow)
{

    FontAwesomeViewer *viewer = new FontAwesomeViewer;
    viewer->show();

    parent.setWindowTitle("Timeline Viewer");
    parent.setStyleSheet("background-color: black;");

    ///
    int fontId = QFontDatabase::addApplicationFont(":/fa-regular-400.ttf");

    printf("fond Id %d\n", fontId);

    QString fontFamily = "Helvetica";
    if (fontId == -1) {
        qWarning("Failed to load FontAwesome font");
    }
    else
        fontFamily = QFontDatabase::applicationFontFamilies(fontId).at(0);

    //QFont fontAwesome(fontFamily);
    QFont fontAwesome(FontAwesomeViewer::fontAwesomeSolid);


    // Create buttons
    QToolButton *addPanelButton = new QToolButton(&parent);
    addPanelButton->setText(QChar(0xf0fe));  // Shift left icon
    addPanelButton->setFont(fontAwesome);

    QToolButton *deletePanelButton = new QToolButton(&parent);
    deletePanelButton->setText(QChar(0xf146));  // Shift left icon
    deletePanelButton->setFont(fontAwesome);

    QToolButton *shiftLeftButton = new QToolButton(&parent);
    shiftLeftButton->setText(QChar(0xf2f6));  // Shift left icon
    shiftLeftButton->setFont(fontAwesome);

    QToolButton *shiftRightButton = new QToolButton(&parent);
    shiftRightButton->setText(QChar(0xf337));  // Group selection icon
    shiftRightButton->setFont(fontAwesome);

    QToolButton *editButton = new QToolButton(&parent);
    editButton->setText(QChar(0xf044));  // Area selection icon
    editButton->setFont(fontAwesome);

    QToolButton *trashButton = new QToolButton(&parent);
    trashButton->setText(QChar(0xf1f8));  // Area selection icon
    trashButton->setFont(fontAwesome);

    QToolButton *backwardFastButton = new QToolButton(&parent);
    backwardFastButton->setText(QChar(0xf049));  // Area selection icon
    backwardFastButton->setFont(fontAwesome);
    // todo add connection to go to prior scene

    QToolButton *backwardStepButton = new QToolButton(&parent);
    backwardStepButton->setText(QChar(0xf048));  // Area selection icon
    backwardStepButton->setFont(fontAwesome);
    // todo add connection to go to prior shot

    QToolButton *playButton = new QToolButton(&parent);
    playButton->setText(QChar(0xf04b));  // Area selection icon
    playButton->setFont(fontAwesome);
    // todo add connection to play

    QToolButton *pauseButton = new QToolButton(&parent);
    pauseButton->setText(QChar(0xf04c));  // Area selection icon
    pauseButton->setFont(fontAwesome);
    // todo add connection to go pause

    QToolButton *forwardStepButton = new QToolButton(&parent);
    forwardStepButton->setText(QChar(0xf051));  // Area selection icon
    forwardStepButton->setFont(fontAwesome);
    // todo add connection to go to next shot

    QToolButton *forwardFastButton = new QToolButton(&parent);
    forwardFastButton->setText(QChar(0xf050));  // Area selection icon
    forwardFastButton->setFont(fontAwesome);
    // todo add connection to go to next scene

    QToolButton *settingsButton = new QToolButton(&parent);
    settingsButton->setText(QChar(0xf1de));  // Single selection icon
    settingsButton->setFont(fontAwesome);


    QToolButton *viewModeButton = new QToolButton(&parent); // New toggle button
        viewModeButton->setText(QChar(0xf0c9)); // Grid icon for stacked view
        viewModeButton->setFont(fontAwesome);
        viewModeButton->setCheckable(true);
        viewModeButton->setStyleSheet(
            "QToolButton { border: 2px solid black; color: white; background: black; border-radius: 5px; }"
            "QToolButton:checked { background-color: #32CD32; border: 2px solid #32CD32; color: white; border-radius: 5px; }"
        );

    // --- Create and configure the TimeLineView
    //CustomGraphicsScene *scene = new CustomGraphicsScene();
    TimeLineView *timelineView = new TimeLineView(&parent);

    //timelineView->setScene(scene);
    QGraphicsScene *scene = timelineView->scene();
    timelineView->setMinimumSize(80, 60); // Set a minimum size for the timeline view
    //timelineView->scale(0.5, 1);

    // Create a QSpinBox for scale control
    QSpinBox *scaleControl = new QSpinBox(&parent);
    scaleControl->setFixedWidth(100);
    scaleControl->setRange(10, 200); // Set range from 1x to 10x
    scaleControl->setValue(100); // Initial scale factor set to 1
    scaleControl->setPrefix("Scale: ");
    //scaleControl->setSuffix("x");

    // Create a QSlider for scale control
    //QSlider *scaleSlider = new QSlider(Qt::Horizontal, &parent);
    CustomSlider *scaleSlider = new CustomSlider(Qt::Horizontal, &parent);
    scaleSlider->setRange(10, 200); // Matching range with the spin box
    scaleSlider->setValue(100); // Initial value matching the spin box
    scaleSlider->setFixedWidth(400);

    // Create a new QGraphicsView as a custom scrollbar
    ScrollbarView *scrollbarView = new ScrollbarView(&parent);
    scrollbarView->setFixedHeight(28);  // Set the height of the scrollbar
    scrollbarView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Connect the scale control to the view scaling functionality
    QObject::connect(scaleControl, QOverload<int>::of(&QSpinBox::valueChanged),
                     [timelineView, scaleSlider](int scaleValue) {
                         // Adjust only the x-scale based on the spin box value
                         //qreal currentYScale = timelineView->transform().m22(); // Get the current y-scale
                         //timelineView->setTransform(QTransform::fromScale(100.f/scaleValue, currentYScale));
                         // update the timeline
                         timelineView->setUiScale(scaleValue/100.f);

                         bool oldState = scaleSlider->blockSignals(true);
                         scaleSlider->setValue(scaleValue);
                         scaleSlider->blockSignals(oldState);
                     });

    //connect(scaleSlider, &CustomSlider::prepareForScaling, this, &YourClass::handlePrepareForScaling);

    QObject::connect(scaleSlider, &CustomSlider::prepareForScaling, [timelineView]() {
        // Capture the center point when the user starts to change the scale
        printf("prepare For Sliding\n");
        fflush(stdout);
        QPointF focus = timelineView->getCenterPoint();
        timelineView->setZoomFocus(focus);
    });

    QObject::connect(scaleSlider, &QSlider::valueChanged, [timelineView, scaleControl](int value) {
        //qreal scaleY = timelineView->transform().m22(); // Maintain current Y scale
        //timelineView->setTransform(QTransform::fromScale(value / 100.0, scaleY));
        printf("valueChanged For Sliding\n");
        fflush(stdout);

        scaleControl->setValue(value); // Update spin box when slider changes
        bool oldState = scaleControl->blockSignals(true);
        scaleControl->setValue(value);
        scaleControl->blockSignals(oldState);

        // update the timeline
        timelineView->setUiScale(value/100.f);
    });

    QObject::connect(timelineView, &TimeLineView::updateZoom,
                     [scaleSlider](double newZoomLevel) {
                         int sliderValue = static_cast<int>(newZoomLevel * 100);  // Convert zoom level to slider value
                         scaleSlider->updateValue(sliderValue);
                     });

    QObject::connect(timelineView, &TimeLineView::updateZoom,
                     [scaleControl](double newZoomLevel) {
                         int spinBoxValue = static_cast<int>(newZoomLevel * 100);  // Convert zoom level to spin box value
                         bool oldState = scaleControl->blockSignals(true);
                         scaleControl->setValue(spinBoxValue);
                         scaleControl->blockSignals(oldState);
                     });


    timelineView->scene()->setSceneRect(0, 0, 20000, 700);  // Adjust dimensions as needed
    timelineView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    timelineView->addCameraKeyFrame("A", 0, "");
    timelineView->addCameraKeyFrame("B", 200, "");
    //timelineView->addCameraKeyFrame("C", 150"");

    // Create tracks
    Track *track1 = new Track("Track 1", 0, 500000, TrackType::Storyboard); // Name, Start Time, Duration in seconds
    Track *track2 = new Track("Track 2", 0, 150000, TrackType::Audio); // Name, Start Time, Duration in seconds
    //Track *track3 = new Track("Track 3", 0, 170000, TrackType::NonLinearMedia); // Name, Start Time, Duration in seconds

    // Add tracks to the timeline
    TrackItem *t1 = timelineView->addTrack(track1);
    TrackItem *t2 = timelineView->addTrack(track2);
    //timelineView->addTrack(track3);

    Segment* segment = new Segment(scene, 0, 400);
    t1->addSegment(segment);
    //t1->addSegment(5, 100);
    // Layout setup (if necessary)

    //scene->addItem(t1);

    bool TestPhonemes = false;

    if(TestPhonemes){

    new MarkerItem(20, 30, 20, 220, segment, "Hello", "HH", "(OS)");
    new MarkerItem(40, 30, 40, 220, segment, "", "EH", "");
    new MarkerItem(70, 30, 70, 220, segment, "", "L", "");
    new MarkerItem(90, 30, 90, 220, segment, "", "OW", "");
    new MarkerItem(110, 30, 110, 220, segment, "", "*", "");

    new MarkerItem(120, 30, 10, 220, segment, "World", "W", "");
    new MarkerItem(140, 30, 140, 220, segment, "", "ER", "");
    new MarkerItem(170, 30, 170, 220, segment, "", "L", "");
    new MarkerItem(190, 30, 190, 220, segment, "", "D", "");
    new MarkerItem(225, 30, 225, 220, segment, "<SIL>", "", "");
    //layout->addStretch(1); // Adds a spacer to push the timeline to the left




    Segment* segment2 = new Segment(scene, 200, 500);
    t2->addSegment(segment2);
    new PanelMarker(0, 30, 0, 220, segment2, "Panel 1", "HH", "(OS)", "shotUuid");
    new PanelMarker(140, 30, 140, 220, segment2, "Panel 2", "OW", "WIDE", "shotUuid");





    ShotSegment* segment3 = new ShotSegment(scene, 600, 500);
    t2->addSegment(segment3);
    //new MarkerItem(70, 30, 70, 220, segment3, "", "L", "");

}

    // Layout setup
    /*
    QHBoxLayout *layout = new QHBoxLayout(&mainWindow); // Use QHBoxLayout for horizontal layout
    layout->addWidget(timelineView);
    layout->addStretch(1); // Adds a spacer to push the timeline to the left
*/

    // Create a QComboBox for visual style selection
    QComboBox *styleComboBox = new QComboBox(&parent);
    styleComboBox->addItem("Grayscale");
    styleComboBox->addItem("Inferno");
    styleComboBox->addItem("Viridis");
    styleComboBox->addItem("Cividis");
    styleComboBox->addItem("Plasma");
    styleComboBox->addItem("Turbo");
    styleComboBox->addItem("Roseus");

    styleComboBox->addItem("None");

    styleComboBox->setCurrentText("None");
    timelineView->setVisualStyle("None");

    // Create a checkbox for waveform display
    QCheckBox *waveformCheckbox = new QCheckBox("Waveform", &parent);
    waveformCheckbox->setChecked(false);



    QSpacerItem *spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    QHBoxLayout *topTopLayout = new QHBoxLayout(&parent);
    QVBoxLayout *toplayout = new QVBoxLayout(&parent);
    QHBoxLayout *hlayout = new QHBoxLayout(&parent);

    QVBoxLayout *gfxlayout = new QVBoxLayout(&parent);

    gfxlayout->setSpacing(0);  // Remove spacing between the elements
    topTopLayout->setSpacing(0);
    topTopLayout->setContentsMargins(0,0,0,0);

    // Grouping for toggle buttons (play/pause)
    QButtonGroup *toggleGroup = new QButtonGroup(&parent);
    toggleGroup->setExclusive(true);
    toggleGroup->addButton(playButton);
    toggleGroup->addButton(pauseButton);

    // Setup for play/pause toggle buttons without borders
    // Play Button
    playButton->setCheckable(true);
    playButton->setStyleSheet(
        "QToolButton { "
        "    border: 2px solid black; "  // Black border when not selected
        "    color: #32CD32; "           // LimeGreen text color
        "    background: black; "  // Transparent background
        "    border-radius: 5px; "       // Rounded corners with 5-pixel radius
        "} "
        "QToolButton:checked { "
        "    background-color: #32CD32; "  // LimeGreen background when selected
        "    border: 2px solid #32CD32; "  // LimeGreen border when selected
        "    color: white; "               // Optionally change the text color to white when selected
        "    border-radius: 5px; "
        "} "
        );

    // Pause Button
    pauseButton->setCheckable(true);
    pauseButton->setStyleSheet(
        "QToolButton { "
        "    border: 2px solid black; "  // Black border when not selected
        "    color: #FF4500; "           // OrangeRed text color
        "    background: black; "  // Transparent background
        "    border-radius: 5px; "       // Rounded corners with 5-pixel radius
        "} "
        "QToolButton:checked { "
        "    background-color: #FF4500; "  // OrangeRed background when selected
        "    border: 2px solid #FF4500; "  // OrangeRed border when selected
        "    color: white; "               // Optionally change the text color to white when selected
        "    border-radius: 5px; "
        "} "
        );

    hlayout->addWidget(scaleControl); // Add the scale control at the top
    hlayout->addWidget(scaleSlider); // Add the scale control at the top
    hlayout->addWidget(styleComboBox);  // Add the style combobox
    hlayout->addWidget(waveformCheckbox);

    hlayout->addWidget(addPanelButton);
    hlayout->addWidget(deletePanelButton);

    hlayout->addWidget(shiftLeftButton);
    hlayout->addWidget(shiftRightButton);
    hlayout->addWidget(editButton);
    hlayout->addWidget(trashButton);
    hlayout->addWidget(backwardFastButton);
    hlayout->addWidget(backwardStepButton);
    hlayout->addWidget(playButton);
    hlayout->addWidget(pauseButton);
    hlayout->addWidget(forwardStepButton);
    hlayout->addWidget(forwardFastButton);
    hlayout->addWidget(settingsButton);
    hlayout->addWidget(viewModeButton); // Add toggle button

    hlayout->addSpacerItem(spacer);
    gfxlayout->addWidget(timelineView);
    gfxlayout->addWidget(scrollbarView);

    toplayout->addLayout(hlayout);
    toplayout->addLayout(gfxlayout);

    // Create audio meter
    AudioMeterWidget *audioMeter = new AudioMeterWidget(&parent);
    audioMeter->resize(100, 600);
    audioMeter->setMinimumWidth(70);
    audioMeter->setMaximumWidth(70);
    audioMeter->show();

    topTopLayout->addLayout(toplayout);
    topTopLayout->addWidget(audioMeter);

    parent.setLayout(topTopLayout);
    //parent.setLayout(toplayout);

    QObject::connect(addPanelButton, &QToolButton::clicked, [timelineView]() {
        timelineView->onAddPanel();
    });

    QObject::connect(deletePanelButton, &QToolButton::clicked, [timelineView]() {
        timelineView->onDeletePanel();
    });

    QObject::connect(playButton, &QToolButton::clicked, myMainWindow, &MainWindow::play);
    QObject::connect(pauseButton, &QToolButton::clicked, myMainWindow, &MainWindow::pause);
    QObject::connect(forwardStepButton, &QToolButton::clicked, myMainWindow, &MainWindow::nextShot);
    QObject::connect(forwardFastButton, &QToolButton::clicked, myMainWindow, &MainWindow::nextScene);
    QObject::connect(backwardStepButton, &QToolButton::clicked, myMainWindow, &MainWindow::prevShot);
    QObject::connect(backwardFastButton, &QToolButton::clicked, myMainWindow, &MainWindow::prevScene);

    QObject::connect(settingsButton, &QToolButton::clicked, [timelineView]() {
        timelineView->onOptionsDialog();
    });

    // Connect ScrollbarView to TimelineView's scrollbar
    QObject::connect(scrollbarView, &ScrollbarView::valueChanged,
                     timelineView->horizontalScrollBar(), &QScrollBar::setValue);
    /*
    // Connect TimelineView's scrollbar to ScrollbarView
    QObject::connect(timelineView->horizontalScrollBar(), &QScrollBar::valueChanged,
                     scrollbarView, &ScrollbarView::setValue);
*/
    // Setup connections
    QObject::connect(timelineView, &TimeLineView::updateScroll,
                     scrollbarView, &ScrollbarView::setValue);

    // Connect the visual style combo box to the timeline view
    QObject::connect(styleComboBox, &QComboBox::currentTextChanged,
                     timelineView, &TimeLineView::setVisualStyle);

    QObject::connect(waveformCheckbox, &QCheckBox::stateChanged, timelineView, &TimeLineView::toggleWaveformDisplay);

    QObject::connect(viewModeButton, &QToolButton::clicked, [timelineView, viewModeButton]() {
            timelineView->toggleViewMode();
            viewModeButton->setText(timelineView->getViewMode() == TimeLineView::StackedView ? QChar(0xf0c9) : QChar(0xf338));
        });

    scrollbarView->setRange(0,20000);

    // Show the main window
    parent.show();

#ifdef WIN32
    t1->loadAudio("WarnerTest", "../../GameEngine/Applications/GameEditor/WarnerTest/TT.257.500.ACT.B.TEST44100.wav");
    t2->loadAudio("WarnerTest", "../../GameEngine/Applications/GameEditor/WarnerTest/TT.257.500.ACT.B.TEST44100.wav");
#else
    t1->loadAudio("WarnerTest", "/users/andreascarlen/GameFusion/GameEngine/Applications/GameEditor/WarnerTest/TT.257.500.ACT.B.TEST44100.wav");
    t2->loadAudio("WarnerTest", "/users/andreascarlen/GameFusion/GameEngine/Applications/GameEditor/WarnerTest/TT.257.500.ACT.B.TEST44100.wav");
#endif




    return timelineView;
}



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindowBoarder), llamaModel(nullptr), scriptBreakdown(nullptr), logger(new PromptLogger())
{
    undoStack = new QUndoStack(this); // Initialize undo stack

    ui->setupUi(this);

    ProjectContext::instance().projectJson()["resolution"] = QJsonArray{1920, 1080};
    ProjectContext::instance().projectJson()["canvas_margin"] = QString("20");
    ProjectContext::instance().projectJson()["canvas_percent"] = 20;
    ProjectContext::instance().projectJson()["canvas"] = QJsonArray{2304, 1296}; // For 20% margin: 1920 + 384, etc.

    paintCanvas = new PaintCanvas();
    paintCanvas->show();



    logger->setServerConfig("http://localhost:50000", "your-secret-token-here");
    // Undo & Redo actions, menu system

    // Create Edit menu
    QMenu* editMenu = menuBar()->addMenu(tr("Edit"));
    undoAction = undoStack->createUndoAction(this, tr("Undo"));
    redoAction = undoStack->createRedoAction(this, tr("Redo"));
    undoAction->setShortcut(QKeySequence::Undo); // Ctrl+Z
    redoAction->setShortcut(QKeySequence::Redo); // Ctrl+Y
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);

    // Import actions

    QAction *importScriptAction = ui->menuFile->addAction(tr("&Import Script"));
    connect(importScriptAction, &QAction::triggered, this, &MainWindow::importScript);

    QAction *importAudioSegmentAction = ui->menuFile->addAction(tr("&Import Audio Segment..."));
    connect(importAudioSegmentAction, &QAction::triggered, this, &MainWindow::importAudioSegment);

    QAction *importAudioTrackAction = ui->menuFile->addAction(tr("&Import Audio Track..."));
    connect(importAudioTrackAction, &QAction::triggered, this, &MainWindow::importAudioTrack);

    QMenu* trackMenu = menuBar()->addMenu(tr("&Track"));

    QAction* addAudioTrackAction = new QAction(tr("Add Audio Track"), this);
    QAction* addVideoTrackAction = new QAction(tr("Add Video Track"), this);
    QAction* addAnimationTrackAction = new QAction(tr("Add Animation Track"), this);
    QAction* addSubtitleTrackAction = new QAction(tr("Add Subtitle Track"), this);
    trackMenu->addAction(addAudioTrackAction);
    trackMenu->addAction(addVideoTrackAction);
    trackMenu->addAction(addAnimationTrackAction);
    trackMenu->addAction(addSubtitleTrackAction);

    // Connect the action to a slot
    connect(addAudioTrackAction, &QAction::triggered, this, &MainWindow::addAudioTrack);

    //
    // Storyboard
    //
    QMenu* storyboardMenu = menuBar()->addMenu(tr("&Storyboard"));


    // Scene submenu
    QMenu *sceneMenu = storyboardMenu->addMenu(tr("Scene"));
    newSceneAct = sceneMenu->addAction(tr("New Scene..."));
    splitSceneAct = sceneMenu->addAction(tr("Split Scene..."));
    renameSceneAct = sceneMenu->addAction(tr("Rename Scene..."));
    duplicateSceneAct = sceneMenu->addAction(tr("Duplicate Scene"));
    sceneMenu->addSeparator();
    deleteSceneAct = sceneMenu->addAction(tr("Delete Scene"));

    splitSceneAct->setEnabled(false);
    renameSceneAct->setEnabled(false);
    duplicateSceneAct->setEnabled(false);
    deleteSceneAct->setEnabled(false);

    // Shot submenu
    shotMenu = storyboardMenu->addMenu(tr("Shot"));
    newShotAct = shotMenu->addAction(tr("New Shot"));
    editShotAct = shotMenu->addAction(tr("Edit Shot"));
    renameShotAct = shotMenu->addAction(tr("Rename Shot"));
    shotMenu->addSeparator();
    duplicateShotAct = shotMenu->addAction(tr("Duplicate Shot"));
    shotMenu->addSeparator();
    deleteShotAct = shotMenu->addAction(tr("Delete Shot"));

    editShotAct->setEnabled(false);
    renameShotAct->setEnabled(false);
    duplicateShotAct->setEnabled(false);
    deleteShotAct->setEnabled(false);

    // Panel menu
    QMenu *panelMenu = storyboardMenu->addMenu(tr("Panel"));
    newPanelAct = panelMenu->addAction(tr("New Panel"));
    insertPanelAct = panelMenu->addAction(tr("Insert Panel"));
    editPanelAct = panelMenu->addAction(tr("Edit Panel"));
    renamePanelAct = panelMenu->addAction(tr("Rename Panel"));
    duplicatePanelAct = panelMenu->addAction(tr("Duplicate Panel"));
    panelMenu->addSeparator();
    copyPanelAct = panelMenu->addAction(tr("Copy Panel"));
    cutPanelAct = panelMenu->addAction(tr("Cut Panel"));
    pastePanelAct = panelMenu->addAction(tr("Paste Panel"));
    panelMenu->addSeparator();
    clearPanelAct = panelMenu->addAction(tr("Clear Panel"));
    panelMenu->addSeparator();
    deletePanelAct = panelMenu->addAction(tr("Delete Panel"));



    newPanelAct->setEnabled(false);
    insertPanelAct->setEnabled(false);
    editPanelAct->setEnabled(false);
    renamePanelAct->setEnabled(false);
    duplicatePanelAct->setEnabled(false);
    copyPanelAct->setEnabled(false);
    cutPanelAct->setEnabled(false);
    pastePanelAct->setEnabled(false);
    clearPanelAct->setEnabled(false);
    deletePanelAct->setEnabled(false);

    // --- Layer menu

    // Create Layer submenu
    QMenu* layerMenu = new QMenu("Layer", storyboardMenu);
    storyboardMenu->addMenu(layerMenu);

    // New Layer
    QAction* newLayerAction = new QAction("New Layer...", layerMenu);
    layerMenu->addAction(newLayerAction);
    connect(newLayerAction, &QAction::triggered, this, &MainWindow::onLayerAdd);

    // Clear Layer
    QAction* clearLayerAction = new QAction("Clear Layer", layerMenu);
    layerMenu->addAction(clearLayerAction);
    connect(clearLayerAction, &QAction::triggered, this, &MainWindow::onLayerClear);

    // Duplicate Layer submenu
    QMenu* duplicateLayerMenu = new QMenu("Duplicate Layer", layerMenu);
    layerMenu->addMenu(duplicateLayerMenu);
    QAction* duplicateCurrentPanelAction = new QAction("Current Panel", duplicateLayerMenu);
    QAction* duplicateThroughShotAction = new QAction("Through Shot", duplicateLayerMenu);
    duplicateLayerMenu->addAction(duplicateCurrentPanelAction);
    duplicateLayerMenu->addAction(duplicateThroughShotAction);
    connect(duplicateCurrentPanelAction, &QAction::triggered, this, &MainWindow::onLayerDuplicate);
    connect(duplicateThroughShotAction, &QAction::triggered, this, &MainWindow::onLayerDuplicateThroughShot);

    // Alias Layer submenu
    QMenu* aliasLayerMenu = new QMenu("Instance Layer", layerMenu);
    layerMenu->addMenu(aliasLayerMenu);
    QAction* aliasToSelectedPanelsAction = new QAction("To Selected Panels", aliasLayerMenu);
    QAction* aliasThroughShotAction = new QAction("Through Shot", aliasLayerMenu);
    aliasLayerMenu->addAction(aliasToSelectedPanelsAction);
    aliasLayerMenu->addAction(aliasThroughShotAction);
    connect(aliasToSelectedPanelsAction, &QAction::triggered, this, &MainWindow::onLayerInstanceToSelectedPanels);
    connect(aliasThroughShotAction, &QAction::triggered, this, &MainWindow::onLayerInstanceThroughShot);

    // Copy and Paste
    QAction* copyLayerAction = new QAction("Copy Layer", layerMenu);
    QMenu* pasteLayerMenu = new QMenu("Paste Layer As...", layerMenu);
    layerMenu->addAction(copyLayerAction);
    layerMenu->addMenu(pasteLayerMenu);
    QAction* pasteAsAliasAction = new QAction("Instance", pasteLayerMenu);
    QAction* pasteAsDuplicateAction = new QAction("Duplicate", pasteLayerMenu);
    pasteLayerMenu->addAction(pasteAsAliasAction);
    pasteLayerMenu->addAction(pasteAsDuplicateAction);
    connect(copyLayerAction, &QAction::triggered, this, &MainWindow::onLayerCopy);
    connect(pasteAsAliasAction, &QAction::triggered, this, &MainWindow::onLayerPasteAsInstance);
    connect(pasteAsDuplicateAction, &QAction::triggered, this, &MainWindow::onLayerPasteAsDuplicate);

    // Rename and Delete
    QAction* renameLayerAction = new QAction("Rename Layer...", layerMenu);
    QAction* deleteLayerAction = new QAction("Delete Layer", layerMenu);
    layerMenu->addAction(renameLayerAction);
    layerMenu->addAction(deleteLayerAction);
    connect(renameLayerAction, &QAction::triggered, this, &MainWindow::onLayerRename);
    connect(deleteLayerAction, &QAction::triggered, this, &MainWindow::onLayerDelete);

    // Layer Properties
    QAction* layerPropertiesAction = new QAction("Layer Properties...", layerMenu);
    layerMenu->addAction(layerPropertiesAction);
    connect(layerPropertiesAction, &QAction::triggered, this, &MainWindow::onLayerProperties);

    // Group and Ungroup
    QAction* groupLayersAction = new QAction("Group Layers", layerMenu);
    QAction* ungroupLayersAction = new QAction("Ungroup Layers", layerMenu);
    layerMenu->addAction(groupLayersAction);
    layerMenu->addAction(ungroupLayersAction);
    connect(groupLayersAction, &QAction::triggered, this, &MainWindow::onGroupLayers);
    connect(ungroupLayersAction, &QAction::triggered, this, &MainWindow::onUngroupLayers);

    connect(ui->layerListWidget->model(), &QAbstractItemModel::rowsMoved, this, &MainWindow::onLayerReordered);
    connect(ui->toolButton_layerDup, &QToolButton::clicked, this, &MainWindow::onLayerDuplicate);
    //connect(paint, &PaintArea::layerModified, this, &MainWindow::onPaintAreaLayerModified);
    //connect(paint, &PaintArea::keyFramePositionChanged, this, &MainWindow::onKeyFramePositionChanged);

    //
    // --- Existing camera actions
    //
    QMenu *cameraMenu = storyboardMenu->addMenu(tr("Camera"));
    newCameraAct = cameraMenu->addAction(tr("New Camera"));
    renameCameraAct = cameraMenu->addAction(tr("Rename Camera "));
    QAction *duplicateCameraAct = cameraMenu->addAction(tr("Duplicate Camera (Experimental)"));
    cameraMenu->addSeparator();
    copyCameraAct = cameraMenu->addAction(tr("Copy Camera"));
    pastCameraAct = cameraMenu->addAction(tr("Past Camera"));
    cameraMenu->addSeparator();
    deleteCameraAct = cameraMenu->addAction(tr("Delete Camera"));

    //
    // Storyboard menu actions
    //

    // Connect scene actions
    connect(newSceneAct, &QAction::triggered, this, &MainWindow::onNewScene);
    connect(splitSceneAct, &QAction::triggered, this, &MainWindow::onSplitScene);
    connect(renameSceneAct, &QAction::triggered, this, &MainWindow::onRenameScene);
    connect(duplicateSceneAct, &QAction::triggered, this, &MainWindow::onDuplicateScene);
    connect(deleteSceneAct, &QAction::triggered, this, &MainWindow::onDeleteScene);

    duplicateSceneAct->setEnabled(false);

    // Connect shot actions
    connect(newShotAct, &QAction::triggered, this, &MainWindow::onNewShot);
    connect(editShotAct, &QAction::triggered, this, &MainWindow::onEditShot);
    connect(deleteShotAct, &QAction::triggered, this, &MainWindow::onDeleteShot);
    connect(renameShotAct, &QAction::triggered, this, &MainWindow::onRenameShot);
    connect(duplicateShotAct, &QAction::triggered, this, &MainWindow::onDuplicateShot);

    // Connect panel actions
    connect(newPanelAct, &QAction::triggered, this, &MainWindow::onNewPanel);
    connect(insertPanelAct, &QAction::triggered, this, &MainWindow::onInsertPanel);
    connect(editPanelAct, &QAction::triggered, this, &MainWindow::onEditPanel);
    connect(renamePanelAct, &QAction::triggered, this, &MainWindow::onRenamePanel);
    connect(duplicatePanelAct, &QAction::triggered, this, &MainWindow::onDuplicatePanel);
    connect(copyPanelAct, &QAction::triggered, this, &MainWindow::onCopyPanel);
    connect(cutPanelAct, &QAction::triggered, this, &MainWindow::onCutPanel);
    connect(pastePanelAct, &QAction::triggered, this, &MainWindow::onPastePanel);
    connect(clearPanelAct, &QAction::triggered, this, &MainWindow::onClearPanel);
    connect(deletePanelAct, &QAction::triggered, this, &MainWindow::onDeletePanel);

    // Connect camera actions
    connect(newCameraAct, &QAction::triggered, this, &MainWindow::onNewCamera);
    connect(copyCameraAct, &QAction::triggered, this, &MainWindow::onCopyCamera);
    connect(pastCameraAct, &QAction::triggered, this, &MainWindow::onPastCamera);
    connect(duplicateCameraAct, &QAction::triggered, this, &MainWindow::onDuplicateCamera);
    connect(deleteCameraAct, &QAction::triggered, this, &MainWindow::onDeleteCamera);
    connect(renameCameraAct, &QAction::triggered, this, &MainWindow::onRenameCamera);

    //
    // Camera Global Shortcuts
    //

    newCameraAct->setShortcut(QKeySequence("Ctrl+Shift+C"));

    //
    // Camera Side Panel
    //

    cameraSidePanel = new CameraSidePanel(this);
    cameraSidePanel->setMaximumWidth(350);
    ui->dockCameras->setWidget(cameraSidePanel);
    connect(cameraSidePanel, &CameraSidePanel::cameraFrameUpdated,
            this, &MainWindow::onCameraFrameUpdated);
    connect(cameraSidePanel, &CameraSidePanel::requestCameraThumbnail,
            this, &MainWindow::onRequestCameraThumbnail);
    //
    //
    //

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(1000);

    playbackTimer = new QTimer(this);
    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::onPlaybackTick);

    setAcceptDrops(true);

    QObject::connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(loadProject()));
    QObject::connect(ui->actionNew, SIGNAL(triggered()), this, SLOT(newProject()));
    QObject::connect(ui->actionEdit_Project, SIGNAL(triggered()), this, SLOT(editProject()));
    QObject::connect(ui->actionSave, SIGNAL(triggered()), this, SLOT(saveProject()));
    QObject::connect(ui->actionAuto_Save, SIGNAL(triggered()), this, SLOT(toggleProject()));
    QObject::connect(ui->actionPost_issue, SIGNAL(triggered()), this, SLOT(postIssue()));
    QObject::connect(ui->actionTeam_email, SIGNAL(triggered()), this, SLOT(teamEmail()));
    QObject::connect(ui->actionLogin, SIGNAL(triggered()), this, SLOT(login()));
    QObject::connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(quit()));

    QObject::connect(ui->actionWhite, SIGNAL(triggered()), this, SLOT(setWhiteTheme()));
    QObject::connect(ui->actionDark, SIGNAL(triggered()), this, SLOT(setDarkTheme()));
    QObject::connect(ui->actionBlack, SIGNAL(triggered()), this, SLOT(setBlackTheme()));

    connect(ui->actionPainterOptions, &QAction::triggered, this, &MainWindow::showOptionsDialog);
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(about()));

    connect(ui->actionExport_as_PDF, &QAction::triggered, this, &MainWindow::exportStoryboardPDF);
    connect(ui->actionExport_Movie, &QAction::triggered, this, &MainWindow::exportMovie);
    connect(ui->actionExport_EDL, &QAction::triggered, this, &MainWindow::exportEDL);


    connect(ui->actionColor_Palette, &QAction::triggered, this, &MainWindow::colorPalette);

    setupDockPanels();

    paint = new MainWindowPaint;
    ui->splitter->insertWidget(1, paint);
    paint->show();

    /* Create a default dump panel scene
     *
     */
    {
        GameFusion::Panel panel;
        GameFusion::Layer layer;
        panel.layers.push_back(layer);
        paint->getPaintArea()->setPanel(panel);
    }



    connect(paint->getPaintArea(), &PaintArea::cameraFrameAdded, this, &MainWindow::onCameraFrameAddedFromPaint);
    connect(paint->getPaintArea(), &PaintArea::cameraFrameUpdated, this, &MainWindow::onCameraFrameUpdated);
    connect(paint->getPaintArea(), &PaintArea::cameraFrameUpdated, cameraSidePanel, &CameraSidePanel::updateCameraFrame);
    connect(paint->getPaintArea(), &PaintArea::cameraFrameDeleted, this, &MainWindow::onCameraFrameDeleted);
    connect(paint->getPaintArea(), &PaintArea::toolModeChanged, this, &MainWindow::onToolModeChanged);
    connect(paint->getPaintArea(), &PaintArea::strokeSelected, this, &MainWindow::onStrokeSelected);

    connect(cameraSidePanel, &CameraSidePanel::cameraFrameUpdated,
            paint->getPaintArea(), &PaintArea::updateCameraFrameUI);

    //
    // Test image in shot tree
    //new QTreeWidgetItem(shotsTreeWidget);
    QTreeWidget *shotTree = ui->shotsTreeWidget;
    shotTree->setIconSize(QSize(256, 256));

    QTreeWidgetItem *newItem = new QTreeWidgetItem(shotTree);
    newItem->setText(0, "Something");
    newItem->setIcon(1, QIcon("2019-01-05.png"));

    shotTree->topLevelItem(0)->setIcon(1, QIcon("2018-12-03 (1).png"));
    shotTree->topLevelItem(1)->setIcon(1, QIcon("2018-12-03.png"));
    shotTree->topLevelItem(2)->setIcon(1, QIcon("2018-12-15.png"));
    shotTree->topLevelItem(3)->setIcon(1, QIcon("2019-01-05 (5).png"));
    newItem = new QTreeWidgetItem(shotTree);
    newItem->setText(0, "SH100");
    newItem->setIcon(1, QIcon("2018-09-15 (6).png"));
    newItem = new QTreeWidgetItem(shotTree);
    newItem->setText(0, "SH101");
    newItem->setIcon(1, QIcon("2018-09-12 (3).png"));
    newItem = new QTreeWidgetItem(shotTree);
    newItem->setText(0, "SH102");
    newItem->setIcon(1, QIcon("2018-09-15 (4).png"));
    //newItem->setSizeHint(1, QSize(256, 144));

    ProjectContext::instance().projectJson()["fps"] = 25.0;

    //shotTree->addTopLeveItem(newItem);

    //
    //// RETURN

    timeLineView = createTimeLine(*ui->timeline, this);

    connect(timeLineView, &TimeLineView::timeCursorMoved,
            this, &MainWindow::onTimeCursorMoved);
    connect(timeLineView, &TimeLineView::addPanel,
            this, &MainWindow::newPanel);
    connect(timeLineView, &TimeLineView::deletePanel,
            this, &MainWindow::deletePanel);
    connect(timeLineView, &TimeLineView::optionsDialog,
            this, &MainWindow::timelineOptions);
    connect(timeLineView, &TimeLineView::cameraKeyFrameUpdated,
            this, &MainWindow::timelineCameraUpdate);
    connect(timeLineView, &TimeLineView::cameraKeyFrameDeleted,
            this, &MainWindow::timelineCameraDeleted);
    connect(timeLineView, &TimeLineView::keyframeAdded,
            this, &MainWindow::onKeyframeAdded);
    connect(timeLineView, &TimeLineView::keyframeDeleted,
            this, &MainWindow::onKeyframeDeleted);
    connect(timeLineView, &TimeLineView::keyframeUpdated,
            this, &MainWindow::onKeyframeUpdated);
    connect(timeLineView, &TimeLineView::keyframePairUpdated,
            this, &MainWindow::onGroupedKeyframeUpdated);

    // Add Callback for Tree Item Selection
    connect(ui->shotsTreeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);

    // Layer connections
    connect(ui->layerListWidget, &QListWidget::currentItemChanged,
            this, &MainWindow::onLayerSelectionChanged);

    connect(ui->layerListWidget, &QListWidget::itemChanged,
            this, &MainWindow::onLayerVisibilityChanged);
    connect(ui->layerListWidget->model(), &QAbstractItemModel::rowsMoved,
            this, &MainWindow::onLayerReordered);

    // Connect UI widgets to PaintArea slots
    connect(ui->comboBox_layerBlendMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLayerBlendMode);
    connect(ui->toolButton_layerImage, &QToolButton::clicked,
            this, &MainWindow::onLayerLoadImage);
    connect(ui->spinBox_layerOpacity, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onLayerOpacity);
    connect(ui->toolButton_layerFX, &QToolButton::clicked,
            this, &MainWindow::onLayerFX);
    connect(ui->toolButton_layerAdd, &QToolButton::clicked,
            this, &MainWindow::onLayerAdd);
    connect(ui->toolButton_layerTrash, &QToolButton::clicked,
            this, &MainWindow::onLayerDelete);
    connect(ui->toolButton_layerMoveUp, &QToolButton::clicked,
            this, &MainWindow::onLayerMoveUp);
    connect(ui->toolButton_layerMoveDown, &QToolButton::clicked,
            this, &MainWindow::onLayerMoveDown);
    connect(ui->toolButton_layerDup, &QToolButton::clicked,
            this, &MainWindow::onLayerDuplicate);


    ui->layerListWidget->setDragDropMode(QAbstractItemView::InternalMove);
    ui->layerListWidget->setDefaultDropAction(Qt::MoveAction);

    //ui->layerListWidget->setColumnWidth(0, 240); // Thumbnail column
    //ui->layerListWidget->setColumnWidth(1, 200); // Name column
    ui->layerListWidget->setIconSize(QSize(240, 135)); // Match thumbnail size
    //ui->layerListWidget->setViewMode(QListView::ListMode);
    ui->layerListWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // Connect PaintArea's layerThumbnailComputed signal
    connect(this->paint->getPaintArea(), &PaintArea::layerThumbnailComputed, this, &MainWindow::updateLayerThumbnail);

    //ui->layerListWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // Initialize comboBox_layerBlendMode
    ui->comboBox_layerBlendMode->clear();
    ui->comboBox_layerBlendMode->addItems({"Opacity", "Multiply", "Screen", "Overlay"});



    // In constructor:


    // Connect new layer control widgets
    connect(ui->spinBox_layerRotation, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onLayerRotationChanged);
    connect(ui->doubleSpinBox_layerPosX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onLayerPosXChanged);
    connect(ui->doubleSpinBox_layerPosY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onLayerPosYChanged);
    connect(ui->doubleSpinBox_layerScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onLayerScaleChanged);
    ///
    ///

    connect(paint->getPaintArea(), &PaintArea::layerAdded,
            this, &MainWindow::onPaintAreaLayerAdded);

    connect(paint->getPaintArea(), &PaintArea::layerModified,
            this, &MainWindow::onPaintAreaLayerModified);

    connect(paint->getPaintArea(), &PaintArea::layerErasedStrokes,
            this, &MainWindow::onPaintAreaEraseStrokes);

    connect(paint->getPaintArea(), &PaintArea::compositImageModified,
            this, &MainWindow::onPaintAreaImageModified);

    connect(paint->getPaintArea(), &PaintArea::keyFramePositionChanged,
            this, &MainWindow::onKeyFramePositionChanged);

    connect(paint->getPaintArea(), &PaintArea::LayerPositionChanged,
            this, &MainWindow::onLayerPositionChanged);

    // Initialize new widgets
    ui->spinBox_layerRotation->setRange(-3600, 3600);
    ui->spinBox_layerRotation->setValue(0);
    ui->doubleSpinBox_layerPosX->setRange(-38400, 38400);
    ui->doubleSpinBox_layerPosX->setValue(0.0);
    ui->doubleSpinBox_layerPosY->setRange(-38400, 38400);
    ui->doubleSpinBox_layerPosY->setValue(0.0);
    ui->doubleSpinBox_layerScale->setRange(0., 100.0);
    ui->doubleSpinBox_layerScale->setSingleStep(0.001);
    ui->doubleSpinBox_layerScale->setValue(1.0);

    ui->spinBox_layerRotation->installEventFilter(this);
    ui->doubleSpinBox_layerPosX->installEventFilter(this);
    ui->doubleSpinBox_layerPosY->installEventFilter(this);
    ui->doubleSpinBox_layerScale->installEventFilter(this);

    ui->dockLayers->setStyleSheet(R"(
    QLabel, QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QListWidget, QPushButton {
        font-size: 10pt;
    }
)");

    ui->shotsTreeWidget->setStyleSheet(R"(
    QLabel, QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTreeWidget, QListWidget, QPushButton {
        font-size: 10pt;
    }
)");

    shotPanel->setStyleSheet(R"(
    QLabel, QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTreeWidget, QListWidget, QPushButton {
        font-size: 10pt;
    }
)");

    QFont fa = FontAwesomeViewer::fontAwesomeSolid;
    //fa.setPixelSize(8);
    ui->toolButton_layerAdd->setFont(fa);
    ui->toolButton_layerDup->setFont(fa);
    ui->toolButton_layerImage->setFont(fa);
    ui->toolButton_layerTrash->setFont(fa);
    ui->toolButton_layerMoveUp->setFont(fa);
    ui->toolButton_layerMoveDown->setFont(fa);

    ui->toolButton_layerAdd->setText(QChar(0xF0FE));
    ui->toolButton_layerDup->setText(QChar(0xF24D));
    ui->toolButton_layerImage->setText(QChar(0xF03E));
    ui->toolButton_layerTrash->setText(QChar(0xF2ED));
    ui->toolButton_layerMoveUp->setText(QChar(0xf062));
    ui->toolButton_layerMoveDown->setText(QChar(0xf063));

    // Load the font explicitly
        int fontId = QFontDatabase::addApplicationFont("resources/fonts/fa-solid-900.ttf");
        if (fontId == -1) {
            qDebug() << "Failed to load Font Awesome font!";

        }

        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (families.isEmpty()) {
            qDebug() << "No font families found!";

        }

        QString fontFamily = families.at(0);
        qDebug() << "Using font family:" << fontFamily;

        QFont faFont(fontFamily);
        faFont.setPointSize(16);  // Try large size


    QFont toolFonts = FontAwesomeViewer::fontAwesomeSolid;
    //toolFonts.setPixelSize(14);
    //paint->createTools(&toolFonts);
    //paint->createTools(&FontAwesomeViewer::fontAwesomeSolid);
    QTimer::singleShot(5000, this, [ this]() {
            GameFusion::Log().info("create tools post\n");
            paint->createTools(&FontAwesomeViewer::fontAwesomeSolid);
        });

    //

    strokeDock = new StrokeAttributeDockWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, strokeDock);

    connect(strokeDock, &StrokeAttributeDockWidget::strokePropertiesChanged,
            paint->getPaintArea(), &PaintArea::setStrokeProperties);

    connect(strokeDock, &StrokeAttributeDockWidget::strokePropertiesChanged,
            paintCanvas, &PaintCanvas::setStrokeProperties);

    strokeDock->setStyleSheet("QLabel { font-size: 10px; } QSlider, QComboBox, QSpinBox { margin-bottom: 4px; }");

    strokeDock->setStyleSheet(R"(
QLabel {
    font-size: 10px;
}

QSlider {
    min-height: 16px;
}

QSlider::groove:horizontal {
    height: 6px;
    border-radius: 3px;
}

QSlider::handle:horizontal {
    background: #999;
    border: 1px solid #999;
    width: 14px;
    height: 14px;
    margin: -5px 0; /* centers handle vertically */
    border-radius: 7px;
}

QSlider::groove:vertical {
    border: 1px solid #ccc;
    width: 6px;
    background: #eee;
    border-radius: 3px;
}

QSlider::handle:vertical {
    background: #999;
    border: 1px solid #777;
    width: 12px;
    height: 12px;
    margin: 0 -4px; /* centers handle horizontally */
    border-radius: 6px;
}

QComboBox, QSpinBox {
    margin-bottom: 4px;
}
)");


    /******************************/

    this->setDockNestingEnabled(true);

    ui->dockCameras->setVisible(false);

    this->splitDockWidget(ui->dockLayers, strokeDock, Qt::Horizontal);

    ui->dockCameras->setMaximumWidth(350);
    strokeDock->setMaximumWidth(350);

    // Load settings


    loadSettings();
    //autoSaveAction->setChecked(autoSave);

    // Setup timers
    dirtyCheckTimer = new QTimer(this);
    dirtyCheckTimer->setTimerType(Qt::CoarseTimer);
    connect(dirtyCheckTimer, &QTimer::timeout, this, &MainWindow::onCheckDirtyTimer);
    dirtyCheckTimer->start(1000); // Every 1 second

    autoSaveTimer = new QTimer(this);
    autoSaveTimer->setTimerType(Qt::CoarseTimer);
    connect(autoSaveTimer, &QTimer::timeout, this, &MainWindow::onAutoSaveTimer);
    if (autoSave) {
        autoSaveTimer->start(30000); // Every 30 seconds
    }

    setTabletTracking(true);

    paint->getPaintArea()->setOutputResolution(
                ProjectContext::instance().projectJson()["resolution"].toArray()[0].toInt(),
                ProjectContext::instance().projectJson()["resolution"].toArray()[1].toInt());

    paint->getPaintArea()->setCanvasSize(
                ProjectContext::instance().projectJson()["canvas"].toArray()[0].toInt(),
                ProjectContext::instance().projectJson()["canvas"].toArray()[1].toInt());
    return;
    //
    QList <int> list;
    list += 150;
    list += 500;
    list += 500;

    ui->splitter->setSizes(list);
}

void MainWindow::setupDockPanels()
{
    // Create and configure shotPanel inside a scroll area
    shotPanel = new ShotPanelWidget;
    shotPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidget(shotPanel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);


    // 1. Ensure existing dockShots is set up
    ui->dockShots->setFeatures(QDockWidget::DockWidgetMovable |
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetClosable);
    ui->dockShots->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, ui->dockShots);  // Dock to left if not already

    // 2. Create new dock for Shots (scrollArea)
    QDockWidget *dockShotsNew = new QDockWidget(tr("Shots Panel"), this);
    dockShotsNew->setObjectName("dockShotsNew");
    dockShotsNew->setFeatures(QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetFloatable |
                              QDockWidget::DockWidgetClosable);
    dockShotsNew->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dockShotsNew->setWidget(scrollArea);  // Assuming scrollArea is your shots/timeline widget

    // 3. Create new dock for Script (perfectScript)
    perfectScript = new PerfectScriptWidget(this);
    QDockWidget *dockScript = new QDockWidget(tr("Script"), this);
    dockScript->setObjectName("dockScript");
    dockScript->setFeatures(QDockWidget::DockWidgetMovable |
                             QDockWidget::DockWidgetFloatable |
                             QDockWidget::DockWidgetClosable);
    dockScript->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dockScript->setWidget(perfectScript);

    // 4. Dock both new docks to left and tabify them with existing dockShots
    addDockWidget(Qt::LeftDockWidgetArea, dockShotsNew);
    addDockWidget(Qt::LeftDockWidgetArea, dockScript);

    // Tabify: Stack as tabs in the same area (order: dockShots on top, then Shots, then Script)
    tabifyDockWidget(ui->dockShots, dockShotsNew);
    tabifyDockWidget(dockShotsNew, dockScript);  // Chains them

    // 5. Raise the first tab (dockShots) as default
    ui->dockShots->raise();

    // 6. Optional: Set initial sizes (adjust as needed)
    ui->dockShots->setMinimumWidth(300);
    dockShotsNew->setMinimumWidth(300);
    dockScript->setMinimumWidth(300);

    ui->dockShots->setWindowTitle("Shot List");

    // Remove old splitter insertion if any
    // (No longer needed since docks handle layout)
}

void MainWindow::setWhiteTheme()
{
    printf("set to white\n");
    SetWhiteTheme();
}

void MainWindow::setDarkTheme()
{
    SetDarkTheme();
}

void MainWindow::setBlackTheme()
{
    SetBlackTheme();
}
MainWindow::~MainWindow()
{

}


void MainWindow::quit()
{
    exit(0);
}

void MainWindow::postIssue()
{

}

void MainWindow::teamEmail()
{

}

void MainWindow::login()
{

}

void MainWindow::update()
{
    static int update_count(0);
    printf("update %d\n", update_count);
    update_count++;

}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    printf("dragEnter\n");

    //if (event->mimeData()->hasFormat("text/plain"))
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    printf("Got dropEvent!\n");

    //
    //-----------------------------------------------
    //

    GameFusion::Str plainText = (char*)event->mimeData()->text().constData();
    printf("drop mime data = '%s'\n", (char*)plainText);

    const QMimeData* mimeData = event->mimeData();

    {
        QByteArray data = mimeData->data("FileNameW");
        QString filename = QString::fromUtf16((ushort*)data.data(), data.size() / 2);
        Str tmp = (char*)filename.toLatin1().constData();
        printf("**** fileNameW = %s\n", (char*)tmp);
    }

    // check for our needed mime type, here a file or a list of files
    if (mimeData->hasUrls())
    {
        QStringList pathList;
        QList<QUrl> urlList = mimeData->urls();

        // extract the local paths of the files
        for (int i = 0; i < urlList.size(); i++)
        {
            QString file = urlList.at(i).toLocalFile();
            Str file_str = (char*)file.toLatin1().constData();
            pathList.append(urlList.at(i).toLocalFile());
            printf("local path for file %d : %s\n", i, (char*)file_str);
        }
    }

    {

        QList<QUrl> urls = event->mimeData()->urls();

        for (int i = 0; i < urls.length(); i++)
        {
            printf("url %d\n", i);
            GameFusion::Str urlText = (char*)urls[i].fileName().toLatin1().constData();;
            printf("urlText '%s'\n", (char*)urlText);
        }
    }

    //textBrowser->setPlainText(event->mimeData()->text());
    //mimeTypeCombo->clear();
    //mimeTypeCombo->addItems(event->mimeData()->formats());

    event->acceptProposedAction();
}


void MainWindow::tabletEvent(QTabletEvent* event) {
    //cursorPos = event->posF().toPoint();
    float currentPressure = event->pressure(); // [0.0, 1.0]
    GameFusion::Log().info() << "Main Window pressure "<<(float)currentPressure<<"\n";
}

void MainWindow::about()
{
    int gmajor(1);
    int gminor(0);
    int rev(1);

    GameFusion::Str title;
    title.sprintf("DropPublish 2018 (v %d.%d)", gmajor, gminor);
    printf("DropPublish version : major %d minor %d rev %d", gmajor, gminor, rev);

    GameFusion::Str message;

    message += "Layout (c) 2018 - GAME FUSION - Andreas Carlen\n\n";
    //message += "Build SHA1 ID #173178c796e76ada82b749070314fc0dcd121f76\n\n";
    //
    message += GameFusion::Str().sprintf("Using OpenSSL, Qt 5.6 and GameFusion API", gmajor, gminor, rev);
    QMessageBox::about(0, "About", (char*)message);


}

#include <QPointer>
static QPointer<MainWindow> g_mainWindowInstance = nullptr;

bool MainWindow::initializeLlamaClient()
{
    QString modelPath ="/Users/andreascarlen/Library/Application Support/StarGit/models/Qwen2.5-7B-Instruct-1M-Q4_K_M.gguf";
    QString backend = "Metal";
    if (llamaClient) {
        return true; // Already initialized
    }

#ifdef _WIN32
    llamaClient = LlamaClient::Create(backend.toStdString(), "LlamaEngine.dll");
#elif __APPLE__
    llamaClient = LlamaClient::Create(backend.toStdString(), "LlamaEngine.dylib");
#endif
    if (!llamaClient) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to create LlamaClient: %1").arg(QString::fromStdString(LlamaClient::GetCreateError())));
        return false;
    }

    // Define parameter values
    float temperature = 0.7f;
    int max_tokens = 4096*4;

    // Initialize ModelParameter array
    ModelParameter params[] = {
        {"temperature", PARAM_FLOAT, &temperature},
        {"context_size", PARAM_INT, &max_tokens},
        {"max_tokens", PARAM_INT, &max_tokens}
    };

    // Set global GUI-safe pointer
    g_mainWindowInstance = this;

    if (!llamaClient->loadModel(modelPath.toStdString(), params, 2, [](const char* msg) {
            qDebug() << "Model load:" << msg;

            Log().info() << msg;
            if(Str(msg).startsWith("[ERROR]")){
                Log().error() << msg;

                // Call GUI code from main thread
                QString message = QString::fromUtf8(msg);
                QMetaObject::invokeMethod(g_mainWindowInstance, [message]() {
                    QMessageBox::critical(g_mainWindowInstance, QObject::tr("Llama Engine Error"), message);
                }, Qt::QueuedConnection);
            }
        })) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load Llama model: %1").arg(modelPath));
        delete llamaClient;
        llamaClient=nullptr;
        return false;
    }

    llamaModel = new LlamaModel(nullptr, llamaClient);

    return llamaClient->createSession(1);
}

void addShot(QTreeWidgetItem* shotItem, const GameFusion::Shot &shot, float fps) {

    QString cameraText = QString::fromStdString(shot.camera.movement + " / " + shot.camera.framing);
    QString audioText = QString::fromStdString(shot.audio.ambient);
    if (!shot.audio.sfx.empty()) {
        audioText += " | SFX: ";
        for (size_t i = 0; i < shot.audio.sfx.size(); ++i) {
            audioText += QString::fromStdString(shot.audio.sfx[i]);
            if (i < shot.audio.sfx.size() - 1) audioText += ", ";
        }
    }

    /*
    QString characterDialogSummary;
    for (const auto& c : shot.characters) {
        QString line = QString::fromStdString(c.name);
        if (!c.dialogue.empty()) {
            line += ": ";
            if (!c.dialogParenthetical.empty())
                line += "(" + QString::fromStdString(c.dialogParenthetical) + ") ";
            line += QString::fromStdString(c.dialogue);
        }
        characterDialogSummary += line + "\n";
    }
    characterDialogSummary = characterDialogSummary.trimmed();
    */

    shotItem->setText(0, QString::fromStdString(shot.name));
    shotItem->setText(1, QString::fromStdString(shot.type));
    shotItem->setText(2, QString::fromStdString(shot.transition));
    shotItem->setText(3, cameraText);
    shotItem->setText(4, QString::fromStdString(shot.lighting));
    shotItem->setText(5, audioText);
    shotItem->setText(6, QString::fromStdString(shot.description));
    shotItem->setText(7, QString::number(shot.frameCount));
    shotItem->setText(8, QString::fromStdString(shot.timeOfDay));
    shotItem->setText(9, shot.restore ? "true" : "false");
    shotItem->setText(10, QString::fromStdString(shot.fx));
    shotItem->setText(11, QString::fromStdString(shot.notes));
    shotItem->setText(12, QString::fromStdString(shot.intent));
    //shotItem->setText(13, characterDialogSummary);

    if(!shot.characters.empty()){
        for (const auto& c : shot.characters) {
            auto* charItem = new QTreeWidgetItem(shotItem);
            charItem->setText(0, QString::fromStdString(c.name));
            charItem->setText(1, QString::number(c.dialogNumber));
            charItem->setText(2, QString::fromStdString(c.emotion));
            charItem->setText(3, QString::fromStdString(c.intent));
            charItem->setText(4, c.onScreen ? "on screen" : "off screen");
            charItem->setText(5, QString::fromStdString(c.dialogParenthetical));
            charItem->setText(6, QString::fromStdString(c.dialogue));
        }
    }

    float mspf = fps > 0 ? 1000./fps : 1;

    if (!shot.panels.empty()) {
        for (const auto& panel : shot.panels) {
            QTreeWidgetItem* panelItem = new QTreeWidgetItem(shotItem);
            panelItem->setText(0, QString::fromStdString(panel.name));
            panelItem->setText(1, QString::fromStdString(panel.description));
            panelItem->setText(2, QString::number(panel.startTime/mspf));
            panelItem->setText(3, QString::number(panel.durationTime/mspf));
            panelItem->setText(4, QString::fromStdString(panel.uuid));

            // Store UUID or full Panel pointer (if lifetime is stable)
            panelItem->setData(0, Qt::UserRole, QString::fromStdString(panel.uuid));
        }
    }
}

void MainWindow::importAudioTrack(){
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import Audio File Track"), "", tr("Audio Files (*.wav; *.aiff)"));;

    if (fileName.isEmpty()) {
        return; // User canceled
    }

    Track *track = new Track("Track 3", 0, 150000, TrackType::NonLinearMedia);

    TrackItem *trx = timeLineView->addTrack(track);
    trx->loadAudio("WarnerTest", fileName.toUtf8().constData());
}

void MainWindow::importAudioSegment() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import Audio File Track"), "", tr("Audio Files (*.wav; *.aiff)"));;

    if (fileName.isEmpty()) {
        return; // User canceled
    }

    QGraphicsScene *gfxscene = timeLineView->scene();
    TrackItem *track = timeLineView->getTrack(1);

    AudioSegment *segment = new AudioSegment(gfxscene, 600, 500);
    segment->loadAudio("Audio Test", fileName.toUtf8().constData());
    track->addSegment(segment);
}

void MainWindow::importScript()
{

    // Open file dialog to select .fdx file
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import Script"), "", tr("Final Draft Files (*.fdx)"));
    if (fileName.isEmpty()) {
        return; // User canceled
    }

    // Initialize LlamaClient if not already done (adjust model path as needed)
    if (!initializeLlamaClient()) {
        return;
    }

    // Create or update ScriptBreakdown instance
    if(scriptBreakdown)
        delete scriptBreakdown; // Clean up previous instance

    GameScript* dictionary = NULL;
    GameScript* dictionaryCustom = NULL;

    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();

    scriptBreakdown = new ScriptBreakdown(fileName.toStdString().c_str(), fps, dictionary, dictionaryCustom, llamaClient, logger);


    // Threaded breakdown using QThread
    QThread* thread = new QThread;
    BreakdownWorker* worker = new BreakdownWorker(scriptBreakdown);

    worker->moveToThread(thread);

    // Connect signals
    connect(thread, &QThread::started, worker, &BreakdownWorker::process);
    connect(worker, &BreakdownWorker::finished, this, [=](bool success) {
        thread->quit();
        thread->wait();
        worker->deleteLater();
        thread->deleteLater();

        if (!success) {
            QMessageBox::critical(this, tr("Error"), tr("Failed to process script: %1").arg(fileName));
            delete scriptBreakdown;
            scriptBreakdown = nullptr;
            return;
        }

        // Reuse your existing UI update code here ↓
        ui->shotsTreeWidget->clear();

        //const auto& shots = scriptBreakdown->getShots();

        const auto& sequences = scriptBreakdown->getSequences();
        if (!sequences.empty()) {
            for (const auto& seq : sequences) {
                QTreeWidgetItem* seqItem = new QTreeWidgetItem(ui->shotsTreeWidget);


                seqItem->setText(0, QString::fromStdString(seq.name));
                seqItem->setIcon(1, QIcon("sequence_icon.png"));
                for (const auto& scene : seq.scenes) {
                    QTreeWidgetItem* sceneItem = new QTreeWidgetItem(seqItem);

                    sceneItem->setText(0, QString::fromStdString(scene.name));
                    sceneItem->setIcon(1, QIcon("scene_icon.png"));
                    for (const auto& shot : scene.shots) {
                        QTreeWidgetItem* shotItem = new QTreeWidgetItem(sceneItem);
                        addShot(shotItem, shot, fps);

                    }
                }
           }
        } else {
            const auto& scenes = scriptBreakdown->getScenes();
            for (const auto& scene : scenes) {
                if(scene.markedForDeletion)
                    continue;
                QTreeWidgetItem* sceneItem = new QTreeWidgetItem(ui->shotsTreeWidget);
                sceneItem->setText(0, QString::fromStdString(scene.name));
                sceneItem->setIcon(1, QIcon("scene_icon.png"));
                for (const auto& shot : scene.shots) {
                    QTreeWidgetItem* shotItem = new QTreeWidgetItem(sceneItem);
                    addShot(shotItem, shot, fps);
                }
            }
        }

        ui->shotsTreeWidget->expandAll();

        QMessageBox::information(this, tr("Success"), tr("Script imported successfully: %1").arg(fileName));
    });

    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    // Optional per-shot signal
    /*
    connect(worker, &BreakdownWorker::shotCreated, this, [=](const QString& sequence, const QString& scene, const QString& shotName) {
        // Implement UI progressive update here if breakdownScript supports emitting per shot
    });
    */
    thread->start();
}

void MainWindow::consoleCommandActivated(int index)
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(sender());
    if (!comboBox) {
        qWarning("consoleCommandActivated: sender is not a QComboBox!");
        return;
    }

    QString commandText = comboBox->itemText(index);
    if(consoleCommand(commandText))
        comboBox->setCurrentText("");
}



void MainWindow::updateShots(){
    if(!scriptBreakdown) return;

    ui->shotsTreeWidget->clear();

    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();

    const auto& shots = scriptBreakdown->getShots();
    for (const auto& shot : shots) {
        QTreeWidgetItem* shotItem = new QTreeWidgetItem(ui->shotsTreeWidget);
        addShot(shotItem, shot, fps);
    }

    ui->shotsTreeWidget->expandAll();
}

void MainWindow::updateScenes(){
    if(!scriptBreakdown) return;

    ui->shotsTreeWidget->clear();

    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();

    const auto& scenes = scriptBreakdown->getScenes();
    for (const auto& scene : scenes) {
        if(scene.markedForDeletion)
            continue;
        QTreeWidgetItem* sceneItem = new QTreeWidgetItem(ui->shotsTreeWidget);
        sceneItem->setText(0, QString::fromStdString(scene.name));
        sceneItem->setIcon(1, QIcon("scene_icon.png"));
        for (const auto& shot : scene.shots) {
            QTreeWidgetItem* shotItem = new QTreeWidgetItem(sceneItem);
            addShot(shotItem, shot, fps);
        }
    }

    ui->shotsTreeWidget->expandAll();
}

//

// In MainWindow.cpp
ShotSegment* MainWindow::createShotSegment(GameFusion::Shot& shot, GameFusion::Scene& scene, CursorItem* sceneMarker) {
    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    qreal mspf = 1000.0f / fps;

    // Calculate shot timing
    qreal shotTimeStart = shot.startTime == -1 ? 0 : shot.startTime;
    qreal shotDuration = shot.endTime == -1 ? shot.frameCount * mspf : shot.endTime - shotTimeStart;

    // Create ShotSegment
    ShotSegment* segment = new ShotSegment(timeLineView->scene(), shotTimeStart, shotDuration);

    segment->setUuid(shot.uuid.c_str());
    if (sceneMarker) {
        segment->setSceneMarker(sceneMarker);
        //sceneMarker->setTimePosition(shotTimeStart); // Sync scene marker position
    }

    // Set callbacks
    segment->setSegmentUpdateCallback([this, mspf](Segment* segment) {
        ShotContext shotContext = findShotByUuid(segment->getUuid().toStdString());
        if (shotContext.isValid()) {
            shotContext.scene->dirty = true;
            updateWindowTitle(true);
            shotContext.shot->startTime = segment->timePosition();
            shotContext.shot->endTime = segment->timePosition() + segment->getDuration();
            shotContext.shot->frameCount = segment->getDuration() / mspf;
            GameFusion::Log().info() << "Updated shot " << shotContext.shot->name.c_str()
                                     << " with UUID: " << shotContext.shot->uuid.c_str()
                                     << ", new start time: " << shotContext.shot->startTime
                                     << ", new end time: " << shotContext.shot->endTime << "\n";
            ShotSegment* shotSegment = dynamic_cast<ShotSegment*>(segment);
            if (shotSegment) {
                CursorItem *sceneMarker = shotSegment->getSceneMarkerPointer();
                if (sceneMarker)
                    sceneMarker->setTimePosition(segment->timePosition());
            }
        } else {
            GameFusion::Log().info() << "Shot not found for UUID: " << segment->getUuid().toUtf8().constData() << "\n";
        }
    });

    segment->setMarkerUpdateCallback([this](const QString& uuid, float newStartTime, float newDuration) {
        PanelContext panelContext = findPanelByUuid(uuid.toStdString());
        if (panelContext.isValid()) {
            panelContext.scene->dirty = true;
            updateWindowTitle(true);
            panelContext.panel->startTime = newStartTime;
            panelContext.panel->durationTime = newDuration;
            GameFusion::Log().info() << "Updated panel with UUID: " << uuid.toUtf8().constData()
                                     << ", new start time: " << newStartTime
                                     << ", new duration: " << newDuration << "\n";
        } else {
            GameFusion::Log().info() << "Panel not found for UUID: " << uuid.toUtf8().constData() << "\n";
        }
    });

    Log().info() << "  Shot Start Frame: " << shot.name.c_str() << " " << (int)(shot.startTime/mspf)
                 << " ms start: " << (float)shot.startTime << "\n";

    // Set shot and panel labels
    segment->marker()->setShotLabel(shot.name.c_str());
    //segment->marker()->setPanelName(shot.name.c_str());

    // Handle panels
    int panelIndex = 0;
    for (const auto& panel : shot.panels) {
        QString thumbnailPath = ProjectContext::instance().currentProjectPath() + "/thumbnails/panel_" + panel.uuid.c_str() + ".png";
        if (!QFile::exists(thumbnailPath)) {
            thumbnailPath = ProjectContext::instance().currentProjectPath() + "/movies/" + panel.thumbnail.c_str();
        }

        PanelMarker* panelMarker = (panelIndex == 0) ? segment->marker() : new PanelMarker(0, 30, 0, 220, segment, panel.name.c_str(), "", shot.uuid.c_str(), "");
        panelMarker->setShotUuid(shot.uuid.c_str());
        panelMarker->setPanelName(QString::fromStdString(panel.name));
        panelMarker->loadThumbnail(thumbnailPath);
        panelMarker->setUuid(panel.uuid.c_str());
        panelMarker->setStartTimePos(panel.startTime);
        panelIndex++;
    }

    // Update markers
    segment->updateMarkersEndTime();

    return segment;
}

void MainWindow::insertShotSegment(const GameFusion::Shot& shot, ShotIndices shotIndices, const GameFusion::Scene sceneRef, double cursorTime, CursorItem *sceneMarker) {
    GameFusion::Scene* scene = findSceneByUuid(sceneRef.uuid);
    //int insertIndex = 0;

    bool createSceneMarker = false;

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    qreal mspf = 1000.0f / fps;

    // TODO if scene does not exist then we need to create it - not sure about this
    /*
    if(!scene){
        GameFusion::Scene newScene;
        newScene.uuid = sceneRef.uuid;
        newScene.name = sceneRef.name;
        scriptBreakdown->getScenes().push_back(newScene);
        scene = &scriptBreakdown->getScenes().back();
        GameFusion::Log().info() << "Created new scene with UUID " << scene->uuid.c_str() << "\n";

        createSceneMarker = true;
    }
    */

    if (scene) {
        auto& shots = scene->shots;

        // Insert shot into scene
        shots.insert(shots.begin() + shotIndices.shotIndex, shot);
        GameFusion::Shot &insertedShot = shots.at(shotIndices.shotIndex);

        // Update scene state
        scene->dirty = true;
        //scriptBreakdown->updateShotTimings(*scene);

        // Create scene marker if this is the first and only shot in a new scene
        //CursorItem *sceneMarker = nullptr;
        /*
        if(createSceneMarker) {
            sceneMarker = timeLineView->addSceneMarker(shot.startTime, scene->name.c_str());
            sceneMarker->setUuid(scene->uuid.c_str());
            GameFusion::Log().debug() << "Created scene marker for UUID " << scene->uuid.c_str() << "\n";
        }
        */

        // Create and insert ShotSegment
        ShotSegment* shotSegment = createShotSegment(insertedShot, *scene, sceneMarker);
        TrackItem* track = timeLineView->getTrack(0);
       // int segmentIndex = track->insertSegmentAtIndex(shotIndices.segmentIndex, shotSegment);

        int segmentIndex = shotIndices.segmentIndex;
        track->addSegment(shotSegment);
        track->updateSegmentPositions();
        track->update();


        // Shift camera keyframes for subsequent shots
        long shotDuration = insertedShot.endTime - insertedShot.startTime;
        // Shift camera keyframes for subsequent shots
        for (auto& allScene : scriptBreakdown->getScenes()) {
            for (auto& allShot : allScene.shots) {
                if (allShot.startTime >= insertedShot.endTime) {
                    for (auto& camera : allShot.cameraAnimation.frames) {
                        CameraKeyFrame *cameraKeyFrame = timeLineView->getCameraKeyFrame(camera.uuid.c_str());
                        if(cameraKeyFrame){
                            long oldTime = cameraKeyFrame->timeMs;
                            cameraKeyFrame->timeMs += shotDuration;
                            //timeLineView->cameraKeyFrameUpdated(camera.uuid.c_str(), cameraKeyFrame->timeMs-shotDuration, camera.panelUuid.c_str());
                            GameFusion::Log().debug() << "Shifted camera keyframe " << camera.uuid.c_str() << " to time from " << oldTime << " to " << cameraKeyFrame->timeMs << "\n";
                        }
                        else{
                            GameFusion::Log().debug() << "Shifted camera errer, not found in timeline " << camera.uuid.c_str() << "\n";
                        }
                    }
                }
            }
        }

        // Add new camera keyframes
        for(auto &camera: insertedShot.cameraAnimation.frames) {
            QString panelUuid = camera.panelUuid.c_str();
            long timeMs = -1;
            for (const auto& panel : insertedShot.panels) {
                if (panel.uuid == camera.panelUuid) {
                    timeMs = insertedShot.startTime + panel.startTime + camera.frameOffset * mspf;
                    GameFusion::Log().debug() << "Found parent panel start time " << panel.startTime;
                    break;
                }
            }
            if (timeMs != -1) {
                timeLineView->addCameraKeyFrame(camera.uuid.c_str(), timeMs, panelUuid);
                GameFusion::Log().debug() << "Adding camera at " << timeMs << " " << camera.uuid.c_str();
            }
        }

        // update the scene list widget
        updateScenes();

        // force update and timeline to postion
        timeLineView->setTimeCursor(cursorTime);

        GameFusion::Log().info() << "Inserted shot " << shot.uuid.c_str() << " at segment index " << shotIndices.segmentIndex << "\n";

        return;
    }

    return;
}

ShotIndices MainWindow::deleteShotSegment(ShotContext &shotContext, double currentTime, bool updateTime) {
    if(!shotContext.isValid()){
        GameFusion::Log().warning() << "Invalid ShotContext provided";
        return {-1, -1};
    }

    GameFusion::Shot toDelete = *shotContext.shot;
    double shotDuration = toDelete.endTime - toDelete.startTime; // Store duration for shifting

    // Remove from Scene.shots
    auto& shots = shotContext.scene->shots;
    auto it = std::find_if(shots.begin(), shots.end(),
                           [&](const GameFusion::Shot& s) { return s.uuid == shotContext.shot->uuid; });

    // compute the index value
    int shotIndex = -1;
    if (it != shots.end()) {
        // Compute the index
        shotIndex = static_cast<int>(std::distance(shots.begin(), it));

        // Remove camera keyframes
        for (const auto& camera : it->cameraAnimation.frames) {
            timeLineView->removeCameraKeyFrame(camera.uuid.c_str());
            GameFusion::Log().debug() << "Removed camera keyframe " << camera.uuid.c_str();
        }

        shots.erase(it);
        GameFusion::Log().info() << "Removed Shot " << shotContext.shot->uuid.c_str() << " from Scene";
    } else {
        GameFusion::Log().warning() << "Shot with UUID " << shotContext.shot->uuid.c_str() << " not found in Scene";
        return {-1, -1};
    }

    TrackItem* track = timeLineView->getTrack(0);
    int segmentIndex = track->deleteSegment(toDelete.uuid.c_str(), updateTime);

    // Shift camera keyframes for subsequent shots
    for (auto& scene : scriptBreakdown->getScenes()) {
        for (auto& shot : scene.shots) {
            if (shot.startTime >= toDelete.startTime) {
                for (auto& camera : shot.cameraAnimation.frames) {
                    CameraKeyFrame *cameraKeyFrame = timeLineView->getCameraKeyFrame(camera.uuid.c_str());
                    if(cameraKeyFrame){
                        long oldTime = cameraKeyFrame->timeMs;
                        cameraKeyFrame->timeMs -= shotDuration;
                        //timeLineView->cameraKeyFrameUpdated(camera.uuid.c_str(), cameraKeyFrame->timeMs-shotDuration, camera.panelUuid.c_str());

                        GameFusion::Log().debug() << "Shifted camera keyframe " << camera.uuid.c_str() << " to time from " << oldTime << " to " << cameraKeyFrame->timeMs << "\n";
                    }
                    else {
                        GameFusion::Log().debug() << "Shifted camera failed, camera not found in timeline " << camera.uuid.c_str() << "\n";

                    }
                }
            }
        }
    }

    // Update ScriptBreakdown
    //scriptBreakdown->updateShotTimings(*shotContext.scene);
    shotContext.scene->dirty = true;

    // Update UI and set timeline cursor
    updateScenes();
    timeLineView->setTimeCursor(currentTime);
    timeLineView->update();

    return {shotIndex, segmentIndex};
}

void MainWindow::addLayerKeyFrames(TrackItem* track, long panelStartTime, const GameFusion::Panel& panel) {
    qreal mspf = 1000.0f / ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    QString panelUuid = QString::fromStdString(panel.uuid);
    QString panelName = QString::fromStdString(panel.name);
    for (const auto& layer : panel.layers) {
        QString layerUuid = QString::fromStdString(layer.uuid);
        QString layerName = QString::fromStdString(layer.name);
        QString shotUuid;
        QString shotName;
        QString sceneUuid;
        QString sceneName;

        for (const auto& scene : scriptBreakdown->getScenes()) {
            for (const auto& shot : scene.shots) {
                for (const auto& p : shot.panels) {
                    if (p.uuid == panel.uuid) {
                        shotUuid = QString::fromStdString(shot.uuid);
                        shotName = QString::fromStdString(shot.name);
                        sceneUuid = QString::fromStdString(scene.uuid);
                        sceneName = QString::fromStdString(scene.name);
                        break;
                    }
                }
                if (!shotUuid.isEmpty()) break;
            }
            if (!shotUuid.isEmpty()) break;
        }

        // Add keyframes for motion
        AttributeTrackItem* motionAttr = track->getAttributeTrack("motion");
        if (!motionAttr) {
            motionAttr = track->addAttribute("motion");
        }
        for (const auto& kf : layer.motionKeyframes) {
            long timeMs = panelStartTime + kf.time * mspf;
            QVariantMap mot;
            mot["x"] = kf.x;
            mot["y"] = kf.y;
            mot["scale"] = kf.scale;
            mot["rotation"] = kf.rotation;
            mot["uuid"] = QString::fromStdString(kf.uuid);
            mot["shotUuid"] = shotUuid;
            mot["shotName"] = shotName;
            mot["panelUuid"] = panelUuid;
            mot["panelName"] = panelName;
            mot["layerUuid"] = layerUuid;
            mot["layerName"] = layerName;
            mot["sceneUuid"] = shotUuid;
            mot["sceneName"] = sceneName;

            motionAttr->addKeyframe(timeMs, mot);
        }

        // Add keyframes for opacity
        AttributeTrackItem* opacityAttr = track->getAttributeTrack("opacity");
        if (!opacityAttr) {
            opacityAttr = track->addAttribute("opacity");
        }
        for (const auto& kf : layer.opacityKeyframes) {
            long timeMs = panelStartTime + kf.time * mspf;
            QVariantMap op;
            op["opacity"] = kf.opacity;
            op["uuid"] = QString::fromStdString(kf.uuid);
            op["shotUuid"] = shotUuid;
            op["panelUuid"] = panelUuid;
            op["layerUuid"] = layerUuid;
            op["shotUuid"] = shotUuid;
            op["shotName"] = shotName;
            op["panelUuid"] = panelUuid;
            op["panelName"] = panelName;
            op["layerUuid"] = layerUuid;
            op["layerName"] = layerName;
            op["sceneUuid"] = shotUuid;
            op["sceneName"] = sceneName;

            opacityAttr->addKeyframe(timeMs, op);
        }
    }
}

void MainWindow::addTimelineKeyFrames(const GameFusion::Shot& shot) {

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    qreal mspf = 1000.0f / fps;

    for (const auto& camera : shot.cameraAnimation.frames) {
        GameFusion::Log().debug() << "Processing camera for shot " << shot.startTime;
        QString panelUuid = camera.panelUuid.c_str();
        long timeMs = -1;
        for (const auto& panel : shot.panels) {
            if (panel.uuid == camera.panelUuid) {
                timeMs = shot.startTime + panel.startTime + camera.frameOffset * mspf;
                GameFusion::Log().debug() << "Found parent panel start time " << panel.startTime;
                break;
            }
        }
        if (timeMs != -1) {
            timeLineView->addCameraKeyFrame(camera.uuid.c_str(), timeMs, panelUuid);
            GameFusion::Log().debug() << "Adding camera at " << timeMs << " " << camera.uuid.c_str();
        } else {
            GameFusion::Log().warning() << "No panel found for camera UUID " << camera.uuid.c_str();
        }
    }
}

QSet<double> MainWindow::getAllKeyframeGlobalTimes() const {
    QSet<double> times;
    if(!scriptBreakdown)
        return times;

    double fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    double mspf = 1000.0 / fps;
    for (const auto& scene : scriptBreakdown->getScenes()) {
        for (const auto& shot : scene.shots) {
            for (const auto& panel : shot.panels) {
                for (const auto& layer : panel.layers) {
                    for (const auto& kf : layer.motionKeyframes) {
                        double globalMs = shot.startTime + panel.startTime + static_cast<double>(kf.time) * mspf;
                        times.insert(globalMs);
                    }
                    for (const auto& kf : layer.opacityKeyframes) {
                        double globalMs = shot.startTime + panel.startTime + static_cast<double>(kf.time) * mspf;
                        times.insert(globalMs);
                    }
                }
            }
        }
    }
    return times;
}

void MainWindow::updateKeyframeDisplay() {
    TrackItem *track = timeLineView->getTrack(0);
    if (!track) return;

    AttributeTrackItem *agg = track->getAttributeTrack("animation");

    if(!agg)
        agg = track->addAttribute("animation");

    if (agg) {
        agg->aggregateTimes = getAllKeyframeGlobalTimes();
        agg->update();
    }


    // Clear and hide attribute tracks
    for (auto attr: track->attributeTracks()) {
        if(attr->name() == "animation")
            continue;
        attr->clearKeyframes();
        //attr->setVisible(false);
    }

    if (selectedLayerUuid.isEmpty() && selectedLayerName.isEmpty()) {
        track->setSeletedLayerName("-");
        timeLineView->updateHeaders();
        return;
    }

    LayerContext layerCts = findLayerByUuid(selectedLayerUuid.toStdString());
    //if(!layerCts.isValid()) return;



    GameFusion::Shot  *shot  = layerCts.shot;
    GameFusion::Panel *panel = layerCts.panel;
    GameFusion::Layer *layer = layerCts.layer;

    if(layer)
        selectedLayerName = layer->name.c_str();

    track->setSeletedLayerName(selectedLayerName);
    timeLineView->updateHeaders();

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    qreal mspf = 1000.0 / fps;

    AttributeTrackItem *motion = track->getAttributeTrack("motion");
    //if (motion) motion->setVisible(true);

    AttributeTrackItem *opacity = track->getAttributeTrack("opacity");
    //if (opacity) opacity->setVisible(true);

    bool localShotMode = false;

    if(localShotMode) {
        for (const auto& kf : layer->motionKeyframes) {
            qreal globalMs = static_cast<qreal>(shot->startTime) + panel->startTime + kf.time * mspf;
            qreal globalFr = static_cast<qreal>(globalMs / mspf);
            QVariantMap mot;
            mot["x"] = kf.x;
            mot["y"] = kf.y;
            mot["scale"] = kf.scale;
            mot["rotation"] = kf.rotation;
            mot["time"] = globalFr;
            mot["uuid"] = kf.uuid.c_str();
            mot["sceneName"] = layerCts.scene->name.c_str();
            mot["sceneUuid"] = layerCts.scene->uuid.c_str();
            mot["shotName"] = layerCts.shot->name.c_str();
            mot["shotUuid"] = layerCts.shot->uuid.c_str();
            mot["panelName"] = layerCts.panel->name.c_str();
            mot["panelUuid"] = layerCts.panel->uuid.c_str();
            mot["layerName"] = layerCts.layer->name.c_str();
            mot["layerUuid"] = layerCts.layer->uuid.c_str();
            if (motion) motion->addKeyframe(globalMs, mot);
        }
        for (const auto& kf : layer->opacityKeyframes) {
            qreal globalMs = static_cast<qreal>(shot->startTime) + panel->startTime + kf.time * mspf;
            qreal globalFr = static_cast<qreal>(globalMs / mspf);
            QVariantMap op;
            op["opacity"] = kf.opacity;
            op["time"] = globalFr;
            op["uuid"] = kf.uuid.c_str();
            op["sceneName"] = layerCts.scene->name.c_str();
            op["sceneUuid"] = layerCts.scene->uuid.c_str();
            op["shotName"] = layerCts.shot->name.c_str();
            op["shotUuid"] = layerCts.shot->uuid.c_str();
            op["panelName"] = layerCts.panel->name.c_str();
            op["panelUuid"] = layerCts.panel->uuid.c_str();
            op["layerName"] = layerCts.layer->name.c_str();
            op["layerUuid"] = layerCts.layer->uuid.c_str();
            if (opacity) opacity->addKeyframe(globalMs, op);
        }
    }
    else {
        std::string layerName = selectedLayerName.toStdString();

        if(layerName.empty())
            return;

        QMap<QString, QList<QPair<qreal, QVariant>>> globalMotionKeys;  // shotUuid -> (timeMs, value)
        QMap<QString, QList<QPair<qreal, QVariant>>> globalOpacityKeys;

        for (const auto& scene : scriptBreakdown->getScenes()) {
                for (const auto& shot : scene.shots) {
                    QString shotUuid = QString::fromStdString(shot.uuid);
                    for (const auto& panel : shot.panels) {
                        QString panelUuid = QString::fromStdString(panel.uuid);
                        for (const auto& layer : panel.layers) {
                            if (layer.name == layerName) {
                                for (const auto& kf : layer.motionKeyframes) {
                                    qreal globalMs = shot.startTime + panel.startTime + kf.time * mspf;
                                    qreal globalFr = static_cast<qreal>(globalMs / mspf);
                                    QVariantMap mot;
                                    mot["x"] = kf.x;
                                    mot["y"] = kf.y;
                                    mot["scale"] = kf.scale;
                                    mot["rotation"] = kf.rotation;
                                    mot["time"] = globalFr;
                                    mot["uuid"] = QString::fromStdString(kf.uuid);
                                    mot["sceneName"] = QString::fromStdString(scene.name);
                                    mot["sceneUuid"] = QString::fromStdString(scene.uuid);
                                    mot["shotName"] = QString::fromStdString(shot.name);
                                    mot["shotUuid"] = QString::fromStdString(shot.uuid);
                                    mot["panelName"] = QString::fromStdString(panel.name);
                                    mot["panelUuid"] = QString::fromStdString(panel.uuid);
                                    mot["layerName"] = QString::fromStdString(layer.name);
                                    mot["layerUuid"] = QString::fromStdString(layer.uuid);
                                    globalMotionKeys[shotUuid].append({globalMs, QVariant(mot)});
                                }
                                for (const auto& kf : layer.opacityKeyframes) {
                                    qreal globalMs = shot.startTime + panel.startTime + kf.time * mspf;
                                    qreal globalFr = static_cast<qreal>(globalMs / mspf);
                                    QVariantMap op;
                                    op["opacity"] = kf.opacity;
                                    op["time"] = globalFr;
                                    op["uuid"] = QString::fromStdString(kf.uuid);
                                    op["sceneName"] = QString::fromStdString(scene.name);
                                    op["sceneUuid"] = QString::fromStdString(scene.uuid);
                                    op["shotName"] = QString::fromStdString(shot.name);
                                    op["shotUuid"] = QString::fromStdString(shot.uuid);
                                    op["panelName"] = QString::fromStdString(panel.name);
                                    op["panelUuid"] = QString::fromStdString(panel.uuid);
                                    op["layerName"] = QString::fromStdString(layer.name);
                                    op["layerUuid"] = QString::fromStdString(layer.uuid);
                                    globalOpacityKeys[shotUuid].append({globalMs, QVariant(op)});
                                }
                            }
                        }
                    }
                }
            }

            AttributeTrackItem *motion = track->getAttributeTrack("motion");
            if (motion) {
                motion->setVisible(true);
                for (const auto& shotKeys : globalMotionKeys) {
                    for (const auto& kv : shotKeys) {
                        motion->addKeyframe(kv.first, kv.second);
                    }
                }
            }

            AttributeTrackItem *opacity = track->getAttributeTrack("opacity");
            if (opacity) {
                opacity->setVisible(true);
                for (const auto& shotKeys : globalOpacityKeys) {
                    for (const auto& kv : shotKeys) {
                        opacity->addKeyframe(kv.first, kv.second);
                    }
                }
            }
    }

}

void MainWindow::updateTimeline(){

    auto& scenes = scriptBreakdown->getScenes();

    episodeDuration.durationMs = 0;
    episodeDuration.frameCount = 0;

    TrackItem * track = timeLineView->getTrack(0);
    QGraphicsScene *gfxscene = timeLineView->scene();

    //gfxscene->clear();

    qreal fps = 25;
    fps = ProjectContext::instance().projectJson()["fps"].toDouble();
    qreal mspf = 1000.f/fps;
    //long startTimeFrame = episodeDuration.frameCount;
    long startTime = 0;
    for (auto& scene : scenes) {

        Log().info() << "Scene Start Frame: " << scene.name.c_str() << " " << startTime << "\n";

        CursorItem *sceneMarker = timeLineView->addSceneMarker(startTime, scene.name.c_str());
        sceneMarker->setUuid(scene.uuid.c_str());

        for (auto& shot : scene.shots) {
            ShotSegment* segment = createShotSegment(shot, scene, sceneMarker);
            track->addSegment(segment);

            addTimelineKeyFrames(shot);


            for (const auto& panel : shot.panels) {
                addLayerKeyFrames(track, shot.startTime + panel.startTime, panel);
            }

            startTime = shot.endTime;
            sceneMarker = nullptr; // Only link first shot to scene marker
        }

        // After each scene, update the global episode duration
        episodeDuration.frameCount = startTime / mspf;
        episodeDuration.durationMs = startTime;
    }

    updateKeyframeDisplay();
}

void MainWindow::updateCharacters(){
    if(!scriptBreakdown) return;

    ui->shotsTreeWidget->clear();

    const auto& characters = scriptBreakdown->getCharacters();
    for (const auto& character : characters) {
        QTreeWidgetItem* characterItem = new QTreeWidgetItem(ui->shotsTreeWidget);

        characterItem->setText(0, QString::fromStdString(character.name));                          // Column 0: Name
        characterItem->setText(1, QString::fromStdString(character.emotion));                      // Column 1: Emotion
        characterItem->setText(2, QString::fromStdString(character.intent));                       // Column 2: Intent
        characterItem->setText(3, character.onScreen ? "true" : "false");                          // Column 3: OnScreen
        characterItem->setText(4, QString::number(character.dialogNumber));                        // Column 4: Dialogue #
        characterItem->setText(5, QString::fromStdString(character.dialogParenthetical));          // Column 5: Parenthetical
        characterItem->setText(6, QString::fromStdString(character.dialogue));                     // Column 6: Line
    }

    ui->shotsTreeWidget->expandAll();
}

bool MainWindow::consoleCommand(const QString &the_command_line)
{
    Log().info() << the_command_line.toUtf8().constData() << "\n";

    if(the_command_line == "printShots"){
        if(scriptBreakdown)
            scriptBreakdown->printShots();
        return true;
    }

    if(the_command_line == "printScenes"){
        if(scriptBreakdown)
            scriptBreakdown->printScenes();
        return true;
    }

    if(the_command_line == "printCharacters"){
        if(scriptBreakdown)
            scriptBreakdown->printCharacters();
        return true;
    }

    if(the_command_line == "updateShots"){
        updateShots();
        return true;
    }

    if(the_command_line == "updateScenes"){
        updateScenes();
        return true;
    }

    if(the_command_line == "updateCharacters"){
        updateCharacters();
        return true;
    }

    // Initialize LlamaClient if not already done (adjust model path as needed)
    if (!initializeLlamaClient()) {
        return false;
    }

    if(llamaModel){

        if(the_command_line == "lmstat"){
            QString info = llamaModel->getContextInfo();
            Log().info() << info.toUtf8().constData() << "\n";
            return true;
        }

        QString prompt = the_command_line;
        QElapsedTimer timer;
        timer.start();
        QString response = llamaModel->generateResponse(
            prompt,
            [](const QString &msg){
                Log().info() << msg.toUtf8().constData();
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            },
            [](const QString final){
                Log().info() << "\n";
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
            );

        qint64 processingTimeMs = timer.elapsed();

        QJsonObject contextJson;
        contextJson["response"] = response;
        contextJson["context_info"] = llamaModel->getContextInfo();
        contextJson["model"] =  llamaModel->getModelFile();
        contextJson["vendor"] = llamaModel->getVendor();
        contextJson["location"] = llamaModel->getLocation();
        contextJson["api_tech"] = llamaModel->getApiTech();
        contextJson["processing_time"] = processingTimeMs;
        logger->logPromptAndResult("Text", "console", prompt.toStdString(), response.toStdString(), llamaModel->getLatestTokenCount(), llamaModel->getLatestCost(), contextJson);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        if (!doc.isNull()) {
           // Process JSON response (e.g., update UI)
           Log::info() << "Console command processed: " << prompt.toUtf8().constData() << "\n";
        } else {
           Log::error("Invalid JSON response from console command\n");
        }

        qDebug() << "Response: " << response;
    }
    else
        return false;

    return true;
}

bool loadJsonWithDetails(const QString &filePath, QJsonDocument &docOut, QString &errorOut)
{
    QFile file(filePath);
    if (!file.exists()) {
        errorOut = QString("File does not exist: %1").arg(filePath);
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        errorOut = QString("Could not open file for reading: %1").arg(filePath);
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    docOut = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        int offset = parseError.offset;
        int line = 1;
        int lineStart = 0;
        int lineEnd = jsonData.size();

        // Find line number
        for (int i = 0; i < offset && i < jsonData.size(); ++i) {
            if (jsonData[i] == '\n') {
                ++line;
                lineStart = i + 1;
            }
        }

        // Find end of line
        for (int i = offset; i < jsonData.size(); ++i) {
            if (jsonData[i] == '\n') {
                lineEnd = i;
                break;
            }
        }

        QByteArray lineContent = jsonData.mid(lineStart, lineEnd - lineStart).trimmed();

        errorOut = QString(
                       "JSON parse error in %1:\n"
                       "→ %2\n"
                       "At offset %3 (line %4):\n"
                       "%5")
                       .arg(filePath)
                       .arg(parseError.errorString())
                       .arg(offset)
                       .arg(line)
                       .arg(QString::fromUtf8(lineContent));
        return false;
    }

    return true;
}

void MainWindow::loadProject() {
    QString projectDir = QFileDialog::getExistingDirectory(this, "Select Project Folder");
    if (projectDir.isEmpty())
        return;

    loadProject(projectDir);
}

void MainWindow::loadProject(QString projectDir){

    // Clear the undo stack
    undoStack->clear();

    currentPanelUuid.clear();

    if(scriptBreakdown){
        delete scriptBreakdown;
        scriptBreakdown = nullptr;
    }
    timeLineView->clear();

    currentPanel = nullptr;

    paint->newImage();

    QString projectFilePath = QDir(projectDir).filePath("project.json");
    QJsonDocument projectDoc;
    QString errorMsg;
    if (!loadJsonWithDetails(projectFilePath, projectDoc, errorMsg)) {
        QMessageBox::critical(this, "Error", errorMsg);
        Log().info() << errorMsg.toUtf8().constData();
        return;
    }

    if (!projectDoc.isObject()) {
        QString msg = "project.json does not contain a top-level JSON object.";
        QMessageBox::critical(this, "Error", msg);
        Log().info() << msg.toUtf8().constData() << "\n";
        return;
    }

    // Validate required fields
    //QJsonObject projectObj = projectDoc.object();
    ProjectContext::instance().projectJson() = projectDoc.object();
    QStringList requiredFields = { "projectName", "fps" };
    QStringList missingFields;

    ProjectContext::instance().projectJson()["projectPath"] = projectDir;
    /*
    ProjectContext::instance().projectJson()["resolution"] = QJsonArray{1920, 1080};
    ProjectContext::instance().projectJson()["canvas_margin"] = QString("20");
    ProjectContext::instance().projectJson()["canvas_percent"] = 20;
    ProjectContext::instance().projectJson()["canvas"] = QJsonArray{2304, 1296}; // For 20% margin: 1920 + 384, etc.
*/
    for (const QString &field : requiredFields) {
        if (!ProjectContext::instance().projectJson().contains(field)) {
            missingFields << field;
        }
    }

    if (!missingFields.isEmpty()) {
        QString msg = QString("Missing required field(s) in %1:\n→ %2")
                          .arg(projectFilePath)
                          .arg(missingFields.join(", "));
        QMessageBox::critical(this, "Invalid Project", msg);
        Log().info() << msg.toUtf8().constData();
        return;
    }

    // (Optional) Store project metadata
    ProjectContext::instance().setCurrentProjectName( ProjectContext::instance().projectJson()["projectName"].toString() );
    ProjectContext::instance().setCurrentProjectPath( projectDir );

    // Set project path in logger
    logger->setProjectPath(projectDir);

    paint->getPaintArea()->setProjectPath(projectDir);
    int newCanvasW = ProjectContext::instance().projectJson()["canvas"].toArray()[0].toInt();
    int newCanvasH = ProjectContext::instance().projectJson()["canvas"].toArray()[1].toInt();
    int newResW = ProjectContext::instance().projectJson()["resolution"].toArray()[0].toInt();
    int newResH = ProjectContext::instance().projectJson()["resolution"].toArray()[1].toInt();
    paint->getPaintArea()->setCanvasSize(newCanvasW, newCanvasH);
    paint->getPaintArea()->setOutputResolution(newResW, newResH);
    paint->getPaintArea()->update();


    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();
    //scriptBreakdown = new ScriptBreakdown("", fps);
    loadScript();

    // Load scenes
    QDir scenesDir(QDir(projectDir).filePath("scenes"));

    qDebug() << "Scenes directory absolute path:" << scenesDir.absolutePath();
    Log().info() << "Looking for scenes in: " << scenesDir.absolutePath().toUtf8().constData();

    if (!scenesDir.exists()) {
        QMessageBox::critical(this, "Missing Folder", "scenes/ folder not found in project.");
        return;
    }

    qDebug() << "Scenes directory contains:" << scenesDir.entryList(QDir::AllEntries);
    Log().info() << "Entries in scenes directory:"
                 << scenesDir.entryList(QDir::AllEntries).join(", ").toUtf8().constData();


    bool foundErrors = false;
    QStringList errors;
    QStringList sceneFiles = scenesDir.entryList(QDir::Files);
    for (const QString &fileName : sceneFiles) {

        if (!fileName.endsWith(".json", Qt::CaseInsensitive)){
            Log().info() << "Skipping file "<< fileName.toUtf8().constData() << "\n";
            continue;
        }

        Log().info() << "Processing file "<< fileName.toUtf8().constData() << "\n";

        QString baseName = QFileInfo(fileName).completeBaseName(); // Strip .json
        QStringList parts = baseName.split('_');

        if (parts.size() < 2) {
            qDebug() << "Skipping malformed scene file:" << fileName;
            Log().info() << "Skipping malformed scene file:" << fileName.toUtf8().constData() << "\n";
            foundErrors = true;
            errors << "Skipping malformed scene file: " + fileName;
            continue;
        }

        QString sceneId = parts[0];  // "0001"
        QString sceneName = parts.mid(1).join('_'); // Rejoin the rest, e.g. "SCENE_002"

        QJsonDocument sceneDoc;
        QString errorMsg;
        QString sceneFilePath = scenesDir.filePath(fileName);

        if (!loadJsonWithDetails(sceneFilePath, sceneDoc, errorMsg)) {
            Log().info() << errorMsg.toUtf8().constData();
            foundErrors = true;
            errors << errorMsg;
            continue;
        }

        QJsonObject sceneObj;
        if(sceneDoc.isArray()) {
            sceneObj["shots"] = sceneDoc.array();
            sceneObj["sceneId"] = sceneId;
            sceneObj["name"] = sceneName;
        }
        else if (sceneDoc.isObject()) {
            sceneObj = sceneDoc.object();
            if(!sceneObj.contains("shots"))
                sceneObj["shots"] = QJsonArray();
            if(!sceneObj.contains("sceneId"))
                sceneObj["sceneId"] = sceneId;
            if(!sceneObj.contains("name"))
                sceneObj["name"] = sceneName;
        }
        else{
            qWarning() << "Invalid JSON format in" << fileName;
            Log().info() << "Invalid JSON format in" << fileName.toUtf8().constData() << "\n";
            errors << "Invalid JSON format in " + fileName;
            foundErrors = true;
            continue;
        }

        scriptBreakdown->loadScene(sceneName, sceneObj, QFileInfo(fileName).fileName());  // Assuming you have this method
    }

    if(foundErrors){


        ErrorDialog errorDialog("Errors occurred while loading project", errors, this);
        errorDialog.exec();
    }
    else {
        //QMessageBox::information(this, "Project Loaded", "Project loaded successfully.");
        Log().info() << "Project loaded successfully.\n";
    }

    updateScenes();
    updateTimeline();

    loadAudioTracks();

    // Post-load: Select the earliest panel by startTime across all scenes
    if (scriptBreakdown) {
        qint64 earliestTime = -1;
        QTreeWidgetItem* targetItem = nullptr;
        QTreeWidgetItemIterator it(ui->shotsTreeWidget);
        float fps = ProjectContext::instance().projectJson()["fps"].toDouble();
        float mspf = fps > 0 ? 1000.0 / fps : 1.0;

        while (*it) {
            QString uuid = (*it)->data(0, Qt::UserRole).toString();
            if (!uuid.isEmpty()) {
                auto panelContext = findPanelByUuid(uuid.toStdString());
                if (panelContext.isValid()) {
                    qint64 startTimeMs = static_cast<qint64>(panelContext.panel->startTime / mspf);
                    if (earliestTime == -1 || startTimeMs < earliestTime) {
                        earliestTime = startTimeMs;
                        targetItem = *it;
                    }
                }
            }
            ++it;
        }

        if (targetItem) {
            ui->shotsTreeWidget->setCurrentItem(targetItem);
            onTreeItemClicked(targetItem, 0); // Trigger callback
        }
    }

    this->updateWindowTitle(false);


}

void MainWindow::loadScript() {
    if(scriptBreakdown)
        delete scriptBreakdown;

    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();
    GameScript* dictionary = NULL;
    GameScript* dictionaryCustom = NULL;

    QString fileName;
    if(ProjectContext::instance().projectJson().contains("script")) {
        fileName = ProjectContext::instance().currentProjectPath() + "/" + ProjectContext::instance().projectJson()["script"].toString();
        scriptBreakdown = new ScriptBreakdown(fileName.toStdString().c_str(), fps, dictionary, dictionaryCustom, llamaClient, logger);

        if(!scriptBreakdown->load()) {
            Log().error() << "Error(s) importing script: " << scriptBreakdown->error() << "\n";
            Log().info() << "Error(s) importing script: " << scriptBreakdown->error() << "\n";
        }
        else{
            Log().info() << "Imported script: " << scriptBreakdown->fileName() << "\n";
            perfectScript->loadScript(scriptBreakdown);
        }
    }
    else
        scriptBreakdown = new ScriptBreakdown(fileName.toStdString().c_str(), fps, dictionary, dictionaryCustom, llamaClient, logger);
}

ShotContext MainWindow::findShotByUuid(const std::string& uuid) {

    int sceneIndex = -1;

    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        sceneIndex++;
        int shotIndex = -1;
        for (auto& shot : scene.shots) {
            shotIndex++;
            if (shot.uuid == uuid)
                    return { &scene, &shot, sceneIndex, shotIndex };
        }
    }

    return {};
}

PanelContext MainWindow::findPanelByUuid(const std::string& uuid) {

    if(!scriptBreakdown)
        return {};

    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {
            for(auto &panel : shot.panels){
                if (panel.uuid == uuid)
                    return { &scene, &shot, &panel };
            }
        }
    }

    return {};
}

int MainWindow::findPanelIndex(const PanelContext& ctx) {

    if(!scriptBreakdown)
        return -1;

    if(!ctx.isValid())
        return -1;

    int index = -1;
    for(auto &panel : ctx.shot->panels){
        index ++;
        if (panel.uuid == ctx.panel->uuid)
            return index;
    }

    return -1;
}

LayerContext MainWindow::findLayerByUuid(const std::string& uuid) {

    if(!scriptBreakdown)
        return {};

    auto& scenes = scriptBreakdown->getScenes();
    for (Scene& scene : scenes) {
        for (Shot& shot : scene.shots) {
            for(Panel &panel : shot.panels){
                for(Layer &layer : panel.layers)
                    if (layer.uuid == uuid)
                        return { &scene, &shot, &panel, &layer};
            }
        }
    }

    return {};
}

LayerContext MainWindow::findLayerByUuid(const std::string& panelUuid, const std::string& layerUuid) {

    if(!scriptBreakdown)
        return {};

    auto& scenes = scriptBreakdown->getScenes();
    for (Scene& scene : scenes) {
        for (Shot& shot : scene.shots) {
            for(Panel &panel : shot.panels){
                if(panel.uuid == panelUuid){
                    for(Layer &layer : panel.layers)
                        if (layer.uuid == layerUuid)
                            return { &scene, &shot, &panel, &layer};
                    return {};
                }
            }
        }
    }

    return {};
}


PanelContext MainWindow::findPanelForTime(double time, double threshold) {

    if(!scriptBreakdown){
        Log().info() << "Script Breakdown is empty\n";
        return {};
    }
    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {
            if(time < shot.startTime || time > shot.endTime)
                continue;
            for(auto &panel : shot.panels){
                if (time >= (shot.startTime + panel.startTime) &&
                    time < (shot).startTime + (panel.startTime + panel.durationTime)) {

                    return { &scene, &shot, &panel };;
                }
            }
        }
    }

    return {};
}

ShotContext MainWindow::findShotForTime(double time/*, double buffer*/) {
    if (!scriptBreakdown) {
        Log().info() << "Script Breakdown is null\n";
        return {};
    }

    int sceneIndex = -1;
    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        sceneIndex ++;
        for (size_t i = 0; i < scene.shots.size(); ++i) {
            auto& shot = scene.shots[i];

            // 1. Direct match inside this shot
            if (time >= shot.startTime && time < shot.endTime) {
                return { &scene, &shot, sceneIndex, (int)i};
            }

            // 2. Between this shot and the next
            if (i + 1 < scene.shots.size()) {
                auto& nextShot = scene.shots[i + 1];
                if (time >= shot.endTime && time < nextShot.startTime) {
                    // Optional: insert logic could go here
                    Log().info() << "Time is between Shot " << shot.name.c_str()
                                 << " and Shot " << nextShot.name.c_str() << "\n";
                    return { &scene, nullptr, sceneIndex, -1 }; // indicates an insert opportunity
                }
            }
        }
    }

    Log().info() << "No suitable scene found for time " << (float)time << "\n";
    return {};
}


ShotContext MainWindow::findPreviousShot(const GameFusion::Shot& shot)
{
    if (!scriptBreakdown)
        return {};

    // Iterate through all scenes to find the previous shot
    auto& scenes = scriptBreakdown->getScenes();
    GameFusion::Shot* prevShot = nullptr;
    GameFusion::Scene* prevScene = nullptr;

    for (auto sceneIt = scenes.begin(); sceneIt != scenes.end(); ++sceneIt) {
        auto& shots = sceneIt->shots;
        for (auto shotIt = shots.begin(); shotIt != shots.end(); ++shotIt) {
            if (shotIt->uuid == shot.uuid) {
                // Found the target shot
                if (shotIt != shots.begin()) {
                    // Previous shot is in the same scene
                    prevShot = &(*(shotIt - 1));
                    prevScene = &(*sceneIt);
                    return {prevScene, prevShot};
                } else if (sceneIt != scenes.begin()) {
                    // Previous shot is the last shot of the previous scene
                    auto prevSceneIt = sceneIt - 1;
                    if (!prevSceneIt->shots.empty()) {
                        prevShot = &prevSceneIt->shots.back();
                        prevScene = &(*prevSceneIt);
                        return {prevScene, prevShot};
                    }
                }
                // No previous shot (target is first shot in first scene)
                return {};
            }
        }
    }

    // Should not reach here if findShotByUuid worked, but return empty for safety
    GameFusion::Log().warning() << "Shot with UUID " << shot.uuid.c_str() << " not found in scenes";
    return {};
}

CameraContext MainWindow::findCameraForTime(double time) {
    if (!scriptBreakdown)
        return {};

    if (!scriptBreakdown) {
        Log().info() << "Script Breakdown is null\n";
        return {};
    }
/*
    int sceneIndex = -1;
    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        sceneIndex ++;
        for (size_t i = 0; i < scene.shots.size(); ++i) {
            auto& shot = scene.shots[i];

            // 1. Direct match inside this shot
            if (time >= shot.startTime && time < shot.endTime) {
                return { &scene, &shot, sceneIndex, (int)i};
            }

            // 2. Between this shot and the next
            if (i + 1 < scene.shots.size()) {
                auto& nextShot = scene.shots[i + 1];
                if (time >= shot.endTime && time < nextShot.startTime) {
                    // Optional: insert logic could go here
                    Log().info() << "Time is between Shot " << shot.name.c_str()
                                 << " and Shot " << nextShot.name.c_str() << "\n";
                    return { &scene, nullptr, sceneIndex, -1 }; // indicates an insert opportunity
                }
            }
        }
    }
    */
    PanelContext panelContext = findPanelForTime(time);
    if(!panelContext.isValid()){
        return {};
    }


    std::vector<CameraFrame> sortedFrames = panelContext.shot->cameraAnimation.frames;
    std::sort(sortedFrames.begin(), sortedFrames.end(), [](const CameraFrame& a, const CameraFrame& b) {
        return a.frameOffset < b.frameOffset;
    });

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    qreal mspf = 1000.0f / fps;

    GameFusion::CameraFrame *currentCamera = nullptr;
    for(auto &camera: panelContext.shot->cameraAnimation.frames){
        double shotTime = panelContext.shot->startTime;
        double cameraTime = shotTime + camera.frameOffset * mspf;
        if(cameraTime <= time)
            currentCamera = &camera;
        if(cameraTime > time)
            break;
    }

    CameraContext result = {panelContext.scene, panelContext.shot, currentCamera};
    return result;
}

CameraContext MainWindow::findCameraByUuid(const std::string& uuid) {
    if (!scriptBreakdown)
        return {};

    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {
            for (auto& cameraFrame : shot.cameraAnimation.frames) {
                if (cameraFrame.uuid == uuid) {
                    PanelContext panelContext = findPanelByUuid(cameraFrame.panelUuid);
                    return { &scene, &shot, &cameraFrame };
                }
            }
        }
    }

    return {};
}

ShotContext MainWindow::findSceneByPanel(const std::string& panelUuid) {
    if (!scriptBreakdown)
        return {};

    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {
            for (auto& panel : shot.panels) {
                if (panel.uuid == panelUuid) {
                    return { &scene, &shot };
                }
            }
        }
    }

    return {};
}

// In MainWindow.cpp
GameFusion::Scene* MainWindow::findSceneByUuid(const std::string& uuid) {
    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        if (scene.uuid == uuid) {
            return &scene;
        }
    }
    GameFusion::Log().warning() << "Scene with UUID " << uuid.c_str() << " not found";
    return nullptr;
}

int MainWindow::findSceneIndex(const std::string& uuid) {
    auto& scenes = scriptBreakdown->getScenes();
    int index = -1;
    for (auto& scene : scenes) {
        index++;
        if (scene.uuid == uuid) {
            return index;
        }
    }
    GameFusion::Log().warning() << "Scene with UUID " << uuid.c_str() << " not found";
    return index;
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem* item, int column) {
    QVariant uuidData = item->data(0, Qt::UserRole);
    if (!uuidData.isValid())
        return;

    QString uuid = uuidData.toString();

    // Use UUID to look up the panel in your project data
    PanelContext panelContext = findPanelByUuid(uuid.toStdString());

    Scene *scene = panelContext.scene;
    Shot *shot = panelContext.shot;
    Panel *panel = panelContext.panel;

    if (!panel){
        Log().info() << "Panel " << uuid.toUtf8().constData() << " not found\n";
        return;
    }

    timeLineView->getTrack(0)->clearCurrentPanelMarkers();
    Segment* seg = timeLineView->getTrack(0)->getSegmentByUuid(shot->uuid.c_str());
    if(seg){
        PanelMarker *panelMarker = (PanelMarker*)seg->getMarkerItemByUuid(panel->uuid.c_str());
        if(panelMarker) {
            panelMarker->setIsCurrent(!panelMarker->getIsCurrent());
        }
    }

    // TODO print panel selected info ...with Log().info() <<
    Log().info() << "Panel selected:\n"
                 << "  Name: " << panel->name.c_str() << "\n"
                 << "  Description: " << panel->description.c_str() << "\n"
                 << "  Start Time: " << panel->startTime << "\n"
                 << "  Duration: " << panel->durationTime << "\n"
                 << "  Image: " << panel->image.c_str() << "\n"
                 << "  UUID: " << panel->uuid.c_str() << "\n";

    // Load and display the image, e.g. in a QLabel
    //QString imagePath = currentProjectPath + "/movies/" + panel->image.c_str();
    //paint->openImage(imagePath);

    // set time cursor to panel start time
    populateLayerList(panel);
    timeLineView->setTimeCursor((long)(shot->startTime + panel->startTime));

    // Update the panel UI
    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();
    shotPanel->setPanelInfo(scene, shot, panel, fps);

    // TODO set panel info for
    //layerAnd
    //setPanelLayers()
    //ui->dockLayers-> ... todo set panel content, and add check state for visibility per layer in list view with name

    // Populate layer list
    /***
    ui->layerListWidget->clear();
    ui->layerListWidget->clear();

    if (panel->layers.empty()) {
        GameFusion::Log().info() << "No layers in panel " << panel->uuid.c_str() << "\n";
        return;
    }

    for (auto& layer : panel->layers) {
        QString label = QString::fromStdString(layer.name);
        QListWidgetItem* item = new QListWidgetItem(label);

        // Set checkbox for visibility
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);

        // Store layer UUID as item data
        item->setData(Qt::UserRole, QString::fromStdString(layer.uuid));

        ui->layerListWidget->addItem(item);
    }
    ***/

}

void MainWindow::populateLayerList(GameFusion::Panel* panel) {
    if (!panel) return;

    //if(currentPanelUuid == panel->uuid.c_str())
    //    return;

    ui->layerListWidget->clear();

    if (panel->layers.empty()) {
        GameFusion::Log().info() << "No layers in panel " << panel->uuid.c_str() << "\n";
        return;
    }

    // Set a consistent icon size for all items
    ui->layerListWidget->setIconSize(QSize(240, 135));

    for (auto& layer : panel->layers) {
        QString label = QString::fromStdString(layer.name);
        QListWidgetItem* item = new QListWidgetItem(label);

        // Create a dummy black pixmap (240 x 135)
        QPixmap dummyPixmap(240, 135);
        dummyPixmap.fill(Qt::black);

        // Assign the dummy icon
        item->setIcon(QIcon(dummyPixmap));

        // Enable checkbox
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);

        // Store layer UUID
        item->setData(Qt::UserRole, QString::fromStdString(layer.uuid));

        ui->layerListWidget->addItem(item);
    }

        // --- Add Reference Image if it exists ---
        if (!panel->image.empty()) {
            QString imagePath = ProjectContext::instance().currentProjectPath() + "/movies/" + panel->image.c_str();

            if (QFile::exists(imagePath)) {
                // Add a separator (visual)
                QListWidgetItem* separator = new QListWidgetItem("──────────────");
                separator->setFlags(Qt::NoItemFlags); // non-selectable
                ui->layerListWidget->addItem(separator);

                // Load and resize the reference image
                QImage refImage;
                if (refImage.load(imagePath)) {
                    constexpr int thumbWidth = 1920 / 14;  // 192
                    constexpr int thumbHeight = 1080 / 14; // 108
                    QPixmap refPixmap = QPixmap::fromImage(refImage.scaled(
                        thumbWidth,
                        thumbHeight,
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation
                        ));

                    QListWidgetItem* refItem = new QListWidgetItem(tr("Reference"));
                    QFont italicFont = refItem->font();
                    italicFont.setItalic(true);
                    refItem->setFont(italicFont);

                    refItem->setIcon(QIcon(refPixmap));

                    // Reference is always visible but cannot be reordered
                    refItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                    refItem->setCheckState(Qt::Checked); // or Qt::Unchecked if you want to toggle

                    // Store a custom role to identify as reference
                    refItem->setData(Qt::UserRole, "REFERENCE");

                    ui->layerListWidget->addItem(refItem);
                }
            }
        }



    //currentPanelUuid = panel->uuid.c_str();
    paint->getPaintArea()->invalidateAllLayers();
    paint->getPaintArea()->updateCompositeImage();
}

void MainWindow::updateLayerPanelAttributes(GameFusion::Layer &layer)
{
    ui->doubleSpinBox_layerPosX->blockSignals(true);
    ui->doubleSpinBox_layerPosY->blockSignals(true);
    ui->spinBox_layerRotation->blockSignals(true);
    ui->doubleSpinBox_layerScale->blockSignals(true);
    ui->comboBox_layerBlendMode->blockSignals(true);

    // ---

    ui->doubleSpinBox_layerPosX->setValue(layer.x);
    ui->doubleSpinBox_layerPosY->setValue(layer.y);
    ui->spinBox_layerRotation->setValue(layer.rotation);
    ui->doubleSpinBox_layerScale->setValue(layer.scale);

    std::string blendStr = toString(layer.blendMode);
    ui->comboBox_layerBlendMode->setCurrentText(blendStr.c_str());

    // ---

    ui->doubleSpinBox_layerPosX->blockSignals(false);
    ui->doubleSpinBox_layerPosY->blockSignals(false);
    ui->spinBox_layerRotation->blockSignals(false);
    ui->doubleSpinBox_layerScale->blockSignals(false);
    ui->comboBox_layerBlendMode->blockSignals(false);
}

void MainWindow::onLayerSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous) {
    if (!current) {
        selectedLayerUuid = "";
        updateKeyframeDisplay();
        return;
    }

    QString uuid = current->data(Qt::UserRole).toString();
    paint->getPaintArea()->setActiveLayer(uuid);  // You define this in PaintArea
    selectedLayerUuid = uuid;
    updateKeyframeDisplay();
}

void MainWindow::setSelectedLayer(const QString& shotUuid, const QString& panelUuid, const QString& layerUuid) {
    selectedShotUuid = shotUuid;
    selectedPanelUuid = panelUuid;
    selectedLayerUuid = layerUuid;
    updateKeyframeDisplay();
}

void MainWindow::onLayerVisibilityChanged(QListWidgetItem* item) {
    QString uuid = item->data(Qt::UserRole).toString();
    bool visible = item->checkState() == Qt::Checked;
    paint->getPaintArea()->setLayerVisibility(uuid, visible);

    LayerContext layerContext = findLayerByUuid(uuid.toStdString());
    if(layerContext.isValid()){
        layerContext.scene->dirty = true;
        updateWindowTitle(true);
        layerContext.layer->visible = visible;
    }
}


void MainWindow::onLayerBlendMode(int index) {
    GameFusion::BlendMode mode;
    switch (index) {
        case 0: mode = GameFusion::BlendMode::Opacity; break;
        case 1: mode = GameFusion::BlendMode::Multiply; break;
        case 2: mode = GameFusion::BlendMode::Screen; break;
        case 3: mode = GameFusion::BlendMode::Overlay; break;
        default: return;
    }

    // get selected layer
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    for(auto selected: selectedLayers) {
        QString uuid = selected->data(Qt::UserRole).toString();
        LayerContext layerContext = findLayerByUuid(uuid.toStdString());
        if(layerContext.isValid()){
            layerContext.scene->dirty = true;
            updateWindowTitle(true);
            layerContext.layer->blendMode = mode;
            paint->getPaintArea()->updateLayer(*layerContext.layer);
        }
    }
}

void MainWindow::onLayerLoadImage() {
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.isEmpty()) return;

    if (!currentPanel) return;

    // Only one layer can be selected
    QListWidgetItem* selected = selectedLayers.first();
    QString layerUuid = selected->data(Qt::UserRole).toString();
    LayerContext layerContext = findLayerByUuid(layerUuid.toStdString().c_str());
    if (!layerContext.isValid()) return;

    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Images (*.png *.jpg *.bmp)"));
    if (fileName.isEmpty()) return;

    std::string oldImageFilePath = layerContext.layer->imageFilePath;
    std::string newImageFilePath = fileName.toStdString();

    undoStack->push(new LayerLoadImageCommand(layerUuid, QString::fromStdString(currentPanel->uuid), oldImageFilePath, newImageFilePath, this));
}

void MainWindow::onLayerOpacity(int value) {
    float opacity = value / 100.0f; // Map 0-100 to 0.0-1.0

    // get selected layer
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    for(auto selected: selectedLayers) {
        QString uuid = selected->data(Qt::UserRole).toString();
        LayerContext layerContext = findLayerByUuid(uuid.toStdString());
        if(layerContext.isValid()){
            layerContext.scene->dirty = true;
            updateWindowTitle(true);
            layerContext.layer->opacity = opacity;
            paint->getPaintArea()->updateLayer(*layerContext.layer);
        }
    }
}

void MainWindow::onLayerAdd() {
    if (!currentPanel)
        return;

    GameFusion::Layer layer;
    layer.name = "Layer " + std::to_string(currentPanel->layers.size() + 1);
    undoStack->push(new LayerAddCommand(layer, QString::fromStdString(currentPanel->uuid), this));

    return;
}

void MainWindow::onLayerDelete() {
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.isEmpty()) return;

    if (!currentPanel) return;

    //CompositeUndoCommand* composite = new CompositeUndoCommand("Delete Layers");

    std::vector<std::pair<GameFusion::Layer, int>> deletedLayers;
    for (const auto& selected : selectedLayers) {
        std::string toDeleteUuid = selected->data(Qt::UserRole).toString().toStdString();

        LayerContext layerContext = findLayerByUuid(toDeleteUuid.c_str());
        if (!layerContext.isValid()) continue;

        int layerIndex = 0;
        for (auto it = layerContext.panel->layers.begin(); it != layerContext.panel->layers.end(); ++it) {
            if (it->uuid == toDeleteUuid) {
                //deletedLayers.emplace_back(*it, layerIndex);
                //layerContext.panel->layers.erase(it);
                //composite->addCommand(new LayerDeleteCommand(*it, layerIndex, QString::fromStdString(currentPanel->uuid), this));
                undoStack->push(new LayerDeleteCommand(*it, layerIndex, QString::fromStdString(currentPanel->uuid), this));
                return;
            }
            layerIndex++;
        }
    }
/*
    if (!deletedLayers.empty()) {
        undoStack->push(composite);

        PanelContext panelContext = findPanelByUuid(currentPanel->uuid);
        if (panelContext.isValid()) {
            panelContext.scene->dirty = true;
            updateWindowTitle(true);
            long panelStartTime = panelContext.shot->startTime + panelContext.panel->startTime;
            float fps = projectJson["fps"].toDouble();
            paint->getPaintArea()->setPanel(*panelContext.panel, panelStartTime, fps, panelContext.shot->cameraFrames);
            populateLayerList(panelContext.panel);
        }

        paint->getPaintArea()->invalidateAllLayers();
        paint->getPaintArea()->updateCompositeImage();
    }
    */
}

void MainWindow::onLayerFX() {

    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.isEmpty()) return;

    if (!currentPanel) return;

    //CompositeUndoCommand* composite = new CompositeUndoCommand("Change Layer FX");

    for (const auto& selected : selectedLayers) {
        QString layerUuid = selected->data(Qt::UserRole).toString();
        LayerContext layerContext = findLayerByUuid(layerUuid.toStdString().c_str());
        if (!layerContext.isValid()) continue;

        std::string oldFX = layerContext.layer->fx;
        std::string newFX = layerContext.layer->fx.empty() ? "blur" : "";

        //composite->addCommand(new LayerFXCommand(layerUuid, QString::fromStdString(currentPanel->uuid), oldFX, newFX, this));
        undoStack->push(new LayerFXCommand(layerUuid, QString::fromStdString(currentPanel->uuid), oldFX, newFX, this));
        return;
    }

    //if (composite->childCount() > 0) {
    //    undoStack->push(composite);
    //}
}

void MainWindow::onLayerMoveUp() {
    int index = ui->layerListWidget->currentRow();
    if (index <= 0) return; // Cannot move up if at top

    std::vector<QString> oldUuidOrder = getCurrentLayerUuidOrder(currentPanel->uuid.c_str());

    QListWidgetItem* item = ui->layerListWidget->takeItem(index);
    ui->layerListWidget->insertItem(index - 1, item);
    ui->layerListWidget->setCurrentItem(item);

    // Sync layer order from QListWidget to panel->layers
    if (!currentPanel) return;

    std::vector<QString> newUuidOrder;
    for (int i = 0; i < ui->layerListWidget->count(); ++i) {
        QListWidgetItem* widgetItem = ui->layerListWidget->item(i);
        QString uuid = widgetItem->data(Qt::UserRole).toString();
        newUuidOrder.push_back(uuid);
    }

     undoStack->push(new LayerReorderCommand(oldUuidOrder, newUuidOrder, currentPanel->uuid.c_str(), this));
}

std::vector<QString> MainWindow::getCurrentLayerUuidOrder(const QString& panelUuid)  {
    PanelContext panelCtx = findPanelByUuid(panelUuid.toStdString());
    if (!panelCtx.isValid()) return {};
    std::vector<QString> uuidOrder;
    for (const auto& layer : panelCtx.panel->layers) {
        uuidOrder.push_back(QString::fromStdString(layer.uuid));
    }
    return uuidOrder;
}

void MainWindow::onLayerMoveDown() {
    int index = ui->layerListWidget->currentRow();
    int count = ui->layerListWidget->count();
    if (index < 0 || index >= count - 1) return; // Cannot move down if at bottom or no selection

    std::vector<QString> oldUuidOrder = getCurrentLayerUuidOrder(currentPanel->uuid.c_str());

    QListWidgetItem* item = ui->layerListWidget->takeItem(index);
    ui->layerListWidget->insertItem(index + 1, item);
    ui->layerListWidget->setCurrentItem(item);

    // Sync layer order from QListWidget to panel->layers
    if (!currentPanel) return;

    std::vector<QString> newUuidOrder;
    for (int i = 0; i < ui->layerListWidget->count(); ++i) {
        QListWidgetItem* widgetItem = ui->layerListWidget->item(i);
        QString uuid = widgetItem->data(Qt::UserRole).toString();
        newUuidOrder.push_back(uuid);
    }

    undoStack->push(new LayerReorderCommand(oldUuidOrder, newUuidOrder, currentPanel->uuid.c_str(), this));
}

void MainWindow::onLayerDuplicate() {
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.isEmpty()) return;

    if (!currentPanel) return;

    QListWidgetItem* selected = selectedLayers.first();
    QString originalLayerUuid = selected->data(Qt::UserRole).toString();
    LayerContext layerContext = findLayerByUuid(originalLayerUuid.toStdString().c_str());
    if (!layerContext.isValid())
        return;

    // Create a duplicate layer
    QString duplicatedLayerUuid = QUuid::createUuid().toString();

    undoStack->push(new LayerDuplicateCommand(originalLayerUuid, duplicatedLayerUuid,
                                             QString::fromStdString(currentPanel->uuid), this));
}

void MainWindow::onLayerReordered(const QModelIndex &parent,
                                  int start, int end,
                                  const QModelIndex &destination, int row)
{
    // Sync layer order from QListWidget to panel->layers
    if (!currentPanel) return;

    std::vector<QString> oldUuidOrder = getCurrentLayerUuidOrder(currentPanel->uuid.c_str());

    std::vector<GameFusion::Layer> reordered;

    QString uuid;
    for (int i = 0; i < ui->layerListWidget->count(); ++i) {
        QListWidgetItem* item = ui->layerListWidget->item(i);
        uuid = item->data(Qt::UserRole).toString();
        auto it = std::find_if(currentPanel->layers.begin(), currentPanel->layers.end(),
                               [&](const GameFusion::Layer& l) { return l.uuid == uuid.toStdString(); });
        if (it != currentPanel->layers.end()) {
            reordered.push_back(*it);
        }
    }

    currentPanel->layers = std::move(reordered);

    std::vector<QString> newUuidOrder;
    for (const auto& layer : currentPanel->layers) {
        newUuidOrder.push_back(QString::fromStdString(layer.uuid));
    }

    undoStack->push(new LayerReorderCommand(oldUuidOrder, newUuidOrder, currentPanel->uuid.c_str(), this));
}


void MainWindow::onLayerRotationChanged(int value) {
    int index = ui->layerListWidget->currentRow();

    if (index >= 0) {
        QListWidgetItem* item = ui->layerListWidget->item(index);
        if (!item) return;

        QVariant data = item->data(Qt::UserRole);
        LayerContext layerContext = findLayerByUuid(data.toString().toStdString());

        if (layerContext.isValid()) {
            int oldValue = layerContext.layer->rotation;
            undoStack->push(new LayerAttributeCommand(layerContext.layer->uuid.c_str(), QString::fromStdString(layerContext.panel->uuid),
                                                         LayerAttributeType::Rotation,
                                                         oldValue, value, this));
        }
    }
}

void MainWindow::onLayerPosXChanged(double value) {
    int index = ui->layerListWidget->currentRow();

    if (index >= 0) {
        QListWidgetItem* item = ui->layerListWidget->item(index);
        if (!item) return;

        QVariant data = item->data(Qt::UserRole);
        LayerContext layerContext = findLayerByUuid(data.toString().toStdString());

        if (layerContext.isValid()) {
            double oldValue = layerContext.layer->x;
            undoStack->push(new LayerAttributeCommand(layerContext.layer->uuid.c_str(), QString::fromStdString(layerContext.panel->uuid),
                                                         LayerAttributeType::PosX,
                                                         oldValue, value, this));
        }
    }
}

void MainWindow::onLayerPosYChanged(double value) {
    int index = ui->layerListWidget->currentRow();

    if (index >= 0) {
        QListWidgetItem* item = ui->layerListWidget->item(index);
        if (!item) return;

        QVariant data = item->data(Qt::UserRole);
        LayerContext layerContext = findLayerByUuid(data.toString().toStdString());

        if (layerContext.isValid()) {
            double oldValue = layerContext.layer->y;
            undoStack->push(new LayerAttributeCommand(layerContext.layer->uuid.c_str(), QString::fromStdString(layerContext.panel->uuid),
                                                         LayerAttributeType::PosY,
                                                         oldValue, value, this));
        }
    }
}

void MainWindow::onLayerScaleChanged(double value) {
    int index = ui->layerListWidget->currentRow();

    if (index >= 0) {
        QListWidgetItem* item = ui->layerListWidget->item(index);
        if (!item) return;

        QVariant data = item->data(Qt::UserRole);
        LayerContext layerContext = findLayerByUuid(data.toString().toStdString());

        if (layerContext.isValid()) {
            double oldValue = layerContext.layer->scale;
            undoStack->push(new LayerAttributeCommand(layerContext.layer->uuid.c_str(), QString::fromStdString(layerContext.panel->uuid),
                                                         LayerAttributeType::Scale,
                                                         oldValue, value, this));
        }
    }
}


void MainWindow::onPaintAreaLayerModified(const Layer &modLayer) {

    if (!currentPanel) return;

        LayerContext layerContext = findLayerByUuid(modLayer.uuid);
        if (!layerContext.isValid()) return;

        // Capture the original layer state
        GameFusion::Layer originalLayer = *layerContext.layer; // Deep copy

        // Push undo command before modifying the layer
        undoStack->push(new LayerPaintCommand(originalLayer, modLayer,
                                             QString::fromStdString(modLayer.uuid),
                                             QString::fromStdString(currentPanel->uuid), this));


    return;
}

void MainWindow::onPaintAreaEraseStrokes(const Layer &modLayer) {

    if (!currentPanel) return;

        LayerContext layerContext = findLayerByUuid(modLayer.uuid);
        if (!layerContext.isValid()) return;

        // Capture the original layer state
        GameFusion::Layer originalLayer = *layerContext.layer; // Deep copy

        // Push undo command before modifying the layer
        undoStack->push(new LayerPaintErasedStrokes(originalLayer, modLayer,
                                             QString::fromStdString(modLayer.uuid),
                                             QString::fromStdString(currentPanel->uuid), this));


    return;
}

void MainWindow::onPaintAreaLayerAdded(const Layer& layer) {
    // 1. Add to current panel's layers
    if (currentPanel){
        currentPanel->layers.push_back(layer);

        PanelContext panelContext = findPanelByUuid(currentPanel->uuid);
        if(panelContext.isValid()){
            panelContext.scene->dirty = true;
            updateWindowTitle(true);
        }
    }

    // 2. Add to layerListWidget
    QString label = QString::fromStdString(layer.name);
    QListWidgetItem* item = new QListWidgetItem(label);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, QString::fromStdString(layer.uuid));
    ui->layerListWidget->addItem(item);

    // 3. Set it as current/active
    ui->layerListWidget->setCurrentItem(item);

    // 4. Optional: Add to storyboard if needed
    // storyboard->addLayerVisual(layer); or similar
}

#include "NewProjectDialog.h"


void MainWindow::newProject()
{
    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString name = dialog.projectName();
    QString location = dialog.location();
    QString fullPath = location + "/" + name;

    // Create project folder
    QDir dir;
    if (!dir.mkpath(fullPath)) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to create project folder."));
        return;
    }

    // Define project directory structure
    const QStringList subDirs = {
        "assets",
        "assets/LoRA",
        "assets/audio",
        "assets/controlnet",
        "audio/tracks",
        "cameras",
        "movies",
        "panels",
        "references",
        "references/characters",
        "references/environments",
        "references/props",
        "scenes",
        "scripts",
        "thumbnails"
    };

    // Create all subdirectories
    for (const QString& subDir : subDirs) {
        if (!dir.mkpath(fullPath + "/" + subDir)) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Failed to create folder: %1").arg(subDir));
            return;
        }
    }

    // Parse resolution as array
    QString resString = dialog.resolution(); // e.g. "1920x1080"
    QStringList resParts = resString.split('x');
    int resWidth = resParts.value(0).toInt();
    int resHeight = resParts.value(1).toInt();

    // Generate project UUID
    QString projectUUID = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Generate ISO timestamp
    QString createdTime = QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + "Z";

    // Write project metadata
    //QJsonObject projectJson;

    QJsonObject &projectJson = ProjectContext::instance().projectJson();
    projectJson = QJsonObject();

    projectJson["aspectRatio"] = dialog.aspectRatio();
    projectJson["director"] = dialog.director();
    projectJson["estimatedDuration"] = dialog.estimatedDuration();
    projectJson["fps"] = dialog.fps();
    projectJson["notes"] = dialog.notes();
    projectJson["projectCode"] = dialog.projectCode();
    projectJson["projectName"] = name;
    projectJson["resolution"] = QJsonArray{ resWidth, resHeight };
    projectJson["safeFrame"] = dialog.safeFrame();
    projectJson["created"] = createdTime;
    projectJson["version"] = 1;
    projectJson["saveFormat"] = "compact";
    projectJson["scenesPath"] = "scenes";
    projectJson["projectId"] = name;
    projectJson["subtitle"] = dialog.subtitle();
    projectJson["episode_format"] = dialog.episodeFormat();
    projectJson["copyright"] = dialog.copyright();
    projectJson["start_tc"] = dialog.startTC();  // Use in timeline (e.g., offset TC display)

    QString canvasMargin = dialog.canvasMargin();

    int marginPct = 0;
    if(canvasMargin == "Custom")
        marginPct = dialog.canvasMarginCustom().toInt();
    else
        marginPct = dialog.canvasMargin().toInt();  // 0 if empty/invalid

    int marginW = resWidth * marginPct / 100;  // Total extra (split left/right)
    int marginH = resHeight * marginPct / 100;
    int canvasWidth = resWidth + marginW;
    int canvasHeight = resHeight + marginH;

    projectJson["canvas_margin"] = canvasMargin;
    projectJson["canvas_percent"] = marginPct;  // For reference/editing
    projectJson["canvas"] = QJsonArray{canvasWidth, canvasHeight};


    QFile metadataFile(fullPath + "/project.json");
    if (!metadataFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save project metadata."));
        return;
    }

    QJsonDocument doc(projectJson);
    metadataFile.write(doc.toJson());
    metadataFile.close();

    QMessageBox::information(this, tr("Success"), tr("New project created successfully."));

    // Optional: load project into workspace
    loadProject(fullPath);
}

void MainWindow::editProject(){
    QJsonObject &projectJson = ProjectContext::instance().projectJson();

    if (projectJson.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("No project is currently loaded."));
        return;
    }

    NewProjectDialog dialog(this);
    dialog.setEditMode(true);  // Set to edit mode
    dialog.setProjectData(projectJson);  // Populate with current projectJson

    if (dialog.exec() != QDialog::Accepted) {
        return;  // User canceled
    }

    // Update projectJson with new values
    QString name = dialog.projectName();
    QString location = dialog.location();
    QString fullPath = location + "/" + name;

    // Parse resolution
    QString resString = dialog.resolution();
    QStringList resParts = resString.split('x');
    int resWidth = resParts.value(0).toInt();
    int resHeight = resParts.value(1).toInt();

    // Parse canvas margin and compute canvas size
    QString canvasMargin = dialog.canvasMargin();

    int marginPct = dialog.canvasMargin().toInt();
    if(canvasMargin == "Custom")
        marginPct = dialog.canvasMarginCustom().toInt();

    int marginW = resWidth * marginPct / 100;
    int marginH = resHeight * marginPct / 100;
    int canvasWidth = resWidth + marginW;
    int canvasHeight = resHeight + marginH;

    // Update projectJson
    projectJson["projectName"] = name;
    projectJson["aspectRatio"] = dialog.aspectRatio();
    projectJson["director"] = dialog.director();
    projectJson["estimatedDuration"] = dialog.estimatedDuration();
    projectJson["fps"] = dialog.fps();
    projectJson["notes"] = dialog.notes();
    projectJson["projectCode"] = dialog.projectCode();
    projectJson["resolution"] = QJsonArray{resWidth, resHeight};
    projectJson["safeFrame"] = dialog.safeFrame();
    projectJson["canvas"] = QJsonArray{canvasWidth, canvasHeight};
    projectJson["canvas_percent"] = marginPct;
    projectJson["subtitle"] = dialog.subtitle();
    projectJson["episode_format"] = dialog.episodeFormat();
    projectJson["copyright"] = dialog.copyright();
    projectJson["start_tc"] = dialog.startTC();

    // Save to project.json
    QFile metadataFile(ProjectContext::instance().currentProjectPath() + "/project.json");
    if (!metadataFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save project metadata."));
        return;
    }

    QJsonDocument doc(projectJson);
    metadataFile.write(doc.toJson());
    metadataFile.close();

    // Update PaintArea if canvas/resolution changed
    if (paint->getPaintArea()) {
        int newCanvasW = projectJson["canvas"].toArray()[0].toInt();
        int newCanvasH = projectJson["canvas"].toArray()[1].toInt();
        int newResW = projectJson["resolution"].toArray()[0].toInt();
        int newResH = projectJson["resolution"].toArray()[1].toInt();
        paint->getPaintArea()->setCanvasSize(newCanvasW, newCanvasH);
        paint->getPaintArea()->setOutputResolution(newResW, newResH);
        paint->getPaintArea()->update();
    }

    QMessageBox::information(this, tr("Success"), tr("Project updated successfully."));
}

void MainWindow::onTimeCursorMoved(double time)
{
    Log::info() << "Time cursor moved " << (float)time << "\n";
    double cursorTime = timeLineView->getCursorTime();
    Log::info() << "Time cursor time " << (float)cursorTime << "\n";
    //--- Respond to timeline cursor change
    //qDebug() << "Timeline cursor moved to time:" << time;
    //Log().debug() << "Timeline cursor moved to time:" << (float)time << "\n";

    //--- Update other UI parts if needed

    //--- Find the current Scene → Shot → Panel at given time

    /*----------------------------------------------------
     * Find the current Scene → Shot → Panel at given time
     *
     * Project
     * └─ Scenes (each has Shots)
     *      └─ Shots (each has Panels)
     *           └─ Panels (each has startTime, endTime, imagePath or image)
     */

    splitSceneAct->setEnabled(false);
    renameSceneAct->setEnabled(false);
    duplicateSceneAct->setEnabled(false);
    deleteSceneAct->setEnabled(false);

    editShotAct->setEnabled(false);
    renameShotAct->setEnabled(false);
    duplicateShotAct->setEnabled(false);
    deleteShotAct->setEnabled(false);

    newPanelAct->setEnabled(false);
    insertPanelAct->setEnabled(false);
    editPanelAct->setEnabled(false);
    renamePanelAct->setEnabled(false);
    duplicatePanelAct->setEnabled(false);
    copyPanelAct->setEnabled(false);
    cutPanelAct->setEnabled(false);
    pastePanelAct->setEnabled(false);
    clearPanelAct->setEnabled(false);
    deletePanelAct->setEnabled(false);

    newCameraAct->setEnabled(false);
    deleteCameraAct->setEnabled(false);
    renameCameraAct->setEnabled(false);
    copyCameraAct->setEnabled(false);
    pastCameraAct->setEnabled(false);


    Panel* newPanel = nullptr;

    long timeCounter = 0;

    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();
    if(fps <= 0)
    {
        Log().info() << "Invalid project fps " << fps << "\n";
        return;
    }
    float mspf = 1000 / fps;

    if (!scriptBreakdown)
        return;

    auto& scenes = scriptBreakdown->getScenes();
    GameFusion::Shot *theShot = nullptr;


    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {

            if(shot.startTime == -1) {
                shot.startTime = timeCounter;
                shot.endTime = shot.startTime + shot.frameCount*mspf;
            }

            timeCounter = shot.endTime;

            if(time < shot.startTime || time > shot.endTime)
                continue;

            for(auto &panel : shot.panels){
                if (time >= (shot.startTime + panel.startTime) &&
                    time < (shot).startTime + (panel.startTime + panel.durationTime)) {
                    newPanel = &panel;
                    theShot = &shot;
                    break;
                }
            }

            if(newPanel)
                break;
        }
        if(newPanel)
            break;
    }

    // --- enable context menues base on selection
    splitSceneAct->setEnabled(theShot);
    renameSceneAct->setEnabled(theShot);
    duplicateSceneAct->setEnabled(theShot);
    deleteSceneAct->setEnabled(theShot);

    editShotAct->setEnabled(theShot);
    renameShotAct->setEnabled(theShot);
    duplicateShotAct->setEnabled(theShot);
    deleteShotAct->setEnabled(theShot);

    newPanelAct->setEnabled(theShot);
    insertPanelAct->setEnabled(theShot);
    editPanelAct->setEnabled(theShot);
    renamePanelAct->setEnabled(theShot);
    duplicatePanelAct->setEnabled(theShot);
    copyPanelAct->setEnabled(theShot);
    cutPanelAct->setEnabled(theShot);
    if(hasClipboardPanel)
        pastePanelAct->setEnabled(theShot);
    clearPanelAct->setEnabled(theShot);
    deletePanelAct->setEnabled(theShot);

    newCameraAct->setEnabled(theShot);
    deleteCameraAct->setEnabled(theShot);
    renameCameraAct->setEnabled(theShot);
    copyCameraAct->setEnabled(theShot);
    pastCameraAct->setEnabled(theShot);

    //--- If no panel found, show white image
    if (!newPanel) {
        Log().debug() << "No panel at time: " << (float)time << ", showing blank image.";

        if(theShot){
            long panelStartTime = theShot->startTime + currentPanel->startTime;
            paint->getPaintArea()->setPanel(*currentPanel, panelStartTime, fps, theShot->cameraAnimation);
            cameraSidePanel->setCameraList(currentPanel->uuid.c_str(), theShot->cameraAnimation.frames);
        }



        currentPanel = nullptr;
        return;
    }

    long panelStartTime = (theShot && currentPanel) ? theShot->startTime + currentPanel->startTime : 0;

    if(newPanel == currentPanel) {
        paint->getPaintArea()->setCurrentTime(time - panelStartTime);
        paint->getPaintArea()->update();
        return; // no panel change
    }

    currentPanel = newPanel;

    if(theShot) {
        paint->getPaintArea()->setPanel(*currentPanel, panelStartTime, fps, theShot->cameraAnimation);

        if (!isPlaying) {
            populateLayerList(currentPanel);
            cameraSidePanel->setCameraList(currentPanel->uuid.c_str(), theShot->cameraAnimation.frames);
        }
    }
}

void MainWindow::saveProject(){

    if(scriptBreakdown){
        scriptBreakdown->saveModifiedScenes(ProjectContext::instance().currentProjectPath());
    }

    saveAudioTracks();
}

QString generateUniquePanelName(GameFusion::Shot* shot) {
    if (!shot)
        return "Panel1";

    int index = 1;
    QString baseName = "Panel";
    QString candidateName;
    bool nameExists = false;

    do {
        candidateName = baseName + QString::number(index++);
        nameExists = std::any_of(shot->panels.begin(), shot->panels.end(), [&](const Panel& panel) {
            return QString::fromStdString(panel.name) == candidateName;
        });
    } while (nameExists);

    return candidateName;
}

void MainWindow::newPanel(double t) {
    Log().info() << "New Panel @ "<<(float)t<<"\n";



    PanelContext panelContext = findPanelForTime(t);
    if(panelContext.isValid()){
        Log().info() << "Found panel in shot: Scene " << panelContext.scene->name.c_str()
                     << ", Shot " << panelContext.shot->name.c_str()
                     << ", uuid " << panelContext.panel->name.c_str() << ", uuid << " <<panelContext.panel->uuid.c_str() <<"\n";

        qreal panelTimeStart = t - panelContext.shot->startTime;

        bool shiftPressed = QApplication::keyboardModifiers() & Qt::ShiftModifier;

        Panel newPanel;
        if (shiftPressed) {
            Log().info() << "Shift pressed: creating brand new panel (not a duplicate)";
            newPanel = Panel(); // default constructor, empty panel
            newPanel.startTime = panelTimeStart;
            //newPanel.durationTime = 1.0; // or any default duration
            newPanel.name = generateUniquePanelName(panelContext.shot).toUtf8().constData(); // you need to define this
            newPanel.thumbnail = ""; // or generate default thumbnail
            //newPanel.uuid = generateUuid(); // ensure uniqueness
        } else {
            newPanel = panelContext.panel->duplicatePanel(panelTimeStart);
        }

        // Optional: Insert a new panel *after* the found one
        //Panel newPanel = panelContext.panel->duplicatePanel(panelTimeStart); // If you're duplicating

        //insertPanelAfter(panelContext, newPanel);


        // add new panel to time line
        // get ShotSegment for panelContext.shot.uuid
        TrackItem * track = timeLineView->getTrack(0);
        if(!track)
            return;

        Segment *segment = track->getSegmentByUuid(panelContext.shot->uuid.c_str());
        if(segment){

            QString thumbnail = ProjectContext::instance().currentProjectPath() + "/movies/" + newPanel.thumbnail.c_str();

            PanelMarker *panelMarker = new PanelMarker(0, 30, 0, 220, segment, newPanel.name.c_str(), "", panelContext.shot->uuid.c_str(), "");

            panelMarker->setPanelName(QString::fromStdString(newPanel.name));
            if(!newPanel.thumbnail.empty())
                panelMarker->loadThumbnail(thumbnail);
            panelMarker->setUuid(newPanel.uuid.c_str());

            panelMarker->setStartTimePos(panelTimeStart);

            //segment->updateMarkersEndTime();
            int h = segment->getHeight();
            panelMarker->updateHeight(h);

            //newPanel.durationTime = panelMarker->duration();
            panelContext.shot->panels.push_back(newPanel);

            syncPanelDurations(segment, panelContext.shot);
        }

    }
    else {
        Log().info() << "No panel found at time " << (float)t << ". Creating new panel...\n";

        /*
        ShotContext shotContext = findShotForTime(t);

        if (!shotContext.shot) {


            // Insert a new shot here
            QString newShotName = generateUniqueShotName(shotContext.scene);
            Shot newShot;
            newShot.name = newShotName.toStdString();
            newShot.startTime = t;
            newShot.endTime = t + 1.0; // 1 second duration

            Panel defaultPanel;
            defaultPanel.startTime = 0.0;
            defaultPanel.durationTime = 1.0;

            newShot.panels.push_back(defaultPanel);
            shotContext.scene->shots.push_back(newShot); // Or insert into correct position

            Log().info() << "Created new shot " << newShot.name.c_str() << " with one default panel\n";
        }
        else
        {
            Log().info() << "Create new Panel in empty Shot\n";
        }
*/
    }
}

void MainWindow::deletePanel(double t) {
    Log().info() << "Delete Panel @ "<<(float)t<<"\n";

    PanelContext panelContext = findPanelForTime(t);

    if(panelContext.isValid()){
        TrackItem * track = timeLineView->getTrack(0);
        Segment *segment = track->getSegmentByUuid(panelContext.shot->uuid.c_str());
        if(segment){
            bool removedMarker = segment->removeMarkerItemByUuid(panelContext.panel->uuid.c_str());
            if (removedMarker) {
                Log().info() << "Marker deleted on timeline.\n";

                bool removedPanel = panelContext.shot->removePanelByUuid(panelContext.panel->uuid);
                if(removedPanel)
                    Log().info() << "Panel deleted.\n";
            }

            syncPanelDurations(segment, panelContext.shot);
        }
    }
}

QString MainWindow::generateUniqueShotName(Scene* scene) {
    int index = 1;
    QString base = "SHOT";
    while (true) {
        QString candidate = QString("%1_%2").arg(base).arg(index, 3, 10, QChar('0'));
        auto it = std::find_if(scene->shots.begin(), scene->shots.end(),
                               [&](const Shot& s) { return s.name.c_str() == candidate; });
        if (it == scene->shots.end())
            return candidate;
        ++index;
    }
}

void MainWindow::syncPanelDurations(Segment* segment, Shot* shot)
{
    if (!segment || !shot)
        return;

    segment->updateMarkersEndTime();

    for (auto& panel : shot->panels) {
        MarkerItem* panelMarker = segment->getMarkerItemByUuid(panel.uuid.c_str());
        if (panelMarker)
            panel.durationTime = panelMarker->duration();
    }
}

void MainWindow::timelineOptions(){
    TimelineOptionsDialog optionsDialog(this);

    optionsDialog.setOptionsFromTimeline(timeLineView); // <- prefill dialog with timeline state

    if (optionsDialog.exec() == QDialog::Accepted) {
        QString unitsHeader = optionsDialog.getDisplayUnitsHeader();
        QString unitsCursor = optionsDialog.getDisplayUnitsCursor();
        bool snapToFrame = optionsDialog.getSnapToFrame();
        int startTime = optionsDialog.getStartTime();
        int endTime = optionsDialog.getEndTime();
        double playbackSpeed = optionsDialog.getPlaybackSpeed();
        double frameRate = optionsDialog.getFrameRate();

        // Handle the options
        qDebug() << "Header Units:" << unitsHeader;
        qDebug() << "Cursor Units:" << unitsCursor;
        qDebug() << "Snap to Frame:" << snapToFrame;
        qDebug() << "Start Time:" << startTime;
        qDebug() << "End Time:" << endTime;
        qDebug() << "Playback Speed:" << playbackSpeed;
        qDebug() << "Frame Rate:" << frameRate;

        // Apply these options to the timeline
        optionsDialog.applyToTimeline(timeLineView);     // <- apply changes
    }
}

void MainWindow::play() {
    qDebug() << "Play pressed";

    if (isPlaying) return;

    isPlaying = true;

    paint->getPaintArea()->setFpsDisplay(true);
    paint->getPaintArea()->startPlayback();

    // If range not set, default to full timeline length
    if (playbackEnd <= playbackStart)
        playbackEnd = 10000 * 1000; // e.g., 10 milliseconds

    currentPlayTime = playbackStart;
    timeLineView->setTimeCursor(currentPlayTime);

    GameFusion::SoundServer::context()->seek(currentPlayTime);
    GameFusion::SoundServer::context()->play();

    playbackTimer->start(playbackIntervalMs);
}

void MainWindow::pause() {
    qDebug() << "Pause pressed";

    paint->getPaintArea()->setFpsDisplay(false);

    GameFusion::SoundServer::context()->stop();

    if (!isPlaying) return;


    playbackTimer->stop();
    isPlaying = false;
}

void MainWindow::stop() {

    paint->getPaintArea()->setFpsDisplay(false);

    GameFusion::SoundServer::context()->stop();

    if (!isPlaying) return;


    playbackTimer->stop();
    isPlaying = false;
    currentPlayTime = playbackStart;
    timeLineView->setTimeCursor(currentPlayTime);
}

void MainWindow::nextShot() {
    qDebug() << "Next Shot pressed";
    // TODO: Implement forward functionality
    timeLineView->gotoNextSegment();
}

void MainWindow::prevShot() {
    qDebug() << "Prev Shot pressed";
    // TODO: Implement backward functionality
    timeLineView->gotoPrevSegment();
}

void MainWindow::nextScene() {
    qDebug() << "Next Scene pressed";
    // TODO: Implement forward functionality
    timeLineView->gotoNextScene();
}

void MainWindow::prevScene() {
    qDebug() << "Prev Scene pressed";
    // TODO: Implement backward functionality
    timeLineView->gotoPrevScene();
}

void MainWindow::onPlaybackTick(){

    // TODO implement a system to collect the


    //currentPlayTime += playbackIntervalMs;
    //long currentPlayTime = *GameFusion::GameContext->gameTime();
    //currentPlayTime += playbackIntervalMs;

    long currentPlayTime = GameFusion::SoundDevice::Context()->currentTime();

    if (currentPlayTime > playbackEnd) {
        if (loopEnabled) {
            currentPlayTime = playbackStart;
        } else {
            stop(); // calls pause + reset
            return;
        }
    }

    // timelineView->setFrameCursorPosition(currentPlayTime);
    timeLineView->setTimeCursor(currentPlayTime);
}

void MainWindow::addAudioTrack() {
    // Example: assuming you have a pointer to your TimeLineView
    if (!timeLineView)
        return;

    // Create a new empty Track and add it
    Track* newTrack = new Track("Track 3", 0, 170000, TrackType::Audio);
    TrackItem *t1 = timeLineView->addTrack(newTrack);
    //newTrack->setType(Track::Audio); // If you have a type enum or similar
    //timelineView->addTrack(newTrack);

    saveAudioTracks();
}

QString sanitizeTrackName(const QString& name) {
    QString result;
    result.reserve(name.length());

    for (QChar c : name) {
        if (c.isLetterOrNumber()) {
            result.append(c.toLower());
        } else if (!result.endsWith('_')) {
            result.append('_'); // replace spaces/punctuation with underscores
        }
    }

    // Trim trailing underscores
    while (result.endsWith('_'))
        result.chop(1);

    return result;
}

void MainWindow::saveAudioTracks() {
    QString projectDir = ProjectContext::instance().currentProjectPath();  // get root dir (e.g., ".../my_project/")
    QDir trackDir(projectDir + "/audio/tracks");
    if (!trackDir.exists()) {
        trackDir.mkpath(".");
    }

    int trackIndex = 0;
    for (TrackItem *trackItem : timeLineView->trackItems()) {
        if (!trackItem || trackItem->track()->getType() != TrackType::Audio)
            continue;

        trackIndex ++;

        Track *track = trackItem->track();
        if(track->getUuid().isEmpty())
            track->setUuid( QUuid::createUuid().toString(QUuid::WithoutBraces));

        QJsonObject trackObj;
        trackObj["uuid"] = track->getUuid();
        trackObj["name"] = track->getName();
        trackObj["type"] = "audio";
        //trackObj["user"] = currentUser();  // optional
        trackObj["created"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        trackObj["index"] = trackIndex;
        trackObj["tag"] = track->getTag();
        trackObj["description"] = track->getDescription();
        QJsonArray segmentsArray;

        for (Segment *segment : trackItem->segments()) {
            AudioSegment *audio = dynamic_cast<AudioSegment *>(segment);
            if (!audio) continue;

            QJsonObject segObj;
            segObj["uuid"] = audio->getUuid();
            segObj["file"] = audio->filePath();
            segObj["startTime"] = audio->timePosition();         // timeline start (ms)
            segObj["inOffset"] = audio->inOffset();           // crop in (ms)
            segObj["outOffset"] = audio->outOffset();         // crop out (ms)
            segObj["duration"] = audio->getDuration();           // final duration (ms)
            segObj["volume"] = audio->volume();
            segObj["pan"] = audio->pan();

            segmentsArray.append(segObj);
        }

        trackObj["segments"] = segmentsArray;

        // Write to file
        QString trackNameSanitized = sanitizeTrackName(track->getName());
        QString filename = QString("track_%1_%2.json")
                               .arg(trackIndex, 2, 10, QChar('0'))  // index padded with zeros
                               .arg(trackNameSanitized);
        QString filePath = trackDir.filePath(filename);

        //QString filePath = trackDir.filePath("track_" + track->getUuid() + ".json");
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QJsonDocument(trackObj).toJson(QJsonDocument::Indented));
            file.close();
        } else {
            qWarning() << "Failed to write track file:" << filePath;
        }


    }

    qDebug() << "Audio tracks saved successfully.";
}

void MainWindow::loadAudioTracks() {
    QString trackDirPath = ProjectContext::instance().currentProjectPath() + "/audio/tracks";
    QDir trackDir(trackDirPath);
    if (!trackDir.exists()) {
        qWarning() << "Track directory does not exist:" << trackDirPath;
        return;
    }

    QStringList trackFiles = trackDir.entryList(QStringList() << "track_*.json", QDir::Files);
    trackFiles.sort();  // ensure order by filename (track index)

    int trackIndex = 0;
    for (const QString &filename : trackFiles) {
        QFile file(trackDir.filePath(filename));
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open track file:" << filename;
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject()) {
            qWarning() << "Invalid JSON format in" << filename;
            continue;
        }

        QJsonObject obj = doc.object();
        QString name = obj["name"].toString("Unnamed");
        QString uuid = obj["uuid"].toString();
        QString tag = obj["tag"].toString();
        QString description = obj["description"].toString();

        trackIndex ++;

        TrackItem *trackItem = timeLineView->getTrack(trackIndex);
        Track *track = trackItem ? trackItem->track() : nullptr;
        if(track && track->getType() == TrackType::Audio){
            track->setName(name);
        }
        else{
            // Create and add track
            track = new Track(name, 0, 170000, TrackType::Audio);
            TrackItem *trackItem = timeLineView->addTrack(track);
        }

        track->setUuid(uuid);
        track->setTag(tag);
        track->setDescription(description);

        //TrackItem *trackItem = timeLineView->addTrack(track);
        //if (!trackItem) continue;

        // Add segments
        QJsonArray segments = obj["segments"].toArray();
        for (const QJsonValue &segVal : segments) {
            QJsonObject segObj = segVal.toObject();

            QString file = segObj["file"].toString();
            qint64 startTime = segObj["startTime"].toVariant().toLongLong();
            qint64 inOffset = segObj["inOffset"].toVariant().toLongLong();
            qint64 outOffset = segObj["outOffset"].toVariant().toLongLong();
            qint64 duration = segObj["duration"].toVariant().toLongLong();
            float volume = segObj["volume"].toDouble(1.0);
            float pan = segObj["pan"].toDouble(0.0);

            if(!trackItem)
                continue;

            AudioSegment *segment = new AudioSegment(trackItem->scene(), startTime, duration);

            QString absoluteAudioPath;
            if (QDir::isAbsolutePath(file)) {
                absoluteAudioPath = QDir::toNativeSeparators(file); // Use as-is, convert separators
            } else {
                absoluteAudioPath = QDir::toNativeSeparators(ProjectContext::instance().currentProjectPath() + "/" + file); // Relative to project
            }

            segment->loadAudio("Segment", absoluteAudioPath.toUtf8().constData());
            segment->setInOffset(inOffset);
            segment->setOutOffset(outOffset);
            segment->setVolume(volume);
            segment->setPan(pan);

            trackItem->addSegment(segment);
        }
    }

    qDebug() << "Audio tracks loaded successfully.";
}

void MainWindow::onNewCamera() {

    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    CameraContext cameraContext = findCameraForTime(currentTime);
    //PanelContext panelContext = findPanelForTime(currentTime);
    if (!cameraContext.isValid()) {
        QMessageBox::warning(this, "Error", "No valid camera selected. Please select a time in the timeline.");
        return;
    }


    // now we can copy the camera if we want, but here we only realy want to create a new camera at the current time

    //------

    PanelContext panelContext = findPanelForTime(currentTime);
    if(!panelContext.isValid())
        return;

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble();
    qreal mspf = 1000.0f / fps;
    GameFusion::CameraFrame newCamera;

    newCamera.panelUuid = panelContext.panel->uuid;

    //newCamera.frameOffset = (currentTime - panelContext.shot->startTime)/mspf;
    qreal frame = (currentTime - panelContext.shot->startTime) / mspf;
    newCamera.frameOffset = static_cast<int>(qRound(frame));
    newCamera.time = newCamera.frameOffset * mspf; // may not actually be used anywhere
    newCamera.x = 0;
    newCamera.y = 0;
    newCamera.zoom = 1;

    undoStack->push(new AddCameraCommand(this, newCamera, currentTime));
}

void MainWindow::deleteCamera(QString uuid, double cursorTime)
{
    std::string uuidStr = uuid.toStdString();
    CameraContext cameraContext = findCameraByUuid(uuidStr);
    if(!cameraContext.isValid())
        return;

    PanelContext panelContext = findPanelByUuid(cameraContext.camera->panelUuid);
    if(!panelContext.isValid())
        return;

    timeLineView->removeCameraKeyFrame(uuid);
    scriptBreakdown->deleteCameraFrame(uuidStr);

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble();
    long panelStartTime = panelContext.shot->startTime + panelContext.panel->startTime;
    paint->getPaintArea()->setPanel(*panelContext.panel, panelStartTime, fps, panelContext.shot->cameraAnimation);
    cameraSidePanel->setCameraList(panelContext.panel->uuid.c_str(), panelContext.shot->cameraAnimation.frames);
    timeLineView->setTimeCursor(cursorTime);

    panelContext.scene->dirty = true;
}

void MainWindow::newCamera(const GameFusion::CameraFrame newCamera, double currentTime)
{
    PanelContext panelContext = findPanelForTime(currentTime);
    if(!panelContext.isValid())
        return;

    CameraTrack* cameraTrack = timeLineView->getCameraTrack();
    if (!cameraTrack)
        return;

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble();
    qreal mspf = 1000./fps;
    cameraTrack->addKeyFrame(newCamera.uuid.c_str(), panelContext.shot->startTime + newCamera.frameOffset*mspf, panelContext.panel->uuid.c_str());
    timeLineView->update();

    long panelStartTime = panelContext.shot->startTime + panelContext.panel->startTime;

    panelContext.shot->cameraAnimation.frames.push_back(newCamera);
    panelContext.scene->dirty = true;

    paint->getPaintArea()->setPanel(*panelContext.panel, panelStartTime, fps, panelContext.shot->cameraAnimation);
    cameraSidePanel->setCameraList(panelContext.panel->uuid.c_str(), panelContext.shot->cameraAnimation.frames);
    timeLineView->setTimeCursor(currentTime);
}

void MainWindow::onDuplicateCamera() {
    paint->duplicateActiveCamera();
}

void MainWindow::onDeleteCamera() {
    paint->deleteActiveCamera();
}

void MainWindow::onRenameCamera() {

    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available\n";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    CameraContext cameraContext = findCameraForTime(currentTime);
    if (!cameraContext.isValid()) {
        GameFusion::Log().error() << "No valid camera selected at time " << (int)currentTime << "\n";
        QMessageBox::warning(this, "Error", "No valid camera selected. Please select a time with a camera.");
        return;
    }

    QString oldName = QString::fromStdString(cameraContext.camera->name);
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Camera"),
                                            tr("Enter new camera name:"),
                                            QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.isEmpty()) {
        GameFusion::Log().info() << "Camera rename cancelled for UUID " << cameraContext.camera->uuid.c_str() << "\n";
        return;
    }

    undoStack->push(new RenameCameraCommand(this,
                                           QString::fromStdString(cameraContext.camera->uuid),
                                           oldName, newName, currentTime));

    //paint->renameActiveCamera();
}


void MainWindow::renameCamera(const QString& uuid, const QString& newName, double cursorTime) {
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        return;
    }

    CameraContext cameraContext = findCameraByUuid(uuid.toStdString());
    if (!cameraContext.isValid()) {
        GameFusion::Log().error() << "Camera with UUID " << uuid.toUtf8().constData() << " not found\n";
        return;
    }

    // Update CameraFrame name
    std::string oldName = cameraContext.camera->name;
    cameraContext.camera->name = newName.toStdString();
    cameraContext.scene->dirty = true;

    // Update timeline visualization
    CameraTrack* cameraTrack = timeLineView->getCameraTrack();
    if (cameraTrack) {
        CameraKeyFrame* keyFrame = cameraTrack->getKeyFrame(uuid);
        if (keyFrame) {
            keyFrame->name = newName; // Assumes setLabel exists
            cameraTrack->update();
        }
    }

    // Update paint area
    PanelContext panelContext = findPanelByUuid(cameraContext.camera->panelUuid);
    if (panelContext.isValid()) {
        long panelStartTime = panelContext.shot->startTime + panelContext.panel->startTime;
        qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble();
        paint->getPaintArea()->setPanel(*panelContext.panel, panelStartTime, fps, panelContext.shot->cameraAnimation);
    }

    // Update side panel
    cameraSidePanel->setCameraList(QString::fromStdString(cameraContext.camera->panelUuid),
                                   cameraContext.shot->cameraAnimation.frames);

    // Update timeline cursor and scene
    timeLineView->setTimeCursor(cursorTime);
    timeLineView->scene()->update(timeLineView->scene()->sceneRect());
    timeLineView->viewport()->update();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    GameFusion::Log().info() << "Renamed camera UUID " << uuid.toUtf8().constData()
                             << " from " << oldName.c_str() << " to " << newName.toUtf8().constData() << "\n";
}

void MainWindow::onCameraFrameAddedFromPaint(const GameFusion::CameraFrame& frame) {
    scriptBreakdown->addCameraFrame(frame);

    auto& scenes = scriptBreakdown->getScenes();
    GameFusion::Shot *theShot = nullptr;
    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {
            for(auto &panel : shot.panels){
                if(panel.uuid == frame.panelUuid){
                    cameraSidePanel->setCameraList(frame.panelUuid.c_str(), shot.cameraAnimation.frames);


                    return;
                }
            }
        }
    }
}

void MainWindow::onCameraFrameAddedFromSidePanel(const GameFusion::CameraFrame& frame) {
    scriptBreakdown->addCameraFrame(frame);
}

void MainWindow::onCameraFrameUpdated(const GameFusion::CameraFrame& frame, bool isEditing) {

    if (!isEditing) {  // Only for non-edits; assume m_undoStack is your QUndoStack*
        CameraContext cameraCtx = findCameraByUuid(frame.uuid);
        if(!cameraCtx.isValid())
            return;
        GameFusion::CameraFrame oldFrame = *cameraCtx.camera;  // Add this getter if needed
        // Push command for undoable update
        undoStack->push( new UpdateCameraCommand(this, scriptBreakdown, oldFrame, frame, QString::fromUtf8(frame.uuid.c_str())) );
        return;
    }

    setCamera(frame, frame.uuid.c_str(), isEditing);
    // Proceed with update (always, as it's the core action)
    //scriptBreakdown->updateCameraFrame(frame);
    //onRequestCameraThumbnail(frame.uuid.c_str(), isEditing);
}

void MainWindow::setCamera(const GameFusion::CameraFrame &cameraframe, const std::string &cameraUuid, bool isEditing){
    CameraContext cameraCtx = findCameraByUuid(cameraUuid);
    if(!cameraCtx.isValid())
        return;

    scriptBreakdown->updateCameraFrame(cameraframe);
    onRequestCameraThumbnail(cameraUuid.c_str(), isEditing);

    paint->getPaintArea()->updateCamera(cameraframe);
}

void MainWindow::onCameraFrameDeleted(const QString& uuid) {
    scriptBreakdown->deleteCameraFrame(uuid.toStdString());
}

void MainWindow::updateWindowTitle(bool isModified) {
    QString projectName = ProjectContext::instance().projectJson()["projectName"].toString("Untitled Project");
    QString title = projectName;

    if (isModified) {
        title += " *";
    }

    title += " - B-Line";
    setWindowTitle(title);
}

void MainWindow::updateLayerThumbnail(const QString& uuid, const QImage& thumbnail)
{
    // Debug: Check if thumbnail is
    /**********
    if (thumbnail.isNull()) {
        qDebug() << "Thumbnail is null for layer:" << uuid;
        Log().info() << "Thumbnail is null for layer:" << uuid.toUtf8().constData() << "\n";
    } else {
        qDebug() << "Thumbnail size:" << thumbnail.size()
        << "Format:" << thumbnail.format()
        << "Is grayscale:" << thumbnail.isGrayscale();

        Log().info() << "Thumbnail size:" << thumbnail.size().width() << " " << thumbnail.size().height() << "\n";
    }

    bool hasVisiblePixels = false;
    for (int y = 0; y < thumbnail.height() && !hasVisiblePixels; ++y) {
        const QRgb *row = reinterpret_cast<const QRgb*>(thumbnail.constScanLine(y));
        for (int x = 0; x < thumbnail.width(); ++x) {
            if (qAlpha(row[x]) > 0) {
                hasVisiblePixels = true;
                break;
            }
        }
    }
    qDebug() << "Thumbnail has visible pixels:" << hasVisiblePixels;

    // Optionally, display the thumbnail in a temporary QLabel for debugging
    QLabel *debugLabel = new QLabel();
    debugLabel->setPixmap(QPixmap::fromImage(thumbnail));
    debugLabel->setScaledContents(true); // Ensures the pixmap fills the QLabel
    debugLabel->setWindowTitle("Debug Thumbnail");
    debugLabel->resize(thumbnail.size());
    debugLabel->show();
********/

    // Find existing item with matching UUID
    QListWidgetItem *item = nullptr;
    for (int i = 0; i < ui->layerListWidget->count(); ++i) {
        QListWidgetItem *current = ui->layerListWidget->item(i);
        QString currentUuid = current->data(Qt::UserRole).toString();
        if (currentUuid == uuid) {
            item = current;
            break;
        }
    }



    // If no item found, create a new one
    if (!item) {

        return;

        item = new QListWidgetItem(ui->layerListWidget);
        item->setData(Qt::UserRole, uuid); // Store UUID for identification
        ui->layerListWidget->addItem(item);
    }


    // disable the item changed callback to avoid infinit callback loops
    // Disable signals to avoid recursive itemChanged or other event triggers
    bool prevState = ui->layerListWidget->blockSignals(true);
    QString layerName = item->text();
    // Set thumbnail and name
    item->setIcon(QIcon(QPixmap::fromImage(thumbnail))); // First column: thumbnail
    ui->layerListWidget->setIconSize(QSize(250, 150)); // or whatever size you want
    //ui->layerListWidget->setViewMode(QListView::IconMode);
    //ui->layerListWidget->setResizeMode(QListView::Adjust);
    QImage debugThumbnail = thumbnail;
    QPainter p(&debugThumbnail);
    p.setPen(Qt::red);
    p.drawRect(0, 0, debugThumbnail.width() - 1, debugThumbnail.height() - 1);
    p.end();

    item->setIcon(QIcon(QPixmap::fromImage(debugThumbnail)));
    item->setText(layerName); // Second column: name
    // Re-enable signals
    ui->layerListWidget->blockSignals(prevState);

    // In constructor or setupUi()
    ui->layerListWidget->setIconSize(QSize(debugThumbnail.width(), debugThumbnail.height()));
    ui->layerListWidget->update(); // Force redraw
}

void MainWindow::onPaintAreaImageModified(const QString& uuid, const QImage& image, bool isEditing)
{
    if (uuid.isEmpty())
        return;

    PanelContext panelContext = this->findPanelByUuid(uuid.toStdString());
    if (!panelContext.isValid())
        return;

    //--- get the storyboard track
    TrackItem *track = timeLineView->getTrack(0);
    if (!track)
        return;

    //--- get the ShotSegment* segment from the timelineView
    Segment *segment = track->getSegmentByUuid(panelContext.shot->uuid.c_str());
    if (!segment)
        return;

    // Safe cast to ShotSegment
    ShotSegment *shotSegment = dynamic_cast<ShotSegment*>(segment);
    if (!shotSegment)
        return;

    MarkerItem *marker = shotSegment->getMarkerItemByUuid(uuid);
    if (!marker)
        return;

    // Safe cast to PanelMarker
    PanelMarker *panelMarker = dynamic_cast<PanelMarker*>(marker);
    if (!panelMarker)
        return;

    // --- Resize the image to height = 200px, maintain 16:9 aspect ratio
    int targetHeight = 200;
    int targetWidth = static_cast<int>(targetHeight * 16.0 / 9.0);

    QImage resizedImage;

    if (image.isNull()) {
        // Create a white image with 16:9 aspect ratio
        resizedImage = QImage(targetWidth, targetHeight, QImage::Format_ARGB32);
        resizedImage.fill(Qt::white);
    } else
        resizedImage = image.scaled(targetWidth, targetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // If we are done editing, save the thumbnail to disk
    if (!isEditing) {
        QString imagePath = ProjectContext::instance().currentProjectPath() + "/thumbnails/panel_" + uuid + ".png";
        QDir().mkpath(QFileInfo(imagePath).absolutePath()); // Ensure the directory exists
        resizedImage.save(imagePath, "PNG");
    }

    // Set the resized thumbnail in the marker
    panelMarker->setThumbnail(resizedImage);
}

void MainWindow::showOptionsDialog()
{
    OptionsDialog dialog(this);
    // Access PaintArea's settings
    dialog.findChild<QCheckBox*>()->setChecked(
        paint->getPaintArea()->selectionSettings().multiLayerSelection
        );

    if (dialog.exec() == QDialog::Accepted) {
        paint->getPaintArea()->selectionSettings().multiLayerSelection =
            dialog.isMultiLayerSelectionEnabled();

        qDebug() << "Multi-layer selection:"
                 << (paint->getPaintArea()->selectionSettings().multiLayerSelection ? "enabled" : "disabled");
    }
}

#include <QPdfWriter>
#include <QFont>
#include <QDateTime>

void MainWindow::exportStoryboardPDF() {

    exportStoryboardPDF2();
    exportStoryboardPDF3();

    // Configurable panel count (default to 3, mimicking Toon Boom)
    const int panelsPerRow = 3; // Can be made configurable via UI or project settings
    const int margin = 20;
    const int panelWidth = 250; // Base width for single-column panel
    const int panelHeight = 150; // Base height, adjustable for camera movement
    const int headerHeight = 40;
    const int textBlockHeight = 60;

    // Set PDF to landscape mode (e.g., 1080x1920 pixels for HD aspect)
    QPdfWriter pdf(ProjectContext::instance().currentProjectPath() + "/storyboard.pdf");
    pdf.setPageSize(QPageSize::A4);
    pdf.setPageOrientation(QPageLayout::Landscape);
    pdf.setResolution(300); // High resolution for clarity
    QPainter painter(&pdf);

    // Set font for text
    QFont font("Arial", 10);
    painter.setFont(font);

    int pageCount = 0;
    int y = headerHeight + margin;
    int x = margin;
    int panelCount = 0;
    int totalPanels = 0;

    int pageColumn = 0;

    const auto& scenes = scriptBreakdown->getScenes();
    for (const auto& scene : scenes) {
        if(scene.markedForDeletion)
            continue;
        for (const auto& shot : scene.shots) {
            totalPanels += shot.panels.size();
        }
    }

    for (const auto& scene : scriptBreakdown->getScenes()) {
        if(scene.markedForDeletion)
                    continue;
        for (const auto& shot : scene.shots) {
            for (const auto& panel : shot.panels) {
                // Load and determine panel image size based on camera movement

                QString imageRefPath;
                QImage imageRef;
                int screenWidth = 1920;
                int screenHeight = 1080;

                if(!panel.image.empty()){
                    imageRefPath = ProjectContext::instance().currentProjectPath() + "/movies/" + panel.image.c_str();
                    imageRef = QImage(imageRefPath);


                }

                int imageWidth = panelWidth;
                int imageHeight = panelHeight;


                if(!imageRef.isNull() && shot.cameraAnimation.frames.size() > 1) {
                    // For camera movement, span two columns and adjust height
                    imageWidth = panelWidth * 2;
                    // Assume bounding box height is the max of camera frame heights
                    qreal maxHeight = 0;
                    for (const auto& frame : shot.cameraAnimation.frames) {
                        QRect rect(frame.x, frame.y, screenWidth*frame.zoom, screenHeight*frame.zoom);
                        maxHeight = maxHeight > rect.height() ? maxHeight : rect.height();


                        imageHeight = qMin(300, qMax(panelHeight, static_cast<int>(maxHeight * (panelWidth * 2 / rect.width()))));
                    }

                }

                if(shot.cameraAnimation.frames.size() > 1)
                    pageColumn += 2;
                else
                    pageColumn ++;

                // New page or row if needed
                if (x + imageWidth > pdf.width() - margin || panelCount >= panelsPerRow) {
                    y += (imageHeight + textBlockHeight + margin) * 2;
                    x = margin;
                    panelCount = 0;
                }
                if (pageColumn >= 3 || y + imageHeight + textBlockHeight > pdf.height() - margin) {
                    // Add page number and title
                    QString pageText = QString("Storyboard - Page %1 of %2").arg(++pageCount).arg((totalPanels + panelsPerRow - 1) / panelsPerRow);
                    painter.drawText(margin, headerHeight / 2, pageText);
                    pdf.newPage();
                    y = (headerHeight + margin) * 2;
                    x = margin;
                    panelCount = 0;
                    pageColumn = 0;
                    painter.drawText(margin, headerHeight / 2, pageText);
                }

                // Draw panel info above image
                QString panelInfo = QString("Scene %1, Dur TC (Scene): %2s, Panel %3, Dur TC (Panel): %4s")
                                        .arg(scene.name.c_str())
                                        .arg(shot.frameCount / 1000.0, 0, 'f', 2)
                                        .arg(panel.name.c_str())
                                        .arg(panel.durationTime / 1000.0, 0, 'f', 2);
                painter.drawText(x, y - 10, panelInfo);

                // Draw image or placeholder
                if (!imageRef.isNull()) {
                    painter.drawImage(QRect(x, y, panelWidth, panelHeight), imageRef.scaled(panelWidth, panelHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                } else {
                    painter.drawText(x, y + imageHeight / 2, "[Missing Image]");
                }

                // Draw text blocks below image
                int textY = y + imageHeight + 5;
                if (!shot.action.empty()) {
                    painter.drawText(x, textY, QString("Action: %1").arg(QString::fromStdString(shot.action)));
                    textY += 15 * 2;
                }
                for(auto character: shot.characters) {
                //if (!shot.characters...dialog.empty()) {
                    painter.drawText(x, textY, QString("Dialog: %1").arg(QString::fromStdString(character.name)));
                    textY += 15 * 2;
                    painter.drawText(x, textY, QString("%1").arg(QString::fromStdString(character.dialogue)));
                    textY += 15 * 2;
                }
                if (!shot.cameraAnimation.frames.empty()) {
                    painter.drawText(x, textY, QString("Camera Motion: Pos(%1,%2) Rot:%3 Scale:%4")
                                         .arg(shot.cameraAnimation.frames[0].x)
                                         .arg(shot.cameraAnimation.frames[0].y)
                                         .arg(shot.cameraAnimation.frames[0].rotation)
                                         .arg(shot.cameraAnimation.frames[0].zoom));
                    textY += 15 * 2;
                }

                x += imageWidth + margin;
                panelCount++;


            }
        }
    }

    // Final page number
    QString pageText = QString("Storyboard - Page %1 of %2").arg(pageCount).arg((totalPanels + panelsPerRow - 1) / panelsPerRow);
    painter.drawText(margin, headerHeight / 2, pageText);
}

#include <QPdfWriter>
#include <QPageSize>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>

// Function to compute text width and height in pixels and points
QSize computeTextDimensions(const QString& text, const QFont& font, QPainter& painter, qreal maxWidthPoints = 0.0, int alignment = Qt::AlignLeft) {

    // Initialize QFontMetrics with the device for accurate DPI-based measurements
    QFontMetricsF fm(font, painter.device());

    // Calculate DPI and pixel-to-point conversion
    qreal dpi = painter.device()->logicalDpiX(); // Typically 300 for QPdfWriter
    qreal pixelsToPoints = dpi / 72.0; // e.g., 300 / 72 ≈ 4.1667

    // Convert maxWidthPoints to pixels for boundingRect
    qreal maxWidthPixels = maxWidthPoints /** pixelsToPoints*/;

    // Compute text dimensions in pixels using QRectF
    QRectF boundingBox(0, 0, maxWidthPixels > 0 ? maxWidthPixels : INT_MAX, INT_MAX);
    QRectF textRect = fm.boundingRect(boundingBox, alignment | Qt::TextWordWrap, text);

    // Convert to points
    qreal widthPoints = textRect.width() ;
    qreal heightPoints = textRect.height() ;

    return QSize(widthPoints, heightPoints);
}

void MainWindow::exportStoryboardPDF2() {
    QString pdfPath = ProjectContext::instance().currentProjectPath() + "/storyboard2.pdf";
    QPdfWriter pdf(pdfPath);
    pdf.setPageSize(QPageSize(QPageSize::A4)); // A4 landscape
    pdf.setPageOrientation(QPageLayout::Landscape);
    pdf.setResolution(300); // High-quality
    QPainter painter(&pdf);

    const int panelsPerPage = 3; // Configurable later
    const int margin = 50;
    const int panelSpacing = 30;
    float headerHeight = 80;

    // Set up fonts
    QFont headerFont("Arial", 14, QFont::Bold);
    QFont textFont("Arial", 10);
    QFont textFontSmall("Arial", 8);

    QFontMetrics headerFm(headerFont, &pdf);
    QFontMetrics textFm(textFont, &pdf);
    painter.setFont(headerFont);

    // Conversion factor: pixels to points
    qreal dpi = pdf.resolution(); // 300 DPI
    qreal pixelsToPoints = dpi / 72.0; // 300 / 72 ≈ 4.1667

    int pageNumber = 1;

    int x = margin;
    int y = margin + headerHeight;

    // Precompute total pages
    int totalPanels = 0;
    for (const auto& scene : scriptBreakdown->getScenes()) {
        if(scene.markedForDeletion)
                    continue;
        for (const auto& shot : scene.shots) {
            for (const auto& panel : shot.panels) {
                bool spansTwoColumns = !shot.cameraAnimation.frames.empty() && shot.cameraAnimation.frames.size() > 1;
                totalPanels += spansTwoColumns ? 2 : 1;
            }
        }
    }
    int totalPages = (totalPanels + panelsPerPage - 1) / panelsPerPage;

    // Draw initial header
    headerHeight = drawStoryboardHeader(headerFont, painter, pageNumber, totalPages);
    y = margin + headerHeight * 4; // on first page set defaults

    int panelCountOnPage = 0;
    bool priorPagePanelSpansTwoColumns = false;

    for (const auto& scene : scriptBreakdown->getScenes()) {
        if(scene.markedForDeletion)
                    continue;
        for (const auto& shot : scene.shots) {
            int panelNumber = 0;
            for (const auto& panel : shot.panels) {

                panelNumber ++;
                // --- Determine if camera spans two columns ---
                bool spansTwoColumns = !shot.cameraAnimation.frames.empty() && shot.cameraAnimation.frames.size() > 1;

                if ((panelCountOnPage+spansTwoColumns) >= panelsPerPage) {
                    pdf.newPage();
                    pageNumber++;

                    headerHeight = drawStoryboardHeader(headerFont, painter, pageNumber, totalPages);

                    x = margin;
                    y = margin + headerHeight * 4;
                    panelCountOnPage = 0;
                }

                // Calculate panel dimensions in points
                int panelWidth = (pdf.width() - (margin * 2) - (panelSpacing * 2)) / panelsPerPage;
                if (spansTwoColumns)
                    panelWidth = panelWidth * 2 + panelSpacing;
                int panelHeight = panelWidth * 9 / 16; // Maintain 16:9

                QRect panelRect(x, y, panelWidth, panelHeight);

                //------

                //qreal panelTimeStart = panelFrameStart * mspf;
                //qreal panelDuration = panel.durationFrames * mspf;


                // todo , first see if thumbnail exists and use this
                QString thumbnailPath = ProjectContext::instance().currentProjectPath() + "/thumbnails/panel_" + panel.uuid.c_str() + ".png";

                if (!QFile::exists(thumbnailPath)){ // fallback 1

                    qreal panelTimeStart = shot.startTime + panel.startTime;
                    // Set timeline cursor to panel frame to compute thumnail for panel
                    timeLineView->setTimeCursor(panelTimeStart);
                }

                if (!QFile::exists(thumbnailPath)){ // fallback 2
                    thumbnailPath = ProjectContext::instance().currentProjectPath() + "/movies/" + panel.thumbnail.c_str();
                }

                //------

                QImage image(thumbnailPath);
                if (!image.isNull()) {
                    painter.drawImage(panelRect, image);
                    // todo draw rect frame in black, make sure it is visible ontop of image

                    // Set a visible pen for the frame
                    QPen framePen(Qt::black);
                    framePen.setWidthF(2.0); // thicker line for better visibility
                    painter.setPen(framePen);
                    painter.setBrush(Qt::NoBrush); // No fill, only outline

                    // Draw the rectangle frame over the image
                    painter.drawRect(panelRect);
                } else {
                    painter.fillRect(panelRect, Qt::lightGray);
                    painter.drawText(panelRect, Qt::AlignCenter, "[Missing Image]");
                }

                // --- Draw Panel Header Text ---
                painter.setFont(textFont);

                QString scText = QString("SC:  %1").arg(QString::fromStdString(scene.name));
                QString scTcText = QString("TC: %1").arg("00:00:00:00"); // Placeholder
                QString pnlText = QString("PNL: %1").arg(QString::number(panelNumber));
                QString pnlTcText = QString("TC: %1").arg("00:00:01:12"); // Placeholder

                // Left aligned texts
                QSize scSize = computeTextDimensions(scText, textFont, painter);
                QSize pnlSize = computeTextDimensions(pnlText, textFont, painter);

                // Timecode texts (should be aligned vertically)
                QSize scTcSize = computeTextDimensions(scTcText, textFont, painter);
                QSize pnlTcSize = computeTextDimensions(pnlTcText, textFont, painter);

                // Calculate baseline Y for the two lines
                int lineSpacing = 4; // space between lines
                int totalHeight = scSize.height() + lineSpacing + pnlSize.height();
                int yBase = panelRect.y() - totalHeight - 5 * 2; // 5 px margin above panel

                // X positions
                int leftX = panelRect.x();
                int tcAlignedX = panelRect.x() + panelRect.width() * 0.5;

                // Draw first line: SC and SC-TC
                painter.drawText(leftX, yBase + scSize.height(), scText);
                painter.drawText(tcAlignedX, yBase + scSize.height(), scTcText);

                // Draw second line: PNL and PNL-TC
                painter.drawText(leftX, yBase + scSize.height() + lineSpacing + pnlSize.height(), pnlText);
                painter.drawText(tcAlignedX, yBase + scSize.height() + lineSpacing + pnlSize.height(), pnlTcText);



                // --- Draw Text Blocks Underneath ---

                painter.setFont(textFontSmall);

                int textY = panelRect.bottom() + 15;
                if (!shot.action.empty()) {
                    painter.drawText(x, textY, "Action: " + QString::fromStdString(shot.action));
                    textY += 15;
                }

                int yOffset = panelRect.y() + panelRect.height() + margin; // The top of where dialogs start on the PDF
                int xOffset = panelRect.x();

                std::string dialogues =shot.getDialogs();
                if (!dialogues.empty()) {

                    for (const auto& character : shot.characters) {
                        // Build the full text block for this character
                        QString dialogBlock = QString::fromStdString(std::to_string(character.dialogNumber))
                                              + " " + QString::fromStdString(character.name)
                                              + (character.onScreen ? "" : " (OS)") + "\n";

                        if (!character.dialogParenthetical.empty() && character.dialogParenthetical != character.dialogue )
                            dialogBlock += "("+QString::fromStdString(character.dialogParenthetical) + ")"+"\n";

                        dialogBlock += QString::fromStdString(character.dialogue);

                        // Compute the size this text block will occupy
                        QSize textSize = computeTextDimensions(dialogBlock, textFontSmall, painter, panelRect.width());

                        // Define a bounding rectangle for this block
                        QRect textRect(xOffset, yOffset, panelRect.width(), textSize.height());

                        // Draw the text (supports line breaks via Qt::TextWordWrap)
                        painter.drawText(textRect, Qt::AlignLeft | Qt::TextWordWrap, dialogBlock);

                        // Advance yOffset for next dialog
                        yOffset += textSize.height() + margin; // Add padding between dialogs
                    }
                }

                int sectionSpacing = 8;

                auto joinStrings = [](const std::vector<std::string>& values) -> std::string {
                    std::string result;
                    for (size_t i = 0; i < values.size(); ++i) {
                        result += values[i];
                        if (i < values.size() - 1)
                            result += ", "; // or "\n" if you want line breaks
                    }
                    return result;
                };

                auto drawSection = [&](const QString& title, const std::string& value) {
                    if (value.empty())
                        return;

                    // --- Draw horizontal divider ---
                    painter.setPen(QPen(Qt::black, 1));
                    painter.drawLine(xOffset, yOffset, xOffset + panelRect.width(), yOffset);
                    yOffset += lineSpacing * 2; // Padding after divider

                    // --- Compose wrapped text ---
                    QString text = QString("%1: %2").arg(title, QString::fromStdString(value));

                    // Compute text bounding size using word wrap
                    QRect textRect(xOffset,
                                   yOffset,
                                   panelRect.width(),
                                   painter.fontMetrics().boundingRect(
                                                            QRect(0, 0, panelRect.width(), INT_MAX),
                                                            Qt::TextWordWrap,
                                                            text).height());

                    // --- Draw the text with word wrapping ---
                    painter.setPen(Qt::black);
                    painter.drawText(textRect, Qt::AlignLeft | Qt::TextWordWrap, text);

                    // --- Add extra padding after text ---
                    yOffset += textRect.height() + sectionSpacing;
                };

                // Append sections conditionally
                drawSection("Audio SFX", joinStrings(shot.audio.sfx));
                drawSection("Audio Ambient", shot.audio.ambient);
                drawSection("Description", shot.description);
                drawSection("FX", shot.fx);
                drawSection("Intent", shot.intent);
                drawSection("Action", shot.action);
                //drawSection("Notes", shot.notes);
                drawSection("Time Of Day", shot.timeOfDay);
                drawSection("Lighting", shot.lighting);
                drawSection("Transition", shot.transition);
                drawSection("Type", shot.type);

                // --- Advance column position ---
                if (spansTwoColumns)
                    x += panelWidth + panelSpacing; // already spans two columns
                else
                    x += panelWidth + panelSpacing;

                panelCountOnPage++;

                if(spansTwoColumns)
                    panelCountOnPage++;


            }
        }
    }

    painter.end();
}


void MainWindow::exportStoryboardPDF3() {
    const QString outputPath = ProjectContext::instance().currentProjectPath() + "/storyboard3.pdf";
    QPdfWriter pdf(outputPath);
    pdf.setPageSize(QPageSize(QPageSize::A4)); // A4 landscape
    pdf.setPageOrientation(QPageLayout::Landscape);
    pdf.setResolution(300); // High-quality print resolution

    QPainter painter(&pdf);
    if (!painter.isActive()) {
        qWarning() << "Failed to open PDF for writing:" << outputPath;
        return;
    }

    const int margin = 40;
    const int pageWidth = pdf.width();
    const int pageHeight = pdf.height();
    const int columnWidth = (pageWidth - margin * 2) / 3; // 3 panels per row (Toon Boom default)
    const int defaultPanelHeight = 300;

    int x = margin;
    int y = margin;
    int panelsOnRow = 0;

    int pageNumber = 1;
    int totalPages = 1; // Optional: can compute in a pre-pass

    for (const auto& scene : scriptBreakdown->getScenes()) {
        if(scene.markedForDeletion)
                    continue;
        for (const auto& shot : scene.shots) {
            for (const auto& panel : shot.panels) {
                // Determine if this panel spans one or two columns
                bool isDoubleColumn = !shot.cameraAnimation.frames.empty(); // or (shot.cameraAnimation.frames.size() > 1)
                int panelWidth = isDoubleColumn ? columnWidth * 2 : columnWidth;

                // Determine panel height based on image aspect ratio or default
                QImage image(QString::fromStdString(panel.image));
                int panelHeight = defaultPanelHeight;
                if (!image.isNull()) {
                    QSize scaledSize = image.size().scaled(panelWidth, panelHeight, Qt::KeepAspectRatio);
                    panelHeight = scaledSize.height();
                }

                // **Handle page break properly**
                if (y + panelHeight + margin > pageHeight) {
                    pdf.newPage();
                    y = margin;
                    x = margin;
                    panelsOnRow = 0;
                    pageNumber++;
                }

                // Draw the image or placeholder
                if (!image.isNull()) {
                    painter.drawImage(QRect(x, y, panelWidth, panelHeight), image);
                } else {
                    painter.drawRect(QRect(x, y, panelWidth, panelHeight));
                    painter.drawText(x + 10, y + panelHeight / 2, "[Missing Image]");
                }

                // Draw metadata (Scene #, Panel #, Duration, etc.)
                QString meta = QString("Scene: %1 | Panel: %2")
                                   .arg(QString::fromStdString(scene.name))
                                   .arg(QString::fromStdString(panel.uuid));
                painter.drawText(x, y - 10, meta);

                // Draw text fields (Action, Dialog, Notes)
                int textY = y + panelHeight + 20;
                painter.drawText(x, textY, QString::fromStdString(shot.action));
                textY += 20;
                painter.drawText(x, textY, QString::fromStdString(shot.getDialogs()));

                // Update X and Y for next panel
                if (isDoubleColumn) {
                    x += panelWidth + margin;
                    panelsOnRow += 2;
                } else {
                    x += panelWidth + margin;
                    panelsOnRow++;
                }

                // Wrap to next row if three columns filled or double column overflows
                if (panelsOnRow >= 3 || x + columnWidth > pageWidth - margin) {
                    x = margin;
                    y += panelHeight + 100; // Extra space for text
                    panelsOnRow = 0;
                }
            }
        }
    }

    painter.end();
}

qreal MainWindow::drawStoryboardHeader(QFont &headerFont, QPainter &painter, int pageNumber, int totalPages) {
    //QFont headerFont("Arial", 16, QFont::Bold);
    painter.setFont(headerFont);

    const int margin = 50;
    QRect pageRect = painter.viewport(); // Full page dimensions
    QFontMetrics fm(headerFont);

    int dpi = painter.device()->logicalDpiX(); // typically 300 after setResolution
    qreal pixelsToPoints =  dpi / 72.0;

    // Left-aligned title
    // Construct title with project name, date, time, and cool separator
    QString dateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    QString titleText = QString("B-Line Storyboard Export — %1 — %2")
                            .arg(ProjectContext::instance().currentProjectName(), dateTime);

    QSize titleSize = computeTextDimensions(titleText, headerFont, painter);

    QRect titleRect(pageRect.left() + margin, margin, titleSize.width(), titleSize.height());

    painter.drawText(titleRect.x(), titleRect.y()+titleRect.height(), titleText);
    //painter.drawRect(titleRect);

    // Right-aligned page number: Page X/Y
    QString pageText;
    if (totalPages > 0)
        pageText = QString("Page %1/%2").arg(pageNumber).arg(totalPages);
    else
        pageText = QString("Page %1").arg(pageNumber);

    QSize pageNumberSize = computeTextDimensions(pageText, headerFont, painter);

    QRect pageNumberRect(pageRect.right() - margin - pageNumberSize.width(),
                         margin,
                         pageNumberSize.width(),
                         pageNumberSize.height());

    painter.drawText(pageNumberRect, Qt::AlignRight | Qt::AlignVCenter, pageText);
    //painter.drawRect(pageNumberRect);

    qreal headerHeight = pageNumberRect.height(); // TODO compute

    return headerHeight;
}

#include <QProgressDialog>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QApplication>

void MainWindow::exportMovie() {
    Log().info() << "Export Movie\n";

    if (!scriptBreakdown) {
        QMessageBox::warning(this, "Error", "No script loaded. Cannot export movie.");
        return;
    }

    // Pre-scan to compute total duration
    qint64 totalDurationMs = 0;
    float fps = ProjectContext::instance().projectJson()["fps"].toDouble(25.0);  // Default to 25 if not set
    const auto& scenes = scriptBreakdown->getScenes();
    for (const auto& scene : scenes) {
        if(scene.markedForDeletion)
            continue;
        for (const auto& shot : scene.shots) {
            for (const auto& panel : shot.panels) {
                totalDurationMs += panel.durationTime;
            }
        }
    }

    int totalFrames = static_cast<int>((totalDurationMs / 1000.0) * fps);
    if (totalFrames == 0) {
        QMessageBox::warning(this, "Error", "No frames to export.");
        return;
    }

    // Create export directory
    QString safeProjectName = ProjectContext::instance().currentProjectName().replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");  // Sanitize name
    QString dateStr = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString exportDir = ProjectContext::instance().currentProjectPath() + "/animatic/" + safeProjectName + "_" + dateStr;
    if (!QDir().mkpath(exportDir)) {
        QMessageBox::warning(this, "Error", "Failed to create export directory.");
        return;
    }

    // Progress dialog
    QProgressDialog progress("Exporting movie frames...", "Cancel", 0, totalFrames, this);
    progress.setWindowModality(Qt::WindowModal);

    // Export frame by frame
    qint64 currentTimeMs = 0;
    int frameNumber = 0;

    bool updateTimeLine = false; // false may be faster, false may be more precise ?

    paint->getPaintArea()->setExportMode(true);
    while (frameNumber < totalFrames) {
        if (progress.wasCanceled()) {
            Log().info() << "Export canceled by user\n";
            paint->getPaintArea()->setExportMode(false);
            return;
        }

        // Set timeline cursor to current frame
        if(updateTimeLine)
            timeLineView->setTimeCursor((long)currentTimeMs);
        else
            onTimeCursorMoved(currentTimeMs);

        // Render and save frame (assuming renderFrameToImage exists in PaintArea)
        QImage frameImage(1920, 1080, QImage::Format_ARGB32_Premultiplied);
        frameImage.fill(Qt::transparent);

        paint->getPaintArea()->renderFrameToImage(frameImage);
        if (frameImage.isNull()) {
            QMessageBox::warning(this, "Error", "Failed to render frame " + QString::number(frameNumber));
            paint->getPaintArea()->setExportMode(false);
            return;
        }

        QString framePath = exportDir + "/frame_" + QString::number(frameNumber, 10).rightJustified(5, '0') + ".png";
        if (!frameImage.save(framePath)) {
            QMessageBox::warning(this, "Error", "Failed to save frame " + framePath);
            paint->getPaintArea()->setExportMode(false);
            return;
        }

        // Advance to next frame
        currentTimeMs += static_cast<qint64>(1000.0 / fps);
        frameNumber++;
        progress.setValue(frameNumber);
        QApplication::processEvents();  // Keep UI responsive
    }

    progress.setValue(totalFrames);

    // Export Audio
    QString audioPath = exportDir + "/AudioTrack" + ".wav";
    TrackItem *track = timeLineView->getTrack(1);

    GameFusion::SoundStream *stream = track->getSoundStream();
    GameFusion::SoundTrack *atrack = (GameFusion::SoundTrack*)stream; // TODO safe cast and verify not null
    atrack->saveToFile(audioPath.toUtf8().constData());
    Log().info() << "Export audio track to << "<<audioPath.toUtf8().constData()<<"\n";
    QMessageBox::information(this, "Success", "Movie exported to: " + exportDir);

    paint->getPaintArea()->setExportMode(false);
}


void MainWindow::timelineCameraUpdate(const QString& uuid, long frameOffset, const QString& newPanelUuid) {
    /**********

    1.	Find the original camera frame.
    2.	Check if the panel UUID has changed.
    3.	If changed, verify whether it is in the same scene or a different one.
    4.	Move the camera frame to the new shot if necessary.
    5.	Mark the relevant scene as dirty if the camera moved or frame offset changed.

    ***********/


    GameFusion::Log().debug()
    << "Timeline camera update: uuid=" << uuid.toUtf8().constData()
    << ", frameOffset=" << frameOffset
    << ", newPanelUuid=" << newPanelUuid.toUtf8().constData()
    << "\n";

    // 1. Locate the original camera
    CameraContext cameraCtx = findCameraByUuid(uuid.toStdString());
    if (!cameraCtx.isValid()) {
        GameFusion::Log().warning() << "Camera with UUID " << uuid.toUtf8().constData() << " not found.\n";
        return;
    }

    bool markDirty = false;
    bool frameOffsetChanged = (cameraCtx.camera->frameOffset != frameOffset);

    // 2. Handle panel/shot/scene changes
    if (cameraCtx.camera->panelUuid != newPanelUuid.toStdString()) {
        GameFusion::Log().debug()
        << "Panel changed from " << cameraCtx.camera->panelUuid.c_str()
        << " to " << newPanelUuid.toUtf8().constData() << "\n";

        // Locate the new scene/shot for the panel
        ShotContext newCtx = findSceneByPanel(newPanelUuid.toStdString());
        if (!newCtx.isValid()) {
            GameFusion::Log().warning()
            << "New panel UUID " << newPanelUuid.toUtf8().constData() << " not found.\n";
            return;
        }

        // Update panel UUID
        std::string oldPanelUuid = cameraCtx.camera->panelUuid.c_str();
        cameraCtx.camera->panelUuid = newPanelUuid.toStdString();
        markDirty = true; // camera reassignment = dirty

        bool shotChanged = (cameraCtx.shot != newCtx.shot);
        bool sceneChanged = (cameraCtx.scene != newCtx.scene);

        if (shotChanged) {
            // Remove from old shot



            //--- make a copy of the camera data that will be deleted (to be able to insert it again)
            CameraFrame movedCamera = *cameraCtx.camera;
            auto& oldFrames = cameraCtx.shot->cameraAnimation.frames;
            oldFrames.erase(
                std::remove_if(oldFrames.begin(), oldFrames.end(),
                               [&](const GameFusion::CameraFrame& cam) { return cam.uuid == cameraCtx.camera->uuid; }),
                oldFrames.end()
                );

            // Add to new shot
            newCtx.shot->cameraAnimation.frames.push_back(movedCamera);
            cameraCtx.camera = &newCtx.shot->cameraAnimation.frames.back();
            cameraCtx.shot = newCtx.shot;
            cameraCtx.scene = newCtx.scene;

            // Both old and new scenes become dirty if they differ
            markDirty = true;
            if (sceneChanged) {
                newCtx.scene->setDirty(true);
            }

            // if newPanelUuid == currentPanelUuid
            if(newPanelUuid.toStdString() == currentPanel->uuid){
                //--- A CAMERA WAS ADDED TO CURRENT PANEL
                long panelStartTime = cameraCtx.shot->startTime + currentPanel->startTime;
                float fps = ProjectContext::instance().projectJson()["fps"].toDouble();
                paint->getPaintArea()->setPanel(*currentPanel, panelStartTime, fps, cameraCtx.shot->cameraAnimation);
                cameraSidePanel->setCameraList(currentPanel->uuid.c_str(), cameraCtx.shot->cameraAnimation.frames);
            }

            if(oldPanelUuid == currentPanel->uuid){
                //--- A CAMERA WAS REMOVED FROM CURRENT PANEL
                PanelContext panelContext = findPanelByUuid(currentPanel->uuid);
                if(panelContext.isValid()){
                    long panelStartTime = panelContext.shot->startTime + currentPanel->startTime;
                    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();
                    paint->getPaintArea()->setPanel(*currentPanel, panelStartTime, fps, panelContext.shot->cameraAnimation);
                    cameraSidePanel->setCameraList(currentPanel->uuid.c_str(), panelContext.shot->cameraAnimation.frames);
                }
            }
        }
    }

    // 3. Update frame offset if changed
    if (frameOffsetChanged) {
        cameraCtx.camera->frameOffset = frameOffset;
        markDirty = true;
    }

    // 4. Mark current scene as dirty if needed
    if (markDirty) {
        cameraCtx.scene->setDirty(true);
        GameFusion::Log().debug()
            << "Scene " << cameraCtx.scene->uuid.c_str()
            << " marked as dirty due to camera change.\n";
    }

    // TODO update camera side panel if necessary !!!
    paint->getPaintArea()->updateCamera(*cameraCtx.camera);

    // TODO trigger camera thumnail update
    onRequestCameraThumbnail(cameraCtx.camera->uuid.c_str(), false);
}

void MainWindow::timelineCameraDeleted(const QString& uuid) {
    GameFusion::Log().debug()
    << "Timeline camera delete: uuid=" << uuid.toUtf8().constData() << "\n";

    // 1. Locate the camera context
    CameraContext cameraCtx = findCameraByUuid(uuid.toStdString());
    if (!cameraCtx.isValid()) {
        GameFusion::Log().warning()
        << "Cannot delete camera: UUID " << uuid.toUtf8().constData() << " not found.\n";
        return;
    }

    QString panelUuid = cameraCtx.camera->panelUuid.c_str();

    // 2. Remove the camera from its shot
    auto& cameraFrames = cameraCtx.shot->cameraAnimation.frames;
    auto it = std::remove_if(
        cameraFrames.begin(), cameraFrames.end(),
        [&](const GameFusion::CameraFrame& cam) { return cam.uuid == cameraCtx.camera->uuid; }
        );

    if (it == cameraFrames.end()) {
        GameFusion::Log().warning()
        << "Camera UUID " << uuid.toUtf8().constData() << " not found in its shot.\n";
        return;
    }

    cameraFrames.erase(it, cameraFrames.end());

    // 3. Mark scene as dirty
    if (cameraCtx.scene) {
        cameraCtx.scene->setDirty(true);
        GameFusion::Log().debug()
            << "Scene " << cameraCtx.scene->uuid.c_str()
            << " marked dirty after camera deletion.\n";
    }
}


void MainWindow::onToolModeChanged(PaintArea::ToolMode mode)
{
    // Handle mode change in MainWindow
    if (mode == PaintArea::ToolMode::Camera) {
        strokeDock->setVisible(false);
        ui->dockCameras->setVisible(true);
        ui->dockLayers->setVisible(true);
        this->splitDockWidget(ui->dockLayers, ui->dockCameras, Qt::Horizontal);

    } else {
        strokeDock->setVisible(true); // Editable in Vector Edit
        ui->dockCameras->setVisible(false);
        ui->dockLayers->setVisible(true);
        this->splitDockWidget(ui->dockLayers, strokeDock, Qt::Horizontal);
    }
    // Update other UI elements as needed
}

void MainWindow::onStrokeSelected(const SelectionFrameUI& selectedStrokes) {
    // Handle stroke selection update
    // Example: Update UI or log selection
    qDebug() << "Stroke selected:" << selectedStrokes.selectedStrokes.size() << "strokes";
    // Add custom logic here, e.g., update strokeDock or other UI elements
    if (strokeDock) {
        strokeDock->setEnabled(selectedStrokes.selectedStrokes.size() > 0);

        if(selectedStrokes.selectedStrokes.size()){
            StrokeProperties strokeProperties = selectedStrokes.selectedStrokes.back().stroke->getStrokeProperties();
            strokeDock->setStrokeProperties(strokeProperties);
        }
    }
}

void MainWindow::onRequestCameraThumbnail(const QString &uuid, bool isEditing)
{
    if (uuid.isEmpty())
        return;

    QImage cameraThumbnail;
    // Ask PaintArea to generate a pixmap for this camera
    this->paint->getPaintArea()->generateCameraThumbnail(uuid, cameraThumbnail);

    if (cameraThumbnail.isNull())
        return;

    cameraSidePanel->updateCameraThumbnail(uuid, cameraThumbnail);

    // If we are done editing, save the thumbnail to disk
    if (!isEditing) {
        QString imagePath = ProjectContext::instance().currentProjectPath() + "/cameras/camera_" + uuid + ".png";
        QDir().mkpath(QFileInfo(imagePath).absolutePath()); // Ensure the directory exists
        cameraThumbnail.save(imagePath, "PNG");
    }
}

void MainWindow::exportEDL() {
    if (!scriptBreakdown) {
        QMessageBox::warning(this, "Error", "No script loaded. Cannot export EDL.");
        return;
    }

    QString outputPath = ProjectContext::instance().currentProjectPath() + "/animatic.edl";
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open EDL file for writing: " + outputPath);
        return;
    }

    QTextStream out(&file);

    QString projectName = ProjectContext::instance().currentProjectName();
    out << "TITLE: " << projectName << "\n";
    out << "FCM: NON-DROP FRAME\n\n";

    float fps = ProjectContext::instance().projectJson()["fps"].toDouble(25.0);
    const auto& scenes = scriptBreakdown->getScenes();

    auto msToTimecode = [fps](qint64 ms) -> QString {
        // Convert milliseconds to total frames
        qint64 totalFrames = static_cast<qint64>((ms / 1000.0) * fps);

        // Calculate timecode components
        qint64 framesPerHour = static_cast<qint64>(3600 * fps);
        qint64 framesPerMinute = static_cast<qint64>(60 * fps);

        int hours = static_cast<int>(totalFrames / framesPerHour);
        totalFrames %= framesPerHour; // Remaining frames after hours
        int minutes = static_cast<int>(totalFrames / framesPerMinute);
        totalFrames %= framesPerMinute; // Remaining frames after minutes
        int seconds = static_cast<int>(totalFrames / fps);
        int frames = static_cast<int>(totalFrames % static_cast<qint64>(fps));

        return QString("%1:%2:%3:%4")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(frames, 2, 10, QChar('0'));
    };

    // Video edits
    int editNumber = 1;
    QString dateStr = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    qint64 currentRecordTimeMs = 0;
    int clipIndex = 0;

    for (const auto& scene : scenes) {
        if(scene.markedForDeletion)
            continue;
        for (const auto& shot : scene.shots) {
            qint64 shotDurationMs = 0;
            for (const auto& panel : shot.panels) {
                shotDurationMs += panel.durationTime;
            }
            if (shotDurationMs <= 0) continue;

            QString clipName = QString("%1_Animatic_%2-%3.mov")
                                   .arg(projectName)
                                   .arg(dateStr)
                                   .arg(clipIndex, 3, 10, QChar('0'));

            QString sourceIn = "00:00:00:00";
            QString sourceOut = msToTimecode(shotDurationMs);
            QString recordIn = msToTimecode(currentRecordTimeMs);
            QString recordOut = msToTimecode(currentRecordTimeMs + shotDurationMs);

            out << QString("%1    001B V     C        %2 %3 %4 %5  \n")
                       .arg(editNumber, 3, 10, QChar('0'))
                       .arg(sourceIn)
                       .arg(sourceOut)
                       .arg(recordIn)
                       .arg(recordOut);
            out << "* FROM CLIP NAME:  " << clipName << "\n";
            out << "REEL 001B IS CLIP  " << clipName << "\n\n";

            currentRecordTimeMs += shotDurationMs;
            editNumber++;
            clipIndex++;
        }
    }

    // Audio edits (assuming timeline audio track has segments)
    // Reset currentRecordTimeMs if audio is overlaid on the same timeline
    // Here, we place audio edits after video, using their actual positions from the timeline

    TrackItem* audioTrack = timeLineView->getTrack(1); // Assuming index 1 is audio track
    if (audioTrack) {
        // Assuming getSegments() returns std::vector<Segment*>
        auto segments = audioTrack->segments();
        for (const auto& seg : segments) {
            // Assuming dynamic_cast to AudioSegment* if applicable
            AudioSegment* audioSeg = dynamic_cast<AudioSegment*>(seg);
            if (!audioSeg) continue;

            qint64 startMs = audioSeg->timePosition();
            qint64 durationMs = audioSeg->getDuration();
            if (durationMs <= 0) continue;

            QString clipName = QFileInfo(audioSeg->filePath()).completeBaseName(); // Assuming method exists
            if (clipName.isEmpty()) {
                // Generate a name if not available
                clipName = QString("%1-Dial_%2-XX.wav").arg(projectName).arg(clipIndex, 3, 10, QChar('0'));
                clipIndex++;
            }

            QString sourceIn = "00:00:00:00"; // Assuming full clip usage; adjust if trimmed
            QString sourceOut = msToTimecode(durationMs);
            QString recordIn = msToTimecode(startMs);
            QString recordOut = msToTimecode(startMs + durationMs);

            // Main audio channel
            out << QString("%1    001B A     C        %2 %3 %4 %5  \n")
                       .arg(editNumber, 3, 10, QChar('0'))
                       .arg(sourceIn)
                       .arg(sourceOut)
                       .arg(recordIn)
                       .arg(recordOut);
            out << "* FROM CLIP NAME:  " << clipName << "\n";
            out << "REEL 001B IS CLIP " << clipName << "\n";

            // Additional channel (e.g., AUD 3) as in example
            out << QString("%1    001B NONE  C        %2 %3 %4 %5  \n")
                       .arg(editNumber + 1, 3, 10, QChar('0'))
                       .arg(sourceIn)
                       .arg(sourceOut)
                       .arg(recordIn)
                       .arg(recordOut);
            out << "* FROM CLIP NAME:  " << clipName << "\n";
            out << "REEL 001B IS CLIP " << clipName << "\n";
            out << "AUD   3\n\n";

            editNumber += 2;
        }
    } else {
        // Fallback if no audio track: perhaps generate from dialogues
        // For simplicity, skip or add placeholder
        Log().info() << "No audio track found; skipping audio EDL entries.\n";
    }

    file.close();
    QMessageBox::information(this, "Success", "EDL exported to: " + outputPath);
}

void MainWindow::onNewScene()
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    NewSceneDialog dialog(fps, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString sceneName = dialog.getSceneName();
    double durationMs = dialog.getDurationMs();
    int shotCount = dialog.getShotCount();
    bool after = dialog.getInsertMode() == NewSceneDialog::InsertMode::InsertAfter;
    GameFusion::Scene newScene;
    newScene.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    newScene.name = sceneName.toStdString();

    newScene.notes = dialog.getSlugline().toStdString();
    newScene.description = dialog.getDescription().toStdString();
    newScene.tags = dialog.getTags().toStdString();

    // Create shots and panels
    double shotDuration = durationMs / shotCount;
    for (int i = 0; i < shotCount; ++i) {
        GameFusion::Shot shot;
        shot.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        shot.name = QString("Shot %1").arg(i + 1).toStdString();
        shot.startTime = i * shotDuration; // Relative to scene, adjusted later
        shot.endTime = (i + 1) * shotDuration;
        shot.frameCount = qRound(shotDuration * fps / 1000.0);

        GameFusion::Panel panel;
        panel.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        panel.name = "Panel 1";
        panel.startTime = 0.0; // Relative to shot
        panel.durationTime = shotDuration;
        GameFusion::Layer bg;
        bg.name = "BG";
        bg.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        GameFusion::Layer l1;
        l1.name = "Layer 1";
        l1.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        panel.layers.push_back(l1);
        panel.layers.push_back(bg);
        shot.panels.push_back(panel);

        shot.slugline = dialog.getSlugline().toStdString();
        shot.description = dialog.getDescription().toStdString();
        shot.tags = dialog.getTags().toStdString();
        shot.notes = dialog.getNotes().toStdString();

        newScene.shots.push_back(shot);
    }

    ShotContext shotContext = this->findShotForTime(currentTime);

    if(!shotContext.isValid()){
        return;
    }

    undoStack->push(new InsertSceneCommand(this, newScene, shotContext.scene->uuid.c_str(), after, currentTime, "New Scene"));
    GameFusion::Log().info() << "Created new scene " << newScene.uuid.c_str();
}


void MainWindow::onSplitScene() {
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    ShotContext currentCtx = findShotForTime(currentTime);
    if (!currentCtx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    // Ask user: split before or after
    QMessageBox::StandardButton choice = QMessageBox::question(
        this, "Split Scene",
        QString("Do you want to split the scene \"%1\" before or after shot \"%2\"?")
            .arg(QString::fromStdString(currentCtx.scene->name))
            .arg(QString::fromStdString(currentCtx.shot->name)),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes
    );

    if (choice == QMessageBox::Cancel)
        return;

    bool splitBefore = (choice == QMessageBox::Yes); // Yes = before, No = after

    // Propose name for new scene
    QString proposedName = QString::fromStdString(currentCtx.scene->name) +
                           (splitBefore ? "_A" : "_B");
    bool ok;
    QString newSceneName = QInputDialog::getText(
        this, "New Scene Name",
        "Enter name for new split scene:",
        QLineEdit::Normal, proposedName, &ok
    );
    if (!ok || newSceneName.isEmpty())
        return;

    // Push split command to undo stack
    undoStack->push(new SplitSceneCommand(
        this,
        QString::fromStdString(currentCtx.scene->uuid),
        QString::fromStdString(currentCtx.shot->uuid),
        splitBefore,
        newSceneName,
        *currentCtx.scene,
        currentTime
    ));
}

QString MainWindow::splitScene(const QString& sceneUuid, const QString& shotUuid,
                               bool splitBefore, const QString& newSceneName,
                               double cursorTime) {
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available\n";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return {};
    }

    // 1. Find scene + shot
    Scene* scene = findSceneByUuid(sceneUuid.toStdString());
    if (!scene) return {};

    auto it = std::find_if(scene->shots.begin(), scene->shots.end(),
        [&](const Shot& s){ return s.uuid == shotUuid.toStdString(); });
    if (it == scene->shots.end()) return {};

    // 2. Decide split point
    auto splitIt = splitBefore ? it : std::next(it);
    if (splitIt == scene->shots.end()) return {};

    // 3. Create new scene
    Scene newScene;
    newScene.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    newScene.name = newSceneName.toStdString();

    // 4. Move shots into new scene
    newScene.shots.insert(newScene.shots.end(), splitIt, scene->shots.end());
    scene->shots.erase(splitIt, scene->shots.end());

    // insert scene before or after current scene
    std::vector <GameFusion::Scene> &scenes = scriptBreakdown->getScenes();
    auto sceneIt = std::find_if(scenes.begin(), scenes.end(),
            [&](const GameFusion::Scene& s){ return s.uuid == sceneUuid.toStdString(); });

        if (sceneIt != scenes.end()) {
            //if (splitBefore)
            //    scenes.insert(sceneIt, newScene); // insert before
            //else
                scenes.insert(sceneIt + 1, newScene); // insert after
        }

    // 6. Insert new scene after current one
    // here we can simplify and simply create a new scene marker on the timeline
    CursorItem *sceneMarker = timeLineView->addSceneMarker(newScene.shots.front().startTime, newScene.name.c_str());
    sceneMarker->setUuid(newScene.uuid.c_str());

    TrackItem *storyboardTrack = timeLineView->getTrack(0);
    ShotSegment *shotSegment = (ShotSegment*)storyboardTrack->getSegmentByUuid(newScene.shots.front().uuid.c_str());
    shotSegment->setSceneMarker(sceneMarker);

    timeLineView->scene()->update(timeLineView->scene()->sceneRect());
    timeLineView->viewport()->update();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    updateScenes();
    timeLineView->setTimeCursor(cursorTime);

    return QString::fromStdString(newScene.uuid);
}

void MainWindow::mergeScenes(const QString& originalUuid,
                             const QString& newUuid,
                             bool splitBefore,
                             const GameFusion::Scene &originalScene,
                             double cursorTime) {
    // Restore by re-attaching shots from newUuid scene into originalUuid
    // and then delete the split scene.
    // (You can mirror logic from splitScene, but reversed)
    //deleteScene(newUuid, cursorTime);

    // we will simplify here
    GameFusion::Scene *scene = findSceneByUuid(originalUuid.toUtf8().constData());
    *scene = originalScene;
    scene->dirty = true;

    GameFusion::Scene *toDelete = findSceneByUuid(newUuid.toUtf8().constData());
    toDelete->markDeleted(true);


    TrackItem *track = timeLineView->getTrack(0);

    QList<Segment*> & segments = track->segments();
    for(auto segment : segments){
        ShotSegment *shotSegment = (ShotSegment*)segment;
        CursorItem *sceneMarker = shotSegment->getSceneMarkerPointer();
        if (sceneMarker && sceneMarker->getUuid() == newUuid){
            shotSegment->setSceneMarker(nullptr);
            break;
        }
    }
    timeLineView->deleteSceneMarker(newUuid);

    timeLineView->scene()->update(timeLineView->scene()->sceneRect());
    timeLineView->viewport()->update();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    updateScenes();
    timeLineView->setTimeCursor(cursorTime);
}

void MainWindow::onDeleteScene() {
    // Delete selected Scene

    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();

    ShotContext shotContext = findShotForTime(currentTime);
    if(!shotContext.isValid())
        return;

    std::vector<GameFusion::Scene> &scenes = scriptBreakdown->getScenes();

    GameFusion::Scene* prev = scriptBreakdown->getPreviousScene(*shotContext.scene);
    GameFusion::Scene* next = scriptBreakdown->getNextScene(*shotContext.scene);

    if(prev)
        undoStack->push(new DeleteSceneCommand(this, *shotContext.scene, prev->uuid.c_str(), true, currentTime, "Delete Scene"));
    else if(next)
        undoStack->push(new DeleteSceneCommand(this, *shotContext.scene, next->uuid.c_str(), false, currentTime, "Delete Scene"));
    else
        undoStack->push(new DeleteSceneCommand(this, *shotContext.scene, "", false, currentTime, "Delete Scene"));
}

void MainWindow::onRenameScene() {
    // Open dialog to rename selected Scene
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    ShotContext currentCtx = findShotForTime(currentTime);
    if (!currentCtx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    // display qt dialog box with old scene name and prompt for new scene
    // Show dialog with old scene name as default
    bool ok = false;
    QString oldName = currentCtx.scene->name.c_str();
    QString newName = QInputDialog::getText(
        this,
        tr("Rename Scene"),
        tr("Enter new name for the scene:"),
        QLineEdit::Normal,
        oldName,
        &ok
    );

    if (!ok || newName.trimmed().isEmpty()) {
        // User cancelled or entered nothing
        return;
    }

    undoStack->push(new RenameSceneCommand(this, currentCtx.scene->uuid.c_str(), oldName, newName, currentTime, "Rename Scene"));


}

void MainWindow::onDuplicateScene() {
    // Duplicate selected Scene
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available\n";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    ShotContext currentCtx = findShotForTime(currentTime);
    if (!currentCtx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    // Confirm duplication (optional, to avoid accidental duplicates)
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Duplicate Scene",
        "Duplicate the current scene after the original?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    // Create a deep copy of the scene
    GameFusion::Scene duplicatedScene = *currentCtx.scene;  // Shallow copy first (copies shots vector by value, but need deep copy for nested objects)

    // Generate new UUID for the duplicated scene
    duplicatedScene.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

    // Adjust name to indicate it's a copy (e.g., "Original" -> "Copy of Original")
    std::string originalName = duplicatedScene.name;
    if (originalName.empty()) {
        duplicatedScene.name = "Untitled Scene Copy";
    } else {
        duplicatedScene.name = "Copy of " + originalName;
    }

    // Deep copy shots, panels, and layers (generate new UUIDs to avoid conflicts)
    for (auto& shot : duplicatedScene.shots) {
        // New shot UUID
        shot.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

        // Deep copy panels and layers
        for (auto& panel : shot.panels) {
            std::string oldPanelUuid = panel.uuid;
            panel.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

            // Update camera frame UUIDs
            for (auto& camera : shot.cameraAnimation.frames) {
                // update camera panel uuid
                if(camera.panelUuid == oldPanelUuid){
                    camera.panelUuid = panel.uuid;
                    camera.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
                }
                camera.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            }

            // Layers are vectors of structs; copy preserves them, but regenerate UUIDs if needed
            for (auto& layer : panel.layers) {
                layer.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            }
        }
    }



    // Reset timings for duplicate (will be adjusted in insertScene to start after original)
    // Note: insertScene handles absolute positioning based on ref scene's end time
    duplicatedScene.dirty = true;  // Mark as dirty for saving

    // Insert after the original scene (common UX choice)
    bool insertAfter = true;
    undoStack->push(new DuplicateSceneCommand(this, duplicatedScene, currentCtx.scene->uuid.c_str(), insertAfter, currentTime, "Duplicate Scene"));

    GameFusion::Log().info() << "Duplicated scene " << originalName.c_str() << " to new UUID: " << duplicatedScene.uuid.c_str() << "\n";
}

void MainWindow::onNewShot()
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    // Get current time and shot context
    double currentTime = timeLineView->getCursorTime();
    ShotContext currentCtx = findShotForTime(currentTime);
    if (!currentCtx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    // Show dialog to get user input
    QString shotName = "SHOT_" + QString::number(currentCtx.scene->shots.size() * 10);
    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    NewShotDialog dialog(fps, shotName, this);
    if (dialog.exec() != QDialog::Accepted) {
        return; // User canceled
    }

    double durationMs = dialog.getDurationMs();
    bool insertBefore = dialog.isInsertBefore();
    shotName = dialog.getShotName();
    int panelCount = dialog.getPanelCount();

    // Create new shot
    GameFusion::Shot newShot;
    newShot.name = shotName.toStdString();
    newShot.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    newShot.frameCount = qRound(durationMs * fps / 1000.0);
    newShot.startTime = insertBefore ? currentCtx.shot->startTime : currentCtx.shot->endTime;
    newShot.endTime = newShot.startTime + durationMs;

    // Create panels
    double panelDuration = durationMs / panelCount; // Evenly distribute duration
    for (int i = 0; i < panelCount; ++i) {
        GameFusion::Panel panel;
        panel.name = QString("%1_PANEL_%2").arg(shotName).arg(i + 1, 3, 10, QChar('0')).toStdString();
        //panel.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        panel.startTime = i * panelDuration;
        panel.durationTime = panelDuration;
        GameFusion::Layer bg;
        bg.name = "BG";
        GameFusion::Layer l1;
        l1.name = "Layer 1";
        panel.layers.push_back(l1);
        panel.layers.push_back(bg);
        newShot.panels.push_back(panel);
    }

    size_t shotIndex = -1;
    int segmentIndex = -1;
    TrackItem* track = timeLineView->getTrack(0);

    ShotContext prevCtx;

    // Determine shot index
    if (!insertBefore) {
        if(currentCtx.isValid()){
            auto& shots = currentCtx.scene->shots;
            auto it = std::find_if(shots.begin(), shots.end(),
                           [&](const GameFusion::Shot& s) { return s.uuid == currentCtx.shot->uuid; });
            shotIndex = (it == shots.end()) ? shots.size() : (it - shots.begin())+1;
            segmentIndex = track->getSegmentIndexByUuid(QString::fromStdString(currentCtx.shot->uuid))+1;
        }
        else{
            shotIndex = 0;
            segmentIndex = 0;
        }
    }
    else {
        if(currentCtx.isValid()){
            auto& shots = currentCtx.scene->shots;
            auto it = std::find_if(shots.begin(), shots.end(),
                                   [&](const GameFusion::Shot& s) { return s.uuid == currentCtx.shot->uuid; });
            shotIndex = (it == shots.end()) ? shots.size() : (it - shots.begin());
            segmentIndex = track->getSegmentIndexByUuid(QString::fromStdString(currentCtx.shot->uuid));
        }
        else{
            shotIndex = 0;
            segmentIndex = 0;
        }
    }

    // Create ShotIndices
    ShotIndices shotIndices;
    shotIndices.shotIndex = static_cast<int>(shotIndex);
    shotIndices.segmentIndex = segmentIndex;

    // Push to undo stack
    if (!insertBefore)
        undoStack->push(new InsertSegmentCommand(this, shotIndices, newShot, *currentCtx.scene, QString::fromStdString(currentCtx.scene->uuid), currentTime));
    else
        undoStack->push(new InsertSegmentCommand(this, shotIndices, newShot, *currentCtx.scene, QString::fromStdString(currentCtx.scene->uuid), currentTime));

    // UI updates handled by InsertSegmentCommand::redo()
}

void MainWindow::onDeleteShot() {
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    ShotContext currentCtx = findShotByUuid(timeLineView->getTrack(0)->findSegmentForTime(currentTime)->getUuid().toStdString());
    if (!currentCtx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    QString sceneUuid = QString::fromStdString(currentCtx.scene->uuid);
    GameFusion::Shot shotCopy = *currentCtx.shot; // Store shot data for undo

    // Push to undo stack
    undoStack->push(new DeleteSegmentCommand(this, shotCopy, sceneUuid, currentTime));

    // Refresh UI is done in DeleteSegmentCommand->redo()

}

void MainWindow::onRenameShot() {
    // Open dialog to rename selected Shot

    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    ShotContext currentCtx = findShotForTime(currentTime);
    if (!currentCtx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Shot"),
                                            tr("Enter new shot name:"),
                                            QLineEdit::Normal,
                                            QString::fromStdString(currentCtx.shot->name),
                                            &ok);
    if (!ok || newName.isEmpty()) {
        return;
    }

    // Validate shot name
    QRegularExpression validNameRegex("[^a-zA-Z0-9_\\-]");
    if (newName.contains(validNameRegex)) {
        QMessageBox::warning(this, "Error", "Shot name contains invalid characters. Use letters, numbers, underscores, or hyphens.");
        return;
    }

    // Push rename command to undo stack
    undoStack->push(new RenameShotCommand(this, QString::fromStdString(currentCtx.shot->uuid),
                                          QString::fromStdString(currentCtx.scene->uuid),
                                          QString::fromStdString(currentCtx.shot->name),
                                          newName));

}

void MainWindow::onDuplicateShot() {

    // Duplicate selected Shot
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    ShotContext currentCtx = findShotForTime(currentTime);
    if (!currentCtx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    // Create duplicated shot
    GameFusion::Shot dupShot = *currentCtx.shot;
    dupShot.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    dupShot.name = QString("%1_copy").arg(QString::fromStdString(currentCtx.shot->name)).toStdString();
    dupShot.startTime = currentCtx.shot->endTime;
    dupShot.endTime = dupShot.startTime + (currentCtx.shot->endTime - currentCtx.shot->startTime);


    // Update panel UUIDs to ensure uniqueness
    for (auto& panel : dupShot.panels) {
        std::string oldPanelUuid = panel.uuid;
        panel.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

        // Update camera frame UUIDs
        for (auto& camera : dupShot.cameraAnimation.frames) {
            // update camera panel uuid
            if(camera.panelUuid == oldPanelUuid){
                camera.panelUuid = panel.uuid;
                camera.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            }
            camera.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        }

        for (auto& layer : panel.layers) {

            layer.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        }
    }

    // Determine insertion point (immediately after the original shot)
    TrackItem* track = timeLineView->getTrack(0);
    auto& shots = currentCtx.scene->shots;
    auto it = std::find_if(shots.begin(), shots.end(),
                           [&](const GameFusion::Shot& s) { return s.uuid == currentCtx.shot->uuid; });
    int shotIndex = (it == shots.end()) ? shots.size() : (it - shots.begin()) + 1;
    int segmentIndex = track->getSegmentIndexByUuid(QString::fromStdString(currentCtx.shot->uuid)) + 1;

    ShotIndices shotIndices;
    shotIndices.shotIndex = shotIndex;
    shotIndices.segmentIndex = segmentIndex;

    // Push duplicate command to undo stack
    undoStack->push(new DuplicateShotCommand(this, dupShot, shotIndices, *currentCtx.scene, currentTime));
}

void MainWindow::onEditShot()
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    ShotContext currentCtx = findShotForTime(currentTime);
    if (!currentCtx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    NewShotDialog dialog(fps, NewShotDialog::Mode::Edit,
                         QString::fromStdString(currentCtx.shot->name),
                         currentCtx.shot->endTime - currentCtx.shot->startTime,
                         currentCtx.shot->panels.size(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString newName = dialog.getShotName();
    double newDurationMs = dialog.getDurationMs();
    int newPanelCount = dialog.getPanelCount();
    QString sceneUuid = QString::fromStdString(currentCtx.scene->uuid);

    //----------------------

    // Store old shot
    GameFusion::Shot oldShot = *currentCtx.shot;

    // Create new shot with updated parameters
    GameFusion::Shot newShot = oldShot;
    newShot.name = newName.toStdString();
    newShot.frameCount = qRound(newDurationMs * fps / 1000.0);
    newShot.endTime = newShot.startTime + newDurationMs;

    // Adjust panels
    int currentPanelCount = oldShot.panels.size();
    newShot.panels.clear();
    double panelDuration = newDurationMs / newPanelCount;
    for (int i = 0; i < newPanelCount; ++i) {
        GameFusion::Panel panel;
        if (i < currentPanelCount) {
            panel = oldShot.panels[i]; // Preserve existing panel
            panel.durationTime = panelDuration;
            panel.startTime = i * panelDuration;
        } else {
            panel.name = QString("%1_PANEL_%2").arg(newName).arg(i + 1, 3, 10, QChar('0')).toStdString();
            panel.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            panel.startTime = i * panelDuration;
            panel.durationTime = panelDuration;
            GameFusion::Layer bg;
            bg.name = "BG";
            GameFusion::Layer l1;
            l1.name = "Layer 1";
            panel.layers.push_back(l1);
            panel.layers.push_back(bg);
        }
        newShot.panels.push_back(panel);
    }

    //----------------------

    undoStack->push(new EditShotCommand(this, oldShot, newShot, currentTime));
}

void MainWindow::editShotSegment(const GameFusion::Shot &editShot, double cursorTime, bool updateTime){
    ShotContext shotContext = findShotByUuid(editShot.uuid);
    if(!shotContext.isValid())
        return;

    ShotIndices shotIndecies = deleteShotSegment(shotContext, cursorTime, updateTime);
    CursorItem *sceneMarker = nullptr;
    insertShotSegment(editShot, shotIndecies, *shotContext.scene, cursorTime, sceneMarker);
}

void MainWindow::renameShotSegment(const QString &shotUuid, QString newName){

    ShotContext shotContext = findShotByUuid(shotUuid.toStdString());
    if(!shotContext.isValid())
        return;


    shotContext.shot->name = newName.toStdString();
    shotContext.scene->setDirty(true);

    TrackItem* track = timeLineView->getTrack(0);
    ShotSegment* segment = (ShotSegment*) track->getSegmentByUuid(shotUuid);
    if (segment) {
        segment->setName(newName);
        GameFusion::Log().info() << "Renamed shot to " << newName.toUtf8().constData() << "\n";
    } else {
        GameFusion::Log().warning() << "Shot segment not found for UUID " << shotUuid.toUtf8().constData() << "\n";
    }

}

void MainWindow::onNewPanel()
{
    if (!scriptBreakdown) {
            GameFusion::Log().error() << "No ScriptBreakdown available";
            QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
            return;
        }

        double currentTime = timeLineView->getCursorTime();
        PanelContext ctx = findPanelForTime(currentTime);
        if (!ctx.isValid()) {
            QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
            return;
        }

        qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
        NewPanelDialog dialog(fps, NewPanelDialog::Mode::Create, "", 1000.0, this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        GameFusion::Panel newPanel;
        newPanel.name = dialog.getPanelName().toStdString();
        newPanel.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        newPanel.durationTime = dialog.getDurationMs();
        GameFusion::Layer bg;
        bg.name = "BG";
        bg.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        GameFusion::Layer l1;
        l1.name = "Layer 1";
        l1.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        newPanel.layers.push_back(l1);
        newPanel.layers.push_back(bg);

        GameFusion::Shot originalShot = *ctx.shot;
        GameFusion::Shot updatedShot = originalShot;
        NewPanelDialog::InsertMode insertMode = dialog.getInsertMode();

        int panelIndex = findPanelIndex(ctx);

        double shotDuration = originalShot.endTime - originalShot.startTime;
        if (insertMode == NewPanelDialog::InsertMode::SplitAtCursor && panelIndex >= 0) {
            double relativeTime = currentTime - originalShot.startTime;
            auto& panel = originalShot.panels[panelIndex];
            double splitTime = relativeTime - panel.startTime;
            if (splitTime > 0 && splitTime < panel.durationTime) {
                GameFusion::Panel secondHalf = panel;
                secondHalf.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
                secondHalf.startTime = panel.startTime + splitTime;
                secondHalf.durationTime = panel.durationTime - splitTime;
                panel.durationTime = splitTime;
                newPanel.startTime = panel.startTime + splitTime;
                updatedShot.panels[panelIndex] = panel;
                updatedShot.panels.insert(updatedShot.panels.begin() + panelIndex + 1, newPanel);
                updatedShot.panels.insert(updatedShot.panels.begin() + panelIndex + 2, secondHalf);
                for (size_t i = panelIndex + 3; i < updatedShot.panels.size(); ++i) {
                    updatedShot.panels[i].startTime += newPanel.durationTime;
                }
            } else {
                newPanel.startTime = panel.startTime + panel.durationTime;
                updatedShot.panels.insert(updatedShot.panels.begin() + panelIndex + 1, newPanel);
                for (size_t i = panelIndex + 2; i < updatedShot.panels.size(); ++i) {
                    updatedShot.panels[i].startTime += newPanel.durationTime;
                }
            }
        } else if (insertMode == NewPanelDialog::InsertMode::BeforeCurrent && panelIndex >= 0) {
            newPanel.startTime = originalShot.panels[panelIndex].startTime;
            updatedShot.panels.insert(updatedShot.panels.begin() + panelIndex, newPanel);
            for (size_t i = panelIndex + 1; i < updatedShot.panels.size(); ++i) {
                updatedShot.panels[i].startTime += newPanel.durationTime;
            }
        } else {
            // After Current Panel or default (append)
            newPanel.startTime = originalShot.panels.empty() ? 0.0 : originalShot.panels.back().startTime + originalShot.panels.back().durationTime;
            updatedShot.panels.push_back(newPanel);
        }

        updatedShot.endTime = updatedShot.startTime + (updatedShot.panels.empty() ? 0.0 :
            updatedShot.panels.back().startTime + updatedShot.panels.back().durationTime);
        updatedShot.frameCount = qRound((updatedShot.endTime - updatedShot.startTime) * fps / 1000.0);

        undoStack->push(new PanelCommand(this, originalShot, updatedShot, currentTime, "New Panel"));
}

void MainWindow::onInsertPanel() {
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    PanelContext ctx = findPanelForTime(currentTime);
    if (!ctx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    GameFusion::Panel newPanel;
    newPanel.name = "Panel " + std::to_string(ctx.shot->panels.size() + 1);
    newPanel.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    newPanel.durationTime = 1000.0; // Default 1 second
    GameFusion::Layer bg;
    bg.name = "BG";
    bg.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    GameFusion::Layer l1;
    l1.name = "Layer 1";
    l1.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    newPanel.layers.push_back(l1);
    newPanel.layers.push_back(bg);

    GameFusion::Shot originalShot = *ctx.shot;
    GameFusion::Shot updatedShot = originalShot;
    int panelIndex = findPanelIndex(ctx);

    if (panelIndex >= 0) {
        double relativeTime = currentTime - originalShot.startTime;
        auto& panel = originalShot.panels[panelIndex];
        double splitTime = relativeTime - panel.startTime;
        if (splitTime > 0 && splitTime < panel.durationTime) {
            // Split current panel
            GameFusion::Panel firstHalf = panel;
            firstHalf.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            firstHalf.startTime = panel.startTime + splitTime;
            firstHalf.durationTime = panel.durationTime - splitTime;

            int newPaneDuration = panel.durationTime - splitTime;
            panel.durationTime = splitTime;

            newPanel.startTime = panel.startTime + splitTime;
            newPanel.durationTime = newPaneDuration;
            updatedShot.panels[panelIndex] = panel;
            updatedShot.panels.insert(updatedShot.panels.begin() + panelIndex + 1, newPanel);


            for (size_t i = panelIndex + 3; i < updatedShot.panels.size(); ++i) {
                updatedShot.panels[i].startTime += newPanel.durationTime;
            }
        } else {
            // Insert after current panel
            newPanel.startTime = panel.startTime + panel.durationTime;
            updatedShot.panels.insert(updatedShot.panels.begin() + panelIndex + 1, newPanel);
            for (size_t i = panelIndex + 2; i < updatedShot.panels.size(); ++i) {
                updatedShot.panels[i].startTime += newPanel.durationTime;
            }
        }
    } else {
        // Append to shot
        newPanel.startTime = originalShot.panels.empty() ? 0.0 : originalShot.panels.back().startTime + originalShot.panels.back().durationTime;
        updatedShot.panels.push_back(newPanel);
    }

    updatedShot.endTime = updatedShot.startTime + (updatedShot.panels.empty() ? 0.0 :
        updatedShot.panels.back().startTime + updatedShot.panels.back().durationTime);
    updatedShot.frameCount = qRound((updatedShot.endTime - updatedShot.startTime) * fps / 1000.0);

    undoStack->push(new PanelCommand(this, originalShot, updatedShot, currentTime, "Insert Panel"));
}

void MainWindow::onEditPanel()
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    PanelContext ctx = this->findPanelForTime(currentTime);
    if (!ctx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    int panelIndex = findPanelIndex(ctx);
    if (panelIndex < 0) {
        QMessageBox::warning(this, "Error", "No valid panel selected. Please select a time within a panel.");
        return;
    }

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    const auto& panel = ctx.shot->panels[panelIndex];
    NewPanelDialog dialog(fps, NewPanelDialog::Mode::Edit, QString::fromStdString(panel.name),
                         panel.durationTime, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    GameFusion::Shot originalShot = *ctx.shot;
    GameFusion::Shot updatedShot = originalShot;
    updatedShot.panels[panelIndex].name = dialog.getPanelName().toStdString();
    double oldDuration = updatedShot.panels[panelIndex].durationTime;
    updatedShot.panels[panelIndex].durationTime = dialog.getDurationMs();
    double durationDelta = dialog.getDurationMs() - oldDuration;
    for (size_t i = panelIndex + 1; i < updatedShot.panels.size(); ++i) {
        updatedShot.panels[i].startTime += durationDelta;
    }
    updatedShot.endTime = updatedShot.startTime + (updatedShot.panels.empty() ? 0.0 :
        updatedShot.panels.back().startTime + updatedShot.panels.back().durationTime);
    updatedShot.frameCount = qRound((updatedShot.endTime - updatedShot.startTime) * fps / 1000.0);

    undoStack->push(new PanelCommand(this, originalShot, updatedShot, currentTime, "Edit Panel"));
}

void MainWindow::onRenamePanel()
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    PanelContext ctx = this->findPanelForTime(currentTime);
    if (!ctx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    int panelIndex = findPanelIndex(ctx);
    if (panelIndex < 0) {
        QMessageBox::warning(this, "Error", "No valid panel selected. Please select a time within a panel.");
        return;
    }

    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Panel"), tr("Enter new panel name:"),
                                            QLineEdit::Normal, QString::fromStdString(ctx.shot->panels[panelIndex].name), &ok);
    if (!ok || newName.isEmpty()) {
        return;
    }

    QRegularExpression validNameRegex("[^a-zA-Z0-9_\\-]");
    if (newName.contains(validNameRegex)) {
        QMessageBox::warning(this, "Error", "Panel name contains invalid characters. Use letters, numbers, underscores, or hyphens.");
        return;
    }

    GameFusion::Shot originalShot = *ctx.shot;
    GameFusion::Shot updatedShot = originalShot;
    updatedShot.panels[panelIndex].name = newName.toStdString();

    undoStack->push(new PanelCommand(this, originalShot, updatedShot, currentTime, "Rename Panel"));
}

void MainWindow::onDuplicatePanel()
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    PanelContext ctx = this->findPanelForTime(currentTime);
    if (!ctx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    int panelIndex = findPanelIndex(ctx);
    if (panelIndex < 0) {
        QMessageBox::warning(this, "Error", "No valid panel selected. Please select a time within a panel.");
        return;
    }

    GameFusion::Shot originalShot = *ctx.shot;
    GameFusion::Shot updatedShot = originalShot;
    GameFusion::Panel dupPanel = originalShot.panels[panelIndex];
    dupPanel.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    dupPanel.name = QString("%1_copy").arg(QString::fromStdString(dupPanel.name)).toStdString();
    for (auto& layer : dupPanel.layers) {
        layer.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }
    dupPanel.startTime = originalShot.panels[panelIndex].startTime + originalShot.panels[panelIndex].durationTime;
    updatedShot.panels.insert(updatedShot.panels.begin() + panelIndex + 1, dupPanel);
    for (size_t i = panelIndex + 2; i < updatedShot.panels.size(); ++i) {
        updatedShot.panels[i].startTime += dupPanel.durationTime;
    }
    updatedShot.endTime = updatedShot.startTime + (updatedShot.panels.empty() ? 0.0 :
        updatedShot.panels.back().startTime + updatedShot.panels.back().durationTime);
    updatedShot.frameCount = qRound((updatedShot.endTime - updatedShot.startTime) * ProjectContext::instance().projectJson()["fps"].toDouble(24.0) / 1000.0);

    undoStack->push(new PanelCommand(this, originalShot, updatedShot, currentTime, "Duplicate Panel"));
}

void MainWindow::onCopyPanel()
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    PanelContext ctx = findPanelForTime(currentTime);
    if (!ctx.isValid() || ctx.shot->panels.empty()) {
        QMessageBox::warning(this, "Error", "No valid panel selected. Please select a time within a panel.");
        return;
    }

    int panelIndex = findPanelIndex(ctx);
    if (panelIndex < 0) {
        QMessageBox::warning(this, "Error", "No valid panel selected. Please select a time within a panel.");
        return;
    }

    clipboardPanel = ctx.shot->panels[panelIndex];
    hasClipboardPanel = true;
    GameFusion::Log().info() << "Copied panel " << clipboardPanel.uuid.c_str();
}

void MainWindow::onCutPanel()
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    PanelContext ctx = findPanelForTime(currentTime);
    if (!ctx.isValid() || ctx.shot->panels.empty()) {
        QMessageBox::warning(this, "Error", "No valid panel selected. Please select a time within a panel.");
        return;
    }

    int panelIndex = findPanelIndex(ctx);
    if (panelIndex < 0 || ctx.shot->panels.size() <= 1) {
        QMessageBox::warning(this, "Error", ctx.shot->panels.size() <= 1 ?
            "Cannot cut the last panel in the shot." : "No valid panel selected. Please select a time within a panel.");
        return;
    }

    clipboardPanel = ctx.shot->panels[panelIndex];
    hasClipboardPanel = true;

    GameFusion::Shot originalShot = *ctx.shot;
    GameFusion::Shot updatedShot = originalShot;
    double deletedDuration = updatedShot.panels[panelIndex].durationTime;
    updatedShot.panels.erase(updatedShot.panels.begin() + panelIndex);
    for (size_t i = panelIndex; i < updatedShot.panels.size(); ++i) {
        updatedShot.panels[i].startTime -= deletedDuration;
    }
    updatedShot.endTime = updatedShot.startTime + (updatedShot.panels.empty() ? 0.0 :
        updatedShot.panels.back().startTime + updatedShot.panels.back().durationTime);
    updatedShot.frameCount = qRound((updatedShot.endTime - updatedShot.startTime) * ProjectContext::instance().projectJson()["fps"].toDouble(24.0) / 1000.0);

    undoStack->push(new PanelCommand(this, originalShot, updatedShot, currentTime, "Cut Panel"));
    GameFusion::Log().info() << "Cut panel " << clipboardPanel.uuid.c_str();
}

void MainWindow::onPastePanel()
{
    if (!scriptBreakdown || !hasClipboardPanel) {
        GameFusion::Log().error() << "No ScriptBreakdown or clipboard panel available";
        QMessageBox::warning(this, "Error", !scriptBreakdown ? "No ScriptBreakdown available." :
            "No panel in clipboard. Copy or cut a panel first.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    PanelContext ctx = findPanelForTime(currentTime);
    if (!ctx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    GameFusion::Shot originalShot = *ctx.shot;
    GameFusion::Shot updatedShot = originalShot;
    GameFusion::Panel pastePanel = clipboardPanel;


    pastePanel.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

    for (auto& layer : pastePanel.layers) {
            layer.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        }

    for (auto& cameraFrame : updatedShot.cameraAnimation.frames) {
        if(cameraFrame.panelUuid == ctx.panel->uuid){
            //cameraFrame.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            cameraFrame.panelUuid = pastePanel.uuid;
        }
    }

    int panelIndex = findPanelIndex(ctx);
    bool pastOverwrite = true;

    if(pastOverwrite){
        // Overwrite the selected panel
            double oldDuration = updatedShot.panels[panelIndex].durationTime;
            pastePanel.startTime = updatedShot.panels[panelIndex].startTime;
            updatedShot.panels[panelIndex] = pastePanel;
            double durationDelta = pastePanel.durationTime - oldDuration;
            for (size_t i = panelIndex + 1; i < updatedShot.panels.size(); ++i) {
                updatedShot.panels[i].startTime += durationDelta;
            }
    }
    else{
        if (panelIndex >= 0) {
            pastePanel.startTime = originalShot.panels[panelIndex].startTime + originalShot.panels[panelIndex].durationTime;
            updatedShot.panels.insert(updatedShot.panels.begin() + panelIndex + 1, pastePanel);
            for (size_t i = panelIndex + 2; i < updatedShot.panels.size(); ++i)
                updatedShot.panels[i].startTime += pastePanel.durationTime;
        }
        else {
            pastePanel.startTime = originalShot.panels.empty() ? 0.0 : originalShot.panels.back().startTime + originalShot.panels.back().durationTime;
            updatedShot.panels.push_back(pastePanel);
        }
    }

    updatedShot.endTime = updatedShot.startTime + (updatedShot.panels.empty() ? 0.0 :
        updatedShot.panels.back().startTime + updatedShot.panels.back().durationTime);
    updatedShot.frameCount = qRound((updatedShot.endTime - updatedShot.startTime) * ProjectContext::instance().projectJson()["fps"].toDouble(24.0) / 1000.0);

    undoStack->push(new PanelCommand(this, originalShot, updatedShot, currentTime, "Paste Panel"));
    GameFusion::Log().info() << "Pasted panel " << pastePanel.uuid.c_str();
}

void MainWindow::onClearPanel()
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    PanelContext ctx = this->findPanelForTime(currentTime);
    if (!ctx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    int panelIndex = findPanelIndex(ctx);
    if (panelIndex < 0) {
        QMessageBox::warning(this, "Error", "No valid panel selected. Please select a time within a panel.");
        return;
    }

    GameFusion::Shot originalShot = *ctx.shot;
    GameFusion::Shot updatedShot = originalShot;
    updatedShot.panels[panelIndex].layers.clear();
    GameFusion::Layer bg;
    bg.name = "BG";
    bg.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    GameFusion::Layer l1;
    l1.name = "Layer 1";
    l1.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    updatedShot.panels[panelIndex].layers.push_back(l1);
    updatedShot.panels[panelIndex].layers.push_back(bg);

    undoStack->push(new PanelCommand(this, originalShot, updatedShot, currentTime, "Clear Panel"));
    GameFusion::Log().info() << "Cleared panel " << originalShot.panels[panelIndex].uuid.c_str();
}

void MainWindow::onDeletePanel()
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    PanelContext ctx = this->findPanelForTime(currentTime);
    if (!ctx.isValid()) {
        QMessageBox::warning(this, "Error", "No valid shot selected. Please select a time in the timeline.");
        return;
    }

    int panelIndex = findPanelIndex(ctx);
    if (panelIndex < 0) {
        QMessageBox::warning(this, "Error", "No valid panel selected. Please select a time within a panel.");
        return;
    }

    GameFusion::Shot originalShot = *ctx.shot;
    GameFusion::Shot updatedShot = originalShot;
    double deletedDuration = updatedShot.panels[panelIndex].durationTime;
    updatedShot.panels.erase(updatedShot.panels.begin() + panelIndex);
    for (size_t i = panelIndex; i < updatedShot.panels.size(); ++i) {
        updatedShot.panels[i].startTime -= deletedDuration;
    }
    updatedShot.endTime = updatedShot.startTime + (updatedShot.panels.empty() ? 0.0 :
        updatedShot.panels.back().startTime + updatedShot.panels.back().durationTime);
    updatedShot.frameCount = qRound((updatedShot.endTime - updatedShot.startTime) * ProjectContext::instance().projectJson()["fps"].toDouble(24.0) / 1000.0);

    undoStack->push(new PanelCommand(this, originalShot, updatedShot, currentTime, "Delete Panel"));
    GameFusion::Log().info() << "Deleted panel at index " << panelIndex;
}

void MainWindow::onCopyCamera(){
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    CameraContext ctx = findCameraForTime(currentTime);


    if(!ctx.isValid())
        return;

    clipboardCamera = *ctx.camera;
    hasClipboardCamera = true;
}

void MainWindow::onPastCamera(){

    if(!hasClipboardCamera)
        return;

    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        QMessageBox::warning(this, "Error", "No ScriptBreakdown available.");
        return;
    }

    double currentTime = timeLineView->getCursorTime();
    PanelContext panelContext = findPanelForTime(currentTime);
    if(!panelContext.isValid())
        return;

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble();
    qreal mspf = 1000.0f / fps;
    GameFusion::CameraFrame newCamera(clipboardCamera);
    newCamera.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    qreal frame = (currentTime - panelContext.shot->startTime) / mspf;
    newCamera.frameOffset = static_cast<int>(qRound(frame));
    newCamera.time = newCamera.frameOffset * mspf; // may not actually be used anywhere

    undoStack->push(new AddCameraCommand(this, newCamera, currentTime, "Past Camera"));
}

void MainWindow::editScene(GameFusion::Scene& oldScene, GameFusion::Scene& newScene)
{
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        return;
    }

    // Update ScriptBreakdown
    //GameFusion::Scene *ctx = findSceneByUuid(oldScene.uuid);



    // Update timeline

    int segmentIndex = -1;

    double currentTime = timeLineView->getCursorTime();
    for (const auto& shot : oldScene.shots) {
        ShotContext shotContext = findShotByUuid(shot.uuid);

        ShotIndices indices = deleteShotSegment(shotContext, shot.startTime);

        //determin segment insertion point
        if(segmentIndex == -1)
            segmentIndex = indices.segmentIndex;
        //ShotIndices indices = { /* compute indices as needed */ };
        //insertShotSegment(shot, indices, scene, currentTime);
    }

    scriptBreakdown->setScene(newScene, newScene.uuid);

    for (const auto& shot : newScene.shots) {
        ShotContext shotContext = findShotByUuid(shot.uuid);

        ShotIndices indices = {shotContext.shotIndex, segmentIndex};
        CursorItem *sceneMarker = nullptr;
        insertShotSegment(shot, indices, newScene, shot.startTime, sceneMarker);

        segmentIndex ++;
    }

}

void MainWindow::deleteScene(const std::string &uuid, double cursorTime) {
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        return;
    }

    GameFusion::Scene *toDelete = this->findSceneByUuid(uuid);

    GameFusion::Scene sceneCopy = *toDelete;


    // delete scene shots

    for(auto shot: sceneCopy.shots){
        ShotContext shotContext = this->findShotByUuid(shot.uuid);
        if(!shotContext.isValid()){
            continue;
        }
        deleteShotSegment(shotContext, cursorTime);
    }

    // delete scene marker

    if(timeLineView->deleteSceneMarker(uuid.c_str()))
    {
        Log().info() << "Deleted scene marker: " << uuid.c_str() << "\n";
    } else {
        Log().info() << "Marker not found for scene " << uuid.c_str() << "\n";
    }

    toDelete->markDeleted(true);

    // Force full scene and viewport update
    timeLineView->scene()->update(timeLineView->scene()->sceneRect());
    timeLineView->viewport()->update();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    timeLineView->update();
}

void MainWindow::renameScene(QString uuid, QString oldName, QString newName, double cursorTime) {
    // Update scene object
    GameFusion::Scene *scene = findSceneByUuid(uuid.toStdString());
    if(!scene)
        return;

    CursorItem *sceneMarker = timeLineView->getSceneMarkerFromUuid(scene->uuid.c_str());
    if(!sceneMarker){
        GameFusion::Log().error() << "Scene not found in timeline";
        return;
    }

    scene->name = newName.toStdString();
    scene->dirty = true;
    sceneMarker->setLabel(newName);
    //sceneMarker->update();

    timeLineView->scene()->update();  // Invalidate entire scene rect
    timeLineView->viewport()->update();  // Force view repaint
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);  // Flush without blocking UI

    timeLineView->update();
}

void MainWindow::insertScene(GameFusion::Scene &newSceneInput, QString sceneRefUuid, bool insertAfter, double cursorTime){
    if (!scriptBreakdown) {
        GameFusion::Log().error() << "No ScriptBreakdown available";
        return;
    }

    GameFusion::Scene &newScene = newSceneInput;

    GameFusion::Scene *sceneRef = this->findSceneByUuid(sceneRefUuid.toStdString());
    int sceneRefIndex = findSceneIndex(sceneRefUuid.toStdString());
    // Get reference to the inserted scene
    GameFusion::Shot *firstShot=&sceneRef->shots.front();
    GameFusion::Shot *lastShot=&sceneRef->shots.back();

    TrackItem* track = timeLineView->getTrack(0);

    ShotIndices shotIndices;
    long timeStart = 0;

    if(insertAfter){
        timeStart = lastShot->endTime;
        ShotContext shotContext = findShotByUuid(lastShot->uuid);
        if(!shotContext.isValid())
            return;

        shotIndices.shotIndex = 0;
        shotIndices.segmentIndex = track->getSegmentIndexByUuid(lastShot->uuid.c_str())+1;
    }else{
        timeStart = firstShot->startTime;

        ShotContext shotContext = findShotByUuid(lastShot->uuid);
        if(!shotContext.isValid())
            return;
        shotIndices.shotIndex = 0;
        shotIndices.segmentIndex = track->getSegmentIndexByUuid(lastShot->uuid.c_str())-1;
    }

    if(!shotIndices.isValid())
    {
        return;
    }

    GameFusion::Scene emptyScene = newScene;
    emptyScene.shots.clear();
    emptyScene.dirty = true;

    std::vector<GameFusion::Scene> &scenes = scriptBreakdown->getScenes();
    scenes.insert(scenes.begin() + sceneRefIndex+1, emptyScene);

    //GameFusion::Scene insertScene = newScene;


    // Add a scene marker
    CursorItem *sceneMarker = timeLineView->addSceneMarker(timeStart, emptyScene.name.c_str());
    sceneMarker->setUuid(emptyScene.uuid.c_str());
    //segment->setSceneMarker(sceneMarker);

    for(auto &shot: newScene.shots){


        long shotDuration = shot.endTime - shot.startTime;
        shot.startTime = timeStart;
        shot.endTime = timeStart + shotDuration;
        timeStart += shotDuration;

        insertShotSegment(shot, shotIndices, emptyScene, shot.startTime, sceneMarker);

        sceneMarker = nullptr;

        shotIndices.shotIndex ++;
        shotIndices.segmentIndex ++;
    }

    timeLineView->update();
}

void MainWindow::onKeyframeAdded(const QString& attribute, double timeMs, const QVariant& value, const QString& kfUuid, const QString& shotUuid) {

    PanelContext panelCtx = findPanelForTime(timeMs);
    if(!panelCtx.isValid())
        return;

    GameFusion::Layer *layer = nullptr;
    for(auto &L: panelCtx.panel->layers) {
        if(L.name == selectedLayerName.toStdString())
        {
            layer = &L;
            break;
        }
    }

    if(!layer) {
        GameFusion::Log().info() << "Error unable to find layer "<<attribute.toUtf8().constData()<<" in panel "<<panelCtx.panel->name.c_str() << " "<<panelCtx.panel->uuid.c_str()<<"\n";
        return;
    }

    qreal panelStartMs = panelCtx.shot->startTime + panelCtx.panel->startTime;
    qreal relativeMs = timeMs - panelStartMs;
    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    qreal keyTime = qRound((relativeMs / 1000.0) * fps);

    QVariantMap valueMap = value.toMap();

    valueMap["sceneName"] = panelCtx.scene->name.c_str();
    valueMap["sceneUuid"] = panelCtx.scene->uuid.c_str();
    valueMap["shotName"] = panelCtx.shot->name.c_str();
    valueMap["shotUuid"] = panelCtx.shot->uuid.c_str();
    valueMap["panelName"] = panelCtx.panel->name.c_str();
    valueMap["panelUuid"] = panelCtx.panel->uuid.c_str();
    valueMap["layerName"] = layer->name.c_str();
    valueMap["layerUuid"] = layer->uuid.c_str();

    double cursorTime = timeLineView->getCursorTime();

    // Push undo command
    undoStack->push(new KeyframeAddCommand(kfUuid, keyTime, valueMap, layer->uuid.c_str(), panelCtx.panel->uuid.c_str(), panelCtx.shot->uuid.c_str(), this, cursorTime));
}

void MainWindow::onKeyframeDeleted(const QString& kfUuid) {
    for (auto& scene : scriptBreakdown->getScenes()) {
        bool changed = false;
        for (auto& shot : scene.shots) {
            for (auto& panel : shot.panels) {
                for (auto& layer : panel.layers) {
                    auto motIt = std::remove_if(layer.motionKeyframes.begin(), layer.motionKeyframes.end(), [&](const Layer::MotionKeyFrame& kf) {
                        return kf.uuid == kfUuid.toStdString();
                    });
                    if (motIt != layer.motionKeyframes.end()) {
                        layer.motionKeyframes.erase(motIt, layer.motionKeyframes.end());
                        changed = true;
                    }

                    auto opIt = std::remove_if(layer.opacityKeyframes.begin(), layer.opacityKeyframes.end(), [&](const Layer::OpacityKeyFrame& kf) {
                        return kf.uuid == kfUuid.toStdString();
                    });
                    if (opIt != layer.opacityKeyframes.end()) {
                        layer.opacityKeyframes.erase(opIt, layer.opacityKeyframes.end());
                        changed = true;
                    }
                }
            }
        }
        if (changed) scene.setDirty(true);
    }
    updateKeyframeDisplay();
}

KeyframeContext MainWindow::findKeyframeByLayerUuid(const std::string layerUuid, const std::string keyframeUuid){
    KeyframeContext keyframeContext;
    LayerContext layerContext = findLayerByUuid(layerUuid);
    if(!layerContext.isValid()){
        GameFusion::Log().error() << "Unable to find layer " << layerUuid.c_str() << "\n";
        GameFusion::Log().info() << "Unable to find layer " << layerUuid.c_str() << "\n";
        return keyframeContext;
    }

    keyframeContext.scene = layerContext.scene;
    keyframeContext.shot = layerContext.shot;
    keyframeContext.panel = layerContext.panel;
    keyframeContext.layer = layerContext.layer;

    GameFusion::Layer *layer = keyframeContext.layer;

    //if (isMotion)
    {
        auto it = std::find_if(layer->motionKeyframes.begin(), layer->motionKeyframes.end(), [&](const GameFusion::Layer::MotionKeyFrame& kf) {
            return kf.uuid == keyframeUuid;
        });
        if (it != layer->motionKeyframes.end()) {
            keyframeContext.keyframe = &(*it);
            keyframeContext.isMotion = true;
            return keyframeContext;
        }
    }

    {
        auto it = std::find_if(layer->opacityKeyframes.begin(), layer->opacityKeyframes.end(), [&](const GameFusion::Layer::OpacityKeyFrame& kf) {
            return kf.uuid == keyframeUuid;
        });
        if (it != layer->opacityKeyframes.end()) {
            keyframeContext.keyframe = &(*it);
            keyframeContext.isMotion = false;
            return keyframeContext;
        }
    }

    return keyframeContext;
}

QVariantMap MainWindow::getKeyframeValueMap(const KeyframeContext& keyframeContext) {
    QVariantMap valueMap;

    if(!keyframeContext.isValid())
        return valueMap;

    valueMap["shotUuid"] = QString::fromStdString(keyframeContext.shot->uuid);
    valueMap["shotName"] = QString::fromStdString(keyframeContext.shot->name);
    valueMap["panelUuid"] = QString::fromStdString(keyframeContext.panel->uuid);
    valueMap["panelName"] = QString::fromStdString(keyframeContext.panel->name);
    valueMap["layerUuid"] = QString::fromStdString(keyframeContext.layer->uuid);
    valueMap["layerName"] = QString::fromStdString(keyframeContext.layer->name);
    valueMap["sceneUuid"] = QString::fromStdString(keyframeContext.scene->uuid);
    valueMap["sceneName"] = QString::fromStdString(keyframeContext.scene->name);
    valueMap["timeMs"] = keyframeContext.keyframe->time * (1000.0 / ProjectContext::instance().projectJson()["fps"].toDouble(24.0)); // Convert frames to ms
    valueMap["time"] = keyframeContext.keyframe->time; // Convert frames to ms
    valueMap["uuid"] = QString::fromStdString(keyframeContext.keyframe->uuid);

    if(keyframeContext.isMotion){
        Layer::MotionKeyFrame *motion = (Layer::MotionKeyFrame*)keyframeContext.keyframe;
        valueMap["x"] = motion->x;
        valueMap["y"] = motion->y;
        valueMap["scale"] = motion->scale;
        valueMap["rotation"] = motion->rotation;

    }
    else {
        Layer::OpacityKeyFrame *opacity = (Layer::OpacityKeyFrame*)keyframeContext.keyframe;
        valueMap["opacity"] = opacity->opacity;
    }

    return valueMap;
}

void MainWindow::onKeyframeUpdated(const QString& kfUuid, double globalMs, const QVariant& newValue) {

    QVariantMap newValueMap = newValue.toMap();

    QString panelUuid = newValueMap["panelUuid"].toString();
    QString layerUuid = newValueMap["layerUuid"].toString();

    KeyframeContext keyframeContext = findKeyframeByLayerUuid(layerUuid.toStdString(), kfUuid.toStdString());
    if(!keyframeContext.isValid()){
        GameFusion::Log().error() << "Failed to find keyframe "<<kfUuid.toUtf8().constData()<<" for layer "<<layerUuid.toUtf8().constData()<<"\n";
        return;
    }

    QVariantMap oldValueMap = getKeyframeValueMap(keyframeContext);

    keyframeContext.scene->setDirty(true);
    paint->getPaintArea()->updateLayer(*keyframeContext.layer);

    float localMs = globalMs - (keyframeContext.shot->startTime + keyframeContext.panel->startTime);
    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    qreal mspf = 1000.0 / fps;
    float localFr = static_cast<qreal>(localMs / mspf);
    undoStack->push(new KeyframeUpdateCommand(kfUuid, keyframeContext.keyframe->time, oldValueMap, localFr, newValueMap, layerUuid, panelUuid, newValueMap["shotUuid"].toString(), this));
}

void MainWindow::onGroupedKeyframeUpdated(const QString& uuid1, double globalMs1, const QVariant& value1,
                                           const QString& uuid2, double globalMs2, const QVariant& value2) {
    CompositeUndoCommand* composite = new CompositeUndoCommand("Move Keyframe Pair");

    QVariantMap valueMap1 = value1.toMap();
    valueMap1["timeMs"] = globalMs1; // ??? what is the best here
    QString layerUuid1 = valueMap1["layerUuid"].toString();
    KeyframeContext keyframeCtx1 = findKeyframeByLayerUuid(layerUuid1.toStdString(), uuid1.toStdString());
    if(!keyframeCtx1.isValid()){
        GameFusion::Log().error() << "Failed to find values for keyframe "<<uuid1.toUtf8().constData()<<" for layer "<<layerUuid1.toUtf8().constData()<<"\n";
        return;
    }

    QVariantMap oldValueMap1 = getKeyframeValueMap(keyframeCtx1);
    QVariantMap valueMap2 = value2.toMap();
    valueMap2["timeMs"] = globalMs2; // ??? what is the best here
    QString layerUuid2 = valueMap2["layerUuid"].toString();
    KeyframeContext keyframeCtx2 = findKeyframeByLayerUuid(layerUuid2.toStdString(), uuid2.toStdString());
    if(!keyframeCtx2.isValid()){
        GameFusion::Log().error() << "Failed to find values for keyframe "<<uuid1.toUtf8().constData()<<" for layer "<<layerUuid1.toUtf8().constData()<<"\n";
        return;
    }
    QVariantMap oldValueMap2 = getKeyframeValueMap(keyframeCtx2);

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    qreal mspf = 1000.0 / fps;

    float localMs1 = globalMs1 - (keyframeCtx1.shot->startTime + keyframeCtx1.panel->startTime);
    float localFr1 = static_cast<qreal>(localMs1 / mspf);

    float localMs2 = globalMs2 - (keyframeCtx2.shot->startTime + keyframeCtx2.panel->startTime);
    float localFr2 = static_cast<qreal>(localMs2 / mspf);

    composite->addCommand(new KeyframeUpdateCommand(uuid1, keyframeCtx1.keyframe->time, oldValueMap1, localFr1, valueMap1, valueMap1["layerUuid"].toString(), valueMap1["panelUuid"].toString(), valueMap1["shotUuid"].toString(), this));
    composite->addCommand(new KeyframeUpdateCommand(uuid2, keyframeCtx2.keyframe->time, oldValueMap2, localFr2, valueMap2, valueMap2["layerUuid"].toString(), valueMap2["panelUuid"].toString(), valueMap2["shotUuid"].toString(), this));

    undoStack->push(composite);
}

void MainWindow::updateKeyframe(const QString& kfUuid, double localFr, const QVariant& value, const QString& layerUuid, const QString& panelUuid, const QString& shotUuid) {
    // Logic to update keyframe without pushing to undo stack
    // Similar to onKeyframeUpdated body, but without creating command
    QVariantMap valueMap = value.toMap();
    KeyframeContext keyframeCtx = findKeyframeByLayerUuid(layerUuid.toStdString(), kfUuid.toStdString());
    if(!keyframeCtx.isValid())
        return;

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    qreal mspf = 1000.0 / fps;

    if (keyframeCtx.isMotion) {
        Layer::MotionKeyFrame *motion = (Layer::MotionKeyFrame *)keyframeCtx.keyframe;
        motion->x = valueMap["x"].toDouble();
        motion->y = valueMap["y"].toDouble();
        motion->scale = valueMap["scale"].toDouble();
        motion->rotation = valueMap["rotation"].toDouble();
        motion->time = localFr;
        std::sort(keyframeCtx.layer->motionKeyframes.begin(), keyframeCtx.layer->motionKeyframes.end(), [](const GameFusion::Layer::MotionKeyFrame& a, const GameFusion::Layer::MotionKeyFrame& b) {
            return a.time < b.time;
        });
    } else {
        Layer::OpacityKeyFrame *op = (Layer::OpacityKeyFrame *)keyframeCtx.keyframe;
        op->opacity = valueMap["opacity"].toDouble();
        op->time = localFr;
        std::sort(keyframeCtx.layer->opacityKeyframes.begin(), keyframeCtx.layer->opacityKeyframes.end(), [](const GameFusion::Layer::OpacityKeyFrame& a, const GameFusion::Layer::OpacityKeyFrame& b) {
            return a.time < b.time;
        });

    }

    keyframeCtx.scene->setDirty(true);
    paint->getPaintArea()->updateLayer(*keyframeCtx.layer);

    qreal globalMs = static_cast<qreal>(keyframeCtx.shot->startTime) + keyframeCtx.panel->startTime + localFr * mspf;
    qreal globalFr = static_cast<qreal>(globalMs / mspf);

    QString attributeName = keyframeCtx.isMotion ? "motion" : "opacity";
    TrackItem *storyboardTrack = timeLineView->getTrack(0);
    storyboardTrack->updateKeyframe(attributeName, kfUuid, globalMs, QVariant(valueMap)); // Assuming layerName is available or computed
}

void MainWindow::loadSettings() {
    QSettings settings("B-Line", "Storyboard"); // Adjust organization and app name
    autoSave = settings.value("autoSave", false).toBool();

    ui->actionAuto_Save->setChecked(autoSave);
}

void MainWindow::saveSettings() {
    QSettings settings("MyCompany", "MyApp"); // Adjust organization and app name
    settings.setValue("autoSave", autoSave);
}

void MainWindow::onCheckDirtyTimer() {
    if(savePending || !scriptBreakdown)
        return;

    for(auto &scene: scriptBreakdown->getScenes()){
        if(!scene.dirty)
            continue;

        updateWindowTitle(true);
        savePending = true;
        break;
    }
}

void MainWindow::onAutoSaveTimer() {
    if (autoSave && savePending) {
        this->saveProject();
        updateWindowTitle(false);
        savePending = false;
    }
}

void MainWindow::toggleAutoSave(bool checked) {
    autoSave = checked;
    saveSettings();
    if (autoSave) {
        autoSaveTimer->start(30000); // Start timer if enabled
    } else {
        autoSaveTimer->stop(); // Stop timer if disabled
    }
}

void MainWindow::addKeyframe(const QString& kfUuid, double keyTime, const QVariantMap& value,
                             const QString& layerUuid, const QString& panelUuid, const QString& shotUuid, double cursorTime) {
    LayerContext layerCtx = findLayerByUuid(layerUuid.toStdString());
    if (!layerCtx.isValid()) {
        GameFusion::Log().error() << "Invalid layer context for UUID " << layerUuid.toUtf8().constData() << "\n";
        return;
    }
    GameFusion::Layer *layer = layerCtx.layer;
    GameFusion::Panel *panel = layerCtx.panel;
    GameFusion::Shot *shot = layerCtx.shot;

    bool isMotion = value.contains("x");
    if (isMotion) {
        GameFusion::Layer::MotionKeyFrame kf;
        kf.uuid = kfUuid.toStdString();
        kf.time = keyTime;
        kf.x = value.value("x", 0.0f).toFloat();
        kf.y = value.value("y", 0.0f).toFloat();
        kf.scale = value.value("scale", 1.0f).toFloat();
        kf.rotation = value.value("rotation", 0.0f).toFloat();
        layer->motionKeyframes.push_back(kf);
        std::sort(layer->motionKeyframes.begin(), layer->motionKeyframes.end(),
                  [](const GameFusion::Layer::MotionKeyFrame& a, const GameFusion::Layer::MotionKeyFrame& b) {
                      return a.time < b.time;
                  });
    } else {
        GameFusion::Layer::OpacityKeyFrame kf;
        kf.uuid = kfUuid.toStdString();
        kf.time = keyTime;
        kf.opacity = value.value("opacity", 1.0f).toFloat();
        layer->opacityKeyframes.push_back(kf);
        std::sort(layer->opacityKeyframes.begin(), layer->opacityKeyframes.end(),
                  [](const GameFusion::Layer::OpacityKeyFrame& a, const GameFusion::Layer::OpacityKeyFrame& b) {
                      return a.time < b.time;
                  });
    }

    layerCtx.scene->setDirty(true);
    paint->getPaintArea()->updateLayer(*layerCtx.layer);
    updateKeyframeDisplay();

}

void MainWindow::deleteKeyframe(const QString& kfUuid, double time, const QVariantMap& value,
                               const QString& layerUuid, const QString& panelUuid, const QString& shotUuid, double cursorTime) {
    LayerContext layerCtx = findLayerByUuid(layerUuid.toStdString());
    if (!layerCtx.isValid()) {
        GameFusion::Log().error() << "Invalid layer context for UUID " << layerUuid.toUtf8().constData() << "\n";
        return;
    }
    GameFusion::Layer *layer = layerCtx.layer;

    bool isMotion = value.contains("x");
    if (isMotion) {
        auto it = std::find_if(layer->motionKeyframes.begin(), layer->motionKeyframes.end(),
                               [&](const GameFusion::Layer::MotionKeyFrame& kf) {
                                   return kf.uuid == kfUuid.toStdString();
                               });
        if (it != layer->motionKeyframes.end()) {
            layer->motionKeyframes.erase(it);
        }
    } else {
        auto it = std::find_if(layer->opacityKeyframes.begin(), layer->opacityKeyframes.end(),
                               [&](const GameFusion::Layer::OpacityKeyFrame& kf) {
                                   return kf.uuid == kfUuid.toStdString();
                               });
        if (it != layer->opacityKeyframes.end()) {
            layer->opacityKeyframes.erase(it);
        }
    }

    layerCtx.scene->setDirty(true);
    paint->getPaintArea()->updateLayer(*layerCtx.layer);
    updateKeyframeDisplay();
}

void MainWindow::updateLayerListWidget(const std::vector<QString>& orderUuids) {
    ui->layerListWidget->blockSignals(true);

    QMap<QString, QListWidgetItem*> uuidToItem;
    for (int i = 0; i < ui->layerListWidget->count(); ++i) {
        QListWidgetItem* item = ui->layerListWidget->item(i);
        QString uuid = item->data(Qt::UserRole).toString();
        uuidToItem[uuid] = item;
    }

    int insertIndex = 0;
    for (const QString& uuid : orderUuids) {
        if (uuidToItem.contains(uuid)) {
            QListWidgetItem* item = uuidToItem[uuid];
            int currentIndex = ui->layerListWidget->row(item);
            if (currentIndex != insertIndex) {
                ui->layerListWidget->takeItem(currentIndex);
                ui->layerListWidget->insertItem(insertIndex, item);
            }
            insertIndex++;
        }
    }

    ui->layerListWidget->blockSignals(false);
}

void MainWindow::applyLayerOrder(const std::vector<QString>& orderUuids, const QString& panelUuid) {
    PanelContext panelCtx = findPanelByUuid(panelUuid.toStdString());
    if (!panelCtx.isValid()) return;

    std::vector<GameFusion::Layer> reordered;
    for (const QString& uuid : orderUuids) {
        auto it = std::find_if(panelCtx.panel->layers.begin(), panelCtx.panel->layers.end(),
                               [&](const GameFusion::Layer& l) { return l.uuid == uuid.toStdString(); });
        if (it != panelCtx.panel->layers.end()) {
            reordered.push_back(*it);
        }
    }

    panelCtx.panel->layers = reordered;
    panelCtx.scene->dirty = true;
    updateWindowTitle(true);

    long panelStartTime = panelCtx.shot->startTime + panelCtx.panel->startTime;
    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();

    paint->getPaintArea()->setPanel(*panelCtx.panel, panelStartTime, fps, panelCtx.shot->cameraAnimation);
    paint->getPaintArea()->invalidateAllLayers();
    paint->getPaintArea()->updateCompositeImage();
    updateLayerListWidget(orderUuids); // Sync UI
}

void MainWindow::addLayer(const GameFusion::Layer& layer, const QString& panelUuid, int index) {
    PanelContext panelContext = findPanelByUuid(panelUuid.toStdString());
    if (!panelContext.isValid()) return;

    if (index < 0 || index > static_cast<int>(panelContext.panel->layers.size())) {
            index = 0; // Default to inserting at the beginning if index is invalid
        }

    panelContext.panel->layers.insert(panelContext.panel->layers.begin() + index, layer);

    panelContext.scene->dirty = true;
    updateWindowTitle(true);

    long panelStartTime = panelContext.shot->startTime + panelContext.panel->startTime;
    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();

    paint->getPaintArea()->setPanel(*panelContext.panel, panelStartTime, fps, panelContext.shot->cameraAnimation);

    // Update Layer Panel
    populateLayerList(panelContext.panel);

    paint->getPaintArea()->invalidateAllLayers();
    paint->getPaintArea()->updateCompositeImage();
}

void MainWindow::removeLayer(const QString& layerUuid, const QString& panelUuid) {
    PanelContext panelContext = findPanelByUuid(panelUuid.toStdString());
    if (!panelContext.isValid()) return;

    auto it = std::find_if(panelContext.panel->layers.begin(), panelContext.panel->layers.end(),
                           [&](const GameFusion::Layer& l) { return l.uuid == layerUuid.toStdString(); });
    if (it != panelContext.panel->layers.end()) {
        panelContext.panel->layers.erase(it);
    }

    panelContext.scene->dirty = true;
    updateWindowTitle(true);

    long panelStartTime = panelContext.shot->startTime + panelContext.panel->startTime;
    float fps = ProjectContext::instance().projectJson()["fps"].toDouble();

    paint->getPaintArea()->setPanel(*panelContext.panel, panelStartTime, fps, panelContext.shot->cameraAnimation);

    // Update Layer Panel
    populateLayerList(panelContext.panel);

    paint->getPaintArea()->invalidateAllLayers();
    paint->getPaintArea()->updateCompositeImage();
}

void MainWindow::setLayerFX(const QString& layerUuid, const QString& panelUuid, const std::string& fx) {
    PanelContext panelContext = findPanelByUuid(panelUuid.toStdString());
    if (!panelContext.isValid()) return;

    auto it = std::find_if(panelContext.panel->layers.begin(), panelContext.panel->layers.end(),
                           [&](const GameFusion::Layer& l) { return l.uuid == layerUuid.toStdString(); });
    if (it != panelContext.panel->layers.end()) {
        it->fx = fx;
        panelContext.scene->dirty = true;
        updateWindowTitle(true);
        paint->getPaintArea()->updateLayer(*it);
        populateLayerList(panelContext.panel);
        paint->getPaintArea()->invalidateAllLayers();
        paint->getPaintArea()->updateCompositeImage();
    }
}

void MainWindow::setLayerImage(const QString& layerUuid, const QString& panelUuid, const std::string& imageFilePath) {
    PanelContext panelContext = findPanelByUuid(panelUuid.toStdString());
    if (!panelContext.isValid()) return;

    auto it = std::find_if(panelContext.panel->layers.begin(), panelContext.panel->layers.end(),
                           [&](const GameFusion::Layer& l) { return l.uuid == layerUuid.toStdString(); });
    if (it != panelContext.panel->layers.end()) {
        it->imageFilePath = imageFilePath;
        panelContext.scene->dirty = true;
        updateWindowTitle(true);
        if (!imageFilePath.empty()) {
            QImage image;
            if (!image.load(QString::fromStdString(imageFilePath))) {
                qWarning() << "Failed to load image:" << QString::fromStdString(imageFilePath);
                return;
            }
        }

        long panelStartTime = panelContext.shot->startTime + panelContext.panel->startTime;
        float fps = ProjectContext::instance().projectJson()["fps"].toDouble();
        paint->getPaintArea()->setPanel(*panelContext.panel, panelStartTime, fps, panelContext.shot->cameraAnimation);
        populateLayerList(panelContext.panel);
    }
}

void MainWindow::setLayerAttribute(const QString& layerUuid, const QString& panelUuid,
                                  MainWindow::LayerAttributeType attrType,
                                  const std::variant<int, double, GameFusion::BlendMode>& value) {
    PanelContext panelContext = findPanelByUuid(panelUuid.toStdString());
    if (!panelContext.isValid()) return;

    auto it = std::find_if(panelContext.panel->layers.begin(), panelContext.panel->layers.end(),
                           [&](const GameFusion::Layer& l) { return l.uuid == layerUuid.toStdString(); });
    if (it != panelContext.panel->layers.end()) {
        panelContext.scene->dirty = true;
        updateWindowTitle(true);
        switch (attrType) {
            case LayerAttributeType::Rotation:
                it->rotation = std::get<int>(value);
                ui->spinBox_layerRotation->blockSignals(true);
                ui->spinBox_layerRotation->setValue(it->rotation);
                ui->spinBox_layerRotation->blockSignals(false);
                break;
            case LayerAttributeType::PosX:
                it->x = std::get<double>(value);
                ui->doubleSpinBox_layerPosX->blockSignals(true);
                ui->doubleSpinBox_layerPosX->setValue(it->x);
                ui->doubleSpinBox_layerPosX->blockSignals(false);
                break;
            case LayerAttributeType::PosY:
                it->y = std::get<double>(value);
                ui->doubleSpinBox_layerPosY->blockSignals(true);
                ui->doubleSpinBox_layerPosY->setValue(it->y);
                ui->doubleSpinBox_layerPosY->blockSignals(false);
                break;
            case LayerAttributeType::Scale:
                it->scale = std::get<double>(value);
                ui->doubleSpinBox_layerScale->blockSignals(true);
                ui->doubleSpinBox_layerScale->setValue(it->scale/1.);
                ui->doubleSpinBox_layerScale->blockSignals(false);
                break;
            case LayerAttributeType::BlendMode:
                it->blendMode = std::get<GameFusion::BlendMode>(value);
                break;
        }
        paint->getPaintArea()->updateLayer(*it);
        //populateLayerList(panelContext.panel);
        //paint->getPaintArea()->invalidateAllLayers();
        paint->getPaintArea()->updateCompositeImage();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::KeyPress && (obj == ui->spinBox_layerRotation ||
                                              obj == ui->doubleSpinBox_layerPosX ||
                                              obj == ui->doubleSpinBox_layerPosY ||
                                              obj == ui->doubleSpinBox_layerScale)) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        QKeySequence pressed(keyEvent->modifiers() | keyEvent->key());

        if (pressed == QKeySequence::Undo) {
            undoAction->trigger();   // Ctrl+Z ou cmd+Z sur Mac
            return true;             // eat event
        }
        if (pressed == QKeySequence::Redo) {
            redoAction->trigger();   // Ctrl+Y / Ctrl+Shift+Z selon plateforme
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::duplicateLayer(const QString &sourceLayerUuid, const QString &duplicateLayerUuid) {
    LayerContext layerContext = findLayerByUuid(sourceLayerUuid.toStdString().c_str());
    if (!layerContext.isValid())
        return;

    GameFusion::Layer duplicatedLayer = *layerContext.layer;
    duplicatedLayer.uuid = duplicateLayerUuid.toStdString();
    duplicatedLayer.name = layerContext.layer->name + " (Copy)";

    int insertIndex = 0;
    for (size_t i = 0; i < layerContext.panel->layers.size(); ++i) {
        if (layerContext.panel->layers[i].uuid == sourceLayerUuid.toStdString()) {
            insertIndex = static_cast<int>(i);
            break;
        }
    }

    addLayer(duplicatedLayer, layerContext.panel->uuid.c_str(), insertIndex);
}

void MainWindow::updateLayer(const QString& layerUuid, const QString& panelUuid, const GameFusion::Layer& layer) {
    PanelContext panelContext = findPanelByUuid(panelUuid.toStdString());
    if (!panelContext.isValid()) return;

    auto it = std::find_if(panelContext.panel->layers.begin(), panelContext.panel->layers.end(),
                           [&](const GameFusion::Layer& l) { return l.uuid == layerUuid.toStdString(); });
    if (it != panelContext.panel->layers.end()) {
        *it = layer; // Deep copy to update layer
        panelContext.scene->dirty = true;
        updateWindowTitle(true);
        paint->getPaintArea()->updateLayer(*it);
        populateLayerList(panelContext.panel);
        paint->getPaintArea()->invalidateAllLayers();
        paint->getPaintArea()->updateCompositeImage();
    }
}

void MainWindow::onKeyFramePositionChanged(const QString& layerUuid, int keyFrameIndex, double oldX, double oldY, double newX, double newY) {
    if (!currentPanel) return;

    undoStack->push(new LayerKeyFrameCommand(layerUuid, QString::fromStdString(currentPanel->uuid),
                                             keyFrameIndex, oldX, oldY, newX, newY, this));

}

void MainWindow::onLayerPositionChanged(const QString& layerUuid, double oldX, double oldY, double newX, double newY, bool isEditing) {
    if (!currentPanel) return;

    if(isEditing){
        ui->doubleSpinBox_layerPosX->blockSignals(true);
        ui->doubleSpinBox_layerPosX->blockSignals(true);
        ui->doubleSpinBox_layerPosX->setValue(newX);
        ui->doubleSpinBox_layerPosY->setValue(newY);
        ui->doubleSpinBox_layerPosX->blockSignals(false);
        ui->doubleSpinBox_layerPosX->blockSignals(false);
        return;
    }
    undoStack->push(new LayerPositionCommand(layerUuid, QString::fromStdString(currentPanel->uuid),
                                             oldX, oldY, newX, newY, this));
}

void MainWindow::setKeyFramePosition(const QString& layerUuid, const QString& panelUuid, int keyFrameIndex, double x, double y) {

    LayerContext layerContext = findLayerByUuid(panelUuid.toStdString(), layerUuid.toStdString());

    if (!layerContext.isValid()) return;

    auto it = std::find_if(layerContext.panel->layers.begin(), layerContext.panel->layers.end(),
                           [&](const GameFusion::Layer& l) { return l.uuid == layerUuid.toStdString(); });
    if (it == layerContext.panel->layers.end() || keyFrameIndex < 0 || keyFrameIndex >= static_cast<int>(it->motionKeyframes.size())) return;

    Layer::MotionKeyFrame &kf = it->motionKeyframes[keyFrameIndex];

    kf.x = x;
    kf.y = y;

    layerContext.scene->dirty = true;
    updateWindowTitle(true);
    paint->getPaintArea()->updateLayer(*it);
    populateLayerList(layerContext.panel);
    paint->getPaintArea()->invalidateAllLayers();
    paint->getPaintArea()->updateCompositeImage();

    bool isMotion = true;
    KeyframeContext keyCtx = {layerContext.scene, layerContext.shot, layerContext.panel, layerContext.layer, &kf, isMotion};


    QVariantMap valueMap = getKeyframeValueMap(keyCtx);
    TrackItem *track = timeLineView->getTrack(0);
    double panelStartTime = layerContext.panel->startTime;

    qreal fps = ProjectContext::instance().projectJson()["fps"].toDouble(24.0);
    float mspf = fps > 0 ? 1000./fps : 1;
    long timeMs = panelStartTime + kf.time * mspf;
    track->addKeyframeToAttribute("motion", timeMs, valueMap);

    updateLayerPanelAttributes(*layerContext.layer);
}

void MainWindow::setLayerPosition(const QString& layerUuid, const QString& panelUuid, double x, double y) {
    LayerContext layerCtx = findLayerByUuid(layerUuid.toStdString());
    if (!layerCtx.isValid()) return;

    layerCtx.layer->x = x;
    layerCtx.layer->y = y;

    layerCtx.scene->dirty = true;
    //updateWindowTitle(true);
    //paint->getPaintArea()->updateLayer(*layerCtx.layer);
    //populateLayerList(layerCtx.panel);
    updateLayerPanelAttributes(*layerCtx.layer);
    //paint->getPaintArea()->invalidateAllLayers();
    //paint->getPaintArea()->updateCompositeImage();

}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    emit windowShown();
}


void MainWindow::duplicateLayerThroughShot(const QString& layerUuid) {
    if (!currentPanel) return;
    PanelContext panelContext = findPanelByUuid(currentPanel->uuid);
    if (!panelContext.isValid()) return;

    LayerContext layerContext = findLayerByUuid(layerUuid.toStdString().c_str());
    if (!layerContext.isValid()) return;

    int originalIndex = -1;
    for (size_t i = 0; i < layerContext.panel->layers.size(); ++i) {
        if (layerContext.panel->layers[i].uuid == layerUuid.toStdString()) {
            originalIndex = static_cast<int>(i);
            break;
        }
    }
    if (originalIndex == -1) return;

    for (auto& panel : panelContext.shot->panels) {
        if (panel.uuid == currentPanel->uuid) continue;
        QString duplicatedLayerUuid = QUuid::createUuid().toString();
        undoStack->push(new LayerDuplicateCommand(layerUuid, duplicatedLayerUuid,
                                                 QString::fromStdString(panel.uuid), this));
    }
}

void MainWindow::instanceLayerToPanels(const QString& layerUuid, const std::vector<QString>& panelUuids) {
    LayerContext layerContext = findLayerByUuid(layerUuid.toStdString().c_str());
    if (!layerContext.isValid()) return;

    for (const auto& panelUuid : panelUuids) {
        QString aliasLayerUuid = QUuid::createUuid().toString();
        undoStack->push(new InstanceLayerCommand(layerUuid, aliasLayerUuid, panelUuid, this));
    }
}

void MainWindow::renameLayer(const QString& layerUuid, const QString& newName, bool propagateToAliases) {
    LayerContext layerContext = findLayerByUuid(layerUuid.toStdString().c_str());
    if (!layerContext.isValid()) return;

    QString oldName = QString::fromStdString(layerContext.layer->name);
    layerContext.layer->name = newName.toStdString();
    if (propagateToAliases) {
        for (auto& shot : layerContext.scene->shots) {
            for (auto& panel : shot.panels) {
                for (auto& layer : panel.layers) {
                    if (layer.aliasUuid == layerUuid.toStdString()) {
                        layer.name = newName.toStdString();
                    }
                }
            }
        }
    }
    layerContext.scene->dirty = true;
    updateWindowTitle(true);
    populateLayerList(layerContext.panel);
    paint->getPaintArea()->invalidateAllLayers();
    paint->getPaintArea()->updateCompositeImage();
}

bool MainWindow::hasLayerInstances(const std::string& layerUuid) const {
    for(auto &scene :scriptBreakdown->getScenes()) {
        for(auto &shot : scene.shots) {
            for(auto &panel : shot.panels) {
                for(auto &layer : panel.layers) {
                    if(layer.uuid == layerUuid)
                        return true;
                    for(auto &sublayer: layer.layers){
                        if(sublayer.uuid == layerUuid)
                            return true;
                    }
                }
            }
        }
    }
    return false;
}

void MainWindow::groupLayers(const std::vector<QString>& layerUuids, const QString& panelUuid) {
    PanelContext panelContext = findPanelByUuid(panelUuid.toStdString());
    if (!panelContext.isValid() || layerUuids.size() < 2) return;

    GameFusion::Layer groupLayer;
    groupLayer.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    groupLayer.name = "Group " + std::to_string(panelContext.panel->layers.size() + 1);

    std::vector<GameFusion::Layer> groupedLayers;
    for (const auto& uuid : layerUuids) {
        LayerContext layerContext = findLayerByUuid(uuid.toStdString().c_str());
        if (layerContext.isValid()) {
            groupedLayers.push_back(*layerContext.layer);
            removeLayer(uuid, panelUuid);
        }
    }
    groupLayer.layers = groupedLayers;

    addLayer(groupLayer, panelUuid);
}

void MainWindow::ungroupLayers(const QString& layerUuid, const QString& panelUuid) {
    PanelContext panelContext = findPanelByUuid(panelUuid.toStdString());
    if (!panelContext.isValid()) return;

    LayerContext layerContext = findLayerByUuid(layerUuid.toStdString().c_str());
    if (!layerContext.isValid() || !layerContext.layer->isGroup()) return;

    int index = -1;
    for (size_t i = 0; i < panelContext.panel->layers.size(); ++i) {
        if (panelContext.panel->layers[i].uuid == layerUuid.toStdString()) {
            index = static_cast<int>(i);
            break;
        }
    }
    if (index == -1) return;

    for (const auto& subLayer : layerContext.layer->layers) {
        addLayer(subLayer, panelUuid, index++);
    }
    removeLayer(layerUuid, panelUuid);
}

void MainWindow::onLayerDuplicateThroughShot() {
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.isEmpty() || !currentPanel) return;

    QString layerUuid = selectedLayers.first()->data(Qt::UserRole).toString();
    duplicateLayerThroughShot(layerUuid);
}

void MainWindow::onLayerInstanceToSelectedPanels() {
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.isEmpty() || !currentPanel) return;

    QString layerUuid = selectedLayers.first()->data(Qt::UserRole).toString();
    std::vector<QString> panelUuids; // TODO: Implement panel selection logic
    instanceLayerToPanels(layerUuid, panelUuids);
}

void MainWindow::onLayerInstanceThroughShot() {
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.isEmpty() || !currentPanel) return;

    QString layerUuid = selectedLayers.first()->data(Qt::UserRole).toString();
    PanelContext panelContext = findPanelByUuid(currentPanel->uuid);
    if (!panelContext.isValid()) return;

    std::vector<QString> panelUuids;
    for (const auto& panel : panelContext.shot->panels) {
        if (panel.uuid != currentPanel->uuid) {
            panelUuids.push_back(QString::fromStdString(panel.uuid));
        }
    }
    instanceLayerToPanels(layerUuid, panelUuids);
}

void MainWindow::onLayerCopy() {
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.isEmpty() || !currentPanel) return;

    QString layerUuid = selectedLayers.first()->data(Qt::UserRole).toString();
    LayerContext layerContext = findLayerByUuid(layerUuid.toStdString().c_str());
    if (!layerContext.isValid()) return;

    clipboardLayer = *layerContext.layer;
    hasClipboardLayer = true;
}

void MainWindow::onLayerPasteAsInstance() {
    if (!hasClipboardLayer || !currentPanel) return;

    GameFusion::Layer instanceLayer = clipboardLayer;
    QString instanceLayerUuid = QUuid::createUuid().toString();
    instanceLayer.uuid = instanceLayerUuid.toStdString();
    instanceLayer.name = clipboardLayer.name + " (Instance)";
    instanceLayer.aliasUuid = clipboardLayer.uuid;

    undoStack->push(new CopyPasteLayerCommand(instanceLayer, QString::fromStdString(currentPanel->uuid), true, this));
}

void MainWindow::onLayerPasteAsDuplicate() {
    if (!hasClipboardLayer || !currentPanel) return;

    GameFusion::Layer duplicateLayer = clipboardLayer;
    duplicateLayer.uuid = QUuid::createUuid().toString().toStdString();
    duplicateLayer.name = clipboardLayer.name + " (Copy)";
    duplicateLayer.aliasUuid = "";

    undoStack->push(new CopyPasteLayerCommand(duplicateLayer, QString::fromStdString(currentPanel->uuid), false, this));
}

void MainWindow::onLayerRename() {
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.isEmpty() || !currentPanel) return;

    QString layerUuid = selectedLayers.first()->data(Qt::UserRole).toString();
    LayerContext layerContext = findLayerByUuid(layerUuid.toStdString().c_str());
    if (!layerContext.isValid()) return;

    bool propagate = false;
    if (hasLayerInstances(layerUuid.toStdString())) {
        int result = QMessageBox::question(this, "Propagate Rename",
                                          "Propagate rename to instanced layers?",
                                          QMessageBox::Yes | QMessageBox::No);
        propagate = (result == QMessageBox::Yes);
    }

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Layer",
                                            "Layer name:", QLineEdit::Normal,
                                            QString::fromStdString(layerContext.layer->name), &ok);
    if (ok && !newName.isEmpty()) {
        undoStack->push(new RenameLayerCommand(layerUuid, QString::fromStdString(layerContext.layer->name),
                                               newName, propagate, QString::fromStdString(currentPanel->uuid), this));
        renameLayer(layerUuid, newName, propagate);
    }
}

void MainWindow::onLayerProperties() {
    // TODO: Implement LayerPropertiesDialog
    qDebug() << "Layer Properties not implemented yet";
}

void MainWindow::onGroupLayers() {
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.size() < 2 || !currentPanel) return;

    std::vector<QString> layerUuids;
    for (const auto& item : selectedLayers) {
        layerUuids.push_back(item->data(Qt::UserRole).toString());
    }

    QString groupUuid = QUuid::createUuid().toString();
    undoStack->push(new GroupLayersCommand(layerUuids, groupUuid, QString::fromStdString(currentPanel->uuid), this));
    groupLayers(layerUuids, QString::fromStdString(currentPanel->uuid));
}

void MainWindow::onUngroupLayers() {
    // TODO: Implement group selection logic
    QString groupUuid; // Assume group UUID from selection
    if (!currentPanel || groupUuid.isEmpty()) return;

    undoStack->push(new GroupLayersCommand(std::vector<QString>{}, groupUuid, QString::fromStdString(currentPanel->uuid), this));
    ungroupLayers(groupUuid, QString::fromStdString(currentPanel->uuid));
}

void MainWindow::onLayerClear() {
    QList<QListWidgetItem*> selectedLayers = ui->layerListWidget->selectedItems();
    if (selectedLayers.isEmpty() || !currentPanel) return;

    QString layerUuid = selectedLayers.first()->data(Qt::UserRole).toString();
    LayerContext layerContext = findLayerByUuid(layerUuid.toStdString().c_str());
    if (!layerContext.isValid()) return;

    GameFusion::Layer originalLayer = *layerContext.layer;
    GameFusion::Layer clearedLayer = originalLayer;
    clearedLayer.strokes.clear();
    clearedLayer.motionKeyframes.clear();
    clearedLayer.opacityKeyframes.clear();
    clearedLayer.imageFilePath.clear();

    undoStack->push(new ClearLayerCommand(originalLayer, clearedLayer, layerUuid,
                                         QString::fromStdString(currentPanel->uuid), this));
    //updateLayer(layerUuid, QString::fromStdString(currentPanel->uuid), clearedLayer);
}

void MainWindow::colorPalette() {
    ColorPaletteWidget *colorPalette = nullptr;

    // Create only once – reuse if already exists
    if (!colorPalette) {
        colorPalette = new ColorPaletteWidget();  // parented to dock for proper cleanup
        colorPalette->setWindowModality(Qt::WindowModal);  // blocks rest of app
        colorPalette->setWindowTitle("Color Palette");

        // Optional: set reasonable size
        colorPalette->resize(400, 360);

        // Connect the color selection signal
        connect(colorPalette, &ColorPaletteWidget::colorPicked,
                this, [this, colorPalette](const QColor &color) {

                    colorPalette->close();  // auto-close on pick (nice UX)
                });
    }

    // Update palette to current color before showing
    //colorPalette->setCurrentColor(foregroundColor);

    // Show centered over the main window (or dock)
    colorPalette->move(mapToGlobal(rect().center()) - QPoint(colorPalette->width()/2, colorPalette->height()/2));
    colorPalette->show();
    colorPalette->raise();
    colorPalette->activateWindow();
}
