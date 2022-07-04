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

#include <QApplication>
#include <QLoggingCategory>
#include <QSettings>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include "WASimUI.h"
#include "Utils.h"

#define MESSAGE_LOG_PATTERN  \
	"[%{time process}]" \
	"[%{if-debug}--%{endif}%{if-info}++%{endif}%{if-warning}!!%{endif}%{if-critical}**%{endif}%{if-fatal}XX%{endif}] " \
	"%{function}() @%{line} - %{message}"

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
	// if started from console/CLI then reopen stdout/stderr pipes to it for logging.
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}
#endif

	// configure GlyphIconEngine options
	qputenv("GI_LOAD_FONTS_FROM", QByteArrayLiteral(":/fonts"));
	qputenv("GI_LOAD_CMAPS_FROM", QByteArrayLiteral(":/cmaps"));
	qputenv("GI_DEFAULT_FONT", QByteArrayLiteral("Material Icons"));
	//qputenv("GI_DYNAMIC_POLISH", QByteArrayLiteral("0"));
	//qputenv("GI_NO_PAINT_CACHE",   QByteArrayLiteral("0"));

	// all our images are scalable
	qputenv("QT_HIGHDPI_DISABLE_2X_IMAGE_LOADING", QByteArrayLiteral("1"));

	// configure logging categories
	//QLoggingCategory::setFilterRules(QStringLiteral(
		//"glyphIcon.*.info = true\n"
		//"glyphIcon.*.debug = true\n"
	//));
	// set up message logging pattern
	qSetMessagePattern(MESSAGE_LOG_PATTERN);

	QApplication::setStyle("Fusion");  // things style better with Fusion
	QApplication a(argc, argv);
	QApplication::setOrganizationName(WSMCMND_PROJECT_NAME);
	//QApplication::setOrganizationDomain(WSMCMND_PROJECT_URL);
	QApplication::setApplicationName(WSMCMND_GUI_NAME);
	QApplication::setApplicationDisplayName(WSMCMND_APP_NAME);
	QApplication::setApplicationVersion(WSMCMND_VERSION_STR);
	QApplication::setWindowIcon(WASimUiNS::Utils::appIcon());
#ifdef Q_OS_WIN
	// Qt default MS Shell Dlg font is wrong.
	QApplication::setFont(QFont(QStringLiteral("Segoe UI"), 9));
#endif

	QSettings::setDefaultFormat(QSettings::IniFormat);

	WASimUI w;  // will show() itself

	a.setQuitOnLastWindowClosed(true);
	return a.exec();
}
