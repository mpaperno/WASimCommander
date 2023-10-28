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
#include <iterator>
#include <type_traits>
#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMetaType>
#include <QPainter>
#include <QPalette>
#include <QPixmapCache>

#include "wasim_version.h"
#include "WASimCommander.h"
#include "client/WASimClient.h"
//#include "Widgets.h"

#include <QMessageBox>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

#include <comdef.h>

#define WSMCMND_APP_NAME  WSMCMND_PROJECT_NAME " Client UI"

namespace WASimUiNS {

namespace WS = WASimCommander;
namespace WSEnums = WS::Enums;
namespace WSCEnums = WS::Client;

// Custom "+" operator for strong enum types to cast to underlying type.
template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
constexpr auto operator+(T e) noexcept {
	return static_cast<std::underlying_type_t<T>>(e);
}

// for "about" dialog
class RoundedMessageBox : public QMessageBox
{
	Q_OBJECT
public:
	explicit RoundedMessageBox(QWidget *parent = nullptr) :
		QMessageBox(parent)
	{
		setObjectName("RoundedMessageBox");
		setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
		setAttribute(Qt::WA_TranslucentBackground);
		setStyleSheet(QStringLiteral(
			"#RoundedMessageBox { "
				"border-radius: 12px; "
				"border: 2px solid palette(shadow); "
				"background: palette(base); "
			"}"
		));
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		QStyleOption opt;
		opt.initFrom(this);
		style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
		p.end();
	}
};


class Utils
{
		Q_DECLARE_TR_FUNCTIONS(WASimUI)
	public:

		template <typename Range>
		static int indexOfString(Range const& range, const char *c) noexcept
		{
			int i = 0;
			auto b = std::cbegin(range), e = std::cend(range);
			while (b != e && strcmp(*b, c))
				++b, ++i;
			return b == e ? -1 : i;
		}

		template <typename Enum, typename Range /*, std::enable_if_t<std::is_enum<Enum>::value, bool> = true*/>
		static const auto getEnumName(Enum e, const Range &range) noexcept
		{
			using T = typename std::iterator_traits<decltype(std::begin(range))>::value_type;
			const size_t len = std::cend(range) - std::cbegin(range);
			if ((size_t)e < len)
				return range[(size_t)e];
			return T();
		}

		static constexpr uint32_t metaTypeToSimType(/*QMetaType::Type*/ int type)
		{
			switch (type) {
				case QMetaType::Double:
					return WASimCommander::DATA_TYPE_DOUBLE;
				case QMetaType::Float:
					return WASimCommander::DATA_TYPE_FLOAT;
				case QMetaType::Char:
				case QMetaType::SChar:
				case QMetaType::UChar:
				case QMetaType::Bool:
					return WASimCommander::DATA_TYPE_INT8;
				case QMetaType::Short:
				case QMetaType::UShort:
					return WASimCommander::DATA_TYPE_INT16;
				case QMetaType::Int:
				case QMetaType::UInt:
				case QMetaType::Long:
				case QMetaType::ULong:
					return WASimCommander::DATA_TYPE_INT32;
				case QMetaType::LongLong:
				case QMetaType::ULongLong:
					return WASimCommander::DATA_TYPE_INT64;
				default:
					if (type > QMetaType::User)
						return type - QMetaType::User;
					return 0;
			}
		}

		static constexpr int calcResultTypeToMetaType(WASimCommander::Enums::CalcResultType type)
		{
			switch (type) {
				case WASimCommander::Enums::CalcResultType::Double:
					return QMetaType::Double;
				case WASimCommander::Enums::CalcResultType::Integer:
					return QMetaType::Int;
				case WASimCommander::Enums::CalcResultType::String:
					return QMetaType::User + 64;
				case WASimCommander::Enums::CalcResultType::Formatted:
					return QMetaType::User + 256;
				default:
					return QMetaType::UnknownType;
			}
		}

