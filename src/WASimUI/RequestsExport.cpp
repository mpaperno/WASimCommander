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

#include <QCloseEvent>
#include <QFileDialog>
#include <QMenu>

#include "RequestsExport.h"
#include "RequestsFormat.h"
#include "RequestsModel.h"
//#include "Widgets.h"

using namespace WASimUiNS;

static bool editFormEmpty(const Ui::RequestsExport ui)
{
	return ui.cbDefaultCategory->currentText().isEmpty() &&
		ui.cbIdPrefix->currentText().isEmpty() &&
		ui.cbFormat->currentText().isEmpty() &&
		ui.cbDefault->currentText().isEmpty() &&
		(ui.cbReplWhat->currentText().isEmpty() || ui.cbReplCol->currentData().toInt() < 0);
}

static void toggleEditFormBtn(const Ui::RequestsExport ui)
{
	bool en = !editFormEmpty(ui);
	bool sel = ui.tableView->selectionModel()->hasSelection();
	ui.pbSetValues->defaultAction()->setEnabled(en && sel);
	ui.pbClearValues->defaultAction()->setEnabled(en);
	ui.pbRegen->menu()->setEnabled(sel);
	ui.pbRegen->setEnabled(sel);
	ui.pbExportSel->defaultAction()->setEnabled(sel);
}


