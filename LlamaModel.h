#ifndef LLAMAMODEL_H
#define LLAMAMODEL_H

#include <QObject>
#include <QString>


class LlamaClient;

class LlamaModel : public QObject {
    Q_OBJECT

public:

    struct Attachment {
        QString type;
        QString data;

        Attachment(const QString &type = "", const QString &data = "")
            : type(type), data(data) {}
    };

    LlamaModel(QObject *parent = nullptr, LlamaClient *llamaClient=nullptr);
    ~LlamaModel();

    QString generateResponse(const QString &prompt,
                             std::function<void(const QString &)> callback,
                             std::function<void(const QString &)> callbackFinished);

    QString generateResponse(int sessionId, const QString &prompt,
                             std::function<void(const QString &)> callback,
                             std::function<void(const QString &)> callbackFinished);

    QString getContextInfo();

signals:
    void responseGenerated(const QString &piece); // Signal to emit each piece of the response

private:

    QString error_;

    static LlamaClient *client; // possibly use smart pointer here
};

#endif
