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

#include <iostream>

#include <QAction>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMenu>
#include <QMetaType>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QItemSelectionModel>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QToolBar>

#include "WASimUI.h"
#include "RequestsModel.h"
#include "EventsModel.h"
#include "LogRecordsModel.h"
#include "Utils.h"
#include "Widgets.h"

using namespace WASimUiNS;
using namespace WASimCommander;
using namespace WASimCommander::Client;
using namespace WASimCommander::Enums;

// -------------------------------------------------------------
// WASimUIPrivate
// -------------------------------------------------------------

class WASimUIPrivate
{
	friend class WASimUI;
	Q_DECLARE_TR_FUNCTIONS(WASimUI)
public:
	WASimUI *q;
	Ui::WASimUIClass *ui;
	WASimClient *client;
	StatusWidget *statWidget;
	RequestsModel *reqModel;
	EventsModel *eventsModel;
	LogRecordsModel *logModel;
	QAction *initAct = nullptr;
	QAction *connectAct = nullptr;
	ClientStatus clientStatus = ClientStatus::Idle;
	QString lastRequestsFile;
	QString lastEventsFile;
	uint32_t nextCmdToken = 0x0000FFFF;

	WASimUIPrivate(WASimUI *q) : q(q), ui(&q->ui),
		reqModel(new RequestsModel(q)),
		eventsModel(new EventsModel(q)),
		logModel(new LogRecordsModel(q)),
		statWidget(new StatusWidget(q)),
		client(new WASimClient(0xDEADBEEF))
	{
		setupClient();
	}

	void setupClient()
	{
		//client->setNetworkConfigurationId(0);
		//client->setDefaultTimeout(3000);
		client->setLogLevel(LogLevel::Info, LogFacility::All);
		client->setLogLevel(LogLevel::Info, LogFacility::Remote, LogSource::Server);
		client->setClientEventCallback(&WASimUI::clientEvent, q);
		client->setCommandResultCallback(&WASimUI::commandResultReady, q);
		client->setDataCallback(&WASimUI::dataResultReady, q);
		client->setListResultsCallback(&WASimUI::listResults, q);
		client->setLogCallback([=](const LogRecord &l, LogSource s) { emit q->logMessageReady(l, +s); });
	}

	void runCalcCode()
	{
		if (ui->cbCalculatorCode->currentText().isEmpty()) {
			logUiMessage(tr("Calculator string is empty."), CommandId::Exec);
			return;
		}

		double fRes = 0.0;
		std::string sRes = std::string();
		CalcResultType resType = (CalcResultType)ui->cbCalcResultType->currentData().toUInt();
		if SUCCEEDED(client->executeCalculatorCode(ui->cbCalculatorCode->currentText().toStdString(), resType, &fRes, &sRes)) {
			if (resType == CalcResultType::Double || resType == CalcResultType::Integer)
				ui->leCalcResult->setText(QStringLiteral("%1").arg(fRes, 0, 'f', (resType == CalcResultType::Double ? 7 : 0)));
			else if (resType == CalcResultType::String || resType == CalcResultType::Formatted)
				ui->leCalcResult->setText(sRes.empty() ? tr("Result returned empty string") : QString::fromStdString(sRes));
			else
				ui->leCalcResult->setText(tr("Execute succeeded w/out return value."));
		}
		else {
			ui->leCalcResult->setText(tr("Execute failed, check log."));
		}

	}

	void copyCalcCodeToRequest()
	{
		setRequestFormId(-1);
		ui->rbRequestType_Calculated->setChecked(true);
		ui->cbNameOrCode->setCurrentText(ui->cbCalculatorCode->currentText());
		ui->cbRequestCalcResultType->setCurrentIndex(ui->cbCalcResultType->currentIndex());
		//ui->cbValueSize->setCurrentData(Utils::calcResultTypeToMetaType(CalcResultType(ui->cbCalcResultType->currentData().toUInt())));
	}

	void refreshLVars() {
		client->list();
	}

	void lookupItem()
	{
		const QString &varName = ui->cbLookupName->currentText();
		if (varName.isEmpty()) {
			logUiMessage(tr("Lookup item name is empty."), CommandId::Lookup);
			return;
		}
		int32_t result;
		const HRESULT hr = client->lookup((LookupItemType)ui->cbLookupItemType->currentData().toUInt(), varName.toStdString(), &result);
		if SUCCEEDED(hr)
			ui->leLookupResult->setText(QString("%1").arg(result));
		else
			ui->leLookupResult->setText(tr("Lookup failed."));
	}

	void getLocalVar()
	{
		const char vtype = ui->cbGetSetVarType->currentData().toChar().toLatin1();
		const QString &varName = vtype == 'L' ? ui->cbLvars->currentText() : ui->cbVariableName->currentText();
		if (varName.isEmpty()) {
			logUiMessage(tr("Variable name is empty."), CommandId::Get);
			return;
		}
		double result;
		const HRESULT hr = client->getVariable(VariableRequest(vtype, varName.toStdString(), ui->cbSetVarUnitName->currentText().toStdString(), ui->sbGetSetSimVarIndex->value()), &result);
		if (hr == S_OK)
			ui->leVarResult->setText(QString("%1").arg(result, 0, 'f', 7));
		else
			ui->leVarResult->setText(QString("Error: 0x%1").arg((quint32)hr, 8, 16, QChar('0')));
	}

	void setLocalVar(bool create = false) {
		const char vtype = ui->cbGetSetVarType->currentData().toChar().toLatin1();
		const QString &varName = vtype == 'L' ? ui->cbLvars->currentText() : ui->cbVariableName->currentText();
		if (varName.isEmpty()) {
			logUiMessage(tr("Variable name is empty."), CommandId::Set);
			return;
		}
		if (vtype == 'L' && create)
			client->setOrCreateLocalVariable(varName.toStdString(), ui->dsbSetVarValue->value());
		else
			client->setVariable(VariableRequest(vtype, varName.toStdString(), ui->cbSetVarUnitName->currentText().toStdString(), ui->sbGetSetSimVarIndex->value()), ui->dsbSetVarValue->value());
	}

	void copyLocalVarToRequest()
	{
		setRequestFormId(-1);
		const char vtype = ui->cbGetSetVarType->currentData().toChar().toLatin1();
		ui->rbRequestType_Named->setChecked(true);
		ui->cbNameOrCode->setCurrentText(vtype == 'L' ? ui->cbLvars->currentText() : ui->cbVariableName->currentText());
		ui->cbVariableType->setCurrentData(vtype);
		ui->cbUnitName->setCurrentText(ui->cbSetVarUnitName->currentText());
		ui->cbValueSize->setCurrentData(+QMetaType::Double);
	}

	void sendCommandForm()
	{
		if (!client->isConnected()) {
			logUiMessage(tr("Not connected to server, cannot send command."), CommandId::Nak);
			return;
		}
		Command cmd((CommandId)ui->cbCommandId->currentData().toUInt(), ui->sbCmdUData->value(), qPrintable(ui->leCmdSData->text()), ui->sbCmdFData->value(), nextCmdToken++);
		client->sendCommand(cmd);
	}


	// Data Requests handling -------------------------------------------------

	void toggleRequestType()
	{
		bool isCalc = ui->rbRequestType_Calculated->isChecked();
		ui->cbRequestCalcResultType->setVisible(isCalc);
		ui->lblRequestCalcResultType->setVisible(isCalc);
		ui->cbVariableType->setVisible(!isCalc);
		//ui->lblUnit->setVisible(!isCalc);
		//ui->cbUnitName->setVisible(!isCalc);
		toggleRequestVariableType();
	}

