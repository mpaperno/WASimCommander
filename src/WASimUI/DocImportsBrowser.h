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

#include <QCloseEvent>
#include <QItemSelectionModel>
#include <QMetaEnum>
#include <QSqlField>
#include <QSettings>
#include <QStringBuilder>
#include <QWidget>

#include "ui_DocImportsBrowser.h"
#include "DocImports.h"
#include "FilterTableHeader.h"

namespace WASimUiNS {
	namespace DocImports
{

class DocImportsBrowser : public QWidget
{
	Q_OBJECT

public:
	enum ViewMode { FullViewMode, PopupViewMode };
	Q_ENUM(ViewMode)

	DocImportsBrowser(QWidget *parent = nullptr, RecordType type = RecordType::Unknown, ViewMode viewMode = ViewMode::FullViewMode)
		: QWidget(parent),
		m_model(new DocImportsModel(this))
	{
		setObjectName(QStringLiteral("DocImportsBrowser"));
		ui.setupUi(this);

		setWindowTitle(tr("SimConnect SDK Reference Browser"));
		setContextMenuPolicy(Qt::ActionsContextMenu);

		ui.cbRecordType->setCurrentIndex(-1);
		connect(ui.cbRecordType, &DataComboBox::currentDataChanged, this, &DocImportsBrowser::setRecordTypeVar);

		ui.tableView->setWordWrap(false);
		ui.tableView->setEditTriggers(QTableView::NoEditTriggers);
		ui.tableView->horizontalHeader()->setDefaultSectionSize(175);
		ui.tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
		ui.tableView->setFiltersVisible(true);

		ui.tableView->setModel(m_model);

		//addAction(ui.tableView->actionsMenu(this)->menuAction());
		addAction(ui.tableView->columnToggleMenuAction());
		addAction(ui.tableView->fontSizeMenuAction());
		addAction(ui.tableView->filterToggleAction());

		QAction *closeAct = new QAction(QIcon("close.glyph"), tr("Close"), this);
		closeAct->setShortcuts({ QKeySequence(Qt::Key_Escape), QKeySequence::Close });
		connect(closeAct, &QAction::triggered, this, &DocImportsBrowser::close);
		addAction(closeAct);

		ui.textBrowser->document()->setIndentWidth(2.0);
		ui.textBrowser->document()->setDefaultStyleSheet(QStringLiteral(
			"dd { margin-bottom: 6px; }"
		));

		if (viewMode == ViewMode::FullViewMode)
			connect(ui.tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &DocImportsBrowser::showRecordDetails);
		else
			connect(ui.tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &DocImportsBrowser::onCurrentRowChanged);

		connect(ui.tableView, &QTableView::doubleClicked, this, [=](const QModelIndex &idx)  {
			emit itemSelected(ui.tableView->mapToSource(idx));
		});

		setRecordType(type);
		setViewMode(viewMode);
		setInitialSize();
		loadSettings();
	}

	DocImportsModel *model() const { return m_model; }

public Q_SLOTS:
	void setViewMode(ViewMode mode)
	{
		m_viewMode = mode;
		if (mode == ViewMode::FullViewMode) {
			ui.cbRecordType->setVisible(true);
			ui.toolbarLabel->setText(tr("Select Record Type:"));
			ui.lblTitle->setText(
				tr("Data is imported from SimConnect SDK web page documentation. Use the filters in each column to search.")
			);
		}
		else {
			ui.cbRecordType->setVisible(false);
			ui.toolbarLabel->setText("<b>" + DocImports::recordTypeName(m_model->recordType()) + "</b>");
			ui.lblTitle->setText(
				tr("Double-click to select a record and return to the main window. Press <tt>Escape</tt> key to close. Use the filters to search.")
			);
			QMetaObject::invokeMethod(ui.tableView, "setFilterFocus", Qt::QueuedConnection, Q_ARG(int, 2));
		}
	}

