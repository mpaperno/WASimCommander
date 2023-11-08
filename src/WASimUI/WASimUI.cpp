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
#include <QCompleter>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QItemSelectionModel>
#include <QMenu>
#include <QMetaType>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QToolBar>

#include "WASimUI.h"

#include "DocImports.h"
#include "DocImportsBrowser.h"
#include "EventsModel.h"
#include "LogConsole.h"
#include "RequestsExport.h"
#include "RequestsFormat.h"
#include "RequestsModel.h"
#include "Utils.h"
#include "Widgets.h"

using namespace WASimUiNS;
using namespace DocImports;
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

	struct FormWidget {
		QString name;
		QWidget *w;
		QAction *a;
	};

public:
	WASimUI *q;
	Ui::WASimUIClass *ui;
	WASimClient *client;
	StatusWidget *statWidget;
	RequestsModel *reqModel;
	EventsModel *eventsModel;
	RequestsExportWidget *reqExportWidget = nullptr;
	DocImportsBrowser *docBrowserWidget = nullptr;
	QAction *toggleConnAct = nullptr;
	QAction *initAct = nullptr;
	QAction *connectAct = nullptr;
	ClientStatus clientStatus = ClientStatus::Idle;
	QString lastRequestsFile;
	QString lastEventsFile;
	uint32_t nextCmdToken = 0x0000FFFF;
	QVector<FormWidget> formWidgets { };

	WASimUIPrivate(WASimUI *q) : q(q), ui(&q->ui),
		reqModel(new RequestsModel(q)),
		eventsModel(new EventsModel(q)),
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

		// Connect our own signals for client callback handling in a thread-safe manner, marshaling back to GUI thread as needed.
		// The "signal" methods are "emitted" by the Client as callbacks, registered in Private::setupClient().
		QObject::connect(q, &WASimUI::clientEvent, q, &WASimUI::onClientEvent, Qt::QueuedConnection);
		QObject::connect(q, &WASimUI::listResults, q, &WASimUI::onListResults, Qt::QueuedConnection);
		// Data updates can go right to the requests model.
		QObject::connect(q, &WASimUI::dataResultReady, reqModel, &RequestsModel::setRequestValue, Qt::QueuedConnection);
	}

	bool isConnected() const { return client->isConnected(); }

	bool checkConnected()
	{
		if (client->isConnected())
			return true;
		logUiMessage(tr("Server not connected."), CommandId::Nak);
		return false;
	}

	void pingServer() {
		quint32 v = client->pingServer();
		if (v > 0)
			logUiMessage(tr("Server responded to ping with version: %1").arg(v, 8, 16, QChar('0')), CommandId::Ping, LogLevel::Info);
		else
			logUiMessage(tr("Ping request timed out!"), CommandId::Ping);
	}

	void toggleConnection(bool sim, bool wasim)
	{
		if (client->isConnected()) {
			if (wasim)
				client->disconnectServer();
			if (sim)
				client->disconnectSimulator();
			return;
		}
		if (client->isInitialized() && !wasim) {
				client->disconnectSimulator();
				return;
		}
		if (!client->isInitialized()) {
			if (SUCCEEDED(client->connectSimulator()) && wasim)
				client->connectServer();
		}
		else if (wasim && !client->isConnected()) {
			client->connectServer();
		}
	}

	// Calculator Code form -------------------------------------------------

	void runCalcCode()
	{
		if (!checkConnected())
			return;
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

	void updateCalcCodeFormState(const QString &txt) {
		const bool en = !txt.isEmpty();
		ui->btnCalc->defaultAction()->setEnabled(en && isConnected());
		ui->btnAddEvent->defaultAction()->setEnabled(en);
		ui->btnUpdateEvent->defaultAction()->setEnabled(en);
		ui->btnCopyCalcToRequest->defaultAction()->setEnabled(en);
	}

	// Lookups form  -------------------------------------------------

	void lookupItem()
	{
		if (!checkConnected())
			return;
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

	// Variables form -------------------------------------------------

	void refreshLVars() {
		client->list();
	}

	void getLocalVar(bool create = false)
	{
		if (!checkConnected())
			return;
		const char vtype = ui->cbGetSetVarType->currentData().toChar().toLatin1();
		const QString &varName = vtype == 'L' ? ui->cbLvars->currentText() : ui->cbVariableName->currentText();
		if (varName.isEmpty()) {
			logUiMessage(tr("Variable name is empty."), CommandId::Get);
			return;
		}
		double result;
		std::string sRes;
		HRESULT hr;
		if (vtype == 'L' && create)
			hr = client->getOrCreateLocalVariable(varName.toStdString(), &result, ui->dsbSetVarValue->value(), ui->cbSetVarUnitName->currentText().toStdString());
		else
			hr = client->getVariable(VariableRequest(vtype, varName.toStdString(), ui->cbSetVarUnitName->currentText().toStdString(), ui->sbGetSetSimVarIndex->value()), &result, &sRes);
		if (hr == S_OK) {
			if (ui->cbSetVarUnitName->currentText() == QLatin1Literal("string"))
				ui->leVarResult->setText(sRes.empty() ? tr("Result returned empty string") : QString::fromStdString(sRes));
			else
				ui->leVarResult->setText(QString::number(result));
			return;
		}
		ui->leVarResult->setText(QString("Error: 0x%1").arg((quint32)hr, 8, 16, QChar('0')));
		logUiMessage(tr("Variable request failed."), CommandId::Get);
	}

	void setLocalVar(bool create = false)
	{
		if (!checkConnected())
			return;
		const char vtype = ui->cbGetSetVarType->currentData().toChar().toLatin1();
		const QString &varName = vtype == 'L' ? ui->cbLvars->currentText() : ui->cbVariableName->currentText();
		if (varName.isEmpty()) {
			logUiMessage(tr("Variable name is empty."), CommandId::Set);
			return;
		}
		if (vtype == 'L' && create)
			client->setOrCreateLocalVariable(varName.toStdString(), ui->dsbSetVarValue->value(), ui->cbSetVarUnitName->currentText().toStdString());
		else
			client->setVariable(VariableRequest(vtype, varName.toStdString(), ui->cbSetVarUnitName->currentText().toStdString(), ui->sbGetSetSimVarIndex->value()), ui->dsbSetVarValue->value());
	}

	void toggleSetGetVariableType()
	{
		const QChar vtype = ui->cbGetSetVarType->currentData().toChar();
		bool isLocal = vtype == 'L', isSimVar = vtype == 'A';
		ui->wLocalVarsForm->setVisible(isLocal);
		ui->wOtherVarsForm->setVisible(!isLocal);
		ui->wGetSetSimVarIndex->setVisible(isSimVar);
		ui->btnSetCreate->setVisible(isLocal);
		ui->btnGetCreate->setVisible(isLocal);
		bool hasUnit = ui->cbGetSetVarType->currentText().contains('*');
		ui->cbSetVarUnitName->setVisible(hasUnit);
		ui->lblSetVarUnit->setVisible(hasUnit);
		if (isSimVar) {
			ui->cbVariableName->setCompleter(new DocImports::NameCompleter(DocImports::RecordType::SimVars, ui->cbNameOrCode));
			ui->cbVariableName->lineEdit()->addAction(ui->btnFindSimVar->defaultAction(), QLineEdit::TrailingPosition);
		}
		else {
			ui->cbVariableName->lineEdit()->removeAction(ui->btnFindSimVar->defaultAction());
			ui->cbVariableName->resetCompleter();
			if (isLocal)
				ui->cbSetVarUnitName->setCurrentText("");
		}
	}

	void copyLocalVarToRequest()
	{
		setRequestFormId(-1);
		const char vtype = ui->cbGetSetVarType->currentData().toChar().toLatin1();
		ui->rbRequestType_Named->setChecked(true);
		ui->cbNameOrCode->setCurrentText(vtype == 'L' ? ui->cbLvars->currentText() : ui->cbVariableName->currentText());
		ui->cbVariableType->setCurrentData(vtype);
		ui->cbUnitName->setCurrentText(ui->cbSetVarUnitName->currentText());
		ui->cbValueSize->setCurrentData(Utils::unitToMetaType(ui->cbUnitName->currentText()));
	}

	void updateLocalVarsFormState() {
		const bool isLocal = ui->wLocalVarsForm->isVisible();
		const bool haveData = !(isLocal ? ui->cbLvars->currentText().isEmpty() : ui->cbVariableName->currentText().isEmpty());
		const bool en = haveData && isConnected();
		ui->btnGetVar->defaultAction()->setEnabled(en);
		ui->btnSetVar->defaultAction()->setEnabled(en);
		ui->btnSetCreate->defaultAction()->setEnabled(en && isLocal);
		ui->btnGetCreate->defaultAction()->setEnabled(en && isLocal);
		ui->btnCopyLVarToRequest->defaultAction()->setEnabled(haveData);
	}

	// Key Events form -------------------------------------------------

	void sendKeyEventForm()
	{
		if (!checkConnected())
			return;
		if (ui->cbKeyEvent->currentText().isEmpty())  {
			logUiMessage(tr("Key Event Name/ID field is empty, nothing to send."), CommandId::Nak);
			return;
		}
		bool ok;
		quint32 keyId = ui->cbKeyEvent->currentText().toUInt(&ok);
		if (ok) {
			if (!keyId) {
				logUiMessage(tr("Invalid Key Event ID."), CommandId::Nak);
				return;
			}
			client->sendKeyEvent(keyId, (uint32_t)ui->sbKeyEvent_v1->value(), (uint32_t)ui->sbKeyEvent_v2->value(), (uint32_t)ui->sbKeyEvent_v3->value(), (uint32_t)ui->sbKeyEvent_v4->value(), (uint32_t)ui->sbKeyEvent_v5->value());
			return;
		}
		client->sendKeyEvent(ui->cbKeyEvent->currentText().toStdString(), (uint32_t)ui->sbKeyEvent_v1->value(), (uint32_t)ui->sbKeyEvent_v2->value(), (uint32_t)ui->sbKeyEvent_v3->value(), (uint32_t)ui->sbKeyEvent_v4->value(), (uint32_t)ui->sbKeyEvent_v5->value());
	}

	void sendCommandForm()
	{
		if (!checkConnected())
			return;
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
		toggleRequestVariableType();
		if (isCalc)
			ui->cbValueSize->setCurrentData(Utils::calcResultTypeToMetaType(CalcResultType(ui->cbRequestCalcResultType->currentData().toUInt())));
		else
			ui->cbValueSize->setCurrentData(Utils::unitToMetaType(ui->cbUnitName->currentText()));
	}

	void toggleRequestVariableType()
	{
		const QChar type = ui->cbVariableType->currentData().toChar();
		bool isCalc = ui->rbRequestType_Calculated->isChecked();
		bool needIdx = !isCalc && type == 'A';
		ui->lblIndex->setVisible(needIdx);
		ui->sbSimVarIndex->setVisible(needIdx);
		bool hasUnit = !isCalc && ui->cbVariableType->currentText().contains('*');
		ui->lblUnit->setVisible(hasUnit);
		ui->cbUnitName->setVisible(hasUnit);

		if (needIdx) {
			ui->cbNameOrCode->setCompleter(new DocImports::NameCompleter(DocImports::RecordType::SimVars, ui->cbNameOrCode));
			ui->cbNameOrCode->lineEdit()->addAction(ui->btnReqFindSimVar->defaultAction(), QLineEdit::TrailingPosition);
		}
		else {
			// restore default completer
			ui->cbNameOrCode->resetCompleter();
			ui->cbNameOrCode->lineEdit()->removeAction(ui->btnReqFindSimVar->defaultAction());
			if (type == 'L')
				ui->cbUnitName->setCurrentText("");
		}
	}

	void setRequestFormId(uint32_t id)
	{
		ui->wRequestForm->setProperty("requestId", id);
		if ((int)id > -1) {
			ui->lblCurrentRequestId->setText(QString("%1").arg(id));
			ui->btnUpdateRequest->defaultAction()->setEnabled(true);
		}
		else  {
			ui->lblCurrentRequestId->setText(tr("New"));
			ui->btnUpdateRequest->defaultAction()->setEnabled(false);
		}
	}

	void handleRequestForm(bool update)
	{
		if (ui->cbNameOrCode->currentText().isEmpty() || ui->cbValueSize->currentText().isEmpty()) {
			logUiMessage(tr("Name/Code and Size cannot be empty."), CommandId::Subscribe);
			return;
		}

		RequestRecord req;
		int row = -1;
		if (update && ui->wRequestForm->property("requestId").toInt() > -1)
			row = reqModel->findRequestRow(ui->wRequestForm->property("requestId").toUInt());
		if (row < 0)
			req = RequestRecord(reqModel->nextRequestId());
		else
			req = reqModel->getRequest(row);

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

		if (FAILED(client->saveDataRequest(req)) && row < 0)
			return;
		const QModelIndex idx = ui->requestsView->mapFromSource(reqModel->addRequest(req));
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
		ui->cbNameOrCode->setCurrentText(req.nameOrCode);
		ui->cbPeriod->setCurrentData(+req.period);
		ui->sbInterval->setValue(req.interval);
		ui->dsbDeltaEpsilon->setValue(req.deltaEpsilon);
	}

	void clearRequestForm()
	{
		setRequestFormId(-1);
		ui->cbNameOrCode->setCurrentText("");
		ui->cbUnitName->setCurrentText("");
		ui->cbValueSize->setCurrentText("");
		ui->sbSimVarIndex->setValue(0);
		ui->cbPeriod->setCurrentData(+UpdatePeriod::Tick);
		ui->sbInterval->setValue(0);
		ui->dsbDeltaEpsilon->setValue(0.0);
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
		removeRequests(ui->requestsView->selectedRows(RequestsModel::COL_ID));
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
		if (!checkConnected())
			return;
		const QModelIndexList list = ui->requestsView->selectedRows(RequestsModel::COL_ID);
		for (const QModelIndex &idx : list)
			client->updateDataRequest(reqModel->requestId(idx.row()));
	}

	void pauseRequests(bool chk) {
		static const QIcon dataResumeIcon(QStringLiteral("play_arrow.glyph"));
		static const QIcon dataPauseIcon(QStringLiteral("pause.glyph"));
		client->setDataRequestsPaused(chk);
		ui->btnReqestsPause->defaultAction()->setIconText(chk ? tr("Resume") : tr("Suspend"));
		// for some reason the checked icon "on" state doesn't work automatically like it should...
		ui->btnReqestsPause->setIcon(chk ? dataResumeIcon : dataPauseIcon);
	};

	void saveRequests(bool forExport = false)
	{
		if (!reqModel->rowCount())
			return;

		const QString &fname = QFileDialog::getSaveFileName(q, tr("Select a file for saved Requests"), lastRequestsFile, QStringLiteral("INI files (*.ini)"));
		if (fname.isEmpty())
			return;
		lastRequestsFile = fname;
		const int rows = forExport ? RequestsFormat::exportToPluginFormat(reqModel, reqModel->allRequests(), fname) : reqModel->saveToFile(fname);
		logUiMessage(tr("Saved %1 Data Request(s) to file: %2").arg(rows).arg(fname), CommandId::Ack, LogLevel::Info);
	}

	void exportRequests()
	{
		if (!reqModel->rowCount())
			return;

		if (reqExportWidget) {
			if (reqExportWidget->isMinimized()) {
				reqExportWidget->showNormal();
			}
			else {
				reqExportWidget->raise();
				reqExportWidget->activateWindow();
			}
			return;
		}

		reqExportWidget = new RequestsExportWidget(reqModel, q);
		reqExportWidget->setWindowFlag(Qt::Dialog);
		reqExportWidget->setAttribute(Qt::WA_DeleteOnClose);
		reqExportWidget->setLastUsedFile(lastRequestsFile);
		QObject::connect(reqExportWidget, &RequestsExportWidget::lastUsedFileChanged, [&](const QString &fn) { lastRequestsFile = fn; });
		QObject::connect(reqExportWidget, &QObject::destroyed, q, [=]() { reqExportWidget = nullptr; });
		reqExportWidget->show();
	}

	void loadRequests(bool replace)
	{
		const QString &fname = QFileDialog::getOpenFileName(q, tr("Select a saved Requests file"), lastRequestsFile, QStringLiteral("INI files (*.ini)"));
		if (fname.isEmpty())
			return;
		lastRequestsFile = fname;
		if (replace)
			removeAllRequests();

		QFile f(fname);
		if (!f.open(QFile::ReadOnly | QFile::Text)) {
			logUiMessage(tr("Could not open file '%1' for reading").arg(fname), CommandId::Nak, LogLevel::Error);
			return;
		}
		const QString first = f.readLine();
		f.close();
		const bool isNative = first.startsWith(QLatin1Literal("[Requests]"));

		const QList<RequestRecord> &added = isNative ? reqModel->loadFromFile(fname) : RequestsFormat::importFromPluginFormat(reqModel, fname);
		for (const DataRequest &req : added)
			client->saveDataRequest(req, true);  // async

		logUiMessage(tr("Loaded %1 Data Request(s) from file: %2").arg(added.count()).arg(fname), CommandId::Ack, LogLevel::Info);
	}

	void toggleRequestButtonsState()
	{
		bool conn = isConnected();
		bool hasSel = ui->requestsView->selectionModel()->hasSelection();
		bool hasRecords = reqModel->rowCount() > 0;
		ui->btnReqestsRemove->defaultAction()->setEnabled(hasSel);
		ui->btnReqestsUpdate->defaultAction()->setEnabled(hasSel && conn);
		ui->btnReqestsPause->defaultAction()->setEnabled(hasRecords);
		ui->btnReqestsSave->defaultAction()->setEnabled(hasRecords);
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
		removeEvents(ui->eventsView->selectedRows(EventsModel::COL_ID));
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
		if (!checkConnected())
			return;
		const QModelIndexList list = ui->eventsView->selectedRows(EventsModel::COL_ID);
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

	void toggleEventButtonsState()
	{
		bool hasSel = ui->eventsView->selectionModel()->hasSelection();
		ui->btnEventsRemove->defaultAction()->setEnabled(hasSel);
		ui->btnEventsTransmit->defaultAction()->setEnabled(hasSel && isConnected());
	}


	// Utilities  -------------------------------------------------

	void logUiMessage(const QString &msg, CommandId cmd = CommandId::None, LogLevel level = LogLevel::Error)
	{
		ui->wLogWindow->logMessage(+level, msg);
		if (cmd != CommandId::None)
			emit q->commandResultReady(Command(level == LogLevel::Error ? CommandId::Nak : CommandId::Ack, +cmd, qPrintable(msg)));
	}

	void openDocsLookup(DocImports::RecordType type, QComboBox *cb)
	{
		DocImportsBrowser *browser = new DocImportsBrowser(q, type, DocImportsBrowser::ViewMode::PopupViewMode);
		browser->setAttribute(Qt::WA_DeleteOnClose);
		browser->setWindowFlag(Qt::Dialog);
		browser->setWindowModality(Qt::ApplicationModal);
		browser->show();
		const QPoint qPos = q->mapToGlobal(QPoint(0,0));
		const QPoint cbPos = cb->mapToGlobal(QPoint(0, cb->height()));
		const QRect rect(qPos, q->size());
		QPoint pos = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, browser->size(), rect).topLeft();
		pos.setY(rect.y() + cbPos.y() - qPos.y());
		browser->move(pos);
		QObject::connect(browser, &DocImportsBrowser::itemSelected, q, [=](const QModelIndex &row) {
			if (row.isValid())
				cb->setCurrentText(browser->model()->record(row.row()).field("Name").value().toString());
			browser->close();
		});
	}

	void openDocsLookupWindow()
	{
		if (docBrowserWidget) {
			if (docBrowserWidget->isMinimized()) {
				docBrowserWidget->showNormal();
			}
			else {
				docBrowserWidget->raise();
				docBrowserWidget->activateWindow();
			}
			return;
		}

		docBrowserWidget = new DocImportsBrowser(q);
		docBrowserWidget->setAttribute(Qt::WA_DeleteOnClose);
		docBrowserWidget->setWindowFlag(Qt::Dialog);
		docBrowserWidget->show();
		QObject::connect(docBrowserWidget, &QObject::destroyed, q, [=]() { docBrowserWidget = nullptr; });
	}


	// Save/Load UI settings  -------------------------------------------------

	void saveSettings()
	{
		QSettings set;
		set.setValue(QStringLiteral("mainWindowGeo"), q->saveGeometry());
		set.setValue(QStringLiteral("mainWindowState"), q->saveState());
		set.setValue(QStringLiteral("eventsViewState"), ui->eventsView->saveState());
		set.setValue(QStringLiteral("requestsViewState"), ui->requestsView->saveState());
		ui->wLogWindow->saveSettings();

		// Visible form widgets
		set.beginGroup(QStringLiteral("Widgets"));
		for (const FormWidget &vw : qAsConst(formWidgets))
			set.setValue(vw.name, vw.a->isChecked());
		set.endGroup();

		set.setValue(QStringLiteral("useDarkTheme"), Utils::isDarkStyle());
		set.setValue(QStringLiteral("lastRequestsFile"), lastRequestsFile);
		set.setValue(QStringLiteral("lastEventsFile"), lastEventsFile);

		set.beginGroup(QStringLiteral("EditableSelectors"));
		const QList<DeletableItemsComboBox *> editable = q->findChildren<DeletableItemsComboBox *>();
		for (DeletableItemsComboBox *cb : editable)
			set.setValue(cb->objectName(), cb->saveState());
		set.endGroup();

		// Variables form
		set.beginGroup(ui->wLocalVarsForm->objectName());
		set.setValue(ui->cbGetSetVarType->objectName(), ui->cbGetSetVarType->currentData());
		set.endGroup();

		// Requests form
		set.beginGroup(ui->wRequests->objectName());
		set.setValue(ui->bgrpRequestType->objectName(), ui->rbRequestType_Named->isChecked());
		set.setValue(ui->cbVariableType->objectName(), ui->cbVariableType->currentData());
		set.endGroup();
	}

	void readSettings()
	{
		QSettings set;
		q->restoreGeometry(set.value(QStringLiteral("mainWindowGeo")).toByteArray());
		q->restoreState(set.value(QStringLiteral("mainWindowState")).toByteArray());
		ui->eventsView->restoreState(set.value(QStringLiteral("eventsViewState")).toByteArray());
		ui->requestsView->restoreState(set.value(QStringLiteral("requestsViewState")).toByteArray());
		ui->wLogWindow->loadSettings();

		// Visible form widgets
		set.beginGroup(QStringLiteral("Widgets"));
		for (const FormWidget &vw : qAsConst(formWidgets))
			vw.a->setChecked(set.value(vw.name, vw.a->isChecked()).toBool());
		set.endGroup();

		const QString defaultFileLoc = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
		lastRequestsFile = set.value(QStringLiteral("lastRequestsFile"), defaultFileLoc + QStringLiteral("/WASimUI-requests.ini")).toString();
		lastEventsFile = set.value(QStringLiteral("lastEventsFile"), defaultFileLoc + QStringLiteral("/WASimUI-events.ini")).toString();
		const bool useDark = set.value(QStringLiteral("useDarkTheme"), true).toBool();
		if (useDark != Utils::isDarkStyle())
			Utils::toggleAppStyle(useDark);

		set.beginGroup(QStringLiteral("EditableSelectors"));
		const QList<DeletableItemsComboBox *> editable = q->findChildren<DeletableItemsComboBox *>();
		for (DeletableItemsComboBox *cb : editable) {
			// check for old version settings format
			if (set.value(cb->objectName()).canConvert<QStringList>())
				cb->insertEditedItems(set.value(cb->objectName()).toStringList());
			else
				cb->restoreState(set.value(cb->objectName()).toByteArray());
		}
		set.endGroup();

		// Variables form
		set.beginGroup(ui->wLocalVarsForm->objectName());
		ui->cbGetSetVarType->setCurrentData(set.value(ui->cbGetSetVarType->objectName(), ui->cbGetSetVarType->currentData()));
		set.endGroup();

		// Requests form
		set.beginGroup(ui->wRequests->objectName());
		ui->rbRequestType_Named->setChecked(set.value(ui->bgrpRequestType->objectName(), ui->rbRequestType_Named->isChecked()).toBool());
		ui->cbVariableType->setCurrentData(set.value(ui->cbVariableType->objectName(), ui->cbVariableType->currentData()));
		set.endGroup();
	}

};


// -------------------------------------------------------------
// WASimUI
// -------------------------------------------------------------

// Action creation macros used below
#define GLYPH_STR(ICN)   QStringLiteral(##ICN ".glyph")
#define GLYPH_ICON(ICN)  QIcon(GLYPH_STR(ICN))

#define MAKE_ACTION_NW(ACT, TTL, ICN, TT)  QAction *ACT = new QAction(GLYPH_ICON(ICN), TTL, this); ACT->setAutoRepeat(false); ACT->setToolTip(TT)
#define MAKE_ACTION_NB(ACT, TTL, ICN, W, TT)        MAKE_ACTION_NW(ACT, TTL, ICN, TT); ui.##W->addAction(ACT)
#define MAKE_ACTION_NC(ACT, TTL, ICN, BTN, W, TT)   MAKE_ACTION_NB(ACT, TTL, ICN, W, TT); ui.##BTN->setDefaultAction(ACT)

#define MAKE_ACTION_CONN(ACT, M)    connect(ACT, &QAction::triggered, this, [this](bool chk) { d->##M; })
#define MAKE_ACTION_SCUT(ACT, KS)   ACT->setShortcut(KS); ACT->setShortcutContext(Qt::WidgetWithChildrenShortcut)
#define MAKE_ACTION_ITXT(ACT, T)    ACT->setIconText(" " + T)

#define MAKE_ACTION(ACT, TTL, ICN, BTN, W, M, TT)                MAKE_ACTION_NC(ACT, TTL, ICN, BTN, W, TT); MAKE_ACTION_CONN(ACT, M)
#define MAKE_ACTION_D(ACT, TTL, ICN, BTN, W, M, TT)              MAKE_ACTION(ACT, TTL, ICN, BTN, W, M, TT); ACT->setDisabled(true)
#define MAKE_ACTION_SC(ACT, TTL, ICN, BTN, W, M, TT, KS)         MAKE_ACTION(ACT, TTL, ICN, BTN, W, M, TT); MAKE_ACTION_SCUT(ACT, KS)
#define MAKE_ACTION_SC_D(ACT, TTL, ICN, BTN, W, M, TT, KS)       MAKE_ACTION_SC(ACT, TTL, ICN, BTN, W, M, TT, KS); ACT->setDisabled(true)
#define MAKE_ACTION_PB(ACT, TTL, IT, ICN, BTN, W, M, TT, KS)     MAKE_ACTION_SC(ACT, TTL, ICN, BTN, W, M, TT, KS); MAKE_ACTION_ITXT(ACT, IT)
#define MAKE_ACTION_PB_D(ACT, TTL, IT, ICN, BTN, W, M, TT, KS)   MAKE_ACTION_SC_D(ACT, TTL, ICN, BTN, W, M, TT, KS); MAKE_ACTION_ITXT(ACT, IT)
#define MAKE_ACTION_PB_NC(ACT, TTL, IT, ICN, BTN, W, TT)         MAKE_ACTION_NC(ACT, TTL, ICN, BTN, W, TT); MAKE_ACTION_ITXT(ACT, IT)
#define MAKE_ACTION_NW_SC(ACT, TTL, IT, ICN, M, TT, KS)          MAKE_ACTION_NW(ACT, TTL, ICN, TT); MAKE_ACTION_CONN(ACT, M); MAKE_ACTION_ITXT(ACT, IT); MAKE_ACTION_SCUT(ACT, KS)
// ---------------------------------

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
	ui.cbCalculatorCode->setMaxLength(STRSZ_CMD);
	ui.cbVariableName->setMaxLength(STRSZ_CMD);
	ui.cbLvars->lineEdit()->setMaxLength(STRSZ_CMD);
	ui.cbLookupName->setMaxLength(STRSZ_CMD);
	ui.leEventName->setMaxLength(STRSZ_ENAME);
	ui.cbNameOrCode->setMaxLength(STRSZ_REQ);

	// Unit name suggestions completer from imported docs.
	ui.cbUnitName->setCompleter(new DocImports::NameCompleter(DocImports::RecordType::SimVarUnits, ui.cbUnitName), true);
	ui.cbSetVarUnitName->setCompleter(new DocImports::NameCompleter(DocImports::RecordType::SimVarUnits, ui.cbSetVarUnitName), true);
	ui.cbSetVarUnitName->setCompleterOptionsButtonEnabled();
	// Enable clear and suggestion options buttons on the data lookup combos.
	ui.cbCalculatorCode->setCompleterOptions(Qt::MatchContains, QCompleter::PopupCompletion);
	ui.cbCalculatorCode->setClearButtonEnabled();
	ui.cbLvars->setCompleterOptions(Qt::MatchStartsWith, QCompleter::PopupCompletion);
	ui.cbLvars->setClearButtonEnabled();
	ui.cbVariableName->setCompleterOptions(Qt::MatchContains, QCompleter::PopupCompletion);
	ui.cbVariableName->setClearButtonEnabled();
	ui.cbKeyEvent->setCompleterOptions(Qt::MatchContains, QCompleter::PopupCompletion);
	ui.cbKeyEvent->setClearButtonEnabled();
	ui.cbNameOrCode->setCompleterOptions(Qt::MatchContains, QCompleter::PopupCompletion);
	ui.cbNameOrCode->setClearButtonEnabled();
	ui.cbUnitName->setCompleterOptions(Qt::MatchStartsWith, QCompleter::PopupCompletion);

	// Init the calculator editor form
	ui.wCalcForm->setProperty("eventId", -1);
	ui.btnUpdateEvent->setVisible(false);
	// Connect variable selector to enable/disable relevant actions
	connect(ui.cbCalculatorCode, &QComboBox::currentTextChanged, this, [=](const QString &txt) { d->updateCalcCodeFormState(txt); });

	// Key event name completer
	ui.cbKeyEvent->setCompleter(new DocImports::NameCompleter(DocImports::RecordType::KeyEvents, ui.cbKeyEvent));

	// Set up the Requests table view
	ui.requestsView->setExportCategories(RequestsFormat::categoriesList());
	ui.requestsView->setModel(d->reqModel);
	// Hide unused columns
	QHeaderView *hdr = ui.requestsView->header();
	for (int i = RequestsModel::COL_FIRST_META; i <= RequestsModel::COL_LAST_META; ++i)
		hdr->hideSection(i);
	// connect double click action to populate the request editor form
	connect(ui.requestsView, &QTableView::doubleClicked, this, [this](const QModelIndex &idx) { d->populateRequestForm(ui.requestsView->mapToSource(idx)); });
	// Connect to table view selection model to en/disable the remove/update actions when selection changes.
	connect(ui.requestsView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=]() { d->toggleRequestButtonsState(); });

	// Set up the Events table view
	ui.eventsView->setModel(d->eventsModel);
	ui.eventsView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	ui.eventsView->horizontalHeader()->setSectionsMovable(true);
	ui.eventsView->horizontalHeader()->resizeSection(EventsModel::COL_ID, 45);
	ui.eventsView->horizontalHeader()->resizeSection(EventsModel::COL_CODE, 140);
	// connect double click action to populate the event editor form
	connect(ui.eventsView, &QTableView::doubleClicked, this, [this](const QModelIndex &idx) { d->populateEventForm(ui.eventsView->mapToSource(idx)); });
	// Connect to table view selection model to en/disable the remove/update actions when selection changes.
	connect(ui.eventsView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=]() { d->toggleEventButtonsState(); });

	// Set up the Log table view
	ui.wLogWindow->setClient(d->client);

	// Set initial state of Variables form, Local var type is default.
	d->toggleSetGetVariableType();

	// Connect variable selector to enable/disable relevant actions
	connect(ui.cbLvars, &QComboBox::currentTextChanged, this, [=]() { d->updateLocalVarsFormState(); });
	connect(ui.cbVariableName, &QComboBox::currentTextChanged, this, [=]() { d->updateLocalVarsFormState(); });

	// connect to variable type combo box to switch between views for local vars vs. everything else
	connect(ui.cbGetSetVarType, &DataComboBox::currentDataChanged, this, [=](const QVariant &) {
		d->toggleSetGetVariableType();
		d->updateLocalVarsFormState();
	});

	// Init the request editor form
	ui.wRequestForm->setProperty("requestId", -1);
	// Update the Data Request form UI based on default types.
	d->toggleRequestType();

	// Connect the request type radio buttons to toggle the UI.
	connect(ui.bgrpRequestType, QOverload<int, bool>::of(&QButtonGroup::buttonToggled), this, [this](int,bool) { d->toggleRequestType(); });
	// show/hide SimVar index spin box based on type of variable selected
	connect(ui.cbVariableType, &DataComboBox::currentDataChanged, this, [this](const QVariant&) { d->toggleRequestVariableType(); });
	// Connect the data request unit type selector to choose a default result size
	connect(ui.cbUnitName, &DataComboBox::currentTextChanged, this, [this](const QString &data) {
		ui.cbValueSize->setCurrentData(Utils::unitToMetaType(data));
	});
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

	// connect to requests model row removed to check if the current editor needs to be reset, otherwise the "Save" button stays active and re-adds a deleted request.
	connect(d->reqModel, &RequestsModel::rowsAboutToBeRemoved, this, [this](const QModelIndex &, int first, int last) {
		const int current = d->reqModel->findRequestRow(ui.wRequestForm->property("requestId").toInt());
		if (current >= first && current <= last)
			d->setRequestFormId(-1);
	});

	// Set up actions for triggering various events. Actions are typically mapped to UI elements like buttons and menu items and can be reused in multiple places.

	// Network connection actions

	// Toggle overall connection, both the SimConnect part and the WASimModule part.
	QIcon connIco = GLYPH_ICON("link");
	connIco.addFile(GLYPH_STR("link_off"), QSize(), QIcon::Mode::Normal, QIcon::State::On);
	d->toggleConnAct = new QAction(connIco, tr("Connect"), this);
	d->toggleConnAct->setToolTip(tr("<p>Toggle connection to WASimModule Server. This affects both the simulator connection (SimConnect) and the main server.</p>"
	                       "<p>Use the sub-menu for individual actions.</p>"));
	d->toggleConnAct->setCheckable(true);
	d->toggleConnAct->setShortcut(QKeySequence(Qt::Key_F2));
	connect(d->toggleConnAct, &QAction::triggered, this, [this]() { d->toggleConnection(true, true); }, Qt::QueuedConnection);

	// Connect/Disconnect Simulator.
	QIcon initIco = GLYPH_ICON("phone_in_talk");
	initIco.addFile(GLYPH_STR("call_end"), QSize(), QIcon::Mode::Normal, QIcon::State::On);
	d->initAct = new QAction(initIco, tr("Connect to Simulator"), this);
	d->initAct->setCheckable(true);
	d->initAct->setShortcut(QKeySequence(Qt::Key_F5));
	connect(d->initAct, &QAction::triggered, this, [this]() { d->toggleConnection(true, false); }, Qt::QueuedConnection);

	// Connect/Disconnect WASim Server
	d->connectAct = new QAction(connIco, tr("Connect to Server"), this);
	d->connectAct->setCheckable(true);
	d->connectAct->setShortcut(QKeySequence(Qt::Key_F6));
	connect(d->connectAct, &QAction::triggered, this, [this]() { d->toggleConnection(false, true); }, Qt::QueuedConnection);

	// Ping the server.
	QAction *pingAct = new QAction(GLYPH_ICON("leak_add"), tr("Ping Server"), this);
	pingAct->setShortcut(QKeySequence(Qt::Key_F7));
	connect(pingAct, &QAction::triggered, this, [this]() { d->pingServer(); });

	// Sub-menu for individual connection actions.
	QMenu *connectMenu = new QMenu(tr("Connection actions"), this);
	connectMenu->setIcon(GLYPH_ICON("settings_remote"));
	connectMenu->addActions({ d->initAct, pingAct, d->connectAct });
	//d->toggleConnAct->setMenu(connectMenu);

	// Calculator code actions

	// Exec calculator code
	MAKE_ACTION_SC_D(execCalcAct, tr("Execute Calculator Code"), "IcoMoon-Free/calculator", btnCalc, wCalcForm, runCalcCode(), tr("Execute Calculator Code."), QKeySequence(Qt::ControlModifier | Qt::Key_Return));
	// Register calculator code event
	MAKE_ACTION_D(regEventAct, tr("Register Event"), "control_point", btnAddEvent, wCalcForm, registerEvent(false), tr("Register this calculator code as a new Event."));
	// Save edited calculator code event
	MAKE_ACTION_D(saveEventAct, tr("Update Event"), "edit", btnUpdateEvent, wCalcForm, registerEvent(true), tr("Update existing event with new calculator code (name cannot be changed)."));
	// Copy calculator code as new Data Request
	MAKE_ACTION_SC_D(copyCalcAct, tr("Copy to Data Request"), "move_to_inbox", btnCopyCalcToRequest, wCalcForm, copyCalcCodeToRequest(),
	                 tr("Copy Calculator Code to new Data Request."), QKeySequence(Qt::ControlModifier | Qt::Key_Down));

	// Variables section actions

	// Request Local Vars list
	MAKE_ACTION(reloadLVarsAct, tr("Reload L.Vars"), "autorenew", btnList, wVariables, refreshLVars(), tr("Reload Local Variables."));
	// Get local variable value
	MAKE_ACTION_SC_D(getVarAct, tr("Get Variable"), "rotate=180/send", btnGetVar, wVariables, getLocalVar(), tr("Get Variable Value."), QKeySequence(Qt::ControlModifier | Qt::Key_Return));
	// Set variable value
	MAKE_ACTION_SC_D(setVarAct, tr("Set Variable"), "send", btnSetVar, wVariables, setLocalVar(), tr("Set Variable Value."), QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_Return));
	// Set or Create local variable
	MAKE_ACTION_D(setCreateVarAct, tr("Set/Create Variable"), "overlay=\\align=AlignRight\\fg=#17dd29\\add/send", btnSetCreate, wVariables, setLocalVar(true),
	              tr("Set Or Create Local Variable."));
	// Get or Create local variable
	MAKE_ACTION_D(getCreateVarAct, tr("Get/Create Variable"), "overlay=\\align=AlignLeft\\fg=#17dd29\\add/rotate=180/send", btnGetCreate, wVariables, getLocalVar(true),
	              tr("Get Or Create Local Variable. The specified value and unit will be used as defaults if the variable is created."));
	// Copy LVar as new Data Request
	MAKE_ACTION_SC_D(copyVarAct, tr("Copy to Data Request"), "move_to_inbox", btnCopyLVarToRequest, wVariables, copyLocalVarToRequest(), tr("Copy Variable to new Data Request."),
	                 QKeySequence(Qt::ControlModifier | Qt::Key_Down));
	// Open docs import browser for Sim Vars
	MAKE_ACTION_SC(findSimVarAct, tr("Sim Var Lookup"), "search", btnFindSimVar, wVariables, openDocsLookup(DocImports::RecordType::SimVars, ui.cbVariableName),
	               tr("Open a new window to search and select Simulator Variables from imported MSFS SDK documentation."), QKeySequence::Find);
	ui.btnFindSimVar->setVisible(false);  // hide the button, we put the action into the combo box for now

	// Lookup action
	MAKE_ACTION_SC(lookupItemAct, tr("Lookup"), "search", btnVarLookup, wDataLookup, lookupItem(), tr("Query server for ID of named item (Lookup command)."), QKeySequence(Qt::ControlModifier | Qt::Key_Return));

	// Send Key Event Form
	MAKE_ACTION_SC(sendKeyEventAct, tr("Send Key Event"), "send", btnKeyEventSend, wKeyEvent, sendKeyEventForm(), tr("Send the specified Key Event to the server."), QKeySequence(Qt::ControlModifier | Qt::Key_Return));
	// Open docs import browser for Key Events
	MAKE_ACTION_SC(findKeyEventAct, tr("Key Event Lookup"), "search", btnFindEvent, wKeyEvent, openDocsLookup(DocImports::RecordType::KeyEvents, ui.cbKeyEvent),
	               tr("Open a new window to search and select Key Events from imported MSFS SDK documentation."), QKeySequence::Find);
	ui.cbKeyEvent->lineEdit()->addAction(findKeyEventAct, QLineEdit::TrailingPosition);
	ui.btnFindEvent->setHidden(true);  // hide the button, we put the action into the combo box for now

	// Send Command action
	MAKE_ACTION_SC(sendCmdAct, tr("Send Command"), "keyboard_command_key", btnCmdSend, wCommand, sendCommandForm(), tr("Send the selected Command to the server."), QKeySequence(Qt::ControlModifier | Qt::Key_Return));

	// Requests editor form and model view actions

	// connect the Data Request save/add buttons
	MAKE_ACTION_PB(addReqAct, tr("Add Request"), tr("Add"), "add", btnAddRequest, wRequestForm, handleRequestForm(false),
	               tr("Add new request record from current form entries."), QKeySequence(Qt::ControlModifier | Qt::Key_Return));
	MAKE_ACTION_PB_D(saveReqAct, tr("Save Edited Request"), tr("Save"), "edit", btnUpdateRequest, wRequestForm, handleRequestForm(true),
	               tr("Update the existing request record from current form entries."), QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_Return));
	MAKE_ACTION_PB(updReqAct, tr("Clear Form"), tr("Clear"), "scale=.9/backspace", btnClearRequest, wRequestForm, clearRequestForm(),
	               tr("Reset the editor form to default values."), QKeySequence(Qt::ControlModifier | Qt::Key_Backspace));

	// Open docs import browser for Sim Vars
	MAKE_ACTION_SC(findReqSimVarAct, tr("Sim Var Lookup"), "search", btnReqFindSimVar, wRequestForm, openDocsLookup(DocImports::RecordType::SimVars, ui.cbNameOrCode),
	               tr("Open a new window to search and select Simulator Variables from imported MSFS SDK documentation."), QKeySequence::Find);
	ui.btnReqFindSimVar->setVisible(false);  // hide the button, we put the action into the combo box for now

	// Remove selected Data Request(s) from item model/view
	MAKE_ACTION_PB_D(removeRequestsAct, tr("Remove Selected Data Request(s)"), tr("Remove"), "fg=#c2d32e2e/delete_forever", btnReqestsRemove, wRequests, removeSelectedRequests(),
	                 tr("Delete the selected Data Request(s)."), QKeySequence(Qt::ControlModifier | Qt::Key_D));
	// Update data of selected Data Request(s) in item model/view
	MAKE_ACTION_PB_D(updateRequestsAct, tr("Update Selected Data Request(s)"), tr("Update"), "refresh", btnReqestsUpdate, wRequests, updateSelectedRequests(),
	                 tr("Request data update on selected Data Request(s)."), QKeySequence(Qt::ControlModifier | Qt::Key_R, QKeySequence::Refresh));

	// Pause/resume data updates of requests
	MAKE_ACTION_PB_D(pauseRequestsAct, tr("Toggle Updates"), tr("Suspend"), "pause", btnReqestsPause, wRequests, pauseRequests(chk),
	                 tr("Temporarily pause all data value updates on Server side."), QKeySequence(Qt::ControlModifier | Qt::Key_U));
	pauseRequestsAct->setCheckable(true);

	// Save current Requests to a file
	MAKE_ACTION_PB_NC(saveRequestsAct, tr("Save Requests"), tr("Save"), "save", btnReqestsSave, wRequests, tr("Save requests to a file for later use or bring up a dialog with export options."));
	saveRequestsAct->setDisabled(true);
	QMenu *saveRequestsMenu = new QMenu(tr("Requests Save Action"), this);
	saveRequestsMenu->addAction(GLYPH_ICON("keyboard_command_key"), tr("In native WASimUI format"), this, [this]() { d->saveRequests(false); }, QKeySequence::Save);
	saveRequestsMenu->addAction(GLYPH_ICON("overlay=align=AlignRight\\fg=#5eb5ff\\arrow_outward/touch_app"), tr("Export for Touch Portal Plugin with Editor..."),
	                            this, [this]() { d->exportRequests(); }, QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_S));
	saveRequestsMenu->addAction(GLYPH_ICON("touch_app"), tr("Export for Touch Portal Plugin Directly"),
	                            this, [this]() { d->saveRequests(true); }, QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::Key_S));
	saveRequestsAct->setMenu(saveRequestsMenu);

	// Load Requests from a file. This is actually two actions: load and append to existing records + load and replace existing records.
	MAKE_ACTION_PB_NC(loadRequestsAct, tr("Load Requests"), tr("Load"), "folder_open", btnReqestsLoad, wRequests,
	                  tr("<p>Load or Import Requests from a file.</p><p>Files can be in \"native\" <i>WASimUI</> or <i>MSFS Touch Portal Plugin</i> formats. File type is detected automatically.</p>"));
	loadRequestsAct->setShortcut(QKeySequence::Open);
	QMenu *loadRequestsMenu = new QMenu(tr("Requests Load Action"), this);
	loadRequestsMenu->addAction(GLYPH_ICON("view_list"), tr("Replace Existing"), this, [this]() { d->loadRequests(true); }, QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_O));
	loadRequestsMenu->addAction(GLYPH_ICON("playlist_add."), tr("Append to Existing"), this, [this]() { d->loadRequests(false); }, QKeySequence(Qt::ControlModifier | Qt::AltModifier | Qt::Key_O));
	connect(loadRequestsAct, &QAction::triggered, this, [=]() { if (!loadRequestsAct->menu()) d->loadRequests(true); });
	// Change the load/save requests action type depending on number of current rows in data requests model, with or w/out a menu of Add/Replace Existing options.
	connect(d->reqModel, &RequestsModel::rowCountChanged, this, [=](int rows) {
		if (rows) {
			if (!loadRequestsAct->menu())
				loadRequestsAct->setMenu(loadRequestsMenu);
		}
		else if (loadRequestsAct->menu())
			loadRequestsAct->setMenu(nullptr);
		d->toggleRequestButtonsState();
	}, Qt::QueuedConnection);

	// Add column toggle and font size actions to the parent widgets
	ui.wRequestForm->addAction(ui.requestsView->actionsMenu(ui.wRequests)->menuAction());
	ui.wRequests->addAction(ui.requestsView->actionsMenu(ui.wRequests)->menuAction());

	// Registered calculator events model view actions

	// Remove selected Data Request(s) from item model/view
	MAKE_ACTION_PB_D(removeEventsAct, tr("Remove Selected Event(s)"), tr("Remove"), "fg=#c2d32e2e/delete_forever", btnEventsRemove, wEventsList, removeSelectedEvents(),
	                 tr("Delete the selected Event(s)."), QKeySequence(Qt::ControlModifier | Qt::Key_D));
	// Update data of selected Data Request(s) in item model/view
	MAKE_ACTION_PB_D(updateEventsAct, tr("Transmit Selected Event(s)"), tr("Transmit"), "play_for_work", btnEventsTransmit, wEventsList, transmitSelectedEvents(),
	                 tr("Trigger the selected Event(s)."), QKeySequence(Qt::ControlModifier | Qt::Key_T));

	// Save current Events to a file
	MAKE_ACTION_PB_D(saveEventsAct, tr("Save Events"), tr("Save"), "save", btnEventsSave, wEventsList, saveEvents(), tr("Save current Events list to file."), QKeySequence::Save);
	// Load Events from a file. This is actually two actions: load and append to existing records + load and replace existing records.
	MAKE_ACTION_PB_NC(loadEventsAct, tr("Load Events"), tr("Load"), "folder_open", btnEventsLoad, wEventsList, tr("Load saved Events from file."));
	QMenu *loadEventsMenu = new QMenu(tr("Events Load Action"), this);
	QAction *replaceEventsAct = loadEventsMenu->addAction(GLYPH_ICON("view_list"), tr("Replace Existing"));
	QAction *appendEventsAct = loadEventsMenu->addAction(GLYPH_ICON("playlist_add"), tr("Append to Existing"));
	connect(replaceEventsAct, &QAction::triggered, this, [this]() { d->loadEvents(true); });
	connect(appendEventsAct, &QAction::triggered, this, [this]() { d->loadEvents(false); });
	connect(loadEventsAct, &QAction::triggered, this, [=]() { if (!loadEventsAct->menu()) d->loadEvents(true); });
	// Change the action type depending on number of current rows in events model, with or w/out a menu of Add/Replace Existing options.
	connect(d->eventsModel, &EventsModel::rowCountChanged, this, [=](int rows) {
		if (rows) {
			if (!loadEventsAct->menu())
				loadEventsAct->setMenu(loadEventsMenu);
		}
		else if (loadEventsAct->menu())
			loadEventsAct->setMenu(nullptr);
		saveEventsAct->setEnabled(rows > 0);
	}, Qt::QueuedConnection);
	// Add column toggle and font size actions to the parent widget
	ui.wEventsList->addAction(ui.eventsView->actionsMenu(ui.wRequests)->menuAction());

	// Other UI-related actions

	QMenu *viewMenu = new QMenu(tr("View"), this);
	viewMenu->setIcon(GLYPH_ICON("grid_view"));
	//viewMenu->menuAction()->setShortcut(QKeySequence(Qt::AltModifier | Qt::Key_M));

	MAKE_ACTION_NW_SC(docBrowserAct, tr("SimConnect SDK Docs Reference Browser"), tr("Reference"), "search", openDocsLookupWindow(),
	                  tr("<p>Opens a window which allow searching through Simulator Variables, Key Events, and Unit types imported from online SimConnect SDK documentation.</p>"),
	                  QKeySequence(Qt::AltModifier | Qt::Key_R));
	viewMenu->addAction(docBrowserAct);