	void toggleRequestVariableType()
	{
		bool isCalc = ui->rbRequestType_Calculated->isChecked();
		bool needIdx = !isCalc && ui->cbVariableType->currentData().toChar() == 'A';
		ui->lblIndex->setVisible(needIdx);
		ui->sbSimVarIndex->setVisible(needIdx);
		bool hasUnit = !isCalc && ui->cbVariableType->currentText().contains('*');
		ui->lblUnit->setVisible(hasUnit);
		ui->cbUnitName->setVisible(hasUnit);
	}

	void setRequestFormId(uint32_t id)
	{
		ui->wRequestForm->setProperty("requestId", id);
		if ((int)id > -1) {
			ui->lblCurrentRequestId->setText(QString("%1").arg(id));
			ui->pbUpdateRequest->setEnabled(true);
		}
		else  {
			ui->lblCurrentRequestId->setText(tr("New"));
			ui->pbUpdateRequest->setEnabled(false);
		}
	}

	void handleRequestForm(bool update)
	{
		if (ui->cbNameOrCode->currentText().isEmpty() || ui->cbValueSize->currentText().isEmpty()) {
			logUiMessage(tr("Name/Code and Size cannot be empty."), CommandId::Subscribe);
			return;
		}

		uint32_t requestId = ui->wRequestForm->property("requestId").toUInt();
		if ((int)requestId < 0 || !update)
			requestId = reqModel->nextRequestId();

		RequestRecord req(requestId);
		req.requestType = ui->rbRequestType_Calculated->isChecked() ? RequestType::Calculated : RequestType::Named;
		if (req.requestType == RequestType::Calculated) {
			req.calcResultType = (CalcResultType)ui->cbRequestCalcResultType->currentData().toUInt();
		}
		else {
			req.varTypePrefix = ui->cbVariableType->currentData().toChar().toLatin1();
			req.simVarIndex = ui->sbSimVarIndex->value();
		}
		req.setNameOrCode(qPrintable(ui->cbNameOrCode->currentText()));
		req.setUnitName(qPrintable(ui->cbUnitName->currentText()));

		int metaType = QMetaType::UnknownType;
		if (ui->cbValueSize->currentData().isValid())
			req.valueSize = Utils::metaTypeToSimType(req.metaType = ui->cbValueSize->currentData().toInt());
		else
			req.valueSize = ui->cbValueSize->currentText().toUInt();

		req.period = (UpdatePeriod)ui->cbPeriod->currentData().toUInt();
		req.interval = ui->sbInterval->value();
		if (req.period == UpdatePeriod::Millisecond)
			req.interval = qMax(req.interval, 25U);
		if (ui->dsbDeltaEpsilon->isEnabled())
			req.deltaEpsilon = ui->dsbDeltaEpsilon->value();

		//std::cout << req << std::endl;
		setRequestFormId(req.requestId);

		if FAILED(client->saveDataRequest(req))
			return;
		const QModelIndex idx = reqModel->addRequest(req);
		ui->requestsView->selectRow(idx.row());
		ui->requestsView->scrollTo(idx);
	}

	void populateRequestForm(const QModelIndex &idx)
	{
		const RequestRecord &req = reqModel->getRequest(idx.row());
		if ((int)req.requestId < 0)
			return;
		std::cout << req << std::endl;
		setRequestFormId(req.requestId);

		if (req.requestType == RequestType::Calculated) {
			ui->rbRequestType_Calculated->setChecked(true);
			ui->cbRequestCalcResultType->setCurrentData(+req.calcResultType);
		}
		else {
			ui->rbRequestType_Named->setChecked(true);
			ui->cbVariableType->setCurrentData(req.varTypePrefix);
			ui->cbUnitName->setCurrentText(req.unitName);
			ui->sbSimVarIndex->setValue(req.simVarIndex);
		}
		if (req.metaType == QMetaType::UnknownType)
			ui->cbValueSize->setCurrentText(QString("%1").arg(req.valueSize));
		else
			ui->cbValueSize->setCurrentData(req.metaType);
		ui->cbNameOrCode->setCurrentText(req.nameOrCode);
		ui->cbPeriod->setCurrentData(+req.period);
		ui->sbInterval->setValue(req.interval);
		ui->dsbDeltaEpsilon->setValue(req.deltaEpsilon);
	}

	void removeRequests(const QModelIndexList &list)
	{
		for (const QModelIndex &idx : list) {
			const uint32_t requestId = reqModel->requestId(idx.row());
			if ((int)requestId > -1 /*&& reqModel->requestStatus(idx.row())*/)
				client->removeDataRequest(requestId);
		}
		reqModel->removeRequests(list);
	}

	void removeSelectedRequests() {
		removeRequests(reqModel->flattenIndexList(ui->requestsView->selectionModel()->selectedIndexes()));
	}

	void removeAllRequests() {
		const int rows = reqModel->rowCount();
		if (!rows)
			return;
		for (int i = 0; i < rows; ++i)
			client->removeDataRequest(reqModel->requestId(i));
		reqModel->setRowCount(0);
	}

	void updateSelectedRequests()
	{
		if (!client->isConnected()) {
			logUiMessage("Server not connected, cannot request updates.", CommandId::Update);
			return;
		}
		const QModelIndexList list = reqModel->flattenIndexList(ui->requestsView->selectionModel()->selectedIndexes());
		for (const QModelIndex &idx : list)
			client->updateDataRequest(reqModel->requestId(idx.row()));
	}

	void saveRequests()
	{
		if (!reqModel->rowCount())
			return;

		const QString &fname = QFileDialog::getSaveFileName(q, tr("Select a file for saved Requests"), lastRequestsFile, QStringLiteral("INI files (*.ini)"));
		if (fname.isEmpty())
			return;
		lastRequestsFile = fname;
		const int rows = reqModel->saveToFile(fname);
		logUiMessage(tr("Saved %1 Data Request(s) to file: %2").arg(rows).arg(fname), CommandId::Ack, LogLevel::Info);
	}

	void loadRequests(bool replace)
	{
		const QString &fname = QFileDialog::getOpenFileName(q, tr("Select a saved Requests file"), lastRequestsFile, QStringLiteral("INI files (*.ini)"));
		if (fname.isEmpty())
			return;
		lastRequestsFile = fname;
		if (replace)
			removeAllRequests();
		const QList<RequestRecord> &added = reqModel->loadFromFile(fname);
		for (const DataRequest &req : added)
			client->saveDataRequest(req);

		logUiMessage(tr("Loaded %1 Data Request(s) from file: %2").arg(added.count()).arg(fname), CommandId::Ack, LogLevel::Info);
	}


	// Registered calc events -------------------------------------------------

	bool registerEventWithClient(const EventRecord &evt) {
		return SUCCEEDED(client->registerEvent(evt));
	}

	void setEventFormId(uint32_t id)
	{
		ui->wCalcForm->setProperty("eventId", id);
		if ((int)id > -1) {
			ui->btnUpdateEvent->setVisible(true);
			//ui->lblCurrentEventId->setText(QString("%1").arg(id));
		}
		else  {
			ui->btnUpdateEvent->setVisible(false);
			//ui->lblCurrentEventId->setText(tr("New"));
		}
	}

	void registerEvent(bool update = false)
	{
		if (ui->cbCalculatorCode->currentText().isEmpty()) {
			logUiMessage(tr("Code cannot be empty."), CommandId::Register);
			return;
		}

		uint32_t evId = ui->wCalcForm->property("eventId").toUInt();
		if ((int)evId < 0 || !update)
			evId = eventsModel->nextEventId();

		const EventRecord ev(evId, ui->cbCalculatorCode->currentText().toStdString(), ui->leEventName->text().toStdString());
		if (!registerEventWithClient(ev))
			return;

		eventsModel->addEvent(ev);

		// reset the form
		setEventFormId(-1);
	}

