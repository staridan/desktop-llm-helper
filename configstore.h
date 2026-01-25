#ifndef CONFIGSTORE_H
#define CONFIGSTORE_H

#include <QJsonDocument>
#include <QList>
#include <QString>

struct AppSettings {
    QString apiEndpoint;
    QString modelName;
    QString apiKey;
    QString proxy;
    QString hotkey;
    int maxChars = 0;
};

struct TaskDefinition {
    QString name;
    QString prompt;
    bool insertMode = true;
    int maxTokens = 300;
    double temperature = 0.5;
    int responseWidth = 600;
    int responseHeight = 200;
    int responseZoom = 0;
};

struct AppConfig {
    AppSettings settings;
    QList<TaskDefinition> tasks;
};

class ConfigStore {
public:
    static QString configFilePath();
    static AppConfig defaultConfig();
    static AppConfig fromJson(const QJsonDocument &doc, bool *ok = nullptr);
    static QJsonDocument toJson(const AppConfig &config);
};

#endif // CONFIGSTORE_H
