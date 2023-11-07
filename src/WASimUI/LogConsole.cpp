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

#include "LogConsole.h"

#include "Utils.h"
#include "Widgets.h"

#include "client/WASimClient.h"

using namespace WASimUiNS;
using namespace WASimCommander;
using namespace WASimCommander::Client;
using namespace WASimCommander::Enums;

LogConsole::LogConsole(QWidget *parent)
	: QWidget(parent),
	logModel{new LogRecordsModel(this)}
{
	setObjectName(QStringLiteral("LogConsole"));

	ui.setupUi(this);

	ui.logView->setModel(logModel);
	ui.logView->sortByColumn(LogRecordsModel::COL_TS, Qt::AscendingOrder);
	//ui.logView->horizontalHeader()->setSortIndicator(LogRecordsModel::COL_TS, Qt::AscendingOrder);
	ui.logView->horizontalHeader()->setSortIndicatorShown(false);
	ui.logView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	ui.logView->horizontalHeader()->setSectionsMovable(true);
	ui.logView->horizontalHeader()->resizeSection(LogRecordsModel::COL_LEVEL, 70);
	ui.logView->horizontalHeader()->resizeSection(LogRecordsModel::COL_TS, 110);
	ui.logView->horizontalHeader()->resizeSection(LogRecordsModel::COL_SOURCE, 20);
	ui.logView->horizontalHeader()->setToolTip(tr("Severity Level | Timestamp | Source | Message"));

	// Set and connect Log Level combo boxes for Client and Server logging levels
	ui.cbLogLevelCallback->setProperties(      (LogLevel)-1, LogFacility::Remote,  LogSource::Client);
	ui.cbLogLevelFile->setProperties(          (LogLevel)-1, LogFacility::File,    LogSource::Client);
	ui.cbLogLevelConsole->setProperties(       (LogLevel)-1, LogFacility::Console, LogSource::Client);
	ui.cbLogLevelServer->setProperties(        (LogLevel)-1, LogFacility::Remote,  LogSource::Server);
	ui.cbLogLevelServerFile->setProperties(    (LogLevel)-1, LogFacility::File,    LogSource::Server);  // unknown level at startup
	ui.cbLogLevelServerConsole->setProperties( (LogLevel)-1, LogFacility::Console, LogSource::Server);  // unknown level at startup
	// Since the LogLevelComboBox types store the facility and source properties (which we just set), we can use one event handler for all of them.
	auto setLogLevel = [=](LogLevel level) {
		if (LogLevelComboBox *cb = qobject_cast<LogLevelComboBox*>(sender()))
			if (!!wsClient)
				wsClient->setLogLevel(level, cb->facility(), cb->source());
	};
	connect(ui.cbLogLevelCallback,      &LogLevelComboBox::levelChanged, this, setLogLevel);
	connect(ui.cbLogLevelFile,          &LogLevelComboBox::levelChanged, this, setLogLevel);
	connect(ui.cbLogLevelConsole,       &LogLevelComboBox::levelChanged, this, setLogLevel);
	connect(ui.cbLogLevelServer,        &LogLevelComboBox::levelChanged, this, setLogLevel);
	connect(ui.cbLogLevelServerFile,    &LogLevelComboBox::levelChanged, this, setLogLevel);
	connect(ui.cbLogLevelServerConsole, &LogLevelComboBox::levelChanged, this, setLogLevel);

#define FILTER_ACTION(LVL, NAME, BTN) { \
		QAction *act = new QAction(QIcon(Utils::iconNameForLogLevel(LogLevel::LVL)), tr("Toggle %1 Messages").arg(NAME), this); \
		act->setToolTip(tr("Toggle visibility of %1-level log messages.").arg(NAME));  \
		act->setCheckable(true);  \
		act->setChecked(true);  \
		ui.btnLogFilt_##BTN->setDefaultAction(act);  \
		addAction(act); \
		connect(act, &QAction::triggered, this, [this](bool en) { logModel->setLevelFilter(LogLevel::LVL, !en); }); \
	}
	FILTER_ACTION(Error, tr("Error"), ERR);
	FILTER_ACTION(Warning, tr("Warning"), WRN);
	FILTER_ACTION(Info, tr("Info"), INF);
	FILTER_ACTION(Debug, tr("Debug"), DBG);
	FILTER_ACTION(Trace, tr("Trace"), TRC);
#undef FILTER_ACTION

