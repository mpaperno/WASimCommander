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

#include <QWidget>
#include "ui_RequestsExport.h"
#include "DataComboBox.h"

namespace WASimUiNS {

class RequestsModel;

class RequestsExportWidget : public QWidget
{
	Q_OBJECT

public:
	explicit RequestsExportWidget(RequestsModel *model, QWidget *parent = nullptr);
	RequestsModel *model() const;

public Q_SLOTS:
	void setLastUsedFile(const QString &fn) { m_lastFile = fn; };
	void exportAll();
	void exportSelected();

Q_SIGNALS:
	void lastUsedFileChanged(const QString &fn);

protected:
	void closeEvent(QCloseEvent *) override;

private:
	void setModel(RequestsModel *model);
	void exportRecords(bool all = true);
	void ensureDefaultValues();
	void updateBulk();
	void regenIds();
	void regenNames();
	void clearForm();
	void saveSettings() const;
	void loadSettings();

	QString m_lastFile;
	RequestsModel *m_model = nullptr;
	Ui::RequestsExport ui;
};

}  // WASimUiNS
