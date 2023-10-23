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
#include "ui_WASimUI.h"
#include "client/WASimClient.h"

class WASimUIPrivate;

class WASimUI : public QMainWindow
{
	Q_OBJECT

public:
		WASimUI(QWidget *parent = Q_NULLPTR);
		~WASimUI();

signals:
	void clientEvent(const WASimCommander::Client::ClientEvent &ev);
	void commandResultReady(const WASimCommander::Command &c);
	void listResults(const WASimCommander::Client::ListResult &list);
	void dataResultReady(const WASimCommander::Client::DataRequestRecord &r);

protected:
	void closeEvent(QCloseEvent *) override;
	void onClientEvent(const WASimCommander::Client::ClientEvent &ev);
	void onListResults(const WASimCommander::Client::ListResult &list);

private:
	Ui::WASimUIClass ui;
	QScopedPointer<WASimUIPrivate> d;
	friend class WASimUIPrivate;
};
