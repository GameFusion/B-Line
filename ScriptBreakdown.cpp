#include "ScriptBreakdown.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <sstream>

#include "Paragraph.h"
#include "LlamaClient.h"

namespace GameFusion {

struct CallbackData {
    std::string result;
};

ScriptBreakdown::ScriptBreakdown(const Str& fileName, GameScript* dictionary, GameScript* dictionaryCustom, LlamaClient* llamaClient)
    : Script(fileName, dictionary, dictionaryCustom), llamaClient(llamaClient) {}

ScriptBreakdown::~ScriptBreakdown() {}

bool ScriptBreakdown::breakdownScript(LlamaClient* client, const std::string& modelPath, const std::string& backend,
                                      BreakdownMode mode, bool enableSequences) {
    if (!load()) {
        Log::error("Failed to load script file: %s\n", (char*)m_fileName);
        return false;
    }

    if (!initializeLlamaClient(client, modelPath, backend)) {
        Log::error("Failed to initialize LlamaClient\n");
        return false;
    }

    if (!processScenes(mode)) {
        Log::error("Failed to process scenes\n");
        return false;
    }

    if (enableSequences && !processSequences()) {
        Log::warning("Sequence grouping skipped due to processing error\n");
    }

    if (!processActs(mode)) {
        Log::warning("Act grouping skipped due to processing error\n");
    }

    populateUI();
    return true;
}

bool ScriptBreakdown::initializeLlamaClient(LlamaClient* client, const std::string& modelPath, const std::string& backend) {
    if (!client) {
        Log::error("Null LlamaClient provided\n");
        return false;
    }

    if (!client->isModelLoaded()) {
#ifdef _WIN32
        client = LlamaClient::Create(backend, "LlamaEngine.dll");
#elif __APPLE__
        client = LlamaClient::Create(backend, "LlamaEngine.dylib");
#endif
        if (!client) {
            Log::error("LlamaClient creation failed: %s\n", LlamaClient::GetCreateError().c_str());
            return false;
        }

        // Define parameter values
        float temperature = 0.7f;
        int max_tokens = 512;

        // Initialize ModelParameter array
        ModelParameter params[] = {
            {"temperature", PARAM_FLOAT, &temperature},
            {"max_tokens", PARAM_INT, &max_tokens}
        };

        if (!client->loadModel(modelPath, params, 2, [](const char* msg) {
                Log::info("Model load: %s\n", msg);
            })) {
            Log::error("Failed to load Llama model: %s\n", modelPath.c_str());
            return false;
        }
    }
    else return true;

    return client->createSession(1);
}

bool ScriptBreakdown::processScenes(BreakdownMode mode) {
    scenes.clear();
    int sceneCount = 0;

    for (int i = 0; i < m_paragraphs.length(); ++i) {
        Paragraph& p = m_paragraphs[i];
        if (p.styleName() == "SCENE" || (p.styleName() == "ACTION" && (p.text().match("INT.", false) || p.text().match("EXT.", false)))) {
            Scene scene;
            scene.name = Str().sprintf("SCENE_%03d", ++sceneCount);
            if(!p.text().isEmpty())
                scene.heading = p.text().c_str();
            scenes.push_back(scene);
            processShots(scenes.back(), i, mode);
        }
    }

    return !scenes.empty();
}


QString cleanJsonMessage(const QString& commitMessage) {
    // Step 1: Remove "Commit Message:", "Commit Message", and "```"
    QString cleanedMessage = commitMessage;

    cleanedMessage.replace("```json", "", Qt::CaseInsensitive);
    cleanedMessage.replace("```", "", Qt::CaseInsensitive);

    // Step 2: Strip spaces at the beginning and end of each paragraph
    QStringList lines = cleanedMessage.split("\n");
    for (int i = 0; i < lines.size(); ++i) {
        lines[i] = lines[i].trimmed();  // Remove leading and trailing spaces from each line
    }

    // Step 3: Replace multiple blank lines with a single blank line
    // We use a flag to track if we just added a blank line, and skip any consecutive blank lines
    QStringList finalMessage;
    bool lastLineWasBlank = false;
    for (const QString& line : lines) {
        if (line.isEmpty()) {
            if (!lastLineWasBlank) {
                finalMessage.append("");  // Add a single blank line
                lastLineWasBlank = true;  // Mark that we've just added a blank line
            }
        }
        else {
            finalMessage.append(line);  // Add non-blank lines normally
            lastLineWasBlank = false;   // Reset the flag for blank lines
        }
    }

    // Join the cleaned lines back into a single string
    return finalMessage.join("\n");
}


bool ScriptBreakdown::processShots(Scene& scene, int sceneIndex, BreakdownMode mode) {
    int shotCount = 0;
    CallbackData callbackData;

    if (mode == BreakdownMode::FullContext) {
        std::string context;
        for (int i = sceneIndex; i < m_paragraphs.length(); ++i) {
            Paragraph& p = m_paragraphs[i];
            if (p.styleName() == "SCENE" && i != sceneIndex) {
                break; // Next scene starts
            }
            if(!p.text().isEmpty())
                context += p.text().c_str();
            context += "\n";
        }

        std::string prompt = generatePrompt("shots", context);
        llamaClient->clearSession(1);
        if (!llamaClient->generateResponse(1, prompt, streamCallback, finishedCallback, &callbackData)) {
            Log::error("Failed to generate shots for scene: %s\n", scene.name.c_str());
            return false;
        }
    } else { // ChunkedContext
        for (int i = sceneIndex; i < m_paragraphs.length(); ++i) {
            Paragraph& p = m_paragraphs[i];
            if (p.styleName() == "SCENE" && i != sceneIndex) {
                break; // Next scene starts
            }
            std::string chunk;
            if(!p.text().isEmpty())
                chunk = p.text().c_str();
            std::string prompt = generatePrompt("shots", chunk);
            callbackData.result.clear();
            llamaClient->clearSession(1);
            if (!llamaClient->generateResponse(1, prompt, streamCallback, finishedCallback, &callbackData)) {
                Log::error("Failed to generate shots for chunk in scene: %s\n", scene.name.c_str());
                continue;
            }
            const QString cleanData = cleanJsonMessage(callbackData.result.c_str());
            QJsonDocument doc = QJsonDocument::fromJson(cleanData.toUtf8());
            if (!doc.isNull()) {
                QJsonArray shotsArray = doc.array();
                for (const auto& shotJson : shotsArray) {
                    QJsonObject obj = shotJson.toObject();
                    Shot shot;
                    shot.name = Str().sprintf("SHOT_%04d", ++shotCount * 10).c_str();
                    shot.type = obj["type"].toString().toStdString();
                    shot.description = obj["description"].toString().toStdString();
                    shot.frameCount = obj["frameCount"].toInt();
                    shot.dialogue = obj["dialogue"].toString().toStdString();
                    shot.fx = obj["fx"].toString().toStdString();
                    shot.notes = obj["notes"].toString().toStdString();
                    scene.shots.push_back(shot);
                }
            }
        }
    }

    QString cleanData = cleanJsonMessage(callbackData.result.c_str());
    QJsonDocument doc = QJsonDocument::fromJson(cleanData.toUtf8());
    if (doc.isNull()) {
        Log::error("Invalid JSON response for shots\n");
        return false;
    }

    QJsonArray shotsArray = doc.array();
    for (const auto& shotJson : shotsArray) {
        QJsonObject obj = shotJson.toObject();
        Shot shot;
        shot.name = Str().sprintf("SHOT_%04d", ++shotCount * 10).c_str();
        shot.type = obj["type"].toString().toStdString();
        shot.description = obj["description"].toString().toStdString();
        shot.frameCount = obj["frameCount"].toInt();
        shot.dialogue = obj["dialogue"].toString().toStdString();
        shot.fx = obj["fx"].toString().toStdString();
        shot.notes = obj["notes"].toString().toStdString();
        scene.shots.push_back(shot);
    }

    return true;
}

bool ScriptBreakdown::processSequences() {
    sequences.clear();
    if (scenes.empty()) {
        return false;
    }

    std::string context;
    for (const auto& scene : scenes) {
        context += "Scene: " + scene.name + " - " + scene.heading + "\n";
    }

    std::string prompt = generatePrompt("sequences", context);
    CallbackData callbackData;

    llamaClient->clearSession(1);
    if (!llamaClient->generateResponse(1, prompt, streamCallback, finishedCallback, &callbackData)) {
        Log::error("Failed to generate sequences\n");
        return false;
    }

    QString cleanData = cleanJsonMessage(callbackData.result.c_str());
    QJsonDocument doc = QJsonDocument::fromJson(cleanData.toUtf8());
    if (doc.isNull()) {
        Log::error("Invalid JSON response for sequences\n");
        return false;
    }

    QJsonArray seqArray = doc.array();
    int seqIndex = 0;
    for (const auto& seqJson : seqArray) {
        QJsonObject obj = seqJson.toObject();
        Sequence seq;
        seq.name = Str().sprintf("SEQ_%03d", ++seqIndex).c_str();
        QJsonArray sceneIndices = obj["scenes"].toArray();
        for (const auto& idx : sceneIndices) {
            int sceneIdx = idx.toInt();
            if (sceneIdx >= 0 && sceneIdx < scenes.size()) {
                seq.scenes.push_back(scenes[sceneIdx]);
            }
        }
        sequences.push_back(seq);
    }

    return true;
}

bool ScriptBreakdown::processActs(BreakdownMode mode) {
    acts.clear();
    if (scenes.empty()) {
        return false;
    }

    std::string context;
    for (const auto& scene : scenes) {
        context += "Scene: " + scene.name + " - " + scene.heading + "\n";
    }

    std::string prompt = generatePrompt("acts", context);
    CallbackData callbackData;

    if (mode == BreakdownMode::ChunkedContext) {
        // Process in smaller chunks if needed
        // For simplicity, use full context for acts as it's typically smaller
    }

    llamaClient->clearSession(1);
    if (!llamaClient->generateResponse(1, prompt, streamCallback, finishedCallback, &callbackData)) {
        Log::error("Failed to generate acts\n");
        return false;
    }

    QString cleanData = cleanJsonMessage(callbackData.result.c_str());
    QJsonDocument doc = QJsonDocument::fromJson(cleanData.toUtf8());
    if (doc.isNull()) {
        Log::error("Invalid JSON response for acts\n");
        return false;
    }

    QJsonArray actsArray = doc.array();
    int actIndex = 0;
    for (const auto& actJson : actsArray) {
        QJsonObject obj = actJson.toObject();
        Act act;
        act.name = obj["name"].toString().toStdString();
        QJsonArray sceneIndices = obj["scenes"].toArray();
        for (const auto& idx : sceneIndices) {
            int sceneIdx = idx.toInt();
            if (sceneIdx >= 0 && sceneIdx < scenes.size()) {
                act.scenes.push_back(scenes[sceneIdx]);
            }
        }
        acts.push_back(act);
    }

    return true;
}

std::string ScriptBreakdown::generatePrompt(const std::string& type, const std::string& content) {
    std::stringstream prompt;
    if (type == "shots") {
        prompt << "Analyze the following script excerpt and break it into shots. For each shot, provide a JSON object with fields: name (format SHOT_XXXX, increment by 10), type (e.g., CLOSE, MEDIUM, WIDE), description, frameCount (estimate based on content), dialogue, fx, and notes. Ensure names follow ShotGrid conventions.\n\n"
               << content << "\n\nOutput as a JSON array.";
    } else if (type == "sequences") {
        prompt << "Analyze the following scene list and group scenes into sequences based on narrative or location continuity. For each sequence, provide a JSON object with fields: name (format SEQ_XXX), scenes (array of scene indices). Output as a JSON array.\n\n"
               << content;
    } else if (type == "acts") {
        prompt << "Analyze the following scene list and group scenes into acts based on narrative structure (e.g., three-act structure). For each act, provide a JSON object with fields: name (format ACT_X), scenes (array of scene indices). Output as a JSON array.\n\n"
               << content;
    }
    return prompt.str();
}

void ScriptBreakdown::populateUI() {
    // Pseudo-code for UI integration
    for (const auto& scene : scenes) {
        // Add to scene list
        // sceneList->addScene(QString::fromStdString(scene.name), QString::fromStdString(scene.heading));
        for (const auto& shot : scene.shots) {
            // Add to panel view
            // panelView->addShot({
            //     .name = QString::fromStdString(shot.name),
            //     .type = QString::fromStdString(shot.type),
            //     .description = QString::fromStdString(shot.description),
            //     .frameCount = shot.frameCount,
            //     .dialogue = QString::fromStdString(shot.dialogue),
            //     .fx = QString::fromStdString(shot.fx),
            //     .notes = QString::fromStdString(shot.notes)
            // });
            // Position on timeline
            // timeline->positionShot(shot.frameCount);
        }
    }
    for (const auto& seq : sequences) {
        // Add to sequence view if enabled
        // sequenceView->addSequence(QString::fromStdString(seq.name), seq.scenes);
    }
}

std::vector<Act>& ScriptBreakdown::getActs() {
    return acts;
}

std::vector<Scene>& ScriptBreakdown::getScenes() {
    return scenes;
}

std::vector<Sequence>& ScriptBreakdown::getSequences() {
    return sequences;
}

void ScriptBreakdown::streamCallback(const char* msg, void* user_data) {
    CallbackData* data = static_cast<CallbackData*>(user_data);
    data->result += msg;
    Log::info("Streamed response: %s\n", msg);
}

void ScriptBreakdown::finishedCallback(const char* msg, void* user_data) {
    CallbackData* data = static_cast<CallbackData*>(user_data);
    data->result = msg;
    Log::info("Completed response: %s\n", msg);
}

}

