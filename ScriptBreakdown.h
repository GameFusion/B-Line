#ifndef SCRIPTBREAKDOWN_H
#define SCRIPTBREAKDOWN_H

#include "Script.h"
#include "LlamaClient.h"
#include <vector>
#include <string>

#include <QJsonObject>
#include <QUuid>

namespace GameFusion {
/*
struct Dialog {
    int dialogNumber;
    std::string character;
    std::string dialogue;
};
*/

struct Camera {
    std::string movement; // e.g., "PAN LEFT", "STATIC"
    std::string framing;  // e.g., "OVER-THE-SHOULDER", "WIDE ANGLE"
};

struct Audio {
    std::string ambient;         // e.g., "street noise"
    std::vector<std::string> sfx; // e.g., ["barking", "gunshot"]
};

struct CharacterDialog {
    std::string name;
    std::string emotion;
    std::string intent;
    bool onScreen = true;
    int dialogNumber = -1; // Optional: -1 if not present
    std::string dialogParenthetical;
    std::string dialogue;
};

struct Panel {
    std::string uuid;
    std::string name;            // Panel name or ID
    std::string thumbnail;   // Thumbnail path for the panel
    std::string image;   // Image path for the panel
    int startTime = 0;          // Relative to shot start
    int durationTime = 0;
    //int durationFrames = 0;      // Optional, if needed
    std::string description;     // Optional panel-specific note or caption

    Panel() {
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }

    Panel(const std::string& panelName,
          const std::string& thumbPath,
          int start,
          int duration = 0,
          const std::string& desc = "")
        : name(panelName),
        thumbnail(thumbPath),
        startTime(start),
        durationTime(duration),
        description(desc)
    {
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }

    Panel duplicatePanel(int newStartTime = -1) {
        Panel copy;

        copy.name = this->name + "_copy";
        copy.thumbnail = this->thumbnail;
        copy.image = this->image;
        copy.startTime = (newStartTime >= 0) ? newStartTime :  this->startTime +  this->durationTime;
        copy.durationTime =  this->durationTime;
        copy.description =  this->description;

        // New UUID is automatically generated in constructor
        return copy;
    }


};

struct Shot {
    std::string name; // e.g., SHOT_0010
    std::string type; // e.g., CLOSE, MEDIUM, WIDE
    std::string transition;   // e.g., "CUT", "FADE IN"
    Camera camera;            // Movement and framing
    std::string lighting;     // e.g., "soft sunlight"
    Audio audio;              // Ambient and SFX
    std::string description;
    int frameCount;
    std::string timeOfDay;
    bool restore = false;
    std::string fx;
    std::string action; // TODO Use this
    std::string notes;
    std::string intent;       // Narrative/emotional purpose
    std::vector<CharacterDialog> characters;
    std::vector<Panel> panels;
    std::string uuid;
    int startTime=-1;
    int endTime=-1;

    Shot() {
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }

    bool removePanelByUuid(const std::string& panelUuid) {
        auto it = std::remove_if(panels.begin(), panels.end(), [&](const Panel& panel) {
            return panel.uuid == panelUuid;
        });

        if (it != panels.end()) {
            panels.erase(it, panels.end());
            return true;
        }

        return false;
    }

};

struct Scene {
    std::string name; // e.g., SCENE_001
    std::string heading;
    std::string sceneId;
    std::string description;
    std::vector<Shot> shots;
    std::string uuid;

    std::string filename; // File this scene was loaded from or will be saved to
    bool dirty = false;   // Set to true when scene is edited and needs saving

    Scene() {
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }
};

struct Sequence {
    std::string name; // e.g., SEQ_001
    std::vector<Scene> scenes;
};

struct Act {
    std::string name; // e.g., ACT_1
    std::vector<Scene> scenes;
};

class ScriptBreakdown : public Script {
public:
    enum class BreakdownMode {
        FullContext,  // Single prompt with full context
        ChunkedContext // Break context into smaller prompts
    };

    ScriptBreakdown(const Str& fileName, const float fps, GameScript* dictionary = nullptr, GameScript* dictionaryCustom = nullptr, LlamaClient* client=nullptr);
    ~ScriptBreakdown();

    bool breakdownScript(BreakdownMode mode = BreakdownMode::FullContext, bool enableSequences = false);
    std::vector<Act>& getActs();
    std::vector<Scene>& getScenes();
    std::vector<Sequence>& getSequences();
    std::vector<Shot>& getShots();
    std::vector<CharacterDialog>& getCharacters();

    void printScenes() const;
    void printShots() const;
    void printCharacters() const;

    void loadScene(const QString& sceneName, const QJsonObject &sceneObj, const QString filename);

    void saveModifiedScenes(QString projectPath);
private:
    bool initializeLlamaClient(LlamaClient* client, const std::string& modelPath, const std::string& backend);
    bool processScenes(BreakdownMode mode);
    bool processShots(Scene& scene, int sceneIndex, BreakdownMode mode);
    bool processSequences();
    bool processActs(BreakdownMode mode);
    std::string generatePrompt(const std::string& type, const std::string& content);
    void populateUI();

    void addShotFromJson(const QJsonObject& obj, Scene& scene);

    std::vector<Act> acts;
    std::vector<Scene> scenes;
    std::vector<Shot> shots; // duplicate copy of all shots found in scenes
    std::vector<CharacterDialog> characters; // duplicate copy of all dialogs
    std::vector<Sequence> sequences;

    static void streamCallback(const char* msg, void* user_data);
    static void finishedCallback(const char* msg, void* user_data);

    LlamaClient* llamaClient;

    float fps;
};

}

#endif // SCRIPTBREAKDOWN_H
