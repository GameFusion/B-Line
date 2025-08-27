#include "PromptLogger.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "Log.h"

namespace GameFusion {

PromptLogger::PromptLogger(QObject* parent) : QObject(parent), serverUrl("http://localhost:50000"), oauthToken("your-secret-token-here"), projectPath("") {}

void PromptLogger::setServerConfig(const QString& url, const QString& token) {
    serverUrl = url;
    oauthToken = token;
    Log::info() << "PromptLogger: Set server config: URL=" << url.toStdString().c_str() << ", Token=" << (token.isEmpty() ? "empty" : "set");
}

void PromptLogger::logPromptAndResult(const std::string& category, const QString& type, const QString& prompt, const QString& result,
                        int tokens, double cost, const QJsonObject& context,
                        const QString& userId, const QString& projectId,
                        const QString& sessionId, const QUuid& correlationId,
                        const QString& environment, const QJsonObject& errorDetails,
                        int inputSize, int outputSize, const QJsonObject& latencyBreakdown,
                                      const QJsonObject& modelParams){
    QJsonObject logData;
    logData["category"] = QString::fromStdString(category);
    logData["type"] = type;
    logData["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    logData["user"] = QString::fromStdString("default_user"); // TODO: Replace with actual user
    logData["version"] = "1.0"; // TODO: Replace with app/model version
    logData["llm"] = QString::fromStdString("llama"); // TODO: Get from LlamaClient
    logData["prompt"] = prompt;
    logData["response"] = result;
    logData["tokens"] = tokens;
    logData["cost"] = cost;
    if (!context.isEmpty()) {
        logData["context"] = context;
    }

    logData["userId"] = userId;
    logData["projectId"] = projectId;
    logData["sessionId"] = sessionId;
    logData["correlationId"] = correlationId.toString();
    logData["environment"] = environment;
    if (!errorDetails.isEmpty()) {
        logData["errorDetails"] = errorDetails;
    }
    logData["inputSize"] = inputSize;
    logData["outputSize"] = outputSize;
    logData["latencyBreakdown"] = latencyBreakdown;
    logData["modelParams"] = modelParams;
}

void PromptLogger::logPromptAndResult(const std::string& category, const std::string& type, const std::string& prompt, const std::string& result,
                                      int tokens, double cost, const QJsonObject& context) {
    QJsonObject logData;
    logData["category"] = QString::fromStdString(category);
    logData["type"] = QString::fromStdString(type);
    logData["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    logData["user"] = QString::fromStdString("default_user"); // TODO: Replace with actual user
    logData["version"] = "1.0"; // TODO: Replace with app/model version
    logData["llm"] = QString::fromStdString("llama"); // TODO: Get from LlamaClient
    logData["prompt"] = QString::fromStdString(prompt);
    logData["response"] = QString::fromStdString(result);
    logData["tokens"] = tokens;
    logData["cost"] = cost;
    if (!context.isEmpty()) {
        logData["context"] = context;
    }

    QJsonDocument doc(logData);
    bool compact = false;
    QString jsonString;

    if(compact)
        jsonString = doc.toJson(QJsonDocument::Compact);
    else
        // Pretty (indented) JSON
        jsonString = doc.toJson(QJsonDocument::Indented);

    // Determine logs directory based on projectPath
    QString logsDir = projectPath.isEmpty() ? "logs" : projectPath + "/logs";
    QString timestampStr = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd_HH-mm-ss");
    QString localRemote = (serverUrl.isEmpty() || oauthToken.isEmpty()) ? "local" : "remote";
    QString filename = QString("%1/%2_%3_%4_v%5_%6.json")
                           .arg(logsDir)
                           .arg(QString::fromStdString(type))
                           .arg(timestampStr)
                           .arg("default_user")
                           .arg("1")
                           .arg(localRemote);

    // Create logs directory
    QDir dir;
    if (!dir.exists(logsDir)) {
        if (!dir.mkpath(logsDir)) {
            Log::error() << "PromptLogger: Failed to create logs directory: " << logsDir.toStdString().c_str();
            return;
        }
    }

    // Save locally
    QFile localFile(filename);
    if (localFile.open(QIODevice::WriteOnly)) {
        localFile.write(jsonString.toUtf8());
        localFile.close();
        Log::info() << "PromptLogger: Saved prompt log locally: " << filename.toStdString().c_str();
    } else {
        Log::error() << "PromptLogger: Failed to save prompt log: " << filename.toStdString().c_str();
    }

    // Remote send if configured
    if (!serverUrl.isEmpty() && !oauthToken.isEmpty()) {
        QNetworkAccessManager* manager = new QNetworkAccessManager(this);
        QNetworkRequest request(QUrl(serverUrl + "/api/v1/prompt-history"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + oauthToken).toUtf8());

        QNetworkReply* reply = manager->post(request, jsonString.toUtf8());
        connect(reply, &QNetworkReply::finished, this, [reply, filename]() {
            if (reply->error() == QNetworkReply::NoError) {
                Log::info() << "PromptLogger: Successfully sent prompt log to server: " << filename.toStdString().c_str();
            } else {
                QString replyError = reply->errorString();
                Log::error() << "PromptLogger: Failed to send prompt log to server: " << replyError.toStdString().c_str();
            }
            reply->deleteLater();
        });
    }
}

void PromptLogger::setProjectPath(const QString& path) {
    projectPath = path;
    if (projectPath.isEmpty()) {
        Log::warning() << "PromptLogger: Project path set to empty, logs will use current directory";
    } else {
        Log::info() << "PromptLogger: Set project path: " << path.toStdString().c_str();
    }
}

} // namespace GameFusion