RequestsExportWidget::RequestsExportWidget(RequestsModel *model, QWidget *parent)
	: QWidget(parent)
{
	setObjectName(QStringLiteral("RequestsExportWidget"));
	ui.setupUi(this);

	ui.tableView->setExportCategories(RequestsFormat::categoriesList());
	setModel(model);

	ui.cbDefaultCategory->addItem(0, "");
	const auto &cats = RequestsFormat::categoriesList();
	ui.cbDefaultCategory->addItems(cats.values(), cats.keys());

	ui.cbIdPrefix->setClearButtonEnabled();
	ui.cbFormat->setClearButtonEnabled();
	ui.cbDefault->setClearButtonEnabled();

	ui.cbReplCol->addItem("", -1);
	for (int i = RequestsModel::COL_FIRST_META; i <= RequestsModel::COL_LAST_META; ++i)
		if (i != RequestsModel::COL_META_CAT)
			ui.cbReplCol->addItem(m_model->columnNames[i], i);

	ui.cvReplType->addItem(tr("Replace"), 0);
	ui.cvReplType->addItem(tr("Repl. Regex"), 1);
	ui.cbReplWhat->setPlaceholderText(tr("Search for..."));
	ui.cbReplWhat->setClearButtonEnabled();
	ui.cbReplWith->setPlaceholderText(tr("Replace with..."));
	ui.cbReplWith->setClearButtonEnabled();

#define MAKE_ACTION(ACT, TTL, ICN, BTN, M, TT, KS) \
	QAction *ACT = new QAction(QIcon(QStringLiteral(##ICN ".glyph")), TTL, this); ACT->setAutoRepeat(false); \
	ACT->setToolTip(TT); ui.##BTN->setDefaultAction(ACT); addAction(ACT); ACT->setShortcut(KS);  \
	connect(ACT, &QAction::triggered, this, &RequestsExportWidget::M)
#define MAKE_ACTION_PB(ACT, TTL, IT, ICN, BTN, M, TT, KS)  MAKE_ACTION(ACT, TTL, ICN, BTN, M, TT, KS); ACT->setIconText(" " + IT)
#define MAKE_ACTION_PD(ACT, TTL, IT, ICN, BTN, M, TT, KS)  MAKE_ACTION_PB(ACT, TTL, IT, ICN, BTN, M, TT, KS); ACT->setDisabled(true)

	MAKE_ACTION_PB(exportAllAct, tr("Export All Request(s)"), tr("Export All"), "download_for_offline", pbExportAll, exportAll,
								 tr("Export all the Data Request(s) currently shown in the table."), QKeySequence::Save);
	MAKE_ACTION_PD(exportSelAct, tr("Export Selected Request(s)"), tr("Export Selected"), "downloading", pbExportSel, exportSelected,
								 tr("Export only the Data Request(s) currently selected in the table."), QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_S));

	MAKE_ACTION_PD(updateSelAct, tr("Update Selected Request(s)"), tr("Update Selected"), "edit_note", pbSetValues, updateBulk,
								 tr("<p>Bulk-update any selected Data Request(s) in the table. Only non-empty fields will be applied to their respective columns in the selected records.</p>"
								 "<b>Warning! there is no way to undo bulk updates.</b>"), QKeySequence(Qt::ControlModifier | Qt::Key_Return));

	MAKE_ACTION_PD(clearEditAct, tr("Clear Bulk Update Form"), tr("Clear Form"), "scale=.9/backspace", pbClearValues, clearForm,
								 tr("Reset all fields in the editor to empty default values."), QKeySequence(Qt::ControlModifier | Qt::Key_D));

	MAKE_ACTION_PB(closeAct, tr("Close Window"), tr("Close"), "fg=blue/close", pbCancel, close,
								 tr("Close this export window. Any changes made here are preserved."), QKeySequence::Close);

	QMenu *updateMenu = new QMenu(tr("Regenerate Data..."), ui.pbRegen);
	updateMenu->setIcon(QIcon(QStringLiteral("auto_mode.glyph")));
	updateMenu->setToolTip(tr("<p>Regenerate new export IDs or display names based on current values on selected request row(s).</p>"));
	updateMenu->addAction(QIcon(QStringLiteral("format_list_numbered.glyph")), tr("Regenerate Export ID(s)"), this, &RequestsExportWidget::regenIds);
	updateMenu->addAction(QIcon(QStringLiteral("title.glyph")), tr("Regenerate Display Name(s)"), this, &RequestsExportWidget::regenNames);
	updateMenu->setDisabled(true);

	ui.pbRegen->setIcon(updateMenu->icon());
	ui.pbRegen->setMenu(updateMenu);
	ui.pbRegen->setDisabled(true);
	//ui.pbRegen->setHidden(true);

	addAction(updateMenu->menuAction());
	addAction(ui.tableView->actionsMenu(this)->menuAction());

	connect(ui.cbDefaultCategory, &DataComboBox::currentDataChanged, this, [&]() { toggleEditFormBtn(ui); });
	connect(ui.cbIdPrefix, &QComboBox::currentTextChanged, this, [&]() { toggleEditFormBtn(ui); });
	connect(ui.cbFormat, &QComboBox::currentTextChanged, this, [&]() { toggleEditFormBtn(ui); });
	connect(ui.cbDefault, &QComboBox::currentTextChanged, this, [&]() { toggleEditFormBtn(ui); });
	connect(ui.cbReplCol, &DataComboBox::currentDataChanged, this, [&]() { toggleEditFormBtn(ui); });
	connect(ui.cbReplWhat, &QComboBox::currentTextChanged, this, [&]() { toggleEditFormBtn(ui); });

	loadSettings();
}

void RequestsExportWidget::setModel(RequestsModel *model) {
	m_model = model;
	ui.tableView->setModel(model);
	if (!model)
		return;

	ui.tableView->moveColumn(RequestsModel::COL_META_CAT, 0);
	ui.tableView->moveColumn(RequestsModel::COL_META_ID, 1);
	ui.tableView->moveColumn(RequestsModel::COL_META_NAME, 2);

	ui.tableView->hideColumn(RequestsModel::COL_ID);
	ui.tableView->hideColumn(RequestsModel::COL_TYPE);
	ui.tableView->hideColumn(RequestsModel::COL_SIZE);
	ui.tableView->hideColumn(RequestsModel::COL_VALUE);
	ui.tableView->hideColumn(RequestsModel::COL_TIMESATMP);

	ensureDefaultValues();

	connect(ui.tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=](const QItemSelection &sel, const QItemSelection &) {
		toggleEditFormBtn(ui);
		int cnt = ui.tableView->selectionModel()->selectedRows().count();
		const QString txt(tr("Update %1 %2").arg(cnt).arg(cnt == 1 ? tr("Record") : tr("Records")));
		ui.pbSetValues->defaultAction()->setText(txt);
		ui.pbSetValues->defaultAction()->setIconText(txt);
	});

}

void RequestsExportWidget::closeEvent(QCloseEvent *ev) {
	saveSettings();
	ev->accept();
}

RequestsModel * RequestsExportWidget::model() const { return m_model; }

void RequestsExportWidget::exportAll() { exportRecords(true); }
void RequestsExportWidget::exportSelected() { exportRecords(false); }

void RequestsExportWidget::exportRecords(bool all)
{
	if ((all && !m_model->rowCount()) || (!all && !ui.tableView->selectionModel()->hasSelection()))
		return;

	const QString &fname = QFileDialog::getSaveFileName(this, tr("Export %1 Requests").arg(all ? tr("All") : tr("Selected")), m_lastFile, QStringLiteral("INI files (*.ini)"));
	if (fname.isEmpty())
		return;

	if (fname != m_lastFile) {
		m_lastFile = fname;
		Q_EMIT lastUsedFileChanged(fname);
	}

	const QModelIndexList list = all ? m_model->allRequests() : ui.tableView->selectionModel()->selectedRows(RequestsModel::COL_ID);
	RequestsFormat::exportToPluginFormat(m_model, list, fname);
}

void RequestsExportWidget::updateBulk()
{
	const QModelIndexList &list = ui.tableView->selectionModel()->selectedRows();
	if (list.isEmpty() || editFormEmpty(ui))
		return;
	QString cid = ui.cbDefaultCategory->currentData().toString();
	QString idp = ui.cbIdPrefix->currentText();
	QString fmt = ui.cbFormat->currentText();
	QString def = ui.cbDefault->currentText();

	for (const QModelIndex &r : list) {
		if (!cid.isEmpty()) {
			m_model->setData(m_model->index(r.row(), RequestsModel::COL_META_CAT), cid, Qt::EditRole);
			m_model->setData(m_model->index(r.row(), RequestsModel::COL_META_CAT), ui.cbDefaultCategory->currentText(), Qt::ToolTipRole);
		}

		if (!fmt.isEmpty()) {
			if (fmt == "''" || fmt == "\"\"")
				fmt.clear();
			m_model->setData(m_model->index(r.row(), RequestsModel::COL_META_FMT), fmt, Qt::EditRole);
			m_model->setData(m_model->index(r.row(), RequestsModel::COL_META_FMT), fmt, Qt::ToolTipRole);
		}

		if (!def.isEmpty()) {
			if (def == "''" || def == "\"\"")
				def.clear();
			m_model->setData(m_model->index(r.row(), RequestsModel::COL_META_DEF), def, Qt::EditRole);
			m_model->setData(m_model->index(r.row(), RequestsModel::COL_META_DEF), def, Qt::ToolTipRole);
		}

		if (!idp.isEmpty()) {
			const QModelIndex col = m_model->index(r.row(), RequestsModel::COL_META_ID);
			QString val = m_model->data(col, Qt::EditRole).toString();
			if (idp.startsWith('!'))
				val.replace(QRegularExpression("^" + QRegularExpression::escape(idp.remove(0, 1))), "");
			else
				val.prepend(idp);
			m_model->setData(col, val, Qt::EditRole);
			m_model->setData(col, val, Qt::ToolTipRole);
		}

		if (!ui.cbReplWhat->currentText().isEmpty() && ui.cbReplCol->currentData().toInt() > -1) {
			const QModelIndex col = m_model->index(r.row(), ui.cbReplCol->currentData().toInt());
			if (col.isValid()) {
				QString val = m_model->data(col, Qt::EditRole).toString();
				if (ui.cvReplType->currentData().toInt() == 0)
					val.replace(ui.cbReplWhat->currentText(), ui.cbReplWith->currentText());
				else
					val.replace(QRegularExpression(ui.cbReplWhat->currentText()), ui.cbReplWith->currentText());
				m_model->setData(col, val, Qt::EditRole);
				m_model->setData(col, val, Qt::ToolTipRole);
			}
		}

	}
}

void RequestsExportWidget::regenIds()
{
	const QModelIndexList &list = ui.tableView->selectionModel()->selectedRows(RequestsModel::COL_META_ID);
	if (list.isEmpty())
		return;
	RequestRecord req;
	QString id;
	for (const QModelIndex &idx : list) {
		req = m_model->getRequest(idx.row());
		id = RequestsFormat::generateIdFromName(req);
		if (req.properties.value("id").toString() != id) {
			m_model->setData(idx, id, Qt::EditRole);
			m_model->setData(idx, id, Qt::ToolTipRole);
		}
	}
}

void RequestsExportWidget::regenNames()
{
	const QModelIndexList &list = ui.tableView->selectionModel()->selectedRows(RequestsModel::COL_META_NAME);
	if (list.isEmpty())
		return;
	RequestRecord req;
	QString name;
	for (const QModelIndex &idx : list) {
		req = m_model->getRequest(idx.row());
		name = RequestsFormat::generateDefaultName(req);
		if (req.properties.value("id").toString() != name) {
			m_model->setData(idx, name, Qt::EditRole);
			m_model->setData(idx, name, Qt::ToolTipRole);
		}
	}
}

void RequestsExportWidget::ensureDefaultValues()
{
	for (int row = 0; row < m_model->rowCount(); ++row) {
		RequestRecord req = m_model->getRequest(row);
		RequestsFormat::generateRequiredProperties(req);
		m_model->updateFromMetaData(row, req);
	}
}

void RequestsExportWidget::clearForm()
{
	ui.cbDefaultCategory->setCurrentIndex(0);
	ui.cbIdPrefix->clear();
	ui.cbDefault->clear();
	ui.cbFormat->clear();
	ui.cbReplCol->setCurrentData(-1);
	toggleEditFormBtn(ui);
}

void RequestsExportWidget::saveSettings() const
{
	QSettings s;
	s.beginGroup(objectName());
	s.setValue(QStringLiteral("windowGeo"), saveGeometry());
	s.setValue(QStringLiteral("tableViewState"), ui.tableView->saveState());
	const QList<DeletableItemsComboBox *> editable = findChildren<DeletableItemsComboBox *>();
	for (DeletableItemsComboBox *cb : editable)
		s.setValue(cb->objectName(), cb->saveState());
	s.endGroup();
}

void RequestsExportWidget::loadSettings()
{
	QSettings s;
	s.beginGroup(objectName());
	restoreGeometry(s.value(QStringLiteral("windowGeo")).toByteArray());
	ui.tableView->restoreState(s.value(QStringLiteral("tableViewState")).toByteArray());
	const QList<DeletableItemsComboBox *> editable = findChildren<DeletableItemsComboBox *>();
	for (DeletableItemsComboBox *cb : editable)
		cb->restoreState(s.value(cb->objectName()).toByteArray());
	s.endGroup();
}
