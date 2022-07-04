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

#include <algorithm>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <QStandardItemModel>

#include "client/WASimClient.h"
#include "Utils.h"

namespace WASimUiNS
{

// -------------------------------------------------------------
// EventRecord
// -------------------------------------------------------------

/// Registered Calculator Event
struct EventRecord : public WASimCommander::Client::RegisteredEvent
{
	using WASimCommander::Client::RegisteredEvent::RegisteredEvent;

	/// QDataStream operator for serializing to a QVariant (eg. for QSettings storage).
	friend QDataStream& operator<<(QDataStream& out, const EventRecord &r)
	{
		return out << r.eventId << QString::fromStdString(r.code) << QString::fromStdString(r.name);
	}

	/// QDataStream operator for deserializing from a QVariant (eg. for QSettings storage).
	friend QDataStream& operator>>(QDataStream& in, EventRecord &r)
	{
		QString c, n;
		in >> r.eventId;
		in >> c;
		in >> n;
		r.code = c.toStdString();
		r.name = n.toStdString();
		return in;
	}

};
Q_DECLARE_METATYPE(EventRecord)


// -------------------------------------------------------------
// EventsModel
// -------------------------------------------------------------

/// The EventsModel handles storage and retrieval of EventRecord types.
class EventsModel : public QStandardItemModel
{
private:
	Q_OBJECT
	enum Roles { DataRole = Qt::UserRole + 1, StatusRole };

public:
	enum Columns {
		COL_ID,   COL_CODE,   COL_NAME,   COL_ENUM_END
	};
	const QStringList columnNames = {
		tr("ID"), tr("Code"), tr("Name"),
	};

	EventsModel(QObject *parent = nullptr) :
		QStandardItemModel(0, COL_ENUM_END, parent)
	{
		setHorizontalHeaderLabels(columnNames);
		connect(this, &QAbstractItemModel::rowsInserted, this, [=](const QModelIndex &, int, int) { emit rowCountChanged(rowCount()); });
		connect(this, &QAbstractItemModel::rowsRemoved, this, [=](const QModelIndex &, int, int) { emit rowCountChanged(rowCount()); });
	}

	uint32_t nextEventId() { return m_nextEventId++; }

	uint32_t eventId(int row) const {
		if (row >= rowCount())
			return uint32_t(-1);
		return item(row, COL_ID)->data(DataRole).toUInt();
	}

	EventRecord getEvent(int row) const
	{
		EventRecord req(-1);
		if (row >= rowCount())
			return req;
		QStandardItem *idItem = item(row, COL_ID);
		req.eventId = item(row, COL_ID)->data(DataRole).toUInt();
		req.code = item(row, COL_CODE)->text().toStdString();
		req.name = item(row, COL_NAME)->text().toStdString();
		return req;
	}

	QModelIndex addEvent(const EventRecord &req)
	{
		int row = findRow(req.eventId);
		if (row < 0)
			row = rowCount();

		setItem(row, COL_ID, new QStandardItem(QString("%1").arg(req.eventId)));
		item(row, COL_ID)->setData(req.eventId, DataRole);

		setItem(row, COL_CODE, new QStandardItem(QString::fromStdString(req.code)));
		setItem(row, COL_NAME, new QStandardItem(QString::fromStdString(req.name)));

		return index(row, 0);
	}

	void removeEvent(const uint32_t eventId)
	{
		const int row = findRow(eventId);
		if (row > -1)
			removeRow(row);
	}

	void removeEvents(const QList<uint32_t> eventIds)
	{
		for (const uint32_t id : eventIds)
			removeEvent(id);
	}

	void removeEvents(const QModelIndexList &indexes)
	{
		QList<int> rows;
		rows.reserve(indexes.count());
		for (const QModelIndex &idx : indexes)
			rows.append(idx.row());
		std::sort(rows.begin(), rows.end(), std::greater<int>());
		for (const int row : rows)
			removeRow(row);
	}

	int saveToFile(const QString &filepath)
	{
		const int rows = rowCount();
		if (!rows)
			return rows;
		QSettings s(filepath, QSettings::IniFormat);
		s.setAtomicSyncRequired(false);
		s.beginGroup(QStringLiteral("Events"));
		s.remove("");
		for (int i = 0; i < rows; ++i)
			s.setValue(QStringLiteral("%1").arg(i), QVariant::fromValue(getEvent(i)));
		s.endGroup();
		s.sync();
		return rows;
	}

	QList<EventRecord> loadFromFile(const QString &filepath)
	{
		QList<EventRecord> ret;
		QSettings s(filepath, QSettings::IniFormat);
		s.setAtomicSyncRequired(false);
		s.beginGroup(QStringLiteral("Events"));
		const QStringList list = s.childKeys();
		for (const QString &key : list) {
			EventRecord req = s.value(key).value<EventRecord>();
			req.eventId = nextEventId();
			addEvent(req);
			ret << req;
		}
		s.endGroup();
		return ret;
	}

	static inline QModelIndexList flattenIndexList(const QModelIndexList &list)
	{
		QModelIndexList ret;
		QModelIndex lastIdx;
		for (const QModelIndex &idx : list) {
			if (idx.column() == COL_ID && lastIdx.row() != idx.row() && idx.row() < idx.model()->rowCount())
				ret.append(idx);
			lastIdx = idx;
		}
		return ret;
	}

signals:
	void rowCountChanged(int rows);

private:
	uint32_t m_nextEventId = 0;

	int findRow(uint32_t eventId) const
	{
		if (rowCount()) {
			const QModelIndexList src = match(index(0, COL_ID), DataRole, eventId, 1, Qt::MatchExactly);
			//qDebug() << eventId << src << (!src.isEmpty() ? src.first() : QModelIndex());
			if (!src.isEmpty())
				return src.first().row();
		}
		return -1;
	}

};

}
