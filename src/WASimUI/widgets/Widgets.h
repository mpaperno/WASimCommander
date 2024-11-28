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

#include <QDebug>
#include <QBoxLayout>
#include <QLabel>
#include <vector>

#include "client/WASimClient.h"
#include "DataComboBox.h"
#include "DeletableItemsComboBox.h"
#include "../Utils.h"

namespace WASimUiNS
{

//using namespace WSEnums;

/// Displays icon and text representing network connection status.
class StatusWidget : public QWidget
{
	Q_OBJECT
	using ClientEvent = WASimCommander::Client::ClientEvent;
	using ClientEventType = WSCEnums::ClientEventType;
	using ClientStatus = WSCEnums::ClientStatus;
public:

	StatusWidget(QWidget *parent = nullptr) :
		QWidget(parent)
	{
		setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
		iconSize = style()->pixelMetric(QStyle::PixelMetric::PM_ToolBarIconSize, nullptr, this);

		QHBoxLayout *lo = new QHBoxLayout(this);
		lo->setSpacing(12);
		iconLabel = new QLabel(this);
		iconLabel->setFixedSize(iconSize, iconSize);
		textLabel = new QLabel(this);
		QFont font = textLabel->font();
		font.setPointSize(font.pointSize() + 2);
		textLabel->setFont(font);
		versionLabel = new QLabel(this);
		versionLabel->setAlignment(Qt::AlignCenter);
		lo->addWidget(iconLabel, 0);
		lo->addWidget(textLabel, 1);
		lo->addWidget(versionLabel, 0);

		icon.addFile("fg=red/block.glyph", QSize(), QIcon::Disabled, QIcon::Off);               // Idle
		icon.addFile("fg=blue/radio_button_off.glyph", QSize(), QIcon::Selected, QIcon::Off);   // SimConnected
		icon.addFile("fg=green/radio_button_on.glyph", QSize(), QIcon::Normal, QIcon::Off);     // Connected
		icon.addFile("fg=violet/hourglass_empty.glyph", QSize(), QIcon::Active, QIcon::Off);    // Initializing/Connecting/ShuttingDown

		setStatus(ClientEvent{ClientEventType::None, ClientStatus::Idle, "Not Connected"});
		versionLabel->setText(tr("Server\nv (unknown)"));

		setToolTip(tr("<p>This area shows the current network connection status. There are two stages of connection -- to the simulator itself (SimConnect) and to the WASimCommander Server (module).</p>"));
	}

	void setStatus(const ClientEvent &ev)
	{
		switch (ev.status) {
			case ClientStatus::Idle:
				iconLabel->setPixmap(icon.pixmap(iconSize, QIcon::Disabled));
				break;
			case ClientStatus::SimConnected:
				iconLabel->setPixmap(icon.pixmap(iconSize, QIcon::Selected));
				break;
			case ClientStatus::AllConnected:
				iconLabel->setPixmap(icon.pixmap(iconSize, QIcon::Normal));
				break;
			default:
				iconLabel->setPixmap(icon.pixmap(iconSize, QIcon::Active));
				break;
		}
		textLabel->setText(QString::fromStdString(ev.message));
		//qDebug() << "Got status: " << Utils::getEnumName(ev.eventType, WSCEnums::ClientEventTypeNames)
		//	<< "s: 0x" << hex << +ev.status << "m:" << QString::fromStdString(ev.message);
	}

	void setServerVersion(quint32 version)
	{
		if (serverVersion != version) {
			serverVersion = version;
			versionLabel->setText(tr("Server\nv%1.%2.%3.%4").arg(version >> 24).arg((version >> 16) & 0xFF).arg((version >> 8) & 0xFF).arg(version & 0xFF));
		}
	}

private:
	QIcon icon;
	QLabel *iconLabel;
	QLabel *textLabel;
	QLabel *versionLabel;
	int iconSize;
	quint32 serverVersion = 0;
};


/// Displays icon and text representing a Command Ack/Nak response.
class CommandStatusWidget : public QWidget
{
	Q_OBJECT
	using Command = WASimCommander::Command;
public:

	CommandStatusWidget(QWidget *parent = nullptr) :
		QWidget(parent)
	{
		setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
		iconSize = 20; // style()->pixelMetric(QStyle::PixelMetric::PM_ToolBarIconSize, nullptr, this);

		QHBoxLayout *lo = new QHBoxLayout(this);
		lo->setSpacing(12);
		lo->setContentsMargins(8, 1, 10, 2);
		iconLabel = new QLabel(this);
		iconLabel->setFixedSize(iconSize, iconSize);
		tsLabel = new QLabel(this);
		tsLabel->setForegroundRole(QPalette::Link);
		textLabel = new QLabel(this);
		lo->addWidget(iconLabel, 0);
		lo->addWidget(tsLabel, 0);
		lo->addWidget(textLabel, 1);

		icon.addFile("fg=green/thumb_up.glyph", QSize(), QIcon::Normal, QIcon::Off);
		icon.addFile("fg=red/thumb_down.glyph", QSize(), QIcon::Active, QIcon::Off);

		setToolTip(tr("<p>This area shows the result of the last command sent to the Server, which is either an 'Ack' (green thumbs up) or 'Nak' (red thumbs down) and may have additional text details.</p>"));
	}

