#include "taskwidget.h"
#include "ui_taskwidget.h"
#include "configstore.h"
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QHBoxLayout>

namespace {
constexpr const char kDefaultModelLabel[] = "Default";
}

TaskWidget::TaskWidget(QWidget *parent)
    : QWidget(parent)
      , ui(new Ui::TaskWidget) {
    ui->setupUi(this);
    ui->radioInsert->setChecked(true);

    connect(ui->lineEditName, &QLineEdit::textChanged, this, &TaskWidget::configChanged);
    connect(ui->textEditPrompt, &QTextEdit::textChanged, this, &TaskWidget::configChanged);
    connect(ui->radioInsert, &QRadioButton::toggled, this, &TaskWidget::configChanged);
    connect(ui->radioWindow, &QRadioButton::toggled, this, &TaskWidget::configChanged);

    connect(ui->spinBoxMaxTokens, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TaskWidget::configChanged);
    connect(ui->doubleSpinBoxTemperature, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TaskWidget::configChanged);
    connect(ui->comboBoxModel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TaskWidget::configChanged);

    ui->toolButtonRefreshModels->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    ui->toolButtonRefreshModels->setToolTip(tr("Refresh models"));
    ui->toolButtonRefreshModels->setAutoRaise(true);
    connect(ui->toolButtonRefreshModels, &QToolButton::clicked,
            this, &TaskWidget::refreshModelsRequested);

    ui->horizontalLayoutModel->setStretch(0, 0);
    ui->horizontalLayoutModel->setStretch(1, 1);
    ui->horizontalLayoutModel->setStretch(2, 0);

    ui->comboBoxModel->addItem(tr(kDefaultModelLabel), QString());
}

TaskWidget::~TaskWidget() {
    delete ui;
}

QString TaskWidget::name() const {
    return ui->lineEditName->text();
}

QString TaskWidget::prompt() const {
    return ui->textEditPrompt->toPlainText();
}

QString TaskWidget::modelName() const {
    const QVariant data = ui->comboBoxModel->currentData();
    if (data.isValid())
        return data.toString();
    const QString text = ui->comboBoxModel->currentText();
    if (text == QLatin1String(kDefaultModelLabel))
        return QString();
    return text;
}

bool TaskWidget::insertMode() const {
    return ui->radioInsert->isChecked();
}

void TaskWidget::setName(const QString &name) {
    ui->lineEditName->setText(name);
}

void TaskWidget::setPrompt(const QString &prompt) {
    ui->textEditPrompt->setPlainText(prompt);
}

void TaskWidget::setModelName(const QString &modelName) {
    QString normalized = modelName;
    if (normalized == QLatin1String(kDefaultModelLabel))
        normalized.clear();

    QSignalBlocker blocker(ui->comboBoxModel);
    if (ui->comboBoxModel->count() == 0)
        ui->comboBoxModel->addItem(tr(kDefaultModelLabel), QString());

    int index = normalized.isEmpty()
        ? ui->comboBoxModel->findData(QString())
        : ui->comboBoxModel->findData(normalized);
    if (index < 0 && !normalized.isEmpty()) {
        ui->comboBoxModel->addItem(normalized, normalized);
        index = ui->comboBoxModel->findData(normalized);
    }
    if (index < 0)
        index = 0;
    ui->comboBoxModel->setCurrentIndex(index);
}

void TaskWidget::setInsertMode(bool insert) {
    if (insert)
        ui->radioInsert->setChecked(true);
    else
        ui->radioWindow->setChecked(true);
}

int TaskWidget::maxTokens() const {
    return ui->spinBoxMaxTokens->value();
}

double TaskWidget::temperature() const {
    return ui->doubleSpinBoxTemperature->value();
}

void TaskWidget::setMaxTokens(int tokens) {
    ui->spinBoxMaxTokens->setValue(tokens);
}

void TaskWidget::setTemperature(double temp) {
    ui->doubleSpinBoxTemperature->setValue(temp);
}

void TaskWidget::setAvailableModels(const QStringList &models) {
    const QString selected = modelName();

    QSignalBlocker blocker(ui->comboBoxModel);
    ui->comboBoxModel->clear();
    ui->comboBoxModel->addItem(tr(kDefaultModelLabel), QString());
    for (const QString &model : models)
        ui->comboBoxModel->addItem(model, model);

    if (!selected.isEmpty() && ui->comboBoxModel->findData(selected) < 0)
        ui->comboBoxModel->addItem(selected, selected);

    int index = selected.isEmpty()
        ? ui->comboBoxModel->findData(QString())
        : ui->comboBoxModel->findData(selected);
    if (index < 0)
        index = 0;
    ui->comboBoxModel->setCurrentIndex(index);
}

void TaskWidget::setRefreshEnabled(bool enabled) {
    ui->toolButtonRefreshModels->setEnabled(enabled);
}

void TaskWidget::setResponseWindowSize(const QSize &size) {
    if (!size.isValid())
        return;
    responseWidth = size.width();
    responseHeight = size.height();
}

void TaskWidget::setResponseZoom(int zoom) {
    responseZoomValue = zoom;
}

TaskDefinition TaskWidget::toDefinition() const {
    TaskDefinition def;
    def.name = name();
    def.prompt = prompt();
    def.modelName = modelName();
    def.insertMode = insertMode();
    def.maxTokens = maxTokens();
    def.temperature = temperature();
    def.responseWidth = responseWidth;
    def.responseHeight = responseHeight;
    def.responseZoom = responseZoomValue;
    return def;
}

void TaskWidget::applyDefinition(const TaskDefinition &definition) {
    setName(definition.name);
    setPrompt(definition.prompt);
    setModelName(definition.modelName);
    setInsertMode(definition.insertMode);
    setMaxTokens(definition.maxTokens);
    setTemperature(definition.temperature);
    responseWidth = definition.responseWidth;
    responseHeight = definition.responseHeight;
    responseZoomValue = definition.responseZoom;
}
