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

#include <QCompleter>
#include <QMetaEnum>
#include <QSqlError>
#include <QSqlTableModel>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimeZone>
#include <QDebug>

#include "Widgets.h"

namespace WASimUiNS {
	namespace DocImports
{
Q_NAMESPACE

const QString DB_NAME = QStringLiteral("MSFS_SDK_Doc_Import");

enum RecordType : quint8 { Unknown, SimVars, KeyEvents, SimVarUnits };
Q_ENUM_NS(RecordType)

static const QVector<const char *> RecordTypeNames {
	"Unknown", "Simulator Variables", "Key Events", "Variable Units"
};
static QString recordTypeName(RecordType type) {
	return RecordTypeNames.value(type, RecordTypeNames[RecordType::Unknown]);
}

static bool createConnection()
{
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", DB_NAME);
	db.setDatabaseName(DB_NAME + ".sqlite3");
	db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_ENABLE_SHARED_CACHE;QSQLITE_ENABLE_REGEXP=30");
	if (db.open())
		return true;
	qDebug() << "Failed to open database" << DB_NAME << ".sqlite3 with:" << db.lastError();
	db = QSqlDatabase();
	QSqlDatabase::removeDatabase(DB_NAME);
	return false;
}

static QSqlDatabase getConnection(bool open = true)
{
	if (!QSqlDatabase::contains(DB_NAME)) {
		if (!createConnection())
			return QSqlDatabase::database();
	}
	return QSqlDatabase::database(DB_NAME, open);
}


// ----------------------------------------
// DocImportsModel
// ----------------------------------------

class DocImportsModel : public QSqlTableModel
{
	Q_OBJECT

public:
	DocImportsModel(QObject *parent = nullptr, RecordType type = RecordType::Unknown)
		: QSqlTableModel(parent, getConnection())
	{
		setEditStrategy(QSqlTableModel::OnManualSubmit);
		//qDebug() << "Opened DB" << database().connectionName() << database().databaseName();
		if (type != RecordType::Unknown)
			setRecordType(type);
	}

	RecordType recordType() const { return m_recordType; }

	QDateTime lastDataUpdate(RecordType type, QString *fromUrl = nullptr)
	{
		QDateTime lu;
		if (type == RecordType::Unknown)
			return lu;

		const QString table(QMetaEnum::fromType<RecordType>().valueToKey(type));
		QSqlQuery qry("SELECT LastUpdate, FromURL FROM ImportMeta WHERE TableName = ? ", database());
		qry.addBindValue(table);
		qry.exec();
		if (qry.next()) {
			lu = qry.value(0).toDateTime();
			lu.setTimeZone(QTimeZone::utc());
			lu = lu.toLocalTime();
			if (fromUrl)
				*fromUrl = qry.value(1).toString();
		}
		else if (qry.lastError().isValid()) {
			qDebug() << "ImportMeta query for table" << table << "failed with:" << qry.lastError();
		}
		else {
			qDebug() << "ImportMeta query for table" << table << "returned no results.";
		}
		qry.finish();
		return lu;
	}

	void setRecordType(RecordType type)
	{
		m_recordType = type;
		QString table;
		switch (type) {
			case RecordType::SimVars:
			case RecordType::KeyEvents:
			case RecordType::SimVarUnits:
				table = QString(QMetaEnum::fromType<RecordType>().valueToKey(type));
				break;

			default:
				return;
		}

		setTable(table);
		select();

		//setQuery("SELECT * FROM " + table, m_db);
		if (lastError().isValid()) {
			qDebug() << "Query for table" << table << "failed with: " << lastError();
			return;
		}

		removeColumnName("Multiplayer");
		removeColumnName("Deprecated");
	}

	void removeColumnName(const QString &name) {
		const int col = fieldIndex(name);
		if (col > -1)
			removeColumn(col);
	}

	Qt::ItemFlags flags(const QModelIndex &idx) const override {
		if (idx.isValid())
			return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
		return Qt::NoItemFlags;
	}

	QVariant data(const QModelIndex &idx, int role) const override {
		if (role == Qt::ToolTipRole)
			role = Qt::DisplayRole;
		return QSqlTableModel::data(idx, role);
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
		if (role == Qt::EditRole || role == Qt::ToolTipRole)
			return QSqlTableModel::headerData(section, orientation, Qt::DisplayRole);
		return QSqlTableModel::headerData(section, orientation, role);
	}

private:
	RecordType m_recordType = RecordType::Unknown;

};


// ----------------------------------------
// NameCompleter
// ----------------------------------------

class NameCompleter : public QCompleter
{
	Q_OBJECT

public:
	NameCompleter(RecordType recordType, QObject *parent = nullptr) :
		QCompleter(parent)
	{
		DocImportsModel *m = new DocImportsModel(this, recordType);
		const int modelColumn = m->fieldIndex("Name");
		if (recordType != RecordType::SimVarUnits)
			m->setFilter(QStringLiteral("Deprecated = 0"));
		m->setSort(modelColumn, Qt::AscendingOrder);

		setModel(m);
		setCompletionColumn(modelColumn);
		setModelSorting(QCompleter::CaseSensitivelySortedModel);
		setCompletionMode(QCompleter::PopupCompletion);
		setCaseSensitivity(Qt::CaseInsensitive);
		setFilterMode(Qt::MatchContains);
		setMaxVisibleItems(12);
	}

	DocImportsModel *model() const { return (DocImportsModel *)QCompleter::model(); }

};


// ----------------------------------------
// RecordTypeComboBox
// ----------------------------------------

class RecordTypeComboBox : public EnumsComboBox
{
public:
	RecordTypeComboBox(QWidget *p = nullptr) :
		EnumsComboBox(DocImports::RecordTypeNames, DocImports::RecordType::SimVars, p) {
		setToolTip(tr("Select a documentation record type to browse."));
	}
};

	}  // DocImports
}  // WASimUiNS
