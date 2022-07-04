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
// RequestRecord
// -------------------------------------------------------------

/// Subclass of WASimCommander::DataRequest struct which adds some meta data for local tracking purposes,
/// and \c QDataStream operators for de/serializing as a \c QVariant (eg. for QSettings storage).
struct RequestRecord : public WASimCommander::Client::DataRequestRecord
{
	/*
	// Inherited from DataRequest
	uint32_t requestId;
	uint32_t valueSize;
	float deltaEpsilon;
	uint32_t interval;
	UpdatePeriod period;
	RequestType requestType;
	union {
		CalcResultType calcResultType;
		uint8_t simVarIndex;
	};
	char varTypePrefix;
	char nameOrCode[STRSZ_REQ];
	char unitName[STRSZ_UNIT];
	// From DataRequestRecord
	time_t lastUpdate;
	uint32_t dataSize;
	*/
	int metaType = QMetaType::UnknownType;

	using DataRequestRecord::DataRequestRecord;

	/// QDataStream operator for serializing to a QVariant (eg. for QSettings storage).
	friend QDataStream& operator<<(QDataStream& out, const RequestRecord &r)
	{
		// Serialize enum names instead of values.
		out << r.requestId << r.valueSize << r.interval << r.deltaEpsilon;
		out << QString(Utils::getEnumName(r.requestType, WSEnums::RequestTypeNames)) << QString(Utils::getEnumName(r.period, WSEnums::UpdatePeriodNames));
		if (r.requestType == WSEnums::RequestType::Calculated)
			out << QString(Utils::getEnumName(r.calcResultType, WSEnums::CalcResultTypeNames));
		else
			out << r.simVarIndex;
		return out << QString(r.varTypePrefix) << QString(r.nameOrCode) << QString(r.unitName) << r.metaType;
	}

	/// QDataStream operator for deserializing from a QVariant (eg. for QSettings storage).
	friend QDataStream& operator>>(QDataStream& in, RequestRecord &r)
	{
		QString pfx, name, unit, reqType, updPer;
		in >> r.requestId;
		in >> r.valueSize;
		in >> r.interval;
		in >> r.deltaEpsilon;

		in >> reqType;  //r.requestType;
		int enumIdx = Utils::indexOfString(WSEnums::RequestTypeNames, qPrintable(reqType));
		if (enumIdx > -1)
			r.requestType = (WSEnums::RequestType)enumIdx;

		in >> updPer;  // r.period;
		enumIdx = Utils::indexOfString(WSEnums::UpdatePeriodNames, qPrintable(updPer));
		if (enumIdx > -1)
			r.period = (WSEnums::UpdatePeriod)enumIdx;

		if (r.requestType == WSEnums::RequestType::Calculated) {
			QString resType;
			in >> resType;  //r.calcResultType;
			enumIdx = Utils::indexOfString(WSEnums::CalcResultTypeNames, qPrintable(resType));
			if (enumIdx > -1)
				r.calcResultType = (WSEnums::CalcResultType)enumIdx;
		}
		else {
			in >> r.simVarIndex;
		}
		in >> pfx;
		in >> name;
		in >> unit;
		in >> r.metaType;

		if (!name.isEmpty())
			r.setNameOrCode(qPrintable(name));
		if (!unit.isEmpty())
			r.setUnitName(qPrintable(unit));
		if (!pfx.isEmpty())
			r.varTypePrefix = pfx.at(0).toLatin1();
		return in;
	}

};
Q_DECLARE_METATYPE(RequestRecord)

// -------------------------------------------------------------
// RequestsModel
// -------------------------------------------------------------

static uint32_t g_nextRequestId = 0;
static const RequestRecord nullRecord {};

/// The RequestModel handles storage and retrieval of RequestRecord types.
class RequestsModel : public QStandardItemModel
{
private:
	Q_OBJECT

	enum Roles { DataRole = Qt::UserRole + 1, MetaTypeRole };

public:
	enum Columns {
		COL_ID,   COL_TYPE,   COL_RES_TYPE,       COL_NAME,           COL_SIZE,   COL_UNIT,   COL_IDX,   COL_PERIOD,   COL_INTERVAL, COL_EPSILON, COL_VALUE,   COL_TIMESATMP,   COL_ENUM_END
	};
	const QStringList columnNames = {
		tr("ID"), tr("Type"), tr("Res/Var Type"), tr("Name or Code"), tr("Size"), tr("Unit"), tr("Idx"), tr("Period"), tr("Intvl"),   tr("ΔΕ"),   tr("Value"), tr("Last Updt.")
	};