#define WIDGET_VIEW_TOGGLE_ACTION(T, W, V, K)  {\
		QAction *act = new QAction(tr("Show %1 Form").arg(T), this); \
		act->setAutoRepeat(false); act->setCheckable(true); act->setChecked(V);  \
		act->setShortcut(QKeySequence(Qt::AltModifier | Qt::Key_##K)); \
		W->addAction(act); W->setWindowTitle(T); W->setVisible(V);  \
		connect(act, &QAction::toggled, W, &QWidget::setVisible); \
		d->formWidgets.append({T, W, act}); viewMenu->addAction(act); \
	}
	WIDGET_VIEW_TOGGLE_ACTION(tr("Calculator Code"), ui.wCalcForm, true, C)
	WIDGET_VIEW_TOGGLE_ACTION(tr("Variables"), ui.wVariables, true, V)
	WIDGET_VIEW_TOGGLE_ACTION(tr("Lookup"), ui.wDataLookup, true, L)
	WIDGET_VIEW_TOGGLE_ACTION(tr("Key Events"), ui.wKeyEvent, true, K)
	WIDGET_VIEW_TOGGLE_ACTION(tr("API Command"), ui.wCommand, false, A)
	WIDGET_VIEW_TOGGLE_ACTION(tr("Data Request Editor"), ui.wRequestForm, true, R)
#undef WIDGET_VIEW_TOGGLE_ACTION

	viewMenu->addActions({ ui.dwRequests->toggleViewAction(), ui.dwEventsList->toggleViewAction(), ui.dwLog->toggleViewAction() });

	QAction *styleAct = new QAction(GLYPH_ICON("style"), tr("Toggle Dark/Light Theme"), this);
	styleAct->setIconText(tr("Theme"));
	styleAct->setCheckable(true);
	styleAct->setShortcut(tr("Alt+D"));
	connect(styleAct, &QAction::triggered, &Utils::toggleAppStyle);

	QAction *aboutAct = new QAction(GLYPH_ICON("info"), tr("About"), this);
	aboutAct->setShortcut(QKeySequence::HelpContents);
	connect(aboutAct, &QAction::triggered, this, [this]() {Utils::about(this); });

	QAction *projectLinkAct = new QAction(GLYPH_ICON("IcoMoon-Free/github"), tr("Project Site"), this);
	connect(projectLinkAct, &QAction::triggered, this, [this]() { QDesktopServices::openUrl(QUrl(WSMCMND_PROJECT_URL)); });

	// add all actions to this widget, for context menu and shortcut handling
	addActions({
		d->toggleConnAct, connectMenu->menuAction(),
		Utils::separatorAction(this), docBrowserAct, viewMenu->menuAction(), styleAct, aboutAct, projectLinkAct
	});


	// Set up toolbar
	QToolBar *toolbar = new QToolBar(tr("Main Toolbar"), this);
	addToolBar(Qt::TopToolBarArea, toolbar);
	toolbar->setMovable(false);
	toolbar->setObjectName(QStringLiteral("TOOLBAR_MAIN"));
	toolbar->setStyleSheet(QStringLiteral(
		"QToolBar { border: 0; border-bottom: 1px solid palette(mid); spacing: 6px; margin-left: 12px; }"
		"QToolBar::separator { background-color: palette(mid); width: 1px; padding: 0; margin: 6px 8px; }"
	));
	toolbar->addWidget(Utils::spacerWidget(Qt::Horizontal, 6));
	toolbar->addActions({ d->toggleConnAct });
	toolbar->addSeparator();
	toolbar->addActions({ viewMenu->menuAction(), docBrowserAct, styleAct, aboutAct /*, projectLinkAct*/ });
	// default toolbutton menu mode is lame
	if (QToolButton *tb = qobject_cast<QToolButton *>(toolbar->widgetForAction(d->toggleConnAct))) {
		tb->setMenu(connectMenu);
		tb->setPopupMode(QToolButton::MenuButtonPopup);
	}
	if (QToolButton *tb = qobject_cast<QToolButton *>(toolbar->widgetForAction(viewMenu->menuAction())))
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
	resize(1130, 960);
	resizeDocks({ ui.dwLog }, { 270 }, Qt::Vertical);
	// now restore any saved settings
	d->readSettings();
	styleAct->setChecked(Utils::isDarkStyle());

	// Say Hi!
	d->logUiMessage("Hello!", CommandId::Ack, LogLevel::Info);
}

