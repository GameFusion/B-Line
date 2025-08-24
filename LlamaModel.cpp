#include "LlamaModel.h"
#include "LlamaClient.h"
#include "Log.h"

#include <QApplication>

using GameFusion::Log;

LlamaClient* LlamaModel::client = nullptr; // Initialize static member

LlamaModel::LlamaModel(QObject *parent, LlamaClient *llamaClient)
{
    client = llamaClient;
    connect(this, &LlamaModel::responseGenerated, this, [this](const QString &piece) {
        Log().info() << piece.toUtf8().constData();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    });
}

LlamaModel::~LlamaModel() {

    Log().info() << "Unloading LlamaModel\n";
    //if(client)
    //    delete client;
}

QString LlamaModel::generateResponse(const QString &input_prompt,
                                     std::function<void(const QString &)> streamCallback,
                                     std::function<void(const QString &)> callbackFinished) {

    int sessionId=0;
    return generateResponse(sessionId, input_prompt, streamCallback, callbackFinished);
}

QString LlamaModel::generateResponse(int sessionId, const QString &input_prompt,
                                     std::function<void(const QString &)> streamCallback,
                                     std::function<void(const QString &)> callbackFinished){

    // Ensure the model is loaded with the correct backend before generating the response
    if (!client) {
        return "Error: Model not loaded.";
    }
    /*
    std::string result = client->generateResponse(input_prompt.toUtf8().constData(),
      [](const char* msg) {
          Log().info() << msg;
          Globals::processEventsExcludeInput();
          //callback(QString(msg));  // Trigger the provided callback with the message
      },
      [](const char* msg) {
          Log().print() << "Finished Callback: " << msg << "\n";
          //callbackFinished(QString(msg));  // Trigger the finished callback
      }
      );*/

    // Create a shared pointer to keep the function alive
    // Store callbacks in shared pointers to maintain their lifetime
    auto streamCallbackPtr = std::make_shared<std::function<void(const QString&)>>(streamCallback);
    auto finishedCallbackPtr = std::make_shared<std::function<void(const QString&)>>(callbackFinished);

    // Pass the shared pointer's raw pointer as user data
    void* userFunction = static_cast<void*>(streamCallbackPtr.get());
    void* finishedFunction = static_cast<void*>(finishedCallbackPtr.get());

    typedef struct FuntionPointers{
        void *userFunction;
        void *finishedFunction;
    } FuntionPointers;

    FuntionPointers funtionPointers;

    funtionPointers.userFunction = userFunction;
    funtionPointers.finishedFunction = finishedFunction;

    void* userFuntion = static_cast<void*>(&streamCallback);
    // Generate the response using the client
    std::string resultString;

    bool resultStatus = client->generateResponse(sessionId, input_prompt.toUtf8().constData(),
                                                 [](const char* msg, void *funtionPointers) {
                                                     void *userData = ((FuntionPointers*)funtionPointers)->userFunction;
                                                     if (userData) {
                                                         auto* callbackPtr = static_cast<std::function<void(const QString&)>*>(userData);
                                                         if (callbackPtr && *callbackPtr) {
                                                             (*callbackPtr)(QString::fromUtf8(msg));
                                                         }
                                                     }
                                                 },
                                                 [](const char* msg, void *funtionPointers) {
                                                     void *userData = ((FuntionPointers*)funtionPointers)->finishedFunction;
                                                     if (userData) {
                                                         auto* callbackPtr = static_cast<std::function<void(const QString&)>*>(userData);
                                                         if (callbackPtr && *callbackPtr) {
                                                             (*callbackPtr)(QString::fromUtf8(msg));
                                                         }
                                                     }
                                                 },
                                                 &funtionPointers
                                                 );

    return QString::fromStdString(resultString);
}

QString LlamaModel::getContextInfo(){
    if(client)
    {
        std::string info = client->getContextInfo();
        return QString::fromStdString(info);
    }
    return "Llama client is unloaded";
}

QString LlamaModel::getModelFile() {
    if(client){
        return client->getModelFile().c_str();
    }
    return "Llama client is unloaded";
}

QString LlamaModel::getVendor() {
    return "LlamaEngine"; // alternativly this could be an interface to OpenAI, grok, claude, other
}

QString LlamaModel::getLocation() {
    return "local";
}

QString LlamaModel::getApiTech() {
    return "LlamaEngine";
}

int LlamaModel::getLatestTokenCount() {
    if(client)
        return client->getLatestTokenCount();
    return 0;
}

double LlamaModel::getLatestCost() {
    if(client)
        return client->getLatestCost();
    return 0.0;
}