		static QVariant convertValueToType(/*QMetaType::Type*/ int type, const WASimCommander::Client::DataRequestRecord &res)
		{
			QVariant v;
			switch (type) {
				case QMetaType::Double:
					v.setValue(double(res));
					break;
				case QMetaType::Float:
					v.setValue(float(res));
					break;
				case QMetaType::Char:
				case QMetaType::SChar:
					v.setValue(qint16((int8_t)res));  // upcast for printing
					break;
				case QMetaType::UChar:
				case QMetaType::Bool:
					v.setValue(quint16((uint8_t)res));  // upcast for printing
					break;
				case QMetaType::Short:
					v.setValue(qint16(res));
					break;
				case QMetaType::UShort:
					v.setValue(quint16(res));
					break;
				case QMetaType::Int:
				case QMetaType::Long:
					v.setValue(qint32(res));
					break;
				case QMetaType::UInt:
				case QMetaType::ULong:
					v.setValue(quint32(res));
					break;
				case QMetaType::LongLong:
					v.setValue(qint64(res));
					break;
				case QMetaType::ULongLong:
					v.setValue(quint64(res));
					break;
				default:
					v.setValue(QByteArray((const char *)res.data.data(), res.data.size()));
					break;
			}
			return v;
		}

		static int unitToMetaType(QString unit)
		{
			static const QStringList integralUnits {
				"enum", "mask", "flags", "integer",
				"position", "position 16k", "position 32k", "position 128",
				"frequency bcd16", "frequency bcd32", "bco16", "bcd16", "bcd32",
				"seconds", "minutes", "hours", "days", "years",
				"celsius scaler 16k", "celsius scaler 256"
			};
			static const QStringList boolUnits { "bool", "boolean" };

			unit = unit.toLower().simplified();
			if (unit == "string")
				return QMetaType::User + 256;
			if (boolUnits.contains(unit))
				return QMetaType::Bool;
			if (integralUnits.contains(unit))
				return QMetaType::Int;
			return QMetaType::Double;
		}

		static bool isUnitBasedVariableType(const char type) {
			static const std::vector<char> VAR_TYPES_UNIT_BASED = { 'A', 'C', 'E', 'L', 'P' };
			return find(VAR_TYPES_UNIT_BASED.cbegin(), VAR_TYPES_UNIT_BASED.cend(), type) != VAR_TYPES_UNIT_BASED.cend();
		}

		static QString hresultErrorMessage(HRESULT hr)
		{
			// QString("Error: 0x%1").arg(hr, 8, 16, QChar('0'))
			const QString ret = QString("%1").arg(_com_error(HRESULT_CODE(hr)).ErrorMessage());
			qDebug() << ret;
			return ret;
		}

		// Widgets
		//

		static inline QWidget *spacerWidget(const Qt::Orientations &orient = (Qt::Horizontal | Qt::Vertical), const int sz = 0)
		{
			QWidget *sp = new QWidget();
			sp->setEnabled(false);
			QSizePolicy pol(QSizePolicy::Minimum, QSizePolicy::Minimum);
			if (orient & Qt::Horizontal) {
				if (sz) {
					pol.setHorizontalPolicy(QSizePolicy::Fixed);
					sp->setFixedWidth(sz);
				}
				else {
					pol.setHorizontalPolicy(QSizePolicy::Expanding);
				}
			}
			if (orient & Qt::Vertical) {
				if (sz) {
					pol.setVerticalPolicy(QSizePolicy::Fixed);
					sp->setFixedHeight(sz);
				}
				else {
					pol.setVerticalPolicy(QSizePolicy::Expanding);
				}
			}
			sp->setSizePolicy(pol);
			return sp;
		}

		static inline QAction *separatorAction(QObject *parent)
		{
			QAction *act = new QAction(parent);
			act->setSeparator(true);
			return act;
		}

		// Style/theme

		static QIcon appIcon() noexcept { return QIcon(":/images/logo_96.png"); }
		static bool isDarkStyle() noexcept { return QApplication::palette().color(QPalette::Window).value() < 150; }

		static void toggleAppStyle(bool dark)
		{
			QPixmapCache::clear();
			QApplication::setPalette(dark ? darkPalette() : QApplication::style()->standardPalette());
			QApplication::setStyle("Fusion");
			loadStylesheet();
		}