	RequestsModel(QObject *parent = nullptr) :
		QStandardItemModel(0, COL_ENUM_END, parent)
	{
		setHorizontalHeaderLabels(columnNames);
		connect(this, &QAbstractItemModel::rowsInserted, this, [=](const QModelIndex &, int, int) { emit rowCountChanged(rowCount()); });
		connect(this, &QAbstractItemModel::rowsRemoved, this, [=](const QModelIndex &, int, int) { emit rowCountChanged(rowCount()); });
	}

	uint32_t nextRequestId() { return m_nextRequestId++; }

	uint32_t requestId(int row) const {
		if (row >= rowCount())
			return uint32_t(-1);
		return item(row, COL_ID)->data(DataRole).toUInt();
	}

	void setRequestValue(const WASimCommander::Client::DataRequestRecord &res)
	{
		const int row = findRequestRow(res.requestId);
		if (row < 0)
			return;

		// set data display
		const int dataType = item(row, COL_ID)->data(MetaTypeRole).toInt();
		QVariant v = Utils::convertValueToType(dataType, res);
		item(row, COL_VALUE)->setText(v.toString());
		item(row, COL_VALUE)->setData(v);

		// update timestamp column
		const auto ts = QDateTime::fromMSecsSinceEpoch(res.lastUpdate);
		const auto lastts = item(row, COL_TIMESATMP)->data().toULongLong();
		const uint64_t tsDelta = lastts ? res.lastUpdate - lastts : 0;
		item(row, COL_TIMESATMP)->setText(QString("%1 (%2)").arg(ts.toString("hh:mm:ss.zzz")).arg(tsDelta));
		item(row, COL_TIMESATMP)->setData(res.lastUpdate);
		qDebug() << "Saved result" << v << "for request ID" << res.requestId << "ts" << res.lastUpdate << "size" << res.data.size() << "type" << (QMetaType::Type)dataType << "data : " << QByteArray((const char *)res.data.data(), res.data.size()).toHex(':');
	}

	RequestRecord getRequest(int row) const
	{
		if (row >= rowCount())
			return nullRecord;
		QStandardItem *idItem = item(row, COL_ID);

		RequestRecord req(item(row, COL_ID)->data(DataRole).toUInt());
		req.valueSize = item(row, COL_SIZE)->data().toUInt();
		req.requestType = (WSEnums::RequestType)item(row, COL_TYPE)->data().toUInt();
		req.period = (WSEnums::UpdatePeriod)item(row, COL_PERIOD)->data().toUInt();
		req.interval = item(row, COL_INTERVAL)->data(Qt::EditRole).toUInt();
		req.deltaEpsilon = item(row, COL_EPSILON)->data().toFloat();
		if (req.requestType == WSEnums::RequestType::Calculated) {
			req.calcResultType = (WSEnums::CalcResultType)item(row, COL_RES_TYPE)->data().toUInt();
		}
		else {
			req.varTypePrefix = item(row, COL_RES_TYPE)->data().toChar().toLatin1();
			req.simVarIndex = item(row, COL_IDX)->data(Qt::EditRole).toUInt();
		}
		req.setNameOrCode(item(row, COL_NAME)->data(Qt::EditRole).toByteArray().constData());
		req.setUnitName(item(row, COL_UNIT)->data(DataRole).toByteArray().constData());

		req.metaType = item(row, COL_ID)->data(MetaTypeRole).toInt();

		//std::cout << req << std::endl;
		return req;
	}

