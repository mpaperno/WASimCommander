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
	CalcResultType calcResultType;
	uint8_t simVarIndex;
	char varTypePrefix;
	char nameOrCode[STRSZ_REQ];
	char unitName[STRSZ_UNIT];
	// From DataRequestRecord
	time_t lastUpdate;
	std::vector<uint8_t> data;
	*/
	int metaType = QMetaType::UnknownType;
	QVariantMap properties {};

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


public:
	enum Roles { DataRole = Qt::UserRole + 1, MetaTypeRole, PropertiesRole };
	enum Columns {
		COL_ID,
		COL_TYPE,
		COL_RES_TYPE,
		COL_NAME,
		COL_IDX,
		COL_UNIT,
		COL_SIZE,
		COL_PERIOD,
		COL_INTERVAL,
		COL_EPSILON,
		COL_VALUE,
		COL_TIMESATMP,
		COL_META_ID,
		COL_META_NAME,
		COL_META_CAT,
		COL_META_DEF,
		COL_META_FMT,
		COL_ENUM_END,
		COL_FIRST_META = COL_META_ID,
		COL_LAST_META = COL_META_FMT,
	};
	const QStringList columnNames = {
		tr("ID"),
		tr("Type"),
		tr("Res/Var"),
		tr("Name or Code"),
		tr("Idx"),
		tr("Unit"),
		tr("Size"),
		tr("Period"),
		tr("Intvl"),
		tr("ΔΕ"),
		tr("Value"),
		tr("Last Updt."),
		tr("Export ID"),
		tr("Display Name"),
		tr("Category"),
		tr("Default"),
		tr("Format"),
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

	QModelIndexList allRequests() const {
		return match(index(0, COL_ID), Qt::EditRole, "*", -1, Qt::MatchWildcard | Qt::MatchWrap);
	}

	void setRequestValue(const WASimCommander::Client::DataRequestRecord &res)
	{
		const int row = findRequestRow(res.requestId);
		if (row < 0)
			return;

		// set data display
		const int dataType = item(row, COL_ID)->data(MetaTypeRole).toInt();
		QVariant v = Utils::convertValueToType(dataType, res);
		QStandardItem *itm = item(row, COL_VALUE);
		itm->setText(v.toString());
		itm->setData(v, DataRole);
		itm->setToolTip(itm->text());

		// update timestamp column
		const auto ts = QDateTime::fromMSecsSinceEpoch(res.lastUpdate);
		itm = item(row, COL_TIMESATMP);
		const auto lastts = itm->data().toULongLong();
		const uint64_t tsDelta = lastts ? res.lastUpdate - lastts : 0;
		itm->setText(QString("%1 (%2)").arg(ts.toString("hh:mm:ss.zzz")).arg(tsDelta));
		itm->setToolTip(itm->text());
		itm->setData(res.lastUpdate, DataRole);
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

		req.properties["id"] = item(row, COL_META_ID)->data(Qt::EditRole).toString();
		req.properties["name"] = item(row, COL_META_NAME)->data(Qt::EditRole).toString();
		req.properties["categoryId"] = item(row, COL_META_CAT)->data(Qt::EditRole).toString();
		req.properties["category"] = item(row, COL_META_CAT)->data(Qt::ToolTipRole).toString();
		req.properties["default"] = item(row, COL_META_DEF)->data(Qt::EditRole).toString();
		req.properties["format"] = item(row, COL_META_FMT)->data(Qt::EditRole).toString();

		//std::cout << req << std::endl;
		return req;
	}

	QModelIndex addRequest(const RequestRecord &req)
	{
		int row = findRequestRow(req.requestId);
		const bool newRow = row < 0;
		if (newRow)
			row = rowCount();

		QStandardItem *itm = setOrCreateItem(row, COL_ID, QString::number(req.requestId), req.requestId);
		itm->setData(req.metaType, MetaTypeRole);
		itm->setData(req.properties, PropertiesRole);

		itm = setOrCreateItem(row, COL_TYPE, WSEnums::RequestTypeNames[+req.requestType], +req.requestType);

		if (req.requestType == WSEnums::RequestType::Calculated) {
			setOrCreateItem(row, COL_RES_TYPE, WSEnums::CalcResultTypeNames[+req.calcResultType], +req.calcResultType);
			setOrCreateItem(row, COL_IDX, tr("N/A"), false);
			itm = setOrCreateItem(row, COL_UNIT, tr("N/A"), QString(req.unitName), false);
		}
		else {
			setOrCreateItem(row, COL_RES_TYPE, QString(req.varTypePrefix), req.varTypePrefix);
			if (Utils::isUnitBasedVariableType(req.varTypePrefix))
				setOrCreateItem(row, COL_UNIT, req.unitName, QString(req.unitName));
			else
				setOrCreateItem(row, COL_UNIT, tr("N/A"), QString(req.unitName), false);
			if (req.varTypePrefix == 'A')
				setOrCreateItem(row, COL_IDX, QString::number(req.simVarIndex));
			else
				setOrCreateItem(row, COL_IDX, tr("N/A"), false);
		}

		if (req.metaType == QMetaType::UnknownType)
			setOrCreateItem(row, COL_SIZE, QString::number(req.valueSize), req.valueSize);
		else if (req.metaType > QMetaType::User)
			setOrCreateItem(row, COL_SIZE, QString("String (%1 B)").arg(req.metaType - QMetaType::User), req.valueSize);
		else
			setOrCreateItem(row, COL_SIZE, QString("%1 (%2 B)").arg(QString(QMetaType::typeName(req.metaType)).replace("q", "")).arg(QMetaType::sizeOf(req.metaType)), req.valueSize);

		setOrCreateItem(row, COL_PERIOD, WSEnums::UpdatePeriodNames[+req.period], +req.period);
		setOrCreateItem(row, COL_NAME, req.nameOrCode);
		setOrCreateItem(row, COL_INTERVAL, QString::number(req.interval));

		if (req.metaType > QMetaType::UnknownType && req.metaType < QMetaType::User)
			setOrCreateItem(row, COL_EPSILON, QString::number(req.deltaEpsilon), req.deltaEpsilon);
		else
			setOrCreateItem(row, COL_EPSILON, tr("N/A"), req.deltaEpsilon, false);

		if (newRow) {
			setOrCreateItem(row, COL_VALUE, tr("???"));
			setOrCreateItem(row, COL_TIMESATMP, tr("Never"), 0);
		}

		updateFromMetaData(row, req);

		return index(row, 0);
	}

	void updateFromMetaData(int row, const RequestRecord &req)
	{
		// Meta data for exports
		setOrCreateEditableItem(row, COL_META_ID, req.properties.value("id").toString());
		setOrCreateEditableItem(row, COL_META_NAME, req.properties.value("name").toString());
		setOrCreateEditableItem(row, COL_META_CAT, req.properties.value("categoryId").toString(), req.properties.value("category").toString());
		setOrCreateEditableItem(row, COL_META_DEF, req.properties.value("default").toString());
		setOrCreateEditableItem(row, COL_META_FMT, req.properties.value("format").toString());
	}

	void removeRequest(const uint32_t requestId)
	{
		const int row = findRequestRow(requestId);
		if (row > -1)
			removeRow(row);
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

	signals:
		void rowCountChanged(int rows);

	protected:
		QStandardItem *setOrCreateItem(int row, int col, const QString &text, const QVariant &data = QVariant(), bool en = true, bool edit = false, const QString &tt = QString())
		{
			QStandardItem *itm = item(row, col);
			if (!itm){
				setItem(row, col, new QStandardItem());
				itm = item(row, col);
			}
			itm->setText(text);
			itm->setToolTip(tt.isEmpty() ? text : tt);
			itm->setEnabled(en);
			itm->setEditable(edit);
			if (data.isValid())
				itm->setData(data, DataRole);
			return itm;
		}
		QStandardItem *setOrCreateEditableItem(int row, int col, const QString &text, const QString &tt = QString()) {
			return setOrCreateItem(row, col, text, QVariant(), true, true, tt);
		}

	private:
		uint32_t m_nextRequestId = 0;

};

}