	void populateEventForm(const QModelIndex &idx)
	{
		const EventRecord &req = eventsModel->getEvent(idx.row());
		if ((int)req.eventId < 0)
			return;
		qDebug() << req.eventId << req.code.c_str() << req.name.c_str();
		setEventFormId(req.eventId);

		ui->cbCalculatorCode->setCurrentText(QString::fromStdString(req.code));
		ui->leEventName->setText(QString::fromStdString(req.name));
	}

	void removeEvents(const QModelIndexList &list)
	{
		for (const QModelIndex &idx : list) {
			const uint32_t evId = eventsModel->eventId(idx.row());
			if ((int)evId > -1)
				client->removeEvent(evId);
		}
		eventsModel->removeEvents(list);
	}

	void removeSelectedEvents() {
		removeEvents(eventsModel->flattenIndexList(ui->eventsView->selectionModel()->selectedIndexes()));
	}

	void removeAllEvents() {
		const int rows = eventsModel->rowCount();
		if (!rows)
			return;

		for (int i = 0; i < rows; ++i)
			client->removeEvent(eventsModel->eventId(i));
		eventsModel->setRowCount(0);
	}

	void transmitSelectedEvents()
	{
		if (!client->isConnected()) {
			logUiMessage("Server not connected, cannot transmit event.", CommandId::Transmit);
			return;
		}
		const QModelIndexList list = eventsModel->flattenIndexList(ui->eventsView->selectionModel()->selectedIndexes());
		for (const QModelIndex &idx : list)
			client->transmitEvent(eventsModel->eventId(idx.row()));
	}

	void saveEvents()
	{
		if (!eventsModel->rowCount())
			return;

		const QString &fname = QFileDialog::getSaveFileName(q, tr("Select a file for saved Events"), lastEventsFile, QStringLiteral("INI files (*.ini)"));
		if (fname.isEmpty())
			return;
		lastEventsFile = fname;
		const int rows = eventsModel->saveToFile(fname);
		logUiMessage(tr("Saved %1 Event(s) to file: %2").arg(rows).arg(fname), CommandId::Ack, LogLevel::Info);
	}

	void loadEvents(bool replace)
	{
		const QString &fname = QFileDialog::getOpenFileName(q, tr("Select a saved Events file"), lastEventsFile, QStringLiteral("INI files (*.ini)"));
		if (fname.isEmpty())
			return;
		lastEventsFile = fname;
		if (replace)
			removeAllEvents();
		const QList<EventRecord> &added = eventsModel->loadFromFile(fname);
		for (const EventRecord &ev : added)
			registerEventWithClient(ev);

		logUiMessage(tr("Loaded %1 Data Request(s) from file: %2").arg(added.count()).arg(fname), CommandId::Ack, LogLevel::Info);
	}


	// Utilities  -------------------------------------------------

	void logUiMessage(const QString &msg, CommandId cmd = CommandId::None, LogLevel level = LogLevel::Error)
	{
		LogRecord l(level, qPrintable(msg));
		emit q->logMessageReady(l, +LogRecordsModel::LogSource::UI);
		if (cmd != CommandId::None)
			emit q->commandResultReady(Command(level == LogLevel::Error ? CommandId::Nak : CommandId::Ack, +cmd, qPrintable(msg)));
	}


	// Save/Load UI settings  -------------------------------------------------

	void saveSettings()
	{
		QSettings set;
		set.setValue(QStringLiteral("mainWindowGeo"), q->saveGeometry());
		set.setValue(QStringLiteral("mainWindowState"), q->saveState());
		set.setValue(QStringLiteral("eventsViewHeaderState"), ui->eventsView->horizontalHeader()->saveState());
		set.setValue(QStringLiteral("logViewHeaderState"), ui->logView->horizontalHeader()->saveState());

		set.setValue(QStringLiteral("useDarkTheme"), Utils::isDarkStyle());
		set.setValue(QStringLiteral("lastRequestsFile"), lastRequestsFile);
		set.setValue(QStringLiteral("lastEventsFile"), lastEventsFile);

		set.beginGroup(QStringLiteral("EditableSelectors"));
		const QList<DeletableItemsComboBox *> editable = q->findChildren<DeletableItemsComboBox *>();
		for (DeletableItemsComboBox *cb : editable)
			set.setValue(cb->objectName(), cb->editedItems());
		set.endGroup();
	}

	void readSettings()
	{
		QSettings set;
		if (set.contains(QStringLiteral("mainWindowGeo")))
			q->restoreGeometry(set.value(QStringLiteral("mainWindowGeo")).toByteArray());
		if (set.contains(QStringLiteral("mainWindowState")))
			q->restoreState(set.value(QStringLiteral("mainWindowState")).toByteArray());
		if (set.contains(QStringLiteral("eventsViewHeaderState")))
			ui->eventsView->horizontalHeader()->restoreState(set.value(QStringLiteral("eventsViewHeaderState")).toByteArray());
		if (set.contains(QStringLiteral("logViewHeaderState")))
			ui->logView->horizontalHeader()->restoreState(set.value(QStringLiteral("logViewHeaderState")).toByteArray());

		const QString defaultFileLoc = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
		lastRequestsFile = set.value(QStringLiteral("lastRequestsFile"), defaultFileLoc + QStringLiteral("/WASimUI-requests.ini")).toString();
		lastEventsFile = set.value(QStringLiteral("lastEventsFile"), defaultFileLoc + QStringLiteral("/WASimUI-events.ini")).toString();
		const bool useDark = set.value(QStringLiteral("useDarkTheme"), true).toBool();
		if (useDark != Utils::isDarkStyle())
			Utils::toggleAppStyle(useDark);

		set.beginGroup(QStringLiteral("EditableSelectors"));
		const QList<DeletableItemsComboBox *> editable = q->findChildren<DeletableItemsComboBox *>();
		for (DeletableItemsComboBox *cb : editable) {
			if (set.contains(cb->objectName())) {
				QStringList val = set.value(cb->objectName()).toStringList();
				if (val.isEmpty())
					continue;
				if (cb->insertPolicy() != QComboBox::InsertAlphabetically)
					std::sort(val.begin(), val.end());
				cb->insertEditedItems(val);
			}
		}
		set.endGroup();
	}

};


// -------------------------------------------------------------
// WASimUI
// -------------------------------------------------------------

