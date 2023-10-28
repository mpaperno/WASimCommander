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

#include <iostream>
#include <QSaveFile>
#include "RequestsModel.h"

namespace WASimUiNS {
	namespace RequestsFormat
{

	static QMap<QString, QString> categoriesList()
	{
		static QMap<QString, QString> cats {
			{"AutoPilot",          "AutoPilot"},
			{"Camera",             "Camera & Views"},
			{"Communication",      "Radio & Navigation"},
			{"Electrical",         "Electrical"},
			{"Engine",             "Engine"},
			{"Environment",        "Environment"},
			{"Failures",           "Failures"},
			{"FlightInstruments",  "Flight Instruments"},
			{"FlightSystems",      "Flight Systems"},
			{"Fuel",               "Fuel"},
			{"Miscellaneous",      "Miscellaneous"},
			{"SimSystem",          "Simulator System"},
		};
		return cats;
	}

	static QString generateDefaultName(const RequestRecord &req)
	{
		QString name = req.requestType == WASimCommander::Enums::RequestType::Calculated ?
			"CalculatorResult_" + QString::number(req.requestId) : QString(req.nameOrCode).simplified();
		if (req.varTypePrefix == 'A' && req.simVarIndex)
			name += ' ' + QString::number(req.simVarIndex);
		return name;
	}

	static QString generateIdFromName(const RequestRecord &req)
	{
		static const QRegularExpression idRegex = QRegularExpression("(?:\\b|\\W|_)(\\w)");
		// convert ID to CamelCase
		QString id = req.properties.value("name").toString().toLower() /*.replace(' ', '_')*/;
		QRegularExpressionMatchIterator mi = idRegex.globalMatch(id);
		while (mi.hasNext()) {
			QRegularExpressionMatch m = mi.next();
			id.replace(m.capturedStart(), m.capturedLength(), m.captured().toUpper());
		}
		id.replace(' ', "");
		if (req.varTypePrefix == 'A' && req.simVarIndex)
			id += QString::number(req.simVarIndex);
		return id;
	}

	static void generateRequiredProperties(RequestRecord &req)
	{
		// friendly name
		if (req.properties.value("name").toString().isEmpty())
			req.properties["name"] = generateDefaultName(req);
		// required unique ID
		if (req.properties.value("id").toString().isEmpty())
			req.properties["id"] = generateIdFromName(req);
		// sorting category id and name
		if (req.properties.value("categoryId").toString().isEmpty()) {
			req.properties["categoryId"] = QStringLiteral("Miscellaneous");
			req.properties["category"] = QStringLiteral("Miscellaneous");
		}
		// optional default value
		if (req.properties.value("default").toString().isEmpty() && QString(req.unitName) != QLatin1Literal("string"))
			req.properties["default"] = 0;
		// optional formatting string
		if (req.properties.value("format").toString().isEmpty() && (req.valueSize == WS::DATA_TYPE_FLOAT || req.valueSize == WS::DATA_TYPE_DOUBLE))
			req.properties["format"] = QStringLiteral("F2");
	}

	static QTextStream &addField(QTextStream &out, const char *key, const QVariant &value, bool quoted = false) {
		out << key << " = ";
		if (quoted)
			out << '"';
		out << value.toString();
		if (quoted)
			out << '"';
		return out << endl;
	}

	static int exportToPluginFormat(RequestsModel *model, const QModelIndexList &rows, const QString &filepath)
	{
		//const QModelIndexList rows = selection ? selection->selectedRows(RequestsModel::COL_ID) : model->allRequests();
		if (rows.isEmpty())
			return 0;

		QSaveFile *f = new QSaveFile(filepath);
		if (!f->open(QFile::WriteOnly | QFile::Truncate | QFile::Text))
			return 0;
		QTextStream out(f);

		RequestRecord req;
		QString tmp;
		for (const QModelIndex &r : rows) {
			req = model->getRequest(r.row());

			generateRequiredProperties(req);

			out << '[' << req.properties.value("id").toString() << ']' << endl;
			addField(out, "CategoryId", req.properties.value("categoryId"));
			addField(out, "Name", req.properties.value("name"), true);
			addField(out, "VariableType", QChar(req.varTypePrefix));

			tmp = req.nameOrCode;
			if (req.varTypePrefix == 'A' && req.simVarIndex)
				tmp += ':' + QString::number(req.simVarIndex);
			addField(out, "SimVarName", tmp, true);

			if (req.varTypePrefix == 'Q')
				addField(out, "CalcResultType", Utils::getEnumName(req.calcResultType, WSEnums::CalcResultTypeNames));
			else
				addField(out, "Unit", req.unitName, true);

			tmp = req.properties.value("default").toString();
			if (!tmp.isEmpty())
				addField(out, "DefaultValue", tmp, true);

			tmp = req.properties.value("format").toString();
			if (!tmp.isEmpty())
				addField(out, "StringFormat", tmp, true);

			if (req.period != WSEnums::UpdatePeriod::Tick)
				addField(out, "UpdatePeriod", Utils::getEnumName(req.period, WSEnums::UpdatePeriodNames));
			if (req.interval)
				addField(out, "UpdateInterval", req.interval);
			if (req.deltaEpsilon && QString(req.unitName) != QLatin1Literal("string"))
				addField(out, "DeltaEpsilon", QString::number(req.deltaEpsilon));

			out << endl;
		}

		f->commit();
		return rows.count();
	}