	void setStatus(const WASimCommander::Command &resp)
	{
		const char *cmdName = Utils::getEnumName((WSEnums::CommandId)resp.uData, WSEnums::CommandIdNames);
		const QString msg = QString("[%1]: ").arg(cmdName);
		QString details(resp.sData);
		if (details.isEmpty())
			details = resp.commandId == WSEnums::CommandId::Ack ? tr("Command Succeeded") : tr("Command Failed");
		const QIcon::Mode icnMode = resp.commandId == WSEnums::CommandId::Ack ? QIcon::Mode::Normal :  QIcon::Mode::Active;
		iconLabel->setPixmap(icon.pixmap(iconSize, icnMode));
		textLabel->setText(msg + details);
		tsLabel->setText(QTime::currentTime().toString("[hh:mm:ss.zzz]"));
	}

private:
	QIcon icon;
	QLabel *iconLabel;
	QLabel *tsLabel;
	QLabel *textLabel;
	int iconSize;
};


// Generic enumeration selector combo box, for subclassing.
class EnumsComboBox : public DataComboBox
{
public:
	template <typename Range, typename Enum>
	EnumsComboBox(const Range &names, Enum startAt, QWidget *p = nullptr) : DataComboBox(p)
	{
		size_t i = (size_t)startAt;
		auto it = std::cbegin(names),
			en = std::cend(names);
		for (it += i; it < en; ++it)
			addItem(QString(*it), i++);
	}
};

class CalculationTypeComboBox : public EnumsComboBox
{
public:
	CalculationTypeComboBox(QWidget *p = nullptr) :
		EnumsComboBox(WSEnums::CalcResultTypeNames, WSEnums::CalcResultType::None, p)
	{ }
};

class UpdatePeriodComboBox : public EnumsComboBox
{
public:
	UpdatePeriodComboBox(QWidget *p = nullptr) :
		EnumsComboBox(WSEnums::UpdatePeriodNames, WSEnums::UpdatePeriod::Never, p)
	{
		setCurrentIndex(2);
	}
};

class LookupTypeComboBox : public EnumsComboBox
{
public:
	LookupTypeComboBox(QWidget *p = nullptr) :
		EnumsComboBox(WSEnums::LookupItemTypeNames, WSEnums::LookupItemType::LocalVariable, p)
	{
		setToolTip(tr("Named item lookup type."));
	}
};

class CommandIdComboBox : public EnumsComboBox
{
public:
	CommandIdComboBox(QWidget *p = nullptr) :
		EnumsComboBox(WSEnums::CommandIdNames, WSEnums::CommandId::Ping, p)
	{
		setToolTip(tr("Command ID"));
	}
};

class LogLevelComboBox : public EnumsComboBox
{
	Q_OBJECT
	Q_PROPERTY(WSEnums::LogLevel level READ level WRITE setLevel NOTIFY levelChanged USER true)
	Q_PROPERTY(WSEnums::LogFacility facility READ facility WRITE setFacility)
	Q_PROPERTY(WSCEnums::LogSource source READ source WRITE setSource)

public:
	LogLevelComboBox(QWidget *p = nullptr) :
		EnumsComboBox(WSEnums::LogLevelNames, WSEnums::LogLevel::None, p)
	{
		for (int i=0, e=count(); i < e; ++i)
			setItemIcon(i, QIcon(Utils::iconNameForLogLevel((WSEnums::LogLevel)itemData(i).toUInt())));
		connect(this, &LogLevelComboBox::currentDataChanged, this, [=](const QVariant &d) { emit levelChanged(WSEnums::LogLevel(d.toUInt())); });
	}

	void setProperties(WSEnums::LogLevel level, WSEnums::LogFacility facility, WSCEnums::LogSource source = WSCEnums::LogSource::Client)
	{
		setFacility(facility);
		setSource(source);
		setLevel(level);
	}

	WSEnums::LogLevel level() const { return WSEnums::LogLevel(currentData().toUInt()); }
	void setLevel(WSEnums::LogLevel level) { setCurrentData((unsigned)level); }

	WSEnums::LogFacility facility() const { return m_facility; }
	void setFacility(WSEnums::LogFacility facility) { m_facility = facility; }