WASimUI::WASimUI(QWidget *parent) :
	QMainWindow(parent),
	d(new WASimUIPrivate(this))
{
	// need to register WASimAPI structs to use them in queued signals/slots
	qRegisterMetaType<Command>("WASimCommander::Command");
	qRegisterMetaType<LogRecord>("WASimCommander::LogRecord");
	qRegisterMetaType<ClientEvent>("WASimCommander::Client::ClientEvent");
	qRegisterMetaType<ListResult>("WASimCommander::Client::ListResult");
	qRegisterMetaType<DataRequestRecord>("WASimCommander::Client::DataRequestRecord");
	// register stream operators for our own custom types so they can be handled by QSettings
	qRegisterMetaTypeStreamOperators<RequestRecord>("RequestRecord");
	qRegisterMetaTypeStreamOperators<EventRecord>("EventRecord");

	// Set up QtDesigner generated form UI.
	ui.setupUi(this);
	setWindowTitle(qApp->applicationName() + " v" WSMCMND_VERSION_STR);

	// Move central widget into a dock widget, makes layouts more flexible.
	QDockWidget *centralDock = new QDockWidget(tr("Control Panel"), this);
	centralDock->setObjectName(QStringLiteral("FormsDockWidget"));
	centralDock->setAllowedAreas(Qt::AllDockWidgetAreas);
	centralDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	// Use a custom title bar for the "central" dock widget so it looks more "integrated".
	QLabel *titlebar = new QLabel(centralDock->windowTitle(), centralDock);
	titlebar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	titlebar->setFrameShape(QFrame::Box);
	titlebar->setStyleSheet(QStringLiteral("QLabel { padding-bottom: 4px; border: 0; border-bottom: 1px solid palette(mid); }"));
	titlebar->setIndent(9);
	titlebar->setFont(QFont("Segoe UI", 8));
	titlebar->setFocusPolicy(Qt::NoFocus);
	centralDock->setTitleBarWidget(titlebar);
	centralDock->setWidget(takeCentralWidget());
	addDockWidget(Qt::TopDockWidgetArea, centralDock);

	// Re-arrange the dock windows.
	splitDockWidget(centralDock, ui.dwLog, Qt::Vertical);
	splitDockWidget(centralDock, ui.dwEventsList, Qt::Horizontal);
	splitDockWidget(centralDock, ui.dwRequests, Qt::Vertical);
	//splitDockWidget(ui.dwRequests, ui.dwLog, Qt::Vertical);

	// Show last command status on the bottom status area using a custom widget
	CommandStatusWidget *cmdStatWidget = new CommandStatusWidget(ui.statusBar);
	ui.statusBar->addWidget(cmdStatWidget, 1);
	connect(this, &WASimUI::commandResultReady, cmdStatWidget, &CommandStatusWidget::setStatus, Qt::QueuedConnection);

	// set spin box actual min/max values
	ui.dsbSetVarValue->setMinimum(-DBL_MAX);
	ui.dsbSetVarValue->setMaximum(DBL_MAX);
	ui.dsbDeltaEpsilon->setMinimum(-1.0);
	ui.dsbDeltaEpsilon->setMaximum(FLT_MAX);
	// maximum lengths for text edit boxes
	ui.leCmdSData->setMaxLength(STRSZ_CMD);
	ui.cbCalculatorCode->lineEdit()->setMaxLength(STRSZ_CMD);
	ui.cbVariableName->lineEdit()->setMaxLength(STRSZ_CMD);
	ui.cbLvars->lineEdit()->setMaxLength(STRSZ_CMD);
	ui.cbLookupName->lineEdit()->setMaxLength(STRSZ_CMD);
	ui.leEventName->setMaxLength(STRSZ_ENAME);
	ui.cbNameOrCode->lineEdit()->setMaxLength(STRSZ_REQ);

	// Set up the Requests table view
	ui.requestsView->setModel(d->reqModel);
	ui.requestsView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.requestsView->horizontalHeader()->setSectionsMovable(true);
	// connect double click action to populate the request editor form
	connect(ui.requestsView, &QTableView::doubleClicked, this, [this](const QModelIndex &idx) { d->populateRequestForm(idx); });

	// Set up the Events table view
	ui.eventsView->setModel(d->eventsModel);
	ui.eventsView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	ui.eventsView->horizontalHeader()->setSectionsMovable(true);
	ui.eventsView->horizontalHeader()->resizeSection(EventsModel::COL_ID, 45);
	ui.eventsView->horizontalHeader()->resizeSection(EventsModel::COL_CODE, 140);
	// connect double click action to populate the event editor form
	connect(ui.eventsView, &QTableView::doubleClicked, this, [this](const QModelIndex &idx) { d->populateEventForm(idx); });

	// Set up the Log table view
	ui.logView->setModel(d->logModel);
	ui.logView->sortByColumn(LogRecordsModel::COL_TS, Qt::AscendingOrder);
	//ui.logView->horizontalHeader()->setSortIndicator(LogRecordsModel::COL_TS, Qt::AscendingOrder);
	ui.logView->horizontalHeader()->setSortIndicatorShown(false);
	ui.logView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	ui.logView->horizontalHeader()->setSectionsMovable(true);
	ui.logView->horizontalHeader()->resizeSection(LogRecordsModel::COL_LEVEL, 70);
	ui.logView->horizontalHeader()->resizeSection(LogRecordsModel::COL_TS, 110);
	ui.logView->horizontalHeader()->resizeSection(LogRecordsModel::COL_SOURCE, 20);
	ui.logView->horizontalHeader()->setToolTip(tr("Severity Level | Timestamp | Source | Message"));

	// Set initial state of Variables form, Local var type is default.
	ui.wOtherVarsForm->setVisible(false);
	ui.wGetSetSimVarIndex->setVisible(false);

	// Init the request and calc editor forms
	ui.wRequestForm->setProperty("requestId", -1);
	ui.wCalcForm->setProperty("eventId", -1);

	// Update the Data Request form UI based on default type.
	d->toggleRequestType();
	// Connect the request type radio buttons to toggle the UI.
	connect(ui.bgrpRequestType, QOverload<int, bool>::of(&QButtonGroup::buttonToggled), this, [this](int,bool) { d->toggleRequestType(); });
	// show/hide SimVar index spin box based on type of variable selected
	connect(ui.cbVariableType, &DataComboBox::currentDataChanged, this, [this](const QVariant&) { d->toggleRequestVariableType(); });
	// Connect the data request calc result type selector to choose a default result size
	connect(ui.cbRequestCalcResultType, &DataComboBox::currentDataChanged, this, [this](const QVariant &data) {
		ui.cbValueSize->setCurrentData(Utils::calcResultTypeToMetaType(CalcResultType(data.toUInt())));
	});
	// Connect the data request Size combo box to enable Delta E spin box for preset value types and disable it for other types.
	connect(ui.cbValueSize, &DataComboBox::currentTextChanged, this, [this](const QString &) {
		const int mtype = ui.cbValueSize->currentData().toInt();
		ui.dsbDeltaEpsilon->setEnabled(mtype > QMetaType::UnknownType && mtype < QMetaType::User);
	});
	// Connect the Update Period combo box to en/disable Interval spin box based on selection.
	connect(ui.cbPeriod, &DataComboBox::currentDataChanged, this, [this](const QVariant &data) {
		ui.sbInterval->setEnabled(data.toUInt() >= +UpdatePeriod::Tick);
	});
	// connect the Data Request save/add buttons
	connect(ui.pbAddRequest, &QPushButton::clicked, this, [this]() { d->handleRequestForm(false); });
	connect(ui.pbUpdateRequest, &QPushButton::clicked, this, [this]() { d->handleRequestForm(true); });

	// Set and connect Log Level combo boxes for Client and Server logging levels
	ui.cbLogLevelCallback->setProperties(d->client->logLevel(     LogFacility::Remote,  LogSource::Client), LogFacility::Remote,  LogSource::Client);
	ui.cbLogLevelFile->setProperties(d->client->logLevel(         LogFacility::File,    LogSource::Client), LogFacility::File,    LogSource::Client);
	ui.cbLogLevelConsole->setProperties(d->client->logLevel(      LogFacility::Console, LogSource::Client), LogFacility::Console, LogSource::Client);
	ui.cbLogLevelServer->setProperties(d->client->logLevel(       LogFacility::Remote,  LogSource::Server), LogFacility::Remote,  LogSource::Server);
	ui.cbLogLevelServerFile->setProperties(                                                   (LogLevel)-1, LogFacility::File,    LogSource::Server);     // unknown level at startup
	ui.cbLogLevelServerConsole->setProperties(                                                (LogLevel)-1, LogFacility::Console, LogSource::Server);  // unknown level at startup
	// Since the LogLevelComboBox types store the facility and source properties (which we just set), we can use one event handler for all of them.
	auto setLogLevel = [=](LogLevel level) {
		if (LogLevelComboBox *cb = qobject_cast<LogLevelComboBox*>(sender()))
			d->client->setLogLevel(level, cb->facility(), cb->source());
	};
	connect(ui.cbLogLevelCallback,      &LogLevelComboBox::levelChanged, this, setLogLevel);
	connect(ui.cbLogLevelFile,          &LogLevelComboBox::levelChanged, this, setLogLevel);
	connect(ui.cbLogLevelConsole,       &LogLevelComboBox::levelChanged, this, setLogLevel);
	connect(ui.cbLogLevelServer,        &LogLevelComboBox::levelChanged, this, setLogLevel);
	connect(ui.cbLogLevelServerFile,    &LogLevelComboBox::levelChanged, this, setLogLevel);
	connect(ui.cbLogLevelServerConsole, &LogLevelComboBox::levelChanged, this, setLogLevel);

	// Connect our own signals for client callback handling in a thread-safe manner, marshaling back to GUI thread as needed.
	// The "signal" methods are "emitted" by the Client as callbacks, registered in Private::setupClient().
	connect(this, &WASimUI::clientEvent, this, &WASimUI::onClientEvent, Qt::QueuedConnection);
	connect(this, &WASimUI::listResults, this, &WASimUI::onListResults, Qt::QueuedConnection);
	// Data updates can go right to the requests model.
	connect(this, &WASimUI::dataResultReady, d->reqModel, &RequestsModel::setRequestValue, Qt::QueuedConnection);
	// Log messages can go right to the log records model. Log messages may arrive at any time, possibly from different threads,
	// so placing them into a model allow proper sorting (and filtering).
	connect(this, &WASimUI::logMessageReady, d->logModel, &LogRecordsModel::addRecord, Qt::QueuedConnection);
	d->logUiMessage("Hello!", CommandId::Ack, LogLevel::Info);

	// Set up actions for triggering various events. Actions are typically mapped to UI elements like buttons and menu items and can be reused in multiple places.

	// Connect/Disconnect Simulator.
	QIcon initIco(QStringLiteral("phone_in_talk.glyph"));
	initIco.addFile(QStringLiteral("call_end.glyph"), QSize(), QIcon::Mode::Normal, QIcon::State::On);
	d->initAct = new QAction(initIco, tr("Connect to Simulator"), this);
	d->initAct->setCheckable(true);
	connect(d->initAct, &QAction::triggered, this, [this]() {
		if (d->client->isInitialized())
			d->client->disconnectSimulator();
		else
			d->client->connectSimulator();
	}, Qt::QueuedConnection);

	// Connect/Disconnect WASim Server
	QIcon connIco(QStringLiteral("link.glyph"));
	connIco.addFile(QStringLiteral("link_off.glyph"), QSize(), QIcon::Mode::Normal, QIcon::State::On);
	d->connectAct = new QAction(connIco, tr("Connect to Server"), this);
	d->connectAct->setCheckable(true);
	connect(d->connectAct, &QAction::triggered, this, [this]() {
		if (d->client->isConnected())
			d->client->disconnectServer();
		else
			d->client->connectServer();
	}, Qt::QueuedConnection);

	// Ping the server.
	QAction *pingAct = new QAction(QIcon(QStringLiteral("leak_add.glyph")), tr("Ping Server"), this);
	connect(pingAct, &QAction::triggered, this, [this]() { d->client->pingServer(); });


	// Calculator code actions

	// Exec calculator code
	QAction *execCalcAct = new QAction(QIcon(QStringLiteral("IcoMoon-Free/calculator.glyph")), tr("Execute Calculator Code"), this);
	execCalcAct->setToolTip(tr("Execute Calculator Code"));
	execCalcAct->setDisabled(true);
	connect(execCalcAct, &QAction::triggered, this, [this]() { d->runCalcCode(); });
	ui.btnCalc->setDefaultAction(execCalcAct);

	// Register calculator code event
	QAction *regEventAct = new QAction(QIcon(QStringLiteral("control_point.glyph")), tr("Register Event"), this);
	regEventAct->setToolTip(tr("Register this calculator code as a new Event."));
	regEventAct->setDisabled(true);
	connect(regEventAct, &QAction::triggered, this, [this]() { d->registerEvent(false); });
	ui.btnAddEvent->setDefaultAction(regEventAct);

	// Save edited calculator code event
	QAction *saveEventAct = new QAction(QIcon(QStringLiteral("edit.glyph")), tr("Update Event"), this);
	saveEventAct->setToolTip(tr("Update existing event with new calculator code (name cannot be changed)."));
	connect(saveEventAct, &QAction::triggered, this, [this]() { d->registerEvent(true); });
	saveEventAct->setDisabled(true);
	ui.btnUpdateEvent->setDefaultAction(saveEventAct);
	ui.btnUpdateEvent->setVisible(false);

	// Copy calculator code as new Data Request
	QAction *copyCalcAct = new QAction(QIcon(QStringLiteral("move_to_inbox.glyph")), tr("Copy to Data Request"), this);
	copyCalcAct->setToolTip(tr("Copy Calculator Code to new Data Request"));
	copyCalcAct->setDisabled(true);
	connect(copyCalcAct, &QAction::triggered, this, [this]() { d->copyCalcCodeToRequest(); });
	ui.btnCopyCalcToRequest->setDefaultAction(copyCalcAct);

	// Connect variable selector to enable/disable relevant actions
	connect(ui.cbCalculatorCode, &QComboBox::currentTextChanged, this, [=](const QString &txt) {
		const bool en = !txt.isEmpty();
		execCalcAct->setEnabled(en);
		regEventAct->setEnabled(en);
		saveEventAct->setEnabled(en);
		copyCalcAct->setEnabled(en);
	});


	// Variables section actions

	// Request Local Vars list
	QAction *reloadLVarsAct = new QAction(QIcon(QStringLiteral("autorenew.glyph")), tr("Reload L.Vars"), this);
	reloadLVarsAct->setToolTip(tr("Reload Local Variables"));
	connect(reloadLVarsAct, &QAction::triggered, this, [this]() { d->refreshLVars(); });
	ui.btnList->setDefaultAction(reloadLVarsAct);

	// Lookup a variable/unit ID
	QAction *lookupItemAct = new QAction(QIcon(QStringLiteral("search.glyph")), tr("Lookup"), this);
	lookupItemAct->setToolTip(tr("Query server for ID of named item (Lookup command)."));
	connect(lookupItemAct, &QAction::triggered, this, [this]() { d->lookupItem(); });
	ui.btnVarLookup->setDefaultAction(lookupItemAct);

	// Get local variable value
	QAction *getVarAct = new QAction(QIcon(QStringLiteral("rotate=180/send.glyph")), tr("Get Variable"), this);
	getVarAct->setToolTip(tr("Get Variable Value."));
	getVarAct->setDisabled(true);
	connect(getVarAct, &QAction::triggered, this, [this]() { d->getLocalVar(); });
	ui.btnGetVar->setDefaultAction(getVarAct);

	// Set variable value
	QAction *setVarAct = new QAction(QIcon(QStringLiteral("send.glyph")), tr("Set Variable"), this);
	setVarAct->setToolTip(tr("Set Variable Value."));
	setVarAct->setDisabled(true);
	connect(setVarAct, &QAction::triggered, this, [this]() { d->setLocalVar(); });
	ui.btnSetVar->setDefaultAction(setVarAct);

	// Set or Create local variable
	QAction *setCreateVarAct = new QAction(QIcon(QStringLiteral("overlay=\\align=AlignRight\\scale=.95\\fg=#17dde8\\add/send.glyph")), tr("Set/Create Variable"), this);
	setCreateVarAct->setToolTip(tr("Set Or Create Local Variable."));
	setCreateVarAct->setDisabled(true);
	connect(setCreateVarAct, &QAction::triggered, this, [this]() { d->setLocalVar(true); });
	ui.btnSetCreate->setDefaultAction(setCreateVarAct);

	// Copy LVar as new Data Request
	QAction *copyVarAct = new QAction(QIcon(QStringLiteral("move_to_inbox.glyph")), tr("Copy to Data Request"), this);
	copyVarAct->setToolTip(tr("Copy Variable to new Data Request"));
	copyVarAct->setDisabled(true);
	connect(copyVarAct, &QAction::triggered, this, [this]() { d->copyLocalVarToRequest(); });
	ui.btnCopyLVarToRequest->setDefaultAction(copyVarAct);

	auto updateLocalVarsFormState = [=](const QString &) {
		const bool en = (ui.wLocalVarsForm->isVisible() && !ui.cbLvars->currentText().isEmpty()) ||
			(ui.wOtherVarsForm->isVisible() && !ui.cbVariableName->currentText().isEmpty());
		getVarAct->setEnabled(en);
		setVarAct->setEnabled(en);
		setCreateVarAct->setEnabled(en);
		copyVarAct->setEnabled(en);
	};

	// Connect variable selector to enable/disable relevant actions
	connect(ui.cbLvars, &QComboBox::currentTextChanged, this, updateLocalVarsFormState);
	connect(ui.cbVariableName, &QComboBox::currentTextChanged, this, updateLocalVarsFormState);

	// connect to variable type combo box to switch between views for local vars vs. everything else
	connect(ui.cbGetSetVarType, &DataComboBox::currentDataChanged, this, [=](const QVariant &vtype) {
		bool isLocal = vtype.toChar() == 'L';
		ui.wLocalVarsForm->setVisible(isLocal);
		ui.wOtherVarsForm->setVisible(!isLocal);
		ui.wGetSetSimVarIndex->setVisible(!isLocal && vtype.toChar() == 'A');  // sim var index box visible only for... simvars!
		ui.btnSetCreate->setVisible(isLocal);
		bool hasUnit = ui.cbGetSetVarType->currentText().contains('*');
		ui.cbSetVarUnitName->setVisible(hasUnit);
		ui.lblSetVarUnit->setVisible(hasUnit);
		updateLocalVarsFormState(QString());
	});


	// Send Command action
	QAction *sendCmdAct = new QAction(QIcon(QStringLiteral("send.glyph")), tr("Send Command"), this);
	sendCmdAct->setToolTip(tr("Send the selected Command to the server."));
	connect(sendCmdAct, &QAction::triggered, this, [this]() { d->sendCommandForm(); });
	ui.btnCmdSend->setDefaultAction(sendCmdAct);


	// Requests model view actions

	// Remove selected Data Request(s) from item model/view
	QAction *removeRequestsAct = new QAction(QIcon(QStringLiteral("delete_forever.glyph")), tr("Remove Selected Data Request(s)"), this);
	removeRequestsAct->setIconText(tr("Remove"));
	removeRequestsAct->setToolTip(tr("Delete the selected Data Request(s)."));
	removeRequestsAct->setDisabled(true);
	ui.pbReqestsRemove->setDefaultAction(removeRequestsAct);
	connect(removeRequestsAct, &QAction::triggered, this, [this]() { d->removeSelectedRequests(); });

	// Update data of selected Data Request(s) in item model/view
	QAction *updateRequestsAct = new QAction(QIcon(QStringLiteral("refresh.glyph")), tr("Update Selected Data Request(s)"), this);
	updateRequestsAct->setIconText(tr("Update"));
	updateRequestsAct->setToolTip(tr("Request data update on selected Data Request(s)."));
	updateRequestsAct->setDisabled(true);
	ui.pbReqestsUpdate->setDefaultAction(updateRequestsAct);
	connect(updateRequestsAct, &QAction::triggered, this, [this]() { d->updateSelectedRequests(); });

	// Connect to table view selection model to en/disable the remove/update actions when selection changes.
	connect(ui.requestsView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=](const QItemSelection &sel, const QItemSelection &) {
		removeRequestsAct->setDisabled(sel.isEmpty());
		updateRequestsAct->setDisabled(sel.isEmpty());
	});

	// Pause/resume data updates of requests
	QIcon dataPauseIcon(QStringLiteral("pause.glyph"));
	dataPauseIcon.addFile(QStringLiteral("play_arrow.glyph"), QSize(), QIcon::Normal, QIcon::On);
	//dataPauseIcon.addFile(QStringLiteral("play_arrow.glyph"), QSize(), QIcon::Active, QIcon::On);
	QAction *pauseRequestsAct = new QAction(dataPauseIcon, tr("Toggle Updates"), this);
	pauseRequestsAct->setIconText(tr("Suspend"));
	pauseRequestsAct->setToolTip(tr("Temporarily pause all data value updates on Server side."));
	pauseRequestsAct->setCheckable(true);
	pauseRequestsAct->setDisabled(true);
	ui.pbReqestsPause->setDefaultAction(pauseRequestsAct);
	connect(pauseRequestsAct, &QAction::triggered, this, [=](bool chk) {
		static const QIcon dataResumeIcon(QStringLiteral("play_arrow.glyph"));
		d->client->setDataRequestsPaused(chk);
		pauseRequestsAct->setIconText(chk ? tr("Resume") : tr("Suspend"));
		// for some reason the checked icon "on" state doesn't work automatically like it should...
		ui.pbReqestsPause->setIcon(chk ? dataResumeIcon : dataPauseIcon);
	});

	// Save current Requests to a file
	QAction *saveRequestsAct = new QAction(QIcon(QStringLiteral("save.glyph")), tr("Save Requests"), this);
	saveRequestsAct->setIconText(tr("Save"));
	saveRequestsAct->setToolTip(tr("Save current Requests list to file."));
	saveRequestsAct->setDisabled(true);
	ui.pbReqestsSave->setDefaultAction(saveRequestsAct);
	connect(saveRequestsAct, &QAction::triggered, this, [this]() { d->saveRequests(); });

	// Load Requests from a file. This is actually two actions: load and append to existing records + load and replace existing records.
	QAction *loadRequestsAct = new QAction(QIcon(QStringLiteral("folder_open.glyph")), tr("Load Requests"), this);
	loadRequestsAct->setIconText(tr("Load"));
	loadRequestsAct->setToolTip(tr("Load saved Requests from file."));
	QMenu *loadRequestsMenu = new QMenu(tr("Requests Load Action"), this);
	QAction *loadReplaceAct = loadRequestsMenu->addAction(QIcon(QStringLiteral("view_list.glyph")), tr("Replace Existing"));
	QAction *loadAppendAct = loadRequestsMenu->addAction(QIcon(QStringLiteral("playlist_add.glyph")), tr("Append to Existing"));
	ui.pbReqestsLoad->setDefaultAction(loadRequestsAct);
	connect(loadReplaceAct, &QAction::triggered, this, [this]() { d->loadRequests(true); });
	connect(loadAppendAct, &QAction::triggered, this, [this]() { d->loadRequests(false); });
	connect(loadRequestsAct, &QAction::triggered, this, [=]() { if (!loadRequestsAct->menu()) d->loadRequests(true); });
	// Change the action type depending on number of current rows in data requests model, with or w/out a menu of Add/Replace Existing options.
	connect(d->reqModel, &RequestsModel::rowCountChanged, this, [=](int rows) {
		if (rows) {
			if (!loadRequestsAct->menu())
				loadRequestsAct->setMenu(loadRequestsMenu);
		}
		else if (loadRequestsAct->menu()) {
			loadRequestsAct->setMenu(nullptr);
		}
		saveRequestsAct->setEnabled(rows > 0);
		pauseRequestsAct->setEnabled(rows > 0);
	}, Qt::QueuedConnection);


	// Registered calculator events model view actions

	// Remove selected Data Request(s) from item model/view
	QAction *removeEventsAct = new QAction(QIcon(QStringLiteral("delete_forever.glyph")), tr("Remove Selected Event(s)"), this);
	removeEventsAct->setIconText(tr("Remove"));
	removeEventsAct->setToolTip(tr("Delete the selected Event(s)."));
	removeEventsAct->setDisabled(true);
	ui.pbEventsRemove->setDefaultAction(removeEventsAct);
	connect(removeEventsAct, &QAction::triggered, this, [this]() { d->removeSelectedEvents(); });

	// Update data of selected Data Request(s) in item model/view
	QAction *updateEventsAct = new QAction(QIcon(QStringLiteral("rotate=180/play_for_work.glyph")), tr("Transmit Selected Event(s)"), this);
	updateEventsAct->setIconText(tr("Transmit"));
	updateEventsAct->setToolTip(tr("Trigger selected Event(s)."));
	updateEventsAct->setDisabled(true);
	ui.pbEventsTransmit->setDefaultAction(updateEventsAct);
	connect(updateEventsAct, &QAction::triggered, this, [this]() { d->transmitSelectedEvents(); });

	// Connect to table view selection model to en/disable the remove/update actions when selection changes.
	connect(ui.eventsView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=](const QItemSelection &sel, const QItemSelection &) {
		removeEventsAct->setDisabled(sel.isEmpty());
		updateEventsAct->setDisabled(sel.isEmpty());
	});

	// Save current Events to a file
	QAction *saveEventsAct = new QAction(QIcon(QStringLiteral("save.glyph")), tr("Save Events"), this);
	saveEventsAct->setIconText(tr("Save"));
	saveEventsAct->setToolTip(tr("Save current Events list to file."));
	saveEventsAct->setDisabled(true);
	ui.pbEventsSave->setDefaultAction(saveEventsAct);
	connect(saveEventsAct, &QAction::triggered, this, [this]() { d->saveEvents(); });

	// Load Events from a file. This is actually two actions: load and append to existing records + load and replace existing records.
	QAction *loadEventsAct = new QAction(QIcon(QStringLiteral("folder_open.glyph")), tr("Load Events"), this);
	loadEventsAct->setIconText(tr("Load"));
	loadEventsAct->setToolTip(tr("Load saved Events from file."));
	QMenu *loadEventsMenu = new QMenu(tr("Events Load Action"), this);
	QAction *replaceEventsAct = loadEventsMenu->addAction(QIcon(QStringLiteral("view_list.glyph")), tr("Replace Existing"));
	QAction *appendEventsAct = loadEventsMenu->addAction(QIcon(QStringLiteral("playlist_add.glyph")), tr("Append to Existing"));
	ui.pbEventsLoad->setDefaultAction(loadEventsAct);
	connect(replaceEventsAct, &QAction::triggered, this, [this]() { d->loadEvents(true); });
	connect(appendEventsAct, &QAction::triggered, this, [this]() { d->loadEvents(false); });
	connect(loadEventsAct, &QAction::triggered, this, [=]() { if (!loadEventsAct->menu()) d->loadEvents(true); });
	// Change the action type depending on number of current rows in events model, with or w/out a menu of Add/Replace Existing options.
	connect(d->eventsModel, &EventsModel::rowCountChanged, this, [=](int rows) {
		if (rows) {
			if (!loadEventsAct->menu())
				loadEventsAct->setMenu(loadEventsMenu);
		}
		else if (loadEventsAct->menu()) {
			loadEventsAct->setMenu(nullptr);
		}
		saveEventsAct->setEnabled(rows > 0);
	}, Qt::QueuedConnection);


	// Logging window actions

	QAction *filterErrorsAct = new QAction(QIcon(Utils::iconNameForLogLevel(LogLevel::Error)), tr("Toggle Errors"), this);
	filterErrorsAct->setToolTip(tr("Toggle visibility of Error-level log messages."));
	filterErrorsAct->setCheckable(true);
	filterErrorsAct->setChecked(true);
	ui.btnLogFilt_ERR->setDefaultAction(filterErrorsAct);
	connect(filterErrorsAct, &QAction::triggered, this, [this](bool en) { d->logModel->setLevelFilter(LogLevel::Error, !en); });

	QAction *filterWarningsAct = new QAction(QIcon(Utils::iconNameForLogLevel(LogLevel::Warning)), tr("Toggle Warnings"), this);
	filterWarningsAct->setToolTip(tr("Toggle visibility of Warning-level log messages."));
	filterWarningsAct->setCheckable(true);
	filterWarningsAct->setChecked(true);
	ui.btnLogFilt_WRN->setDefaultAction(filterWarningsAct);
	connect(filterWarningsAct, &QAction::triggered, this, [this](bool en) { d->logModel->setLevelFilter(LogLevel::Warning, !en); });

	QAction *filterInfoAct = new QAction(QIcon(Utils::iconNameForLogLevel(LogLevel::Info)), tr("Toggle Info"), this);
	filterInfoAct->setToolTip(tr("Toggle visibility of Information-level log messages."));
	filterInfoAct->setCheckable(true);
	filterInfoAct->setChecked(true);
	ui.btnLogFilt_INF->setDefaultAction(filterInfoAct);
	connect(filterInfoAct, &QAction::triggered, this, [this](bool en) { d->logModel->setLevelFilter(LogLevel::Info, !en); });

	QAction *filterDebugAct = new QAction(QIcon(Utils::iconNameForLogLevel(LogLevel::Debug)), tr("Toggle Debug"), this);
	filterDebugAct->setToolTip(tr("Toggle visibility of Debug-level log messages."));
	filterDebugAct->setCheckable(true);
	filterDebugAct->setChecked(true);
	ui.btnLogFilt_DBG->setDefaultAction(filterDebugAct);
	connect(filterDebugAct, &QAction::triggered, this, [this](bool en) { d->logModel->setLevelFilter(LogLevel::Debug, !en); });

	QAction *filterTraceAct = new QAction(QIcon(Utils::iconNameForLogLevel(LogLevel::Trace)), tr("Toggle Traces"), this);
	filterTraceAct->setToolTip(tr("Toggle visibility of Trace-level log messages."));
	filterTraceAct->setCheckable(true);
	filterTraceAct->setChecked(true);
	ui.btnLogFilt_TRC->setDefaultAction(filterTraceAct);
	connect(filterTraceAct, &QAction::triggered, this, [this](bool en) { d->logModel->setLevelFilter(LogLevel::Trace, !en); });

	QAction *filterServerAct = new QAction(QIcon(Utils::iconNameForLogSource(+LogSource::Server)), tr("Toggle Server Records"), this);
	filterServerAct->setToolTip(tr("Toggle visibility of log messages from Server."));
	filterServerAct->setCheckable(true);
	filterServerAct->setChecked(true);
	ui.btnLogFilt_Server->setDefaultAction(filterServerAct);
	connect(filterServerAct, &QAction::triggered, this, [this](bool en) { d->logModel->setSourceFilter(+LogSource::Server, !en); });

	QAction *filterClientAct = new QAction(QIcon(Utils::iconNameForLogSource(+LogSource::Client)), tr("Toggle Client Records"), this);
	filterClientAct->setToolTip(tr("Toggle visibility of log messages from Client."));
	filterClientAct->setCheckable(true);
	filterClientAct->setChecked(true);
	ui.btnLogFilt_Client->setDefaultAction(filterClientAct);
	connect(filterClientAct, &QAction::triggered, this, [this](bool en) { d->logModel->setSourceFilter(+LogSource::Client, !en); });

	QAction *filterUILogAct = new QAction(QIcon(Utils::iconNameForLogSource(+LogRecordsModel::LogSource::UI)), tr("Toggle UI Records"), this);
	filterUILogAct->setToolTip(tr("Toggle visibility of log messages from this UI."));
	filterUILogAct->setCheckable(true);
	filterUILogAct->setChecked(true);
	ui.btnLogFilt_UI->setDefaultAction(filterUILogAct);
	connect(filterUILogAct, &QAction::triggered, this, [this](bool en) { d->logModel->setSourceFilter(+LogRecordsModel::LogSource::UI, !en); });

	QIcon logPauseIcon(QStringLiteral("pause.glyph"));
	logPauseIcon.addFile(QStringLiteral("play_arrow.glyph"), QSize(), QIcon::Normal, QIcon::On);
	QAction *pauseLogScrollAct = new QAction(logPauseIcon, tr("Pause Log Scroll"), this);
	pauseLogScrollAct->setToolTip(tr("<p>Toggle scrolling of the log window. Scrolling can also be paused by selecting a log entry row.</p>"));
	pauseLogScrollAct->setCheckable(true);
	ui.btnLogPause->setDefaultAction(pauseLogScrollAct);
	connect(pauseLogScrollAct, &QAction::triggered, this, [this](bool en) { if (!en) ui.logView->selectionModel()->clear(); });  // clear log view selection on "un-pause"

	QAction *clearLogWindowAct = new QAction(QIcon(QStringLiteral("delete.glyph")), tr("Clear Log Window"), this);
	clearLogWindowAct->setToolTip(tr("Clear the log window."));
	ui.btnLogClear->setDefaultAction(clearLogWindowAct);
	connect(clearLogWindowAct, &QAction::triggered, this, [this]() { d->logModel->clear(); });

	QIcon wordWrapIcon(QStringLiteral("wrap_text.glyph"));
	wordWrapIcon.addFile(QStringLiteral("notes.glyph"), QSize(), QIcon::Normal, QIcon::On);
	QAction *wordWrapLogWindowAct = new QAction(wordWrapIcon, tr("Word Wrap"), this);
	wordWrapLogWindowAct->setToolTip(tr("Toggle word wrapping of the log window."));
	wordWrapLogWindowAct->setCheckable(true);
	wordWrapLogWindowAct->setChecked(true);
	ui.btnLogWordWrap->setDefaultAction(wordWrapLogWindowAct);
	connect(wordWrapLogWindowAct, &QAction::toggled, this, [this](bool chk) { ui.logView->setWordWrap(chk); ui.logView->resizeRowsToContents(); });

	// connect the log model record added signal to make sure last record remains in view, unless scroll lock is enabled
	connect(d->logModel, &LogRecordsModel::recordAdded, this, [=](const QModelIndex &i) {
		// make sure log view scroll to bottom on insertions, unless a row is selected or scroll pause is set.
		ui.logView->resizeRowToContents(i.row());
		if (!pauseLogScrollAct->isChecked() && !ui.logView->selectionModel()->hasSelection())
			ui.logView->scrollToBottom(/*i, QAbstractItemView::PositionAtBottom*/);
	});
	// connect log viewer selection model to show pause button active while there is a selection
	connect(ui.logView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=](const QItemSelection &sel, const QItemSelection&) {
		pauseLogScrollAct->setChecked(!sel.isEmpty());
	});

	// Other UI-related actions

	QAction *viewAct = new QAction(QIcon(QStringLiteral("grid_view.glyph")), tr("View"), this);
	QMenu *viewMenu = new QMenu(tr("View"), this);
	viewMenu->addActions({ ui.dwRequests->toggleViewAction(), ui.dwEventsList->toggleViewAction(), ui.dwLog->toggleViewAction() });
	viewAct->setMenu(viewMenu);

	QAction *styleAct = new QAction(QIcon(QStringLiteral("style.glyph")), tr("Toggle Dark/Light Theme"), this);
	styleAct->setCheckable(true);
	styleAct->setShortcut(tr("Alt+D"));
	connect(styleAct, &QAction::triggered, &Utils::toggleAppStyle);

	QAction *aboutAct = new QAction(QIcon(QStringLiteral("info.glyph")), tr("About"), this);
	aboutAct->setShortcut(QKeySequence::HelpContents);
	connect(aboutAct, &QAction::triggered, this, [this]() {Utils::about(this); });

	QAction *projectLinkAct = new QAction(QIcon(QStringLiteral("IcoMoon-Free/github.glyph")), tr("Project Site"), this);
	connect(projectLinkAct, &QAction::triggered, this, [this]() { QDesktopServices::openUrl(QUrl(WSMCMND_PROJECT_URL)); });

	// add all actions to this widget, for context menu and shortcut handling
	addActions({
		d->initAct, pingAct, d->connectAct,
		Utils::separatorAction(this), removeRequestsAct, updateRequestsAct, saveRequestsAct, loadRequestsAct,
		Utils::separatorAction(this), removeEventsAct, updateEventsAct, saveEventsAct, loadEventsAct,
		Utils::separatorAction(this), pauseLogScrollAct, clearLogWindowAct, wordWrapLogWindowAct,
		Utils::separatorAction(this), viewAct, styleAct, aboutAct, projectLinkAct
	});


	// Set up toolbar
	QToolBar *toolbar = new QToolBar(tr("Main Toolbar"), this);
	addToolBar(Qt::TopToolBarArea, toolbar);
	toolbar->setMovable(false);
	toolbar->setObjectName(QStringLiteral("TOOLBAR_MAIN"));
	toolbar->setStyleSheet(QStringLiteral("QToolBar { border: 0; border-bottom: 1px solid palette(mid); spacing: 6px; } QToolBar::separator { background-color: palette(mid); width: 1px; padding: 0; margin: 6px 8px; }"));
	toolbar->addActions({ d->initAct, pingAct, d->connectAct });
	toolbar->addSeparator();
	toolbar->addActions({ viewAct, styleAct, aboutAct, projectLinkAct });
	// default toolbutton menu mode is lame
	if (QToolButton *tb = qobject_cast<QToolButton *>(toolbar->widgetForAction(viewAct)))
		tb->setPopupMode(QToolButton::InstantPopup);

	// Add the status widget to the toolbar, with spacers to right-align it with right padding.
	toolbar->addWidget(Utils::spacerWidget(Qt::Horizontal));
	toolbar->addWidget(d->statWidget);
	toolbar->addWidget(Utils::spacerWidget(Qt::Horizontal, 20));

	//setMenuBar(new QMenuBar());
	//QMenu *menu = ui.menuBar->addMenu(tr("Menu"));
	//menu->addActions(this->actions());

	// show window
	show();
	// set up default window geometry.. because automatic isn't good enough
	resize(1130, 940);
	resizeDocks({ ui.dwLog }, { 270 }, Qt::Vertical);
	// now restore any saved settings
	d->readSettings();
	styleAct->setChecked(Utils::isDarkStyle());
}

void WASimUI::onClientEvent(const ClientEvent &ev)
{
	d->clientStatus = ev.status;
	d->statWidget->setStatus(ev);
	d->statWidget->setServerVersion(d->client->serverVersion());
	d->initAct->setChecked(+ev.status & +ClientStatus::SimConnected);
	d->initAct->setText(d->initAct->isChecked() ? tr("Disconnect Simulator") : tr("Connect to Simulator") );
	d->connectAct->setChecked(+ev.status & +ClientStatus::Connected);
	d->connectAct->setText(+d->connectAct->isChecked() ? tr("Disconnect Server") : tr("Connect to Server") );
}

void WASimUI::onListResults(const ListResult &list)
{
	if (list.listType != LookupItemType::LocalVariable)
		return;
	ui.cbLvars->clear();
	for (const auto &pair : list.list)
		ui.cbLvars->addItem(QString::fromStdString(pair.second), pair.first);
}

void WASimUI::closeEvent(QCloseEvent *ev)
{
	d->saveSettings();
	ev->accept();
}

WASimUI::~WASimUI()
{
	if (d->client && d->client->isInitialized())
		d->client->disconnectSimulator();
	delete d->client;
	d.reset(nullptr);
}