void WASimUI::onClientEvent(const ClientEvent &ev)
{
	d->statWidget->setStatus(ev);
	int simConnected = (+ev.status & +ClientStatus::SimConnected);
	int isConnected = (+ev.status & +ClientStatus::Connected);
	d->toggleConnAct->setChecked(simConnected && isConnected);
	d->toggleConnAct->setText(simConnected && isConnected ? tr("Disconnect") : simConnected ? tr("Connect Server") : tr("Connect"));
	d->initAct->setChecked(simConnected);
	d->initAct->setText(simConnected ? tr("Disconnect Simulator") : tr("Connect to Simulator"));
	d->connectAct->setChecked(isConnected);
	d->connectAct->setText(isConnected ? tr("Disconnect Server") : tr("Connect to Server") );
	if ((+d->clientStatus & +ClientStatus::Connected) != isConnected) {
		if (isConnected)
			d->statWidget->setServerVersion(d->client->serverVersion());
		d->updateCalcCodeFormState(ui.cbCalculatorCode->currentText());
		d->updateLocalVarsFormState();
		d->toggleRequestButtonsState();
		d->toggleEventButtonsState();
	}
	d->clientStatus = ev.status;
}

void WASimUI::onListResults(const ListResult &list)
{
	if (list.listType != LookupItemType::LocalVariable)
		return;
	ui.cbLvars->clear();
	for (const auto &pair : list.list)
		ui.cbLvars->addItem(QString::fromStdString(pair.second), pair.first);
	ui.cbLvars->model()->sort(0);
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