		static void loadStylesheet()
		{
			const QString style(readFile(":/style/style.css"));
			if (!style.isEmpty())
				qApp->setStyleSheet(style);
		}

		static const QString iconNameForLogLevel(WASimCommander::Enums::LogLevel l)
		{
			//                                { "NON",   "CRT",       "ERR",       "WRN",       "INF",       "DBG",        "TRC" };
			static const QStringList icons  = { "help",  "report",    "error",     "warning",   "info",      "bug_report", "check_circle" };
			static const QStringList colors = { "black", "#bc2ac9",   "#d32e2e",   "#e5c32b",   "#006eff",   "#859aad",    "#b0aab7" };
			return QStringLiteral("fg=%2/%1.glyph").arg(getEnumName(l, icons), getEnumName(l, colors));
		}

		static const QString iconNameForLogSource(uint8_t s)
		{
			static const QStringList icons  = { "C",       "S",       "U" };
			//static const QStringList colors = { "#5d5d8c", "#8c7f61", "#548c76" };
			static const QStringList bgColors = { "#505d5d8c", "#508c7f61", "#50548c76" };
			return QStringLiteral("Segoe UI,bold/bg=%2/%1.glyph").arg(getEnumName(s, icons), getEnumName(s, bgColors));
		}

		// "About" dialog

		static void about(QWidget *parent = nullptr)
		{
#ifdef Q_OS_MAC
			static QPointer<QMessageBox> oldMsgBox;
			if (oldMsgBox) {
				oldMsgBox->show();
				oldMsgBox->raise();
				oldMsgBox->activateWindow();
				return;
			}
#endif
			RoundedMessageBox *msgBox = new RoundedMessageBox(parent);
			msgBox->setModal(true);
			msgBox->setAttribute(Qt::WA_DeleteOnClose);
			msgBox->setStandardButtons(QMessageBox::Close);
			msgBox->setWindowTitle(tr("About %1").arg(qApp->applicationDisplayName()));
			const QString capText = QStringLiteral("<h3 style=\"font-family: Source Code Pro;\">" WSMCMND_APP_NAME "</h3>");
			const QString dateStr = QDateTime::fromString(WSMCMND_BUILD_DATE, Qt::ISODate).toString("yyyy-MM-dd HH:mm t");
			const QString aboutText = QString(
				"<p><i>" WSMCMND_PROJECT_NAME "</i> (v" WSMCMND_VERSION_STR " from " + dateStr + ").</p>"
				"<p>" WSMCMND_PROJECT_DESCRIPT " A WASM module-based Server and a local Client combination.</p>"
				"<p><i>" WSMCMND_PROJECT_NAME "</i> is an open - source project. Visit the "
			  "<a href=\"" WSMCMND_PROJECT_URL "\" style=\"text-decoration: none;\">project site<sup><font style=\"font-size: 24px; font-family: IcoMoon-Free\">github2</font></sup></a> for more details.</p>"
			  "<p>" WSMCMND_PROJECT_COPYRIGHT "<br/>" WSMCMND_PROJECT_LICENSE "</p>"
				"<p>Built using <a href=\"http://qt.io/\"><i>Qt</i></a> v" QT_VERSION_STR ", used under terms of the GPL v3 license. <i>Qt</i> is a trademark of The Qt Company Ltd.</p>"
			  "<p>The following symbol fonts are included and used in this project under the terms of their respective licenses:</p>"
			  "<p>"
			  "<i><a href=\"https://icomoon.io/#icons-icomoon\">IcoMoon Free</a></i> - IcoMoon.io, GPL v3.<br/>"
			  "<i><a href=\"https://material.io/\">Material Icons</a></i> - Google, Apache License v2.0.<br/>"
			  "</p>"
			);

			msgBox->setText(capText);
			msgBox->setInformativeText(aboutText);
			msgBox->setIconPixmap(appIcon().pixmap(96, 96));
#ifdef Q_OS_MAC
			oldMsgBox = msgBox;
#endif
			msgBox->setMinimumSize(640, 480);
			msgBox->show();
		}

