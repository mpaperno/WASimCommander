#pragma once

#include <QMainWindow>
#include <QSettings>
#include "ui_LogConsole.h"
#include "LogRecordsModel.h"
#include "WASimCommander.h"

class WASimCommander::Client::WASimClient;

class LogConsole : public QWidget
{
	Q_OBJECT

public:
	LogConsole(QWidget *parent = nullptr);
	~LogConsole();

	void setClient(WASimCommander::Client::WASimClient *c);
	WASimUiNS::LogRecordsModel *getModel() const;

public Q_SLOTS:
	void saveSettings(QSettings &set) const;
	void loadSettings(QSettings &set);
	void logMessage(int level, const QString &msg) const;

signals:
	void logMessageReady(const WASimCommander::LogRecord &r, quint8 src) const;

private:
	Ui::LogConsole ui;
	WASimUiNS::LogRecordsModel *logModel = nullptr;
	WASimCommander::Client::WASimClient *wsClient = nullptr;
};
