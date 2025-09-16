#ifndef SCRIPTBREAKDOWN_H
#define SCRIPTBREAKDOWN_H

#include "Script.h"
#include "LlamaClient.h"
#include "BezierPath.h"
#include "BezierCurve.h"

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

class PromptLogger;

struct Camera {
    std::string movement; // e.g., "PAN LEFT", "STATIC"
    std::string framing;  // e.g., "OVER-THE-SHOULDER", "WIDE ANGLE"
};

enum class EasingType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Bezier,
    Cut
};

enum class BlendMode {
    Normal, Multiply, Screen, Overlay
};

inline std::string toString(EasingType easing) {
    switch (easing) {
    case EasingType::Linear:    return "Linear";
    case EasingType::EaseIn:    return "EaseIn";
    case EasingType::EaseOut:   return "EaseOut";
    case EasingType::EaseInOut: return "EaseInOut";
    case EasingType::Bezier:    return "Bezier";
    case EasingType::Cut:       return "Cut";
    default:                    return "Linear";  // Fallback
    }
}

inline EasingType fromString(const std::string& str) {
    if (str == "EaseIn")    return EasingType::EaseIn;
    if (str == "EaseOut")   return EasingType::EaseOut;
    if (str == "EaseInOut") return EasingType::EaseInOut;
    if (str == "Bezier")    return EasingType::Bezier;
    if (str == "Cut")       return EasingType::Cut;
    return EasingType::Linear; // Default fallback
}

inline std::string toString(BlendMode mode) {
    switch (mode) {
    case BlendMode::Normal:   return "Normal";
    case BlendMode::Multiply: return "Multiply";
    case BlendMode::Screen:   return "Screen";
    case BlendMode::Overlay:  return "Overlay";
    default:                  return "Normal";
    }
}

inline BlendMode blendModeFromString(const std::string& str) {
    if (str == "Multiply") return BlendMode::Multiply;
    if (str == "Screen")   return BlendMode::Screen;
    if (str == "Overlay")  return BlendMode::Overlay;
    return BlendMode::Normal;
}

struct CameraFrame {
    std::string name;
    int frameOffset = 0; // frame offset relative to panel start
    float x = 0.0f;
    float y = 0.0f;
    float zoom = 1.0f;
    float rotation = 0.0f;
    std::string panelUuid;     // The panel this keyframe was created in

    std::string uuid;          // Optional: for unique identification
    int time;                  // Time in frames of ms ???, redundan with frameOffset ???, relative to shot start

    CameraFrame(int t, float px, float py, float z = 1.0f, float r = 0.0f, const std::string& ownerPanel = "")
        : time(t), x(px), y(py), zoom(z), rotation(r), panelUuid(ownerPanel)
    {
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }

    CameraFrame() {
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }

    EasingType easing = EasingType::EaseInOut; // Default to smooth
    // Extend this for 3D camera and bezier support
    int easyIn = 10;
    int easyOut = 10;

    // Optional: For Bezier
    Vector2D bezierControl1;
    Vector2D bezierControl2;
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

struct Layer {
    std::string uuid;
    std::string name;
    std::string thumbnail;

    float opacity = 1.0f;
    bool visible = true;

    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;
    float rotation = 0.0f;
    BlendMode blendMode = BlendMode::Normal;
    std::string fx;

    struct KeyFrame {
        int time = 0; // in frames
        float x = 0.0f;
        float y = 0.0f;
        float scale = 1.0f;
        float rotation = 0.0f;
        float opacity = 1.0f;
        EasingType easing = EasingType::Linear;

        // Optional: bezier support
        Vector2D bezierControl1;
        Vector2D bezierControl2;
    };

    //GameFusion::List<GameFusion::BezierPath> strokes; // Strokes for this layer
    //std::vector<GameFusion::BezierPath> strokes;
    std::vector<BezierCurve> strokes; // Updated to BezierCurve
    std::vector<KeyFrame> keyframes;

    std::string imageFilePath; // If set, layer uses this image instead of strokes

    Layer() {
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }

    Layer(const std::string& layerName)
        : name(layerName)
    {
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }

};

struct Panel {
    std::string uuid;
    std::string name;            // Panel name or ID
    std::string thumbnail;   // Thumbnail path for the panel
    std::string image;   // Reference image path for the panel
    int startTime = 0;          // Relative to shot start
    int durationTime = 0;
    //int durationFrames = 0;      // Optional, if needed
    std::string description;     // Optional panel-specific note or caption

    std::vector<GameFusion::Layer> layers; // optional: per panel

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
    std::string tags;
    std::string slugline;
    std::string intent;       // Narrative/emotional purpose
    std::vector<CharacterDialog> characters;
    std::vector<Panel> panels;
    std::vector<CameraFrame> cameraFrames; // Global across panels, time-based
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

    std::string getDialogs() const {
        std::string result;

        for (const auto& character : characters) {
            // Assuming dialogNumber is an integer, convert it to string
            result += std::to_string(character.dialogNumber) + " " + character.name;

            if (character.onScreen)
                result += "\n";
            else
                result += "(OS)\n";

            if (!character.dialogParenthetical.empty())
                result += character.dialogParenthetical + "\n";

            result += character.dialogue + "\n";
        }

        return result;
    }
};

struct Scene {
    std::string name; // e.g., SCENE_001
    std::string heading;
    std::string sceneId;
    std::string description;
    std::vector<Shot> shots;
    std::string uuid;
    std::string tags;
    std::string notes;

    std::string filename; // File this scene was loaded from or will be saved to
    bool dirty = false;   // Set to true when scene is edited and needs saving

    Scene() {
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }

    void setDirty(bool d){dirty=d;}
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

    ScriptBreakdown(const Str& fileName, const float fps, GameScript* dictionary = nullptr, GameScript* dictionaryCustom = nullptr, LlamaClient* client=nullptr, PromptLogger* logger = nullptr);
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

    void addCameraFrame(const CameraFrame& frame);
    bool updateCameraFrame(const CameraFrame& frame);
    bool deleteCameraFrame(const std::string& uuid);

    void updateShotTimings(GameFusion::Scene& scene);

    bool setScene(GameFusion::Scene& scene, const std::string& uuid);
    GameFusion::Scene* getPreviousScene(GameFusion::Scene& refScene);
    GameFusion::Scene* getNextScene(GameFusion::Scene& refScene);

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
    PromptLogger* logger;

    float fps;
};

}

#endif // SCRIPTBREAKDOWN_H
