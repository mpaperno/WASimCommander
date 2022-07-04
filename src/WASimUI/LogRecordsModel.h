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
#include <QDateTime>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QString>

#include "client/WASimClient.h"
#include "Utils.h"

namespace WASimUiNS
{

/// The LogRecordsModel handles storage, sorting, and filtering of `LogRecord` types.
class LogRecordsModel : public QSortFilterProxyModel
{
private:
	Q_OBJECT
	enum Roles { DataRole = Qt::UserRole + 1, LevelRole, SourceRole };

public:
	enum class LogSource : uint8_t {
		Client = +WASimCommander::Client::LogSource::Client,	Server = +WASimCommander::Client::LogSource::Server,	UI
	};
	const QStringList LogSourceEnumNames   = { "Client", "Server", "UI" };

	enum Columns {
		COL_LEVEL,   COL_TS,     COL_SOURCE, COL_MESSAGE,  COL_ENUM_END
	};
	const QStringList columnNames = {
		tr("Level"), tr("Time"), tr("Src"),  tr("Message")
	};

	const QStringList levelNames      = { "NON",   "CRT",       "ERR",       "WRN",       "INF",       "DBG",        "TRC" };
	const QStringList levelBgColors   = { "black", "#3fea00ff", "#3bff0000", "#4da08300", "#21006eff", "#3ba4b1b3",  "transparent" };

	LogRecordsModel(QObject *parent = nullptr) :
		QSortFilterProxyModel(parent),
		m_model(new QStandardItemModel(0, COL_ENUM_END, this))
	{
		m_model->setHorizontalHeaderLabels(columnNames);
		setSourceModel(m_model);

		setSortRole(DataRole);
		sort(COL_TS, Qt::AscendingOrder);

		connect(this, &QAbstractItemModel::rowsInserted, this, [=](const QModelIndex &, int, int) { emit rowCountChanged(rowCount()); });
		connect(this, &QAbstractItemModel::rowsRemoved, this, [=](const QModelIndex &, int, int) { emit rowCountChanged(rowCount()); });
	}

	void addRecord(const WASimCommander::LogRecord &log, quint8 src)
	{
		const int row = m_model->rowCount();

		m_model->setItem(row, COL_LEVEL, new QStandardItem(QIcon(Utils::iconNameForLogLevel(log.level)), Utils::getEnumName(log.level, levelNames)));
		m_model->item(row, COL_LEVEL)->setToolTip(Utils::getEnumName(log.level, WSEnums::LogLevelNames));
		m_model->item(row, COL_LEVEL)->setTextAlignment(Qt::AlignCenter);
		m_model->item(row, COL_LEVEL)->setData(+log.level, LevelRole);
		m_model->item(row, COL_LEVEL)->setData(src, SourceRole);

		const QDateTime dt = QDateTime::fromMSecsSinceEpoch(log.timestamp);
		m_model->setItem(row, COL_TS, new QStandardItem(dt.toString("hh:mm:ss.zzz")));
		m_model->item(row, COL_TS)->setToolTip(dt.toString("ddd MMMM d"));
		m_model->item(row, COL_TS)->setData(dt, DataRole);

		m_model->setItem(row, COL_SOURCE, new QStandardItem(QIcon(Utils::iconNameForLogSource(src)), ""));
		m_model->item(row, COL_SOURCE)->setToolTip(tr("Log Source: %1").arg(Utils::getEnumName(src, LogSourceEnumNames)));
		//m_model->item(row, COL_SOURCE)->setTextAlignment(Qt::AlignCenter);

		const QString msg(log.message);
		m_model->setItem(row, COL_MESSAGE, new QStandardItem(msg));
		m_model->item(row, COL_MESSAGE)->setToolTip(msg);

		decorateStatus(row, log.level, (LogSource)src);

		emit recordAdded(mapFromSource(m_model->index(row, 0)));
	}

	void clear() {
		m_model->removeRows(0, rowCount());
	}

	void addLevelFilter(WSEnums::LogLevel l)
	{
		if (!m_exludeLevels.contains(+l)) {
			m_exludeLevels.append(+l);
			invalidateFilter();
		}
	}

	void removeLevelFilter(WSEnums::LogLevel l)
	{
		const int idx = m_exludeLevels.indexOf(+l);
		if (idx > -1) {
			m_exludeLevels.remove(idx);
			invalidateFilter();
		}
	}

	void setLevelFilter(WSEnums::LogLevel l, bool enable)
	{
		if (enable)
			addLevelFilter(l);
		else
			removeLevelFilter(l);
	}

	void addSourceFilter(uint8_t s)
	{
		if (!m_exludeSources.contains(s)) {
			m_exludeSources.append(s);
			invalidateFilter();
		}
	}

	void removeSourceFilter(uint8_t s)
	{
		const int idx = m_exludeSources.indexOf(s);
		if (idx > -1) {
			m_exludeSources.remove(idx);
			invalidateFilter();
		}
	}

	void setSourceFilter(uint8_t s, bool enable)
	{
		if (enable)
			addSourceFilter(s);
		else
			removeSourceFilter(s);
	}

signals:
	void rowCountChanged(int rows);
	void recordAdded(const QModelIndex &index);

protected:
	bool filterAcceptsRow(int row, const QModelIndex &) const override
	{
		QStandardItem *item = m_model->item(row, COL_LEVEL);
		if (!item)
			return false;
		return !m_exludeLevels.contains(item->data(LevelRole).toUInt()) && !m_exludeSources.contains(item->data(SourceRole).toUInt());
	}

private:
	void decorateStatus(const int row, WSEnums::LogLevel l, LogSource s)
	{
		if (QStandardItem *item = m_model->item(row, COL_LEVEL))
			item->setBackground(QColor(Utils::getEnumName(l, levelBgColors)));
	}

	QStandardItemModel *m_model;
	QVector<uint8_t> m_exludeLevels {};
	QVector<uint8_t> m_exludeSources {};

};

}