	QModelIndex addRequest(const RequestRecord &req)
	{
		int row = findRequestRow(req.requestId);
		const bool newRow = row < 0;
		if (newRow)
			row = rowCount();

		setItem(row, COL_ID, new QStandardItem(QString("%1").arg(req.requestId)));
		item(row, COL_ID)->setData(req.requestId, DataRole);
		item(row, COL_ID)->setData(req.metaType, MetaTypeRole);

		setItem(row, COL_TYPE, new QStandardItem(WSEnums::RequestTypeNames[+req.requestType]));
		item(row, COL_TYPE)->setData(+req.requestType);

		if (req.requestType == WSEnums::RequestType::Calculated) {
			setItem(row, COL_RES_TYPE, new QStandardItem(WSEnums::CalcResultTypeNames[+req.calcResultType]));
			item(row, COL_RES_TYPE)->setData(+req.calcResultType);
			setItem(row, COL_IDX, new QStandardItem(tr("N/A")));
			setItem(row, COL_UNIT, new QStandardItem(tr("N/A")));
			item(row, COL_IDX)->setEnabled(false);
			item(row, COL_UNIT)->setEnabled(false);
		}
		else {
			setItem(row, COL_RES_TYPE, new QStandardItem(QString(req.varTypePrefix)));
			item(row, COL_RES_TYPE)->setData(req.varTypePrefix);
			if (Utils::isUnitBasedVariableType(req.varTypePrefix)) {
				setItem(row, COL_UNIT, new QStandardItem(QString(req.unitName)));
			}
			else {
				setItem(row, COL_UNIT, new QStandardItem(tr("N/A")));
				item(row, COL_UNIT)->setEnabled(false);
			}
			if (req.varTypePrefix == 'A') {
				setItem(row, COL_IDX, new QStandardItem(QString("%1").arg(req.simVarIndex)));
			}
			else {
				setItem(row, COL_IDX, new QStandardItem(tr("N/A")));
				item(row, COL_IDX)->setEnabled(false);
			}
		}
		item(row, COL_UNIT)->setData(QString(req.unitName));

		if (req.metaType == QMetaType::UnknownType)
			setItem(row, COL_SIZE, new QStandardItem(QString("%1").arg(req.valueSize)));
		else if (req.metaType > QMetaType::User)
			setItem(row, COL_SIZE, new QStandardItem(QString("String (%1 B)").arg(req.metaType - QMetaType::User)));
		else
			setItem(row, COL_SIZE, new QStandardItem(QString("%1 (%2 B)").arg(QString(QMetaType::typeName(req.metaType)).replace("q", "")).arg(QMetaType::sizeOf(req.metaType))));
		item(row, COL_SIZE)->setData(req.valueSize);

		setItem(row, COL_PERIOD, new QStandardItem(WSEnums::UpdatePeriodNames[+req.period]));
		item(row, COL_PERIOD)->setData(+req.period);

		setItem(row, COL_NAME, new QStandardItem(QString(req.nameOrCode)));
		setItem(row, COL_INTERVAL, new QStandardItem(QString("%1").arg(req.interval)));

		if (req.metaType > QMetaType::UnknownType && req.metaType < QMetaType::User) {
			setItem(row, COL_EPSILON, new QStandardItem(QString("%1").arg(req.deltaEpsilon, 0, 'f', 7)));
		}
		else  {
			setItem(row, COL_EPSILON, new QStandardItem(tr("N/A")));
			item(row, COL_EPSILON)->setEnabled(false);
		}
		item(row, COL_EPSILON)->setData(req.deltaEpsilon);

		if (newRow) {
			setItem(row, COL_VALUE, new QStandardItem("???"));
			setItem(row, COL_TIMESATMP, new QStandardItem("Never"));
			item(row, COL_TIMESATMP)->setData(0);
		}

		return index(row, 0);
	}

	void removeRequest(const uint32_t requestId)
	{
		const int row = findRequestRow(requestId);
		if (row > -1)
			removeRow(row);
		qDebug() << requestId << row;
	}

	void removeRequests(const QList<uint32_t> requestIds)
	{
		for (const uint32_t id : requestIds)
			removeRequest(id);
	}

	void removeRequests(const QModelIndexList &indexes)
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
		s.beginGroup(QStringLiteral("Requests"));
		s.remove("");
		for (int i = 0; i < rows; ++i)
			s.setValue(QStringLiteral("%1").arg(i), QVariant::fromValue(getRequest(i)));
		s.endGroup();
		s.sync();
		return rows;
	}

	QList<RequestRecord> loadFromFile(const QString &filepath)
	{
		QList<RequestRecord> ret;
		QSettings s(filepath, QSettings::IniFormat);
		s.setAtomicSyncRequired(false);
		s.beginGroup(QStringLiteral("Requests"));
		const QStringList list = s.childKeys();
		for (const QString &key : list) {
			RequestRecord req = s.value(key).value<RequestRecord>();
			req.requestId = nextRequestId();
			addRequest(req);
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
		uint32_t m_nextRequestId = 0;

		int findRequestRow(uint32_t requestId) const
		{
			if (rowCount()) {
				const QModelIndexList src = match(index(0, COL_ID), DataRole, requestId, 1, Qt::MatchExactly);
				//qDebug() << requestId << src << (!src.isEmpty() ? src.first() : QModelIndex());
				if (!src.isEmpty())
					return src.first().row();
			}
			return -1;
		}

};

}