#define FILTER_ACTION(SRC, NAME, BTN) { \
		QAction *act = new QAction(QIcon(Utils::iconNameForLogSource(+##SRC)), tr("Toggle %1 Records").arg(NAME), this); \
		act->setToolTip(tr("Toggle visibility of log messages from %1.").arg(NAME));  \
		act->setCheckable(true);  \
		act->setChecked(true);  \
		ui.btnLogFilt_##BTN->setDefaultAction(act);  \
		addAction(act); \
		connect(act, &QAction::triggered, this, [this](bool en) { logModel->setSourceFilter(+##SRC, !en); }); \
	}
	FILTER_ACTION(LogSource::Server, tr("Server"), Server);
	FILTER_ACTION(LogSource::Client, tr("Client"), Client);
	FILTER_ACTION(LogRecordsModel::LogSource::UI, tr("UI"), UI);
#undef FILTER_ACTION

	QIcon logPauseIcon(QStringLiteral("pause.glyph"));
	logPauseIcon.addFile(QStringLiteral("play_arrow.glyph"), QSize(), QIcon::Normal, QIcon::On);
	QAction *pauseLogScrollAct = new QAction(logPauseIcon, tr("Pause Log Scroll"), this);
	pauseLogScrollAct->setToolTip(tr("<p>Toggle scrolling of the log window. Scrolling can also be paused by selecting a log entry row.</p>"));
	pauseLogScrollAct->setCheckable(true);
	ui.btnLogPause->setDefaultAction(pauseLogScrollAct);
	addAction(pauseLogScrollAct);
	connect(pauseLogScrollAct, &QAction::triggered, this, [this](bool en) { if (!en) ui.logView->selectionModel()->clear(); });  // clear log view selection on "un-pause"

	QAction *clearLogWindowAct = new QAction(QIcon(QStringLiteral("delete.glyph")), tr("Clear Log Window"), this);
	clearLogWindowAct->setToolTip(tr("Clear the log window."));
	ui.btnLogClear->setDefaultAction(clearLogWindowAct);
	addAction(clearLogWindowAct);
	connect(clearLogWindowAct, &QAction::triggered, this, [this]() { logModel->clear(); });

	QIcon wordWrapIcon(QStringLiteral("wrap_text.glyph"));
	wordWrapIcon.addFile(QStringLiteral("notes.glyph"), QSize(), QIcon::Normal, QIcon::On);
	QAction *wordWrapLogWindowAct = new QAction(wordWrapIcon, tr("Log Word Wrap"), this);
	wordWrapLogWindowAct->setToolTip(tr("Toggle word wrapping of the log window."));
	wordWrapLogWindowAct->setCheckable(true);
	wordWrapLogWindowAct->setChecked(true);
	ui.btnLogWordWrap->setDefaultAction(wordWrapLogWindowAct);
	addAction(wordWrapLogWindowAct);
	connect(wordWrapLogWindowAct, &QAction::toggled, this, [this](bool chk) { ui.logView->setWordWrap(chk); ui.logView->resizeRowsToContents(); });

	// connect the log model record added signal to make sure last record remains in view, unless scroll lock is enabled
	connect(logModel, &LogRecordsModel::recordAdded, this, [=](const QModelIndex &i) {
		// make sure log view scroll to bottom on insertions, unless a row is selected or scroll pause is set.
		ui.logView->resizeRowToContents(i.row());
		if (!pauseLogScrollAct->isChecked() && !ui.logView->selectionModel()->hasSelection())
			ui.logView->scrollToBottom(/*i, QAbstractItemView::PositionAtBottom*/);
	});
	// connect log viewer selection model to show pause button active while there is a selection
	connect(ui.logView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=](const QItemSelection &sel, const QItemSelection&) {
		pauseLogScrollAct->setChecked(ui.logView->selectionModel()->hasSelection());
	});

}

LogConsole::~LogConsole()
{}

void LogConsole::setClient(WASimCommander::Client::WASimClient *c) {
	wsClient = c;
	// Log messages can go right to the log records model. Log messages may arrive at any time, possibly from different threads,
	// so placing them into a model allow proper sorting (and filtering).
	connect(this, &LogConsole::logMessageReady, logModel, &LogRecordsModel::addRecord, Qt::QueuedConnection);
	c->setLogCallback([=](const LogRecord &l, LogSource s) { emit logMessageReady(l, +s); });

	ui.cbLogLevelCallback->setLevel(wsClient->logLevel(LogFacility::Remote,  LogSource::Client));
	ui.cbLogLevelFile->setLevel(wsClient->logLevel(    LogFacility::File,    LogSource::Client));
	ui.cbLogLevelConsole->setLevel(wsClient->logLevel( LogFacility::Console, LogSource::Client));
	ui.cbLogLevelServer->setLevel(wsClient->logLevel(  LogFacility::Remote,  LogSource::Server));
}

LogRecordsModel *LogConsole::getModel() const { return logModel; }

void LogConsole::saveSettings() const
{
	QSettings set;
	set.beginGroup(objectName());
	set.setValue(QStringLiteral("logViewHeaderState"), ui.logView->horizontalHeader()->saveState());
	set.endGroup();
}

void LogConsole::loadSettings()
{
	QSettings set;
	set.beginGroup(objectName());
	if (set.contains(QStringLiteral("logViewHeaderState")))
		ui.logView->horizontalHeader()->restoreState(set.value(QStringLiteral("logViewHeaderState")).toByteArray());
	set.endGroup();
}

void LogConsole::logMessage(int level, const QString & msg) const
{
	LogRecord l((LogLevel)level, qPrintable(msg));
	emit logMessageReady(l, +LogRecordsModel::LogSource::UI);
}
