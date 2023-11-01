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

#include <QHeaderView>
#include <QMenu>
#include <QTableView>
//#include <QDebug>

#include "multisort_view/MultisortTableView.h"

namespace WASimUiNS
{

	class CustomTableView : public MultisortTableView
	{
		Q_OBJECT

	public:
		CustomTableView(QWidget *parent)
			: MultisortTableView(parent),
			m_defaultFontSize{font().pointSize()},
			m_headerToggleMenu{new QMenu(tr("Toggle table columns"), this)},
			m_fontSizeMenu{new QMenu(tr("Adjust table font size"), this)}
		{
			setObjectName(QStringLiteral("CustomTableView"));

			setContextMenuPolicy(Qt::ActionsContextMenu);
			setEditTriggers(DoubleClicked | SelectedClicked | AnyKeyPressed);
			setSelectionMode(ExtendedSelection);
			setSelectionBehavior(SelectRows);
			setVerticalScrollMode(ScrollPerItem);
			setHorizontalScrollMode(ScrollPerPixel);
			setGridStyle(Qt::DotLine);
			setSortingEnabled(true);
			setWordWrap(false);
			setCornerButtonEnabled(false);

			verticalHeader()->setVisible(false);
			verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
			verticalHeader()->setMinimumSectionSize(10);
			adjustRowSize();

			setupHeader();

			m_headerToggleMenu->setIcon(QIcon(QStringLiteral("view_column.glyph")));
			m_fontSizeMenu->setIcon(QIcon(QStringLiteral("format_size.glyph")));

			QAction *plusAct = m_fontSizeMenu->addAction(QIcon("arrow_upward.glyph"), tr("Increase font size"), this, &CustomTableView::fontSizeInc);
			plusAct->setShortcuts({ QKeySequence::ZoomIn, QKeySequence(Qt::ControlModifier | Qt::Key_Equal) });
			m_fontSizeMenu->addAction(QIcon("restart_alt.glyph"), tr("Reset font size"), this, &CustomTableView::fontSizeReset, QKeySequence(Qt::ControlModifier | Qt::Key_0));
			m_fontSizeMenu->addAction(QIcon("arrow_downward.glyph"), tr("Decrease font size"), this, &CustomTableView::fontSizeDec, QKeySequence::ZoomOut);
		}

		QHeaderView *header() const { return horizontalHeader(); }
		QAction *columnToggleMenuAction() const { return m_headerToggleMenu->menuAction(); }
		QAction *fontSizeMenuAction() const { return m_fontSizeMenu->menuAction(); }

		QByteArray saveState() const {
			QByteArray state;
			QDataStream ds(&state, QIODevice::WriteOnly);
			ds << header()->saveState();
			ds << font().pointSize();
			return state;
		}

	public Q_SLOTS:
		void setModel(QAbstractItemModel *model)
		{
			MultisortTableView::setModel(model);
			buildHeaderActions();
		}

		void setHorizontalHeader(QHeaderView *hdr)
		{
			MultisortTableView::setHorizontalHeader(hdr);
			setupHeader();
		}

		void moveColumn(int from, int to) const { horizontalHeader()->moveSection(from, to); }

		void setFontSize(int pointSize) {
			QFont f = font();
			if (pointSize > 3 && f.pointSize() != pointSize) {
				f.setPointSize(pointSize);
				setFont(f);
				adjustRowSize();
			}
		}

		void fontSizeReset() {
			QFont f = font();
			if (f.pointSize() != m_defaultFontSize)
				setFontSize(m_defaultFontSize);
		}
		void fontSizeInc() {
			setFontSize(font().pointSize() + 1);
		}
		void fontSizeDec() {
			if (font().pointSize() > 3)
				setFontSize(font().pointSize() - 1);
		}

		void adjustRowSize() {
			verticalHeader()->setDefaultSectionSize(QFontMetrics(font()).lineSpacing() * 1.65);
		}

		bool restoreState(const QByteArray &state)
		{
			if (!model() || state.isEmpty())
				return false;

			QByteArray hdrState;
			int fontSize = m_defaultFontSize;
			QHeaderView *hdr = horizontalHeader();

			QDataStream ds(state);
			if (!ds.atEnd()) {
				ds >> hdrState;
				if (!ds.atEnd())
					ds >> fontSize;
			}
			else {
				hdrState = state;
			}

			setFontSize(fontSize);
			hdr->restoreState(hdrState);

			for (int i = 0; i < model()->columnCount() && i < hdr->actions().length(); ++i)
				hdr->actions().at(i)->setChecked(!hdr->isSectionHidden(i));
			hdr->setSortIndicatorShown(false);
			return true;
		}

	private:

		void setupHeader()
		{
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
			// make sure header has own font size so it doesn't change when grid font is changed
			QFont f(font());
			f.setPointSize(m_defaultFontSize);
			hdr->setFont(f);

			connect(hdr, &QHeaderView::sectionCountChanged, this, &CustomTableView::onSectionCountChanged, Qt::QueuedConnection);
		}

		void onSectionCountChanged(int oldCnt, int newCnt)
		{
			//qDebug() << oldCnt << newCnt;
			if (oldCnt != newCnt && newCnt != horizontalHeader()->actions().length())
				buildHeaderActions();
		}

		void onHeaderToggled(bool on)
		{
			if (QAction *act = qobject_cast<QAction*>(sender())) {
				bool ok;
				int id = act->property("col").toInt(&ok);
				if (ok && id > -1)
					horizontalHeader()->setSectionHidden(id, !on);
			}
		}

		void buildHeaderActions()
		{
			for (int i=0, e=m_headerToggleMenu->actions().length(); i < e; ++i) {
				if (QAction *act = m_headerToggleMenu->actions().value(0, nullptr)) {
					m_headerToggleMenu->removeAction(act);
					disconnect(act, nullptr, this, nullptr);
					act->deleteLater();
				}
			}

			if (!model())
				return;

			QHeaderView *hdr = horizontalHeader();
			for (int i=0; i < model()->columnCount(); ++i) {
				QAction *act = m_headerToggleMenu->addAction(model()->headerData(i, Qt::Horizontal).toString(), this, &CustomTableView::onHeaderToggled);
				act->setCheckable(true);
				act->setChecked(!hdr->isSectionHidden(i));
				act->setProperty("col", i);
			}
			hdr->addActions(m_headerToggleMenu->actions());
		}

		int m_defaultFontSize;
		QMenu *m_headerToggleMenu;
		QMenu *m_fontSizeMenu;
	};

}
