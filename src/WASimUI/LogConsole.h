/*
This file is part of the WASimCommander project.
https://github.com/mpaperno/WASimCommander

COPYRIGHT: (c) Maxim Paperno; All Rights Reserved.

This file may be used under the terms of the GNU General Public License (GPL)
as published by the Free Software Foundation, either version 3 of the Licenses,
or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU GPL is included with this project
and is also available at <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <QMainWindow>
#include <QSettings>
#include "ui_LogConsole.h"
#include "LogRecordsModel.h"
#include "WASimCommander.h"

class WASimCommander::Client::WASimClient;

namespace WASimUiNS {

class LogConsole : public QWidget
{
	Q_OBJECT

public:
	LogConsole(QWidget *parent = nullptr);
	~LogConsole();

	void setClient(WASimCommander::Client::WASimClient *c);
	WASimUiNS::LogRecordsModel *getModel() const;

public Q_SLOTS:
	void saveSettings() const;
	void loadSettings();
	void logMessage(int level, const QString &msg) const;

signals:
	void logMessageReady(const WASimCommander::LogRecord &r, quint8 src) const;

private:
	Ui::LogConsole ui;
	WASimUiNS::LogRecordsModel *logModel = nullptr;
	WASimCommander::Client::WASimClient *wsClient = nullptr;
};

}
