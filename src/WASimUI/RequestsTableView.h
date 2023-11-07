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

#include <QItemEditorCreatorBase>
#include <QHeaderView>
#include <QMenu>
#include <QStyledItemDelegate>
#include <QTableView>

#include "CustomTableView.h"

#include "RequestsModel.h"
#include "Widgets.h"

namespace WASimUiNS
{

class CategoryDelegate : public QStyledItemDelegate
{
	Q_OBJECT
	public:
		QMap<QString, QString> textDataMap {};

		using QStyledItemDelegate::QStyledItemDelegate;

		QWidget *createEditor(QWidget *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
		{
			DataComboBox *cb = new DataComboBox(p);
			cb->addItems(textDataMap);
			connect(cb, &DataComboBox::currentTextChanged, this, &CategoryDelegate::commit);
			return cb;
		}

		void setEditorData(QWidget *editor, const QModelIndex &index) const override {
			editor->setProperty("currentText", index.data(Qt::EditRole));
		}
		void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
			model->setData(index, editor->property("currentText"), Qt::EditRole);
			model->setData(index, editor->property("currentData"), Qt::ToolTipRole);
		}

		void commit() {
			if (DataComboBox *cb = qobject_cast<DataComboBox*>(sender()))
				emit commitData(cb);
		}
};

class RequestsTableView : public CustomTableView
{
	Q_OBJECT

	public:
		RequestsTableView(QWidget *parent)
			: CustomTableView(parent),
			m_cbCategoryDelegate{new CategoryDelegate(this)}
		{
			setObjectName(QStringLiteral("RequestsTableView"));

		}

	public Q_SLOTS:
		void setExportCategories(const QMap<QString, QString> &map) { m_cbCategoryDelegate->textDataMap = map; }

		void setModel(RequestsModel *model)
		{
			CustomTableView::setModel(model);

			QHeaderView *hdr = horizontalHeader();
			hdr->resizeSection(RequestsModel::COL_ID, 40);
			hdr->resizeSection(RequestsModel::COL_TYPE, 65);
			hdr->resizeSection(RequestsModel::COL_RES_TYPE, 55);
			hdr->resizeSection(RequestsModel::COL_NAME, 265);
			hdr->resizeSection(RequestsModel::COL_IDX, 30);
			hdr->resizeSection(RequestsModel::COL_UNIT, 55);
			hdr->resizeSection(RequestsModel::COL_SIZE, 85);
			hdr->resizeSection(RequestsModel::COL_PERIOD, 60);
			hdr->resizeSection(RequestsModel::COL_INTERVAL, 40);
			hdr->resizeSection(RequestsModel::COL_EPSILON, 60);
			hdr->resizeSection(RequestsModel::COL_VALUE, 70);
			hdr->resizeSection(RequestsModel::COL_TIMESATMP, 70);

			hdr->resizeSection(RequestsModel::COL_META_ID, 130);
			hdr->resizeSection(RequestsModel::COL_META_NAME, 175);
			hdr->resizeSection(RequestsModel::COL_META_CAT, 110);
			hdr->resizeSection(RequestsModel::COL_META_DEF, 70);
			hdr->resizeSection(RequestsModel::COL_META_FMT, 50);

			setItemDelegateForColumn(RequestsModel::COL_META_CAT, m_cbCategoryDelegate);
		}

	private:
		CategoryDelegate *m_cbCategoryDelegate;
};

}