	WSCEnums::LogSource source() const { return m_source; }
	void setSource(WSCEnums::LogSource source) { m_source = source; }

signals:
		void levelChanged(WSEnums::LogLevel level);

private:
	WSEnums::LogFacility m_facility = WSEnums::LogFacility::None;
	WSCEnums::LogSource m_source = WSCEnums::LogSource::Client;
};


class VariableTypeComboBox : public DataComboBox
{
	Q_OBJECT
public:
	VariableTypeComboBox(QWidget *p = nullptr) : DataComboBox(p)
	{
		setToolTip(tr("Named variable type. Types marked with a * use Unit specifiers. Most 'L' vars will ignore the Unit (default is 'number')."));

		addItem(tr("A: SimVar *"), 'A');
		addItem(tr("B: Input"),    'B');  // only for gauge modules in FS20
		addItem(tr("C: GPS *"),    'C');
		addItem(tr("E: Env. *"),   'E');
		addItem(tr("H: HTML"),     'H');
		//addItem(tr("I: Instr."),   'I');  // only for gauge modules
		addItem(tr("K: Key"),      'K');
		addItem(tr("L: Local *"),  'L');
		addItem(tr("M: Mouse"),    'M');
		//addItem(tr("O: Comp."),    'O');  // only for gauge modules
		addItem(tr("R: Resource"), 'R');
		addItem(tr("T: Token"),    'T');
		addItem(tr("Z: Custom"),   'Z');

		setCurrentData('L');
	}
};

class ValueSizeComboBox : public DeletableItemsComboBox
{
	Q_OBJECT
public:
	ValueSizeComboBox(QWidget *p = nullptr) : DeletableItemsComboBox(p)
	{
		insertSeparator(0);
		addItem(tr("Double"),     +QMetaType::Double);
		addItem(tr("Float"),      +QMetaType::Float);
		addItem(tr("Int8"),       +QMetaType::SChar);
		addItem(tr("Int16"),      +QMetaType::Short);
		addItem(tr("Int32"),      +QMetaType::Int);
		addItem(tr("Int64"),      +QMetaType::LongLong);
		addItem(tr("UInt8"),      +QMetaType::UChar);
		addItem(tr("UInt16"),     +QMetaType::UShort);
		addItem(tr("UInt32"),     +QMetaType::UInt);
		addItem(tr("UInt64"),     +QMetaType::ULongLong);
		addItem(tr("String8"),    +QMetaType::User + 8);
		addItem(tr("String16"),   +QMetaType::User + 16);
		addItem(tr("String32"),   +QMetaType::User + 32);
		addItem(tr("String64"),   +QMetaType::User + 64);
		addItem(tr("String128"),  +QMetaType::User + 128);
		addItem(tr("String256"),  +QMetaType::User + 256);
		addItem(tr("String512"),  +QMetaType::User + 512);
		addItem(tr("String1024"), +QMetaType::User + 1024);
		setCurrentIndex(-1);
	}
};


class UnitTypeComboBox : public DeletableItemsComboBox
{
	Q_OBJECT
public:
	UnitTypeComboBox(QWidget *p = nullptr) : DeletableItemsComboBox(p)
	{
		setToolTip(tr(
			"<p>Unit Name for the value. For L vars this can be left blank to get the default value of the variable.</p>"
			"<p>The completion suggestions are looked up from imported SimConnect SDK documentation unit types.</p>"
			"<p>Unit types may also be saved for quick selection later by pressing Return after selecting one or typing it in.</p>"
			"<p>Saved items can be removed by right-clicking on them while the list is open.</p>"
		));

		setInsertPolicy(InsertAlphabetically);
		int i = 0;
		addItem(QStringLiteral("bar"), i++);
		addItem(QStringLiteral("Bool"), i++);
		addItem(QStringLiteral("celsius"), i++);
		addItem(QStringLiteral("cm"), i++);
		addItem(QStringLiteral("degrees"), i++);
		addItem(QStringLiteral("Enum"), i++);
		addItem(QStringLiteral("fahrenheit"), i++);
		addItem(QStringLiteral("feet"), i++);
		addItem(QStringLiteral("feet per minute"), i++);
		addItem(QStringLiteral("flags"), i++);
		addItem(QStringLiteral("inches"), i++);
		addItem(QStringLiteral("inHg"), i++);
		addItem(QStringLiteral("KHz"), i++);
		addItem(QStringLiteral("knots"), i++);
		addItem(QStringLiteral("kPa"), i++);
		addItem(QStringLiteral("meters"), i++);
		addItem(QStringLiteral("m/s"), i++);
		addItem(QStringLiteral("MHz"), i++);
		addItem(QStringLiteral("Nm"), i++);
		addItem(QStringLiteral("number"), i++);
		addItem(QStringLiteral("percent"), i++);
		addItem(QStringLiteral("percent over 100"), i++);
		addItem(QStringLiteral("position"), i++);
		addItem(QStringLiteral("position 128"), i++);
		addItem(QStringLiteral("position 16k"), i++);
		addItem(QStringLiteral("position 32k"), i++);
		addItem(QStringLiteral("psf"), i++);
		addItem(QStringLiteral("psi"), i++);
		addItem(QStringLiteral("radians"), i++);
		addItem(QStringLiteral("rpm"), i++);
		addItem(QStringLiteral("seconds"), i++);
		addItem(QStringLiteral("string"), i++);
		addItem(QStringLiteral("volts"), i++);
		addItem(QStringLiteral("Watts"), i++);
		setCurrentIndex(-1);

		lineEdit()->setMaxLength(WASimCommander::STRSZ_UNIT);
	}
};

}
