#include "ScriptBreakdown.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>
#include <QDir>

#include <sstream>

#include "Paragraph.h"
#include "LlamaClient.h"
#include "PromptLogger.h"

namespace GameFusion {

struct CallbackData {
    std::string result;
};

ScriptBreakdown::ScriptBreakdown(const Str& fileName, const float fps, GameScript* dictionary, GameScript* dictionaryCustom, LlamaClient* llamaClient, PromptLogger* logger)
    : Script(fileName, dictionary, dictionaryCustom), fps(fps), llamaClient(llamaClient), logger(logger) {}

ScriptBreakdown::~ScriptBreakdown() {}

bool ScriptBreakdown::breakdownScript(BreakdownMode mode, bool enableSequences) {
    if (!load()) {
        Log::error("Failed to load script file: %s\n", (char*)m_fileName);
        return false;
    }

    if (!llamaClient) {
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
            if(!processShots(scenes.back(), i, mode))
                return false;
        }
    }

    return true;
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

void ScriptBreakdown::addShotFromJson(const QJsonObject& obj, Scene& scene) {
    Shot shot;

    shot.name = obj["name"].toString().toStdString();
    shot.type = obj["type"].toString().toStdString();
    shot.description = obj["description"].toString().toStdString();
    shot.frameCount = obj["frameCount"].toInt();
    shot.timeOfDay = obj["timeOfDay"].toString().toStdString();
    shot.restore = obj.contains("restore") && obj["restore"].toBool();
    shot.fx = obj["fx"].toString().toStdString();
    shot.notes = obj["notes"].toString().toStdString();
    shot.transition = obj["transition"].toString().toStdString();
    shot.lighting = obj["lighting"].toString().toStdString();
    shot.intent = obj["intent"].toString().toStdString();

    if(obj.contains("uuid"))
        shot.uuid = obj["uuid"].toString().toStdString();
    if(obj.contains("startTime"))
        shot.startTime = obj["startTime"].toInt();
    if(obj.contains("endTime"))
        shot.endTime = obj["endTime"].toInt();






    float mspf = fps > 0 ? 1000./fps : 1;

    // Panels (optional support from JSON)
    if (obj.contains("panels")) {
        QJsonArray panelsArray = obj["panels"].toArray();
        for (const auto& panelValue : panelsArray) {
            QJsonObject panelObj = panelValue.toObject();

            Panel panel;
            panel.name = panelObj["name"].toString().toStdString();
            panel.thumbnail = panelObj["thumbnail"].toString().toStdString();
            panel.image = panelObj["image"].toString().toStdString();
            panel.durationTime = panelObj.contains("durationTime")
                                       ? panelObj["durationTime"].toInt()
                                       : shot.frameCount*mspf;

            panel.description = panelObj["description"].toString().toStdString();
            if(panelObj.contains("uuid"))
                panel.uuid = panelObj["uuid"].toString().toStdString();
            if(panelObj.contains("startTime"))
                panel.startTime = panelObj["startTime"].toInt();

            if (panelObj.contains("layers")) {
                QJsonArray layersArray = panelObj["layers"].toArray();
                for (const auto& layerValue : layersArray) {
                    QJsonObject layerObj = layerValue.toObject();
                    Layer layer;

                    layer.uuid = layerObj["uuid"].toString().toStdString();
                    layer.name = layerObj["name"].toString().toStdString();
                    layer.thumbnail = layerObj["thumbnail"].toString().toStdString();
                    layer.opacity = float(layerObj["opacity"].toDouble(1.0));
                    layer.fx = layerObj["opacity"].toString().toStdString();
                    layer.blendMode = blendModeFromString(layerObj["blendMode"].toString().toStdString());
                    layer.visible = layerObj["visible"].toBool(true);
                    layer.x = float(layerObj["x"].toDouble());
                    layer.y = float(layerObj["y"].toDouble());
                    layer.scale = float(layerObj["scale"].toDouble(1.0));
                    layer.rotation = float(layerObj["rotation"].toDouble(0.0));
                    layer.imageFilePath = layerObj["imageFilePath"].toString().toStdString();

                    // Keyframes
                    if (layerObj.contains("keyframes")) {
                        QJsonArray keyframesArray = layerObj["keyframes"].toArray();
                        for (const auto& kfValue : keyframesArray) {
                            QJsonObject kfObj = kfValue.toObject();
                            Layer::KeyFrame kf;
                            kf.time = kfObj["time"].toInt();
                            kf.x = float(kfObj["x"].toDouble());
                            kf.y = float(kfObj["y"].toDouble());
                            kf.scale = float(kfObj["scale"].toDouble(1.0));
                            kf.rotation = float(kfObj["rotation"].toDouble(0.0));
                            kf.opacity = float(kfObj["opacity"].toDouble(1.0));
                            kf.easing = fromString(kfObj["easing"].toString().toStdString());

                            kf.bezierControl1.x() = float(kfObj["bezierControl1X"].toDouble());
                            kf.bezierControl1.y() = float(kfObj["bezierControl1Y"].toDouble());
                            kf.bezierControl2.x() = float(kfObj["bezierControl2X"].toDouble());
                            kf.bezierControl2.y() = float(kfObj["bezierControl2Y"].toDouble());

                            layer.keyframes.push_back(kf);
                        }
                    }

                    // Load strokes
                    if (layerObj.contains("strokes")) {
                        QJsonArray strokeArray = layerObj["strokes"].toArray();
                        //layer.strokes.clear(); // Clear existing strokes

                        for (const auto& strokeVal : strokeArray) {

                            GameFusion::BezierCurve path;
                            if(strokeVal.isObject()){

                                QJsonObject strokeObj = strokeVal.toObject();

                                path.fromJson(strokeObj); // Deserialize handles and strokeProperties
                                layer.strokes.push_back(path);
                            }
                            else {
                                QJsonArray pathArray = strokeVal.toArray();

                                for (const auto& controlPointsVal : pathArray) {
                                    QJsonArray controlPoints = controlPointsVal.toArray();

                                    if (controlPoints.size() != 3) continue;  // Safety check

                                    QJsonArray pArray = controlPoints[0].toArray();
                                    QJsonArray lArray = controlPoints[1].toArray();
                                    QJsonArray rArray = controlPoints[2].toArray();

                                    if (pArray.size() < 2 || lArray.size() < 2 || rArray.size() < 2) continue;

                                    Vector3D p(pArray[0].toDouble(), pArray[1].toDouble(), 0);
                                    Vector3D l(lArray[0].toDouble(), lArray[1].toDouble(), 0);
                                    Vector3D r(rArray[0].toDouble(), rArray[1].toDouble(), 0);

                                    path += GameFusion::BezierControl(p, l, r);
                                }

                                layer.strokes.push_back(path);
                            }
                        }
                    }



                    panel.layers.push_back(layer); // this is not working great
                    // todo change stratagy to re

                } // I get a crash here, when layer is deleted
            }

            // create a default layers if there are none
            if(panel.layers.empty()){
                Layer bg;
                Layer l1;

                bg.name = "BG";
                l1.name = "Layer 1";

                panel.layers.push_back(l1);
                panel.layers.push_back(bg);
            }

            shot.panels.push_back(panel);
        }
    }

    // CameraFrames JSON
    if (obj.contains("cameraFrames") && obj["cameraFrames"].isArray()) {
        QJsonArray framesArray = obj["cameraFrames"].toArray();
        for (const QJsonValue& value : framesArray) {
            if (value.isObject()) {
                QJsonObject frameObj = value.toObject();
                GameFusion::CameraFrame frame;
                frame.name = frameObj["name"].toString().toStdString();
                frame.uuid = frameObj["uuid"].toString().toStdString();
                frame.time = frameObj["time"].toInt();
                frame.x = static_cast<float>(frameObj["x"].toDouble());
                frame.y = static_cast<float>(frameObj["y"].toDouble());
                frame.zoom = static_cast<float>(frameObj["zoom"].toDouble());
                frame.rotation = static_cast<float>(frameObj["rotation"].toDouble());
                frame.panelUuid = frameObj["panelUuid"].toString().toStdString();
                frame.frameOffset = frameObj["frameOffset"].toInt();
                if(frameObj.contains("easing"))
                    frame.easing = fromString(frameObj["easing"].toString().toStdString());

                shot.cameraFrames.push_back(frame);
            }
        }
    }

    // If no panels provided, add default one
    if (shot.panels.empty()) {
        Panel defaultPanel;
        defaultPanel.name = shot.name + "_PANEL_001";
        defaultPanel.thumbnail = obj["thumbnail"].toString().toStdString(); // fallback
        defaultPanel.startTime = 0;
        defaultPanel.durationTime = shot.frameCount*mspf;

        shot.panels.push_back(defaultPanel);
    }

    // Camera block
    if (obj.contains("camera")) {
        QJsonObject cameraObj = obj["camera"].toObject();
        shot.camera.movement = cameraObj["movement"].toString().toStdString();
        shot.camera.framing = cameraObj["framing"].toString().toStdString();
    }

    // Audio block
    if (obj.contains("audio")) {
        QJsonObject audioObj = obj["audio"].toObject();
        shot.audio.ambient = audioObj["ambient"].toString().toStdString();

        QJsonArray sfxArray = audioObj["sfx"].toArray();
        for (const auto& sfxItem : sfxArray) {
            shot.audio.sfx.push_back(sfxItem.toString().toStdString());
        }
    }

    // Characters (with optional dialog)
    QJsonArray characterArray = obj["characters"].toArray();
    for (const auto& characterJson : characterArray) {
        QJsonObject characterObj = characterJson.toObject();
        CharacterDialog character;

        character.name = characterObj["name"].toString().toStdString();
        character.emotion = characterObj["emotion"].toString().toStdString();
        character.intent = characterObj["intent"].toString().toStdString();
        character.onScreen = characterObj.contains("onScreen") ? characterObj["onScreen"].toBool() : true;
        character.dialogNumber = characterObj.contains("dialogNumber") ? characterObj["dialogNumber"].toInt() : -1;
        character.dialogParenthetical = characterObj["dialogParenthetical"].toString().toStdString();
        character.dialogue = characterObj["dialogue"].toString().toStdString();

        shot.characters.push_back(character);

        characters.push_back(character);
    }

    scene.shots.push_back(shot);
    shots.push_back(shot);

    qDebug() << "Loaded scene:" << scene.sceneId << "with" << scene.shots.size() << "shots.";
    Log().info() << "Loaded scene:" << scene.sceneId.c_str() << "with" << (int)scene.shots.size() << "shots.";

    //printCharacters();
}

bool ScriptBreakdown::processShots(Scene& scene, int sceneIndex, BreakdownMode mode) {
    int shotCount = 0;
    CallbackData callbackData;

    llamaClient->clearSession(1);

    if (mode == BreakdownMode::FullContext) {
        std::string context;
        for (int i = sceneIndex; i < m_paragraphs.length(); ++i) {
            Paragraph& p = m_paragraphs[i];
            if (p.styleName() == "SCENE" && i != sceneIndex) {
                break; // Next scene starts
            }

            if(p.styleName() == "ACTION" || p.styleName() == "CHARACTER")
                context += "\n";

            if(!p.text().isEmpty()){
                context += p.styleName() + ": " + p.text().c_str();
                context += "\n";
            }
        }

        if(context.empty())
            return true; // an empty scene will be ignored

        std::string prompt = generatePrompt("shots", context);

        Log::info() << "Prompt: " << prompt.c_str() << "\n";
        if (!llamaClient->generateResponse(1, prompt, streamCallback, finishedCallback, &callbackData)) {
            Log::error("Failed to generate shots for scene: %s\n", scene.name.c_str());
            return false;
        }

        // Log prompt and result (assuming tokenCount and cost are provided by LlamaClient)
        int tokenCount = 0; // TODO: Get from LlamaClient
        double cost = 0.0;  // TODO: Get from LlamaClient

        QJsonObject contextJson;
        contextJson["sceneId"] = QString::fromStdString(scene.sceneId);
        contextJson["sceneName"] = QString::fromStdString(scene.name);

        logger->logPromptAndResult("shots", prompt, callbackData.result, tokenCount, cost, contextJson);

        QString cleanData = cleanJsonMessage(QString::fromStdString(callbackData.result));
        QJsonDocument doc = QJsonDocument::fromJson(cleanData.toUtf8());
        if (!doc.isNull()) {
            QJsonArray shotsArray = doc.array();
            for (const auto& shotJson : shotsArray) {
                QJsonObject shotObj = shotJson.toObject();
                if (!shotObj.contains("name")) {
                    QString shotName = Str().sprintf("SHOT_%04d", ++shotCount * 10).c_str();
                    shotObj["name"] = shotName;
                }
                addShotFromJson(shotObj, scene);
            }
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

            // Structure prompt, result and context in json
            QJsonObject contextJson;
            contextJson["sceneId"] = QString::fromStdString(scene.sceneId);
            contextJson["sceneName"] = QString::fromStdString(scene.name);
            contextJson["paragraphIndex"] = i;

            // Log prompt and result
            int tokenCount = 0; // TODO: Get from LlamaClient
            double cost = 0.0;  // TODO: Get from LlamaClient
            logger->logPromptAndResult("shots", prompt, callbackData.result, tokenCount, cost, contextJson);

            const QString cleanData = cleanJsonMessage(callbackData.result.c_str());
            QJsonDocument doc = QJsonDocument::fromJson(cleanData.toUtf8());
            if (!doc.isNull()) {
                QJsonArray shotsArray = doc.array();
                for (const auto& shotJson : shotsArray) {
                    QJsonObject shotObj = shotJson.toObject();
                    if(!shotObj.contains("name")){
                        QString shotName = Str().sprintf("SHOT_%04d", ++shotCount * 10).c_str();
                        shotObj["name"] = shotName;
                    }
                    addShotFromJson(shotObj, scene);
                }
            }
        }
    }

    QString cleanData = cleanJsonMessage(callbackData.result.c_str());

    // Ensure the "json" folder exists
    QDir dir;
    if (!dir.exists("json")) {
        dir.mkpath("json");
    }

    QString filePath = QString("json/%1_%2.json").arg(shotCount, 4, 10, QChar('0')).arg(scene.name.c_str());
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(cleanData.toUtf8()); // Write the raw JSON string
        file.close();
        Log::info("Saved JSON to %s\n", filePath.toStdString().c_str());
    } else {
        Log::error("Failed to save JSON to %s\n", filePath.toStdString().c_str());
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(cleanData.toUtf8(), &parseError);
    if (doc.isNull()) {





        Log::error("Invalid JSON response for shots\n");
        Log::info("Invalid JSON response for shots\n");

        Log::info("Invalid JSON response for shots: %s (Error at offset %d)\n",
                   parseError.errorString().toStdString().c_str(),
                   parseError.offset);
        Log::error("Invalid JSON response for shots: %s (Error at offset %d)\n",
                   parseError.errorString().toStdString().c_str(),
                   parseError.offset);

        Log().info() << "\nFixing json...\n\n";
        Log().print() << "\nFixing json...\n\n";

        // TODO TODO
        // Todo, use AI to try to fix issue with json
        QString repairPrompt = QString(
                                   "You returned an invalid JSON. The parser error was:\n"
                                   "%1 (offset: %2).\n\n"
                                   "Fix this JSON so that it is valid. If the error is caused by an extra '{', remove it. "
                                   "Preserve the structure as an array of objects."
                                   "\n\n"
                                   "Broken JSON:\n```json\n%3\n```"
                                   ).arg(parseError.errorString(), QString::number(parseError.offset), cleanData);

        llamaClient->clearSession(1);
        CallbackData callbackDataFix;
        if (!llamaClient->generateResponse(1, repairPrompt.toStdString(), streamCallback, finishedCallback, &callbackDataFix)) {
            Log::error("Failed to repair JSON for scene: %s\n", scene.name.c_str());
            return false;
        }

        QJsonObject contextJson;
        contextJson["sceneId"] = QString::fromStdString(scene.sceneId);
        contextJson["sceneName"] = QString::fromStdString(scene.name);
        contextJson["repair"] = true;
        logger->logPromptAndResult("shots_repair", repairPrompt.toStdString(), callbackDataFix.result, 0, 0.0, contextJson); // Log repair attempt

        QString fixedData = cleanJsonMessage(callbackDataFix.result.c_str());
        // create a prompt and
        // save again with fix suffix

        QString fixFilePath = filePath + ".fixed.json";
        QFile fixFile(fixFilePath);
        if (fixFile.open(QIODevice::WriteOnly)) {
            fixFile.write(fixedData.toUtf8());
            fixFile.close();
            Log::info("Saved repaired JSON to %s\n", fixFilePath.toStdString().c_str());
        }

        QJsonParseError fixedParseError;
        QJsonDocument fixedDoc = QJsonDocument::fromJson(fixedData.toUtf8(), &fixedParseError);

        if (fixedDoc.isNull()) {
            Log::error("Failed to repair JSON. Still invalid: %s (offset: %d)\n",
                       fixedParseError.errorString().toStdString().c_str(),
                       fixedParseError.offset);
            return false;
        }

        doc = fixedDoc;

        Log().info() << "Success Fix Json\n";
        Log().print() << "Success Fix Json\n";

        //return false;
    }

    QJsonArray shotsArray = doc.array();
    for (const auto& shotJson : shotsArray) {
        QJsonObject shotObj = shotJson.toObject();
        if(!shotObj.contains("name")){
            QString shotName = Str().sprintf("SHOT_%04d", ++shotCount * 10).c_str();
            shotObj["name"] = shotName;
        }
        addShotFromJson(shotObj, scene);


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

    QJsonObject contextJson;
    contextJson["contextType"] = "sequences";
    logger->logPromptAndResult("sequences", prompt, callbackData.result, 0, 0.0, contextJson); // TODO: Get tokens/cost

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

    QJsonObject contextJson;
    contextJson["contextType"] = "acts";
    logger->logPromptAndResult("acts", prompt, callbackData.result, 0, 0.0, contextJson); // TODO: Get tokens/cost

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

    if (type == "shots") {
        std::string prompt;
        //prompt = "Analyze the following script excerpt and break it into shots. For each shot, provide a JSON object with fields: name (format SHOT_XXXX, increment by 10), type (e.g., CLOSE, MEDIUM, WIDE), description, frameCount (estimate based on content), dialogue, fx, and notes. Ensure names follow ShotGrid conventions.\n\n"
         //      << content << "\n\nOutput as a JSON array.";


        prompt = R"(Analyze the following script excerpt and break it into shots. For each shot, provide a JSON object with the following fields:

- name (format SHOT_XXXX, increment by 10)
- type (e.g., CLOSE, MEDIUM, WIDE)
- description (clear summary of what's shown)
- frameCount (estimated based on content and pacing)
- timeOfDay (e.g., DAY, NIGHT, SUNSET)
- restore (true/false or empty if not applicable)
- dialogs: a list of dialog objects, each containing:
    - dialogNumber
    - character (name of the speaker)
    - dialogue (the spoken line)
- fx (e.g., sound effects, lighting changes, VFX)
- cameraTransition (e.g., CUT, FADE IN, DISSOLVE, WIPE, MATCH CUT)
- cameraMovement (e.g., STATIC, PAN LEFT, ZOOM IN, DOLLY IN, CRANE UP)
- emotion (e.g., TENSE, JOYFUL, CURIOUS — based on tone and mood)
- notes (storyboarding notes or direction)

Ensure shot names follow ShotGrid conventions.
Do not summarize. Every action and line of dialogue should be assigned to one or more shots.
Do not combine separate beats into one shot. Each character movement, reaction, or dialog should get its own shot if needed.

)" + content + R"(

Output as a JSON array.)";

        prompt = R"(Analyze the following script excerpt and break it into shots. For each shot, provide a JSON object with the following fields:

- name (format SHOT_XXXX, increment by 10)
- type (e.g., CLOSE, MEDIUM, WIDE)
- description
- frameCount (estimate based on content)
- timeOfDay (e.g., DAY, NIGHT, SUNSET)
- restore (true/false or empty if not applicable)
- transition (e.g., CUT, FADE IN, DISSOLVE)
- camera:
    - movement (e.g., PAN LEFT, STATIC)
    - framing (e.g., OVER-THE-SHOULDER, WIDE ANGLE)
- lighting (e.g., flickering neon, soft sunlight)
- audio:
    - ambient (e.g., street noise, birdsong)
    - sfx (list of sound effects)
- intent (narrative or emotional purpose of the shot)
- characters: a list of character objects with optional dialog, each containing:
    - name
    - emotion (current emotion of the character)
    - intent (character's emotional or narrative intent)
    - onScreen (true/false)
    - dialogNumber (if applicable)
    - dialogParenthetical (if applicable)
    - dialogue (if applicable, the spoken line)
- notes (any extra information for storyboarding, blocking, or animation)

Each new spoken line should create a new character entry in the list (even if from the same character).
Ensure shot names follow ShotGrid conventions.
Do not summarize. Every action and line of dialogue should be assigned to one or more shots.
Do not combine separate beats into one shot. Each character movement, reaction, or dialog should get its own shot if needed.

)" + content + R"(

Output as a JSON array.)";
        return prompt;
    } else if (type == "sequences") {
        std::stringstream prompt;
        prompt << "Analyze the following scene list and group scenes into sequences based on narrative or location continuity. For each sequence, provide a JSON object with fields: name (format SEQ_XXX), scenes (array of scene indices). Output as a JSON array.\n\n"
               << content;
        return prompt.str();
    } else if (type == "acts") {
        std::stringstream prompt;
        prompt << "Analyze the following scene list and group scenes into acts based on narrative structure (e.g., three-act structure). For each act, provide a JSON object with fields: name (format ACT_X), scenes (array of scene indices). Output as a JSON array.\n\n"
               << content;
        return prompt.str();
    }

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

std::vector<Shot>& ScriptBreakdown::getShots() {
    return shots;
}

std::vector<Sequence>& ScriptBreakdown::getSequences() {
    return sequences;
}

std::vector<CharacterDialog>& ScriptBreakdown::getCharacters(){
    return characters;
}

void ScriptBreakdown::streamCallback(const char* msg, void* user_data) {
    CallbackData* data = static_cast<CallbackData*>(user_data);
    data->result += msg;
    Log::info() << msg;
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ScriptBreakdown::finishedCallback(const char* msg, void* user_data) {
    CallbackData* data = static_cast<CallbackData*>(user_data);
    data->result = msg;
    //Log::info("Completed response: %s\n", msg);
    //QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ScriptBreakdown::printScenes() const {
    Log::info() << "== SCENES ==\n";
    for (const auto& scene : scenes) {
        Log::info() << "Scene: " << scene.name.c_str() << " with " << (int)scene.shots.size() << " shots\n";
        for (const auto& shot : scene.shots) {
            Log::info() << "  Shot: " << shot.name.c_str()
                        << ", Type: " << shot.type.c_str()
                        << ", Desc: " << shot.description.c_str()
                        << ", FrameCount: " << shot.frameCount
                        << "\n";
        }
    }
}

void ScriptBreakdown::printShots() const {
    Log::info() << "== SHOTS (Flat List) ==\n";
    for (const auto& shot : shots) {
        Log::info() << "Shot: " << shot.name.c_str()
                    << ", Type: " << shot.type.c_str()
                    << ", Desc: " << shot.description.c_str()
                    << ", FrameCount: " << shot.frameCount
                    << "\n";
    }
}

void ScriptBreakdown::printCharacters() const {
    Log::info() << "== CHARACTERS IN EPISODE ==\n";

    for (const auto& character : characters) {
        Log::info() << "      Character: " << character.name.c_str()
                    //<< ", Emotion: " << character.emotion.c_str()
                    //<< ", Intent: " << character.intent.c_str()
                    //<< ", OnScreen: " << (character.onScreen ? "true" : "false")
                    << ", Dialogue #: " << character.dialogNumber
                    //<< ", Parenthetical: " << character.dialogParenthetical.c_str()
                    << ", Line: " << character.dialogue.c_str() << "\n";
    }
}

void ScriptBreakdown::loadScene(const QString& sceneName, const QJsonObject &sceneObj, const QString filename) {
    if (!sceneObj.contains("sceneId") || !sceneObj.contains("shots")) {
        qWarning() << "Invalid scene JSON: missing sceneId or shots.";
        Log().info() << "Invalid scene JSON: missing sceneId or shots.";
        return;
    }

    Scene scene;
    scene.filename = filename.toUtf8().constData();
    scene.dirty = false;
    if(sceneObj.contains("sceneId") && !sceneObj["sceneId"].toString().isEmpty())
        scene.sceneId = sceneObj["sceneId"].toString().toStdString();
    if(sceneObj.contains("description") && !sceneObj["description"].toString().isEmpty())
        scene.description = sceneObj["description"].toString().toStdString();
    if(sceneObj.contains("heading") && !sceneObj["heading"].toString().isEmpty())
        scene.heading = sceneObj["heading"].toString().toStdString();
    if(!sceneName.isEmpty())
        scene.name = sceneName.toStdString();
    QJsonArray shotsArray = sceneObj["shots"].toArray();
    for (const QJsonValue &val : shotsArray) {
        if (!val.isObject())
            continue;

        QJsonObject shotObj = val.toObject();

        addShotFromJson(shotObj, scene);
    }

    scenes.push_back(scene);
}

void ScriptBreakdown::saveModifiedScenes(QString projectPath) {
    for (Scene& scene : scenes) {
        if (scene.dirty && !scene.filename.empty()) {
            QJsonArray shotsArray;

            for ( Shot& shot : scene.shots) {
                QJsonObject shotObj;
                shotObj["name"] = QString::fromStdString(shot.name);
                shotObj["type"] = QString::fromStdString(shot.type);
                shotObj["description"] = QString::fromStdString(shot.description);
                shotObj["fx"] = QString::fromStdString(shot.fx);
                shotObj["frameCount"] = shot.frameCount;
                shotObj["timeOfDay"] = QString::fromStdString(shot.timeOfDay);
                shotObj["restore"] = shot.restore;
                shotObj["transition"] = QString::fromStdString(shot.transition);
                shotObj["lighting"] = QString::fromStdString(shot.lighting);
                shotObj["intent"] = QString::fromStdString(shot.intent);
                shotObj["notes"] = QString::fromStdString(shot.notes);
                shotObj["uuid"] = QString::fromStdString(shot.uuid);
                shotObj["startTime"] = shot.startTime;
                shotObj["endTime"] = shot.endTime;


                QJsonObject cameraObj;
                cameraObj["movement"] = QString::fromStdString(shot.camera.movement);
                cameraObj["framing"] = QString::fromStdString(shot.camera.framing);
                shotObj["camera"] = cameraObj;

                QJsonObject audioObj;
                audioObj["ambient"] = QString::fromStdString(shot.audio.ambient);
                QJsonArray sfxArray;
                for ( auto& sfx : shot.audio.sfx)
                    sfxArray.append(QString::fromStdString(sfx));
                audioObj["sfx"] = sfxArray;
                shotObj["audio"] = audioObj;

                QJsonArray characterArray;
                for ( auto& character : shot.characters) {
                    QJsonObject charObj;
                    charObj["name"] = QString::fromStdString(character.name);
                    charObj["emotion"] = QString::fromStdString(character.emotion);
                    charObj["intent"] = QString::fromStdString(character.intent);
                    charObj["onScreen"] = character.onScreen;
                    charObj["dialogNumber"] = character.dialogNumber;
                    charObj["dialogParenthetical"] = QString::fromStdString(character.dialogParenthetical);
                    charObj["dialogue"] = QString::fromStdString(character.dialogue);
                    characterArray.append(charObj);
                }
                shotObj["characters"] = characterArray;

                QJsonArray panelArray;
                for ( auto& panel : shot.panels) {
                    QJsonObject panelObj;
                    panelObj["name"] = QString::fromStdString(panel.name);

                    panelObj["description"] = QString::fromStdString(panel.description);
                    panelObj["thumbnail"] = QString::fromStdString(panel.thumbnail);
                    panelObj["image"] = QString::fromStdString(panel.image);
                    panelObj["uuid"] = QString::fromStdString(panel.uuid);
                    panelObj["startTime"] = panel.startTime;
                    panelObj["durationTime"] = panel.durationTime;

                    QJsonArray layersArray;
                    for ( auto& layer : panel.layers) {
                        QJsonObject layerObj;
                        layerObj["uuid"] = QString::fromStdString(layer.uuid);
                        layerObj["name"] = QString::fromStdString(layer.name);
                        layerObj["thumbnail"] = QString::fromStdString(layer.thumbnail);
                        layerObj["imageFilePath"] = QString::fromStdString(layer.imageFilePath);
                        layerObj["opacity"] = layer.opacity;
                        layerObj["fx"] = QString::fromStdString(layer.fx);
                        layerObj["blendMode"] = QString::fromStdString(toString(layer.blendMode));
                        layerObj["visible"] = layer.visible;
                        layerObj["x"] = layer.x;
                        layerObj["y"] = layer.y;
                        layerObj["scale"] = layer.scale;
                        layerObj["rotation"] = layer.rotation;

                        // Save strokes
                        QJsonArray strokeArray;
                        for (const GameFusion::BezierCurve& path : layer.strokes) {
                            QJsonObject strokeObj;
                            path.toJson(strokeObj); // Serialize handles and strokeProperties
                            strokeArray.append(strokeObj);
                        }
                        layerObj["strokes"] = strokeArray;

                        // Keyframes
                        QJsonArray keyframesArray;
                        for ( auto& kf : layer.keyframes) {
                            QJsonObject kfObj;
                            kfObj["time"] = kf.time;
                            kfObj["x"] = kf.x;
                            kfObj["y"] = kf.y;
                            kfObj["scale"] = kf.scale;
                            kfObj["rotation"] = kf.rotation;
                            kfObj["opacity"] = kf.opacity;
                            kfObj["easing"] = static_cast<int>(kf.easing);
                            kfObj["bezierControl1X"] = kf.bezierControl1.x();
                            kfObj["bezierControl1Y"] = kf.bezierControl1.y();
                            kfObj["bezierControl2X"] = kf.bezierControl2.x();
                            kfObj["bezierControl2Y"] = kf.bezierControl2.y();

                            keyframesArray.append(kfObj);
                        }

                        layerObj["keyframes"] = keyframesArray;
                        layersArray.append(layerObj);
                    }

                    panelObj["layers"] = layersArray;
                    panelArray.append(panelObj);
                }
                shotObj["panels"] = panelArray;

                QJsonArray framesArray;
                for (const auto& frame : shot.cameraFrames) {
                    QJsonObject cameraFrameObj;
                    cameraFrameObj["name"] = QString::fromStdString(frame.name);
                    cameraFrameObj["uuid"] = QString::fromStdString(frame.uuid);
                    cameraFrameObj["time"] = frame.time;
                    cameraFrameObj["x"] = frame.x;
                    cameraFrameObj["y"] = frame.y;
                    cameraFrameObj["zoom"] = frame.zoom;
                    cameraFrameObj["rotation"] = frame.rotation;
                    cameraFrameObj["panelUuid"] = QString::fromStdString(frame.panelUuid);
                    cameraFrameObj["frameOffset"] = frame.frameOffset;
                    cameraFrameObj["easing"] = toString(frame.easing).c_str(); //todo get text val;

                    framesArray.append(cameraFrameObj);
                }
                shotObj["cameraFrames"] = framesArray;

                shotsArray.append(shotObj);
            }

            QJsonDocument doc(shotsArray);
            QString filename = projectPath + "/scenes/" + scene.filename.c_str();
            QFile file(filename);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(doc.toJson(QJsonDocument::Indented));
                file.close();
                scene.dirty = false;
                qDebug() << "Saved scene to" << QString::fromStdString(scene.filename);
                Log().info() << "Saved scene to" << scene.filename.c_str() << "\n";
            } else {
                qWarning() << "Failed to write scene to" << filename;
                Log().info() << "Failed to write scene to" << filename.toUtf8().constData() << "\n";
            }
        }
    }
}

void ScriptBreakdown::addCameraFrame(const CameraFrame& frame) {
    for(GameFusion::Scene &scene: scenes)
        for(GameFusion::Shot & shot: scene.shots)
            for(GameFusion::Panel & panel: shot.panels)
                if(panel.uuid == frame.panelUuid) {
                    shot.cameraFrames.push_back(frame);
                    scene.dirty = true;
                    return;
                }
}

bool ScriptBreakdown::updateCameraFrame(const CameraFrame& frame) {

    for(GameFusion::Scene &scene: scenes)
        for(GameFusion::Shot & shot: scene.shots)
            for(GameFusion::CameraFrame &cameraFrame : shot.cameraFrames)
                if(cameraFrame.uuid == frame.uuid){
                    cameraFrame = frame;
                    scene.dirty = true;
                    return true;
                }
    /*
    auto it = std::find_if(cameraFrames.begin(), cameraFrames.end(),
                           [&](const CameraFrame& f) { return f.uuid == frame.uuid; });

    if (it != cameraFrames.end()) {
        *it = frame;
        return true;
    }*/
    return false;
}

bool ScriptBreakdown::deleteCameraFrame(const std::string& uuid) {
    for (GameFusion::Scene& scene : scenes) {
        for (GameFusion::Shot& shot : scene.shots) {
            auto it = std::remove_if(shot.cameraFrames.begin(), shot.cameraFrames.end(),
                                     [&](const GameFusion::CameraFrame& frame) {
                                         return frame.uuid == uuid;
                                     });

            if (it != shot.cameraFrames.end()) {
                shot.cameraFrames.erase(it, shot.cameraFrames.end());
                return true;
            }
        }
    }
    return false;
}

}