	void setRecordType(RecordType type)
	{
		if (type == RecordType::Unknown || type == m_recordType)
			return;
		saveTypeSettings();
		m_recordType = type;
		m_model->setRecordType(type);
		if (ui.textBrowser->property("currentRow").isValid())
			ui.textBrowser->clear();
		loadTypeSettings();
		ui.lblLastUpdate->setText(tr("Data updated on: &nbsp; <b>%1</b>").arg(m_model->lastDataUpdate(type).toString("d MMMM yyyy")));

		if (!qobject_cast<DataComboBox*>(sender()))
			ui.cbRecordType->setCurrentData((int)type);
	}

	void setRecordTypeVar(const QVariant &type) { setRecordType((RecordType)type.toUInt()); }

	void showRecordDetails(const QModelIndex &key)
	{
		if (m_model->recordType() == RecordType::Unknown || !key.isValid())
			return;

		const QAbstractItemModel *model = ui.tableView->sourceModel();
		const int srcRow = ui.tableView->mapToSource(key).row();
		QString sb(QStringLiteral("<dl>"));
		for (int i = 0; i < model->columnCount(); ++i) {
			const QVariant &d = model->data(model->index(srcRow, i), Qt::DisplayRole);
			if (d.isValid())
				sb.append(QStringLiteral("<dt>%1</dt><dd>%2</dd>").arg(model->headerData(i, Qt::Horizontal).toString(), toHtml(d.toString())));
		}
		sb.append(QStringLiteral("</dl>"));
		ui.textBrowser->setHtml(sb);
		ui.textBrowser->setProperty("currentRow", key.row());
	}

	void saveSettings() const
	{
		QSettings s;
		s.beginGroup(objectName());
		s.beginGroup(QString(QMetaEnum::fromType<ViewMode>().valueToKey(m_viewMode)));
		s.setValue(QStringLiteral("windowGeo"), saveGeometry());
		s.setValue(QStringLiteral("splitterState"), ui.splitter->saveState());
		s.endGroup();
		s.endGroup();
	}

	void loadSettings()
	{
		QSettings s;
		s.beginGroup(objectName());
		s.beginGroup(QString(QMetaEnum::fromType<ViewMode>().valueToKey(m_viewMode)));
		restoreGeometry(s.value(QStringLiteral("windowGeo")).toByteArray());
		ui.splitter->restoreState(s.value(QStringLiteral("splitterState")).toByteArray());
		s.endGroup();
		s.endGroup();
	}

	void saveTypeSettings() const
	{
		if (m_model->recordType() == RecordType::Unknown)
			return;
		QSettings s;
		s.beginGroup(objectName());
		s.beginGroup(QString(QMetaEnum::fromType<RecordType>().valueToKey(m_recordType)));
		s.setValue(QStringLiteral("tableViewState"), ui.tableView->saveState());
		s.endGroup();
		s.endGroup();
	}

	void loadTypeSettings()
	{
		if (m_model->recordType() == RecordType::Unknown)
			return;
		QSettings s;
		s.beginGroup(objectName());
		s.beginGroup(QString(QMetaEnum::fromType<RecordType>().valueToKey(m_recordType)));
		ui.tableView->restoreState(s.value(QStringLiteral("tableViewState")).toByteArray());
		s.endGroup();
		s.endGroup();
	}

Q_SIGNALS:
		void itemSelected(const QModelIndex &row);

protected:
	void closeEvent(QCloseEvent *ev) override {
		saveSettings();
		saveTypeSettings();
		ev->accept();
	}

private Q_SLOTS:
	void onCurrentRowChanged(const QModelIndex &sel, const QModelIndex &prev) {
		if (prev.isValid())
			showRecordDetails(sel);
	}

	void setInitialSize() {
		if (m_viewMode == ViewMode::PopupViewMode)
			resize(width(), 300);
	}

private:
	QString toHtml(const QVariant &txt)
	{
		QString ret(txt.toString().toHtmlEscaped());
		ret.replace('\n', "<br/>");
		return ret;
	}

	Ui::DocImportsBrowserClass ui;
	DocImportsModel *m_model;
	RecordType m_recordType = RecordType::Unknown;
	ViewMode m_viewMode = ViewMode::FullViewMode;
};

	}  // DocImports
}  // WASimUiNS