		static const QPalette darkPalette()
		{
			QPalette palette;
			const QColor cText("#FFE1E1EE");
			const QColor disText = cText.darker();
			palette.setColor(QPalette::All,      QPalette::WindowText,      cText);
			palette.setColor(QPalette::Disabled, QPalette::WindowText,      disText);
			palette.setColor(QPalette::All,      QPalette::Text,            cText);
			palette.setColor(QPalette::Disabled, QPalette::Text,            disText);
			palette.setColor(QPalette::All,      QPalette::BrightText,      cText);
			palette.setColor(QPalette::Disabled, QPalette::BrightText,      disText);
			palette.setColor(QPalette::All,      QPalette::ButtonText,      cText.lighter(105));
			palette.setColor(QPalette::Disabled, QPalette::ButtonText,      disText);
			palette.setColor(QPalette::All,      QPalette::HighlightedText, cText);
			palette.setColor(QPalette::Disabled, QPalette::HighlightedText, disText);

			const QColor cLight("#787878");
			palette.setColor(QPalette::All,      QPalette::Light, cLight);
			palette.setColor(QPalette::Disabled, QPalette::Light, cLight.darker());

			const QColor cMidLight("#585858");
			palette.setColor(QPalette::All,      QPalette::Midlight, cMidLight);
			palette.setColor(QPalette::Disabled, QPalette::Midlight, cMidLight.darker());

			const QColor cMid("#3F3F3F");
			palette.setColor(QPalette::All,      QPalette::Mid, cMid);
			palette.setColor(QPalette::Disabled, QPalette::Mid, cMid.darker());

			const QColor cMidDark("#2F2F2F");
			palette.setColor(QPalette::All,      QPalette::Button, cMidDark.lighter(125));
			palette.setColor(QPalette::Disabled, QPalette::Button, cMidDark.darker());
			palette.setColor(QPalette::All,      QPalette::Window, cMidDark);
			//palette.setColor(QPalette::Disabled, QPalette::Window, cMidDark);

			const QColor cBase("#272727");
			palette.setColor(QPalette::All,      QPalette::Base, cBase);
			palette.setColor(QPalette::Disabled, QPalette::Base, cBase.darker());

			const QColor cDark("#1E1E1E");
			palette.setColor(QPalette::All,      QPalette::Dark, cDark);
			//palette.setColor(QPalette::Disabled, QPalette::Dark, cDark);
			palette.setColor(QPalette::All,      QPalette::AlternateBase, cDark);
			palette.setColor(QPalette::Disabled, QPalette::AlternateBase, cDark.darker());

			const QColor cShdw("#050505");
			palette.setColor(QPalette::All,      QPalette::Shadow, cShdw);
			palette.setColor(QPalette::Disabled, QPalette::Shadow, cShdw.darker());

			const QColor cHlt("#E1063e5e");
			palette.setColor(QPalette::All,      QPalette::Highlight, cHlt);
			palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor("#9Cbbd5ff"));

			const QColor cLnkTxt("#5eb5ff");
			palette.setColor(QPalette::All,      QPalette::Link, cLnkTxt);
			palette.setColor(QPalette::Disabled, QPalette::Link, cLnkTxt.darker());

			const QColor cToolTip("#274968");
			palette.setColor(QPalette::All,      QPalette::ToolTipBase, cToolTip);
			palette.setColor(QPalette::Disabled, QPalette::ToolTipBase, cToolTip.darker());
			const QColor cToolTipTxt("#DDDDDF");
			palette.setColor(QPalette::All,      QPalette::ToolTipText, cToolTipTxt);
			palette.setColor(QPalette::Disabled, QPalette::ToolTipText, cToolTipTxt.darker());

#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
			const QColor cPlaceholder("#CF888888");
			palette.setColor(QPalette::All,      QPalette::PlaceholderText, cPlaceholder);
			palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, cPlaceholder.darker());
#endif
			return palette;
		}

		static QByteArray readFile(const QString &file)
		{
			QByteArray ret;
			QFile fh(file);
			if (fh.open(QFile::ReadOnly)) {
				ret = fh.readAll();
				fh.close();
			}
			else {
				qWarning() << "Could not read file" << file << "error:" << fh.errorString();
			}
			return ret;
		}

};

}
