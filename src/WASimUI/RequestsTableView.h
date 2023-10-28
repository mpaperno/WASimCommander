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

#include "multisort_view/MultisortTableView.h"

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

class RequestsTableView : public MultisortTableView
{
	Q_OBJECT

	public:
		RequestsTableView(QWidget *parent)
			: MultisortTableView(parent),
			m_cbCategoryDelegate{new CategoryDelegate(this)},
			m_defaultFontSize{font().pointSize()}
		{
			setObjectName(QStringLiteral("RequestsTableView"));

			setContextMenuPolicy(Qt::ActionsContextMenu);
			setEditTriggers(QAbstractItemView::AllEditTriggers);
			setSelectionMode(QAbstractItemView::ExtendedSelection);
			setSelectionBehavior(QAbstractItemView::SelectRows);
			setIconSize(QSize(16, 16));
			setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
			setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
			setGridStyle(Qt::DotLine);
			setSortingEnabled(true);
			setWordWrap(false);
			setCornerButtonEnabled(false);
			verticalHeader()->setVisible(false);

			QHeaderView *hdr = horizontalHeader();
			hdr->setCascadingSectionResizes(false);
			hdr->setMinimumSectionSize(20);
			hdr->setDefaultSectionSize(80);
			hdr->setHighlightSections(false);
			hdr->setSortIndicatorShown(false);
			hdr->setStretchLastSection(true);
			hdr->setSectionsMovable(true);
			hdr->setSectionResizeMode(QHeaderView::Interactive);
			hdr->setContextMenuPolicy(Qt::ActionsContextMenu);
			hdr->setToolTip(tr(
				"<p>"
					"- <tt>CTRL-click</tt> to sort on multiple columns.<br/>"
					"- <tt>Right-click</tt> for menu to toggle column visibility.<br/>"
					"- <tt>Click-and-drag</tt> headings to re-arrange columns.<br/>"
					"- <tt>Double-click</tt> dividers to adjust column width to fit widest content.<br/>"
				"</p>"
			));

			m_fontActions.reserve(3);
			m_fontActions.append(new QAction(QIcon("arrow_upward.glyph"), tr("Increase font size"), this));
			m_fontActions.append(new QAction(QIcon("restart_alt.glyph"), tr("Reset font size"), this));
			m_fontActions.append(new QAction(QIcon("arrow_downward.glyph"), tr("Decrease font size"), this));
			m_fontActions[0]->setShortcuts({ QKeySequence::ZoomIn, QKeySequence(Qt::ControlModifier | Qt::Key_Equal) });
			m_fontActions[1]->setShortcut(QKeySequence(Qt::ControlModifier | Qt::Key_0));
			m_fontActions[2]->setShortcut(QKeySequence::ZoomOut);
			connect(m_fontActions[0], &QAction::triggered, this, &RequestsTableView::fontSizeInc);
			connect(m_fontActions[1], &QAction::triggered, this, &RequestsTableView::fontSizeReset);
			connect(m_fontActions[2], &QAction::triggered, this, &RequestsTableView::fontSizeDec);
		}

		QHeaderView *header() const { return horizontalHeader(); }
		QByteArray saveState() const { return header()->saveState(); }
		const QList<QAction*> &fontActions() const { return m_fontActions; }

		QMenu *columnToggleActionsMenu(QWidget *parent) const {
			QMenu *menu = new QMenu(tr("Toggle table columns"), parent);
			menu->setIcon(QIcon(QStringLiteral("view_column.glyph")));
			menu->addActions(header()->actions());
			return menu;
		}

		QMenu *fontActionsMenu(QWidget *parent) const {
			QMenu *menu = new QMenu(tr("Adjust font size"), parent);
			menu->setIcon(QIcon(QStringLiteral("format_size.glyph")));
			menu->addActions(m_fontActions);
			return menu;
		}

	public Q_SLOTS:
		void setExportCategories(const QMap<QString, QString> &map) { m_cbCategoryDelegate->textDataMap = map; }
		void moveColumn(int from, int to) const { horizontalHeader()->moveSection(from, to); }

		void setModel(RequestsModel *model)
		{
			MultisortTableView::setModel(model);

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

			for (int i=0; i < model->columnCount(); ++i) {
				QAction *act = new QAction(model->headerData(i, Qt::Horizontal).toString(), this);
				act->setCheckable(true);
				act->setChecked(!hdr->isSectionHidden(i));
				act->setProperty("col", i);
				hdr->addAction(act);

				connect(act, &QAction::triggered, this, [=](bool chk) {
					if (QAction *act = qobject_cast<QAction*>(sender())) {
						int id = act->property("col").toInt();
						if (id > -1)
							horizontalHeader()->setSectionHidden(id, !chk);
					}
				});
			}

		}

		void fontSizeReset() {
			QFont f = font();
			if (f.pointSize() != m_defaultFontSize) {
				f.setPointSize(m_defaultFontSize);
				setFont(f);
				resizeRowsToContents();
			}
		}
		void fontSizeInc() {
			QFont f = font();
			f.setPointSize(f.pointSize() + 1);
			setFont(f);
			resizeRowsToContents();
		}
		void fontSizeDec() {
			if (font().pointSize() > 2) {
				QFont f = font();
				f.setPointSize(f.pointSize() - 1);
				setFont(f);
				resizeRowsToContents();
			}
		}

		bool restoreState(const QByteArray &state)
		{
			if (!model())
				return false;
			QHeaderView *hdr = horizontalHeader();
			hdr->restoreState(state);
			for (int i = 0; i < model()->columnCount() && i < hdr->actions().length(); ++i)
				hdr->actions().at(i)->setChecked(!hdr->isSectionHidden(i));
			hdr->setSortIndicatorShown(false);
			return true;
		}

	private:
		CategoryDelegate *m_cbCategoryDelegate;
		QList<QAction*> m_fontActions { };
		int m_defaultFontSize;
};

}
