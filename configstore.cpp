#include "configstore.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>

namespace {
TaskDefinition taskFromJson(const QJsonObject &obj) {
    TaskDefinition task;
    task.name = obj.value("name").toString();
    task.prompt = obj.value("prompt").toString();
    task.insertMode = obj.value("insert").toBool(true);
    task.maxTokens = obj.value("maxTokens").toInt(300);
    task.temperature = obj.value("temperature").toDouble(0.5);
    const int width = obj.value("responseWidth").toInt(600);
    const int height = obj.value("responseHeight").toInt(200);
    task.responseWidth = width > 0 ? width : 600;
    task.responseHeight = height > 0 ? height : 200;
    task.responseZoom = obj.value("responseZoom").toInt(0);
    return task;
}

QJsonObject taskToJson(const TaskDefinition &task) {
    return QJsonObject{
        {"name", task.name},
        {"prompt", task.prompt},
        {"insert", task.insertMode},
        {"maxTokens", task.maxTokens},
        {"temperature", task.temperature},
        {"responseWidth", task.responseWidth},
        {"responseHeight", task.responseHeight},
        {"responseZoom", task.responseZoom}
    };
}
} // namespace

QString ConfigStore::configFilePath() {
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                              + QDir::separator()
                              + QCoreApplication::applicationName();
    QDir().mkpath(configDir);
    return configDir + QDir::separator() + "config.json";
}

AppConfig ConfigStore::defaultConfig() {
    AppConfig config;
    config.settings.apiEndpoint = "https://api.openai.com/v1/chat/completions";
    config.settings.modelName = "gpt-4.1-mini";
    config.settings.apiKey = "YOUR_API_KEY_HERE";
    config.settings.proxy = "";
    config.settings.hotkey = "Win+Z";
    config.settings.maxChars = 1000;

    TaskDefinition task;
    task.name = QString::fromUtf8("\xF0\x9F\xA7\xA0 Explane");
    task.prompt = "Explain the meaning of what will be written. The explanation should be brief, in 1-4 sentences.";
    task.insertMode = false;
    task.maxTokens = 300;
    task.temperature = 0.5;
    config.tasks.append(task);
    return config;
}

AppConfig ConfigStore::fromJson(const QJsonDocument &doc, bool *ok) {
    if (ok)
        *ok = false;

    AppConfig config;
    if (doc.isNull() || !doc.isObject())
        return config;

    const QJsonObject root = doc.object();
    const QJsonObject settings = root.value("settings").toObject();

    config.settings.apiEndpoint = settings.value("apiEndpoint").toString();
    config.settings.modelName = settings.value("modelName").toString();
    config.settings.apiKey = settings.value("apiKey").toString();
    config.settings.proxy = settings.value("proxy").toString();
    config.settings.hotkey = settings.value("hotkey").toString();
    config.settings.maxChars = settings.value("maxChars").toInt();

    const QJsonArray tasksArray = root.value("tasks").toArray();
    for (const QJsonValue &value : tasksArray) {
        if (value.isObject())
            config.tasks.append(taskFromJson(value.toObject()));
    }

    if (ok)
        *ok = true;
    return config;
}

QJsonDocument ConfigStore::toJson(const AppConfig &config) {
    QJsonObject settings{
        {"apiEndpoint", config.settings.apiEndpoint},
        {"modelName", config.settings.modelName},
        {"apiKey", config.settings.apiKey},
        {"proxy", config.settings.proxy},
        {"hotkey", config.settings.hotkey},
        {"maxChars", config.settings.maxChars}
    };

    QJsonArray tasksArray;
    for (const TaskDefinition &task : config.tasks)
        tasksArray.append(taskToJson(task));

    QJsonObject root{
        {"settings", settings},
        {"tasks", tasksArray}
    };

    return QJsonDocument(root);
}