	static QString cleanValue(const QSettings &s, const QString &key, const QString &def = QString())
	{
		// QSettings doesn't properly strip trailing comments that start with '#' (';' are OK), so we have to do it..  :(
		static const QRegularExpression stripTrailingComments("(?<!\")\\s*#[^\"]*$");
		QString val = s.value(key, def).toString();
		val.replace(stripTrailingComments, "");
		return val;
	}

	static QList<RequestRecord> importFromPluginFormat(RequestsModel *model, const QString &filepath)
	{
		QSettings s(filepath, QSettings::IniFormat);
		s.setAtomicSyncRequired(false);

		int enumIdx;
		QList<RequestRecord> ret;
		const QStringList list = s.childGroups();
		for (const QString &key : list) {
			s.beginGroup(key);
			if (!s.contains("Name") || !s.contains("SimVarName"))
				continue;

			RequestRecord req;
			QModelIndex ri = model->match(model->index(0, RequestsModel::COL_META_ID), Qt::EditRole, key, 1, Qt::MatchExactly | Qt::MatchWrap).value(0);
			if (ri.isValid())
				req = model->getRequest(ri.row());
			else
				req = RequestRecord(model->nextRequestId(), 'A', nullptr, 0);

			req.properties["id"] = key;
			req.properties["name"] = s.value("Name");
			req.properties["categoryId"] = cleanValue(s, "CategoryId", "Miscellaneous").trimmed();
			req.properties["category"] = categoriesList().value(req.properties["categoryId"].toString());

			if (s.contains("VariableType"))
				req.varTypePrefix = qPrintable(cleanValue(s, "VariableType"))[0];

			QString simVarName = cleanValue(s, "SimVarName").trimmed();

			if (req.varTypePrefix == 'Q') {
				req.requestType = WSEnums::RequestType::Calculated;
				enumIdx = Utils::indexOfString(WSEnums::CalcResultTypeNames, qPrintable(cleanValue(s, "CalcResultType", "Double").trimmed()));
				req.calcResultType = enumIdx > -1 ? (WSEnums::CalcResultType)enumIdx : WSEnums::CalcResultType::Double;
				req.metaType = Utils::calcResultTypeToMetaType(req.calcResultType);
				req.valueSize = Utils::metaTypeToSimType(req.metaType);
			}
			else {
				req.requestType = WSEnums::RequestType::Named;
				req.setUnitName(qPrintable(cleanValue(s, "Unit", "number").trimmed()));
				req.metaType = Utils::unitToMetaType(req.unitName);
				req.valueSize = Utils::metaTypeToSimType(req.metaType);
				if (req.varTypePrefix == 'A' && simVarName.contains(':')) {
					const QStringList ni = simVarName.split(':');
					if (ni.length() == 2) {
						simVarName = ni.first();
						req.simVarIndex = ni.last().toInt();
					}
				}
			}
			req.setNameOrCode(qPrintable(simVarName));

			if (s.contains("UpdatePeriod")) {
				enumIdx = Utils::indexOfString(WSEnums::UpdatePeriodNames, qPrintable(cleanValue(s, "UpdatePeriod").trimmed()));
				if (enumIdx > -1)
					req.calcResultType = (WSEnums::CalcResultType)enumIdx;
			}

			if (s.contains("Interval"))
				req.interval = s.value("Interval").toUInt();
			if (s.contains("DeltaEpsilon"))
				req.deltaEpsilon = s.value("DeltaEpsilon").toFloat();

			if (s.contains("StringFormat"))
				req.properties["format"] = s.value("StringFormat");
			if (s.contains("DefaultValue"))
				req.properties["default"] = s.value("DefaultValue");

			model->addRequest(req);
			ret << req;
			s.endGroup();
		}
		return ret;
	}


	}
}
