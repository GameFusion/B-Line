#ifndef PROMPT_LOGGER_H
#define PROMPT_LOGGER_H

// Andreas Carlen June 12 2025

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <string>

namespace GameFusion {

class PromptLogger : public QObject {
    Q_OBJECT
public:
    enum class Type {
        Text,
        ImageGeneration,
        Movie,
        Multimodal,
        Diffusion,
        Ocr,
        ImageAnalysis,
        AudioGeneration,
        VideoAnalysis,
        CodeGeneration,
        DataAnalysis,
        Other
    };

    explicit PromptLogger(QObject* parent = nullptr);
    void setServerConfig(const QString& url, const QString& token);
    void logPromptAndResult(const std::string& category, const std::string& type, const std::string& prompt, const std::string& result,
                            int tokens, double cost, const QJsonObject& context = QJsonObject());

    void logPromptAndResult(const std::string& category, const QString& type, const QString& prompt, const QString& response,
                            int tokens, double cost, const QJsonObject& context,
                            const QString& userId, const QString& projectId,
                            const QString& sessionId, const QUuid& correlationId,
                            const QString& environment, const QJsonObject& errorDetails,
                            int inputSize, int outputSize, const QJsonObject& latencyBreakdown,
                            const QJsonObject& modelParams);

    void setProjectPath(const QString& path);

private:
    QString serverUrl;
    QString oauthToken;
    QString projectPath; // Store project path for logs
};

} // namespace GameFusion

#endif // PROMPT_LOGGER_H
