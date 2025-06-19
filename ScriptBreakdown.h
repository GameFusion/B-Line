#ifndef SCRIPTBREAKDOWN_H
#define SCRIPTBREAKDOWN_H

#include "Script.h"
#include "LlamaClient.h"
#include <vector>
#include <string>

namespace GameFusion {

struct Shot {
    std::string name; // e.g., SHOT_0010
    std::string type; // e.g., CLOSE, MEDIUM, WIDE
    std::string description;
    int frameCount;
    std::string dialogue;
    std::string fx;
    std::string notes;
};

struct Scene {
    std::string name; // e.g., SCENE_001
    std::string heading;
    std::vector<Shot> shots;
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

    ScriptBreakdown(const Str& fileName, GameScript* dictionary = nullptr, GameScript* dictionaryCustom = nullptr, LlamaClient* client=nullptr);
    ~ScriptBreakdown();

    bool breakdownScript(LlamaClient* client, const std::string& modelPath, const std::string& backend = "CPU",
                         BreakdownMode mode = BreakdownMode::FullContext, bool enableSequences = false);
    std::vector<Act>& getActs();
    std::vector<Scene>& getScenes();
    std::vector<Sequence>& getSequences();

private:
    bool initializeLlamaClient(LlamaClient* client, const std::string& modelPath, const std::string& backend);
    bool processScenes(BreakdownMode mode);
    bool processShots(Scene& scene, int sceneIndex, BreakdownMode mode);
    bool processSequences();
    bool processActs(BreakdownMode mode);
    std::string generatePrompt(const std::string& type, const std::string& content);
    void populateUI();

    std::vector<Act> acts;
    std::vector<Scene> scenes;
    std::vector<Sequence> sequences;
    static void streamCallback(const char* msg, void* user_data);
    static void finishedCallback(const char* msg, void* user_data);

    LlamaClient* llamaClient;
};

}

#endif // SCRIPTBREAKDOWN_H
