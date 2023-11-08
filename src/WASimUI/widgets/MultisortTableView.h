/*
This file is part of the WASimCommander project.
https://github.com/mpaperno/WASimCommander

Original version from <https://github.com/dimkanovikov/MultisortTableView>;  Used under GPL v3 license; Modifications applied.

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

#include <QApplication>
#include <QHeaderView>
#include <QTableView>
#include <QSortFilterProxyModel>

#include "MultiColumnProxyModel.h"

class MultisortTableView : public QTableView
{
	Q_OBJECT
	public:
		explicit MultisortTableView(QWidget *parent = 0) :
			QTableView(parent),
			m_isSortingEnabled(false),
			m_proxyModel(new MultiColumnProxyModel(this)),
			m_modifier(Qt::ControlModifier)
		{
			QTableView::setSortingEnabled(false);  // we do our own sorting
			setupHeader();
		}

		/// Overrides parent method for internal handling of sorting state.
		bool isSortingEnabled() const { return m_isSortingEnabled; }
		/// Returns the custom proxy model used for sorting and filtering.
		MultiColumnProxyModel *proxyModel() const { return m_proxyModel; }
		/// Returns the original source model.
		QAbstractItemModel *sourceModel() const { return m_proxyModel->sourceModel(); }

		virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const { return m_proxyModel->mapToSource(proxyIndex); }
		virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const { return m_proxyModel->mapFromSource(sourceIndex); }
		virtual QItemSelection mapSelectionToSource(const QItemSelection &proxySelection) const { return m_proxyModel->mapSelectionToSource(proxySelection); }
		virtual QItemSelection mapSelectionFromSource(const QItemSelection &sourceSelection) const { return m_proxyModel->mapSelectionFromSource(sourceSelection); }

		bool hasSelection() const { return selectionModel()->hasSelection(); }
		QItemSelection selection() const { mapSelectionToSource(selectionModel()->selection()); }

		QModelIndexList selectedRows(int column = 0) const
		{
			QModelIndexList sel = selectionModel()->selectedRows(column);
			for (QModelIndex &i : sel)
				i = mapToSource(i);
			return sel;
		}

	public Q_SLOTS:
		/// Set key modifier to handle multicolumn sorting.
		void setModifier(Qt::KeyboardModifier modifier) { m_modifier = modifier; }

		/// Overrides parent method for internal handling of sorting state.
		void setSortingEnabled(bool enable) {
			m_isSortingEnabled = enable;
			if (enable)
				m_proxyModel->sortColumn(horizontalHeader()->sortIndicatorSection(), false, (qint8)horizontalHeader()->sortIndicatorOrder());
		}

		virtual void setStringFilter(int col, QString pattern, int role = Qt::DisplayRole) {
			m_proxyModel->setStringPatternFilter(col, pattern, role);
		}

		virtual void setDisplayRoleStringFilter(int col, QString pattern) {
			m_proxyModel->setStringPatternFilter(col, pattern, Qt::DisplayRole);
		}

		virtual void setRegExpFilter(int col, const QRegExp& pattern, int role = Qt::DisplayRole) {
			m_proxyModel->setRegExpFilter(col, pattern, Qt::DisplayRole);
		}

		/// Overrides parent method for hooking into header's clicked signal and remove default sort indicator.
		virtual void setHorizontalHeader(QHeaderView *hdr) {
			QTableView::setHorizontalHeader(hdr);
			setupHeader();
		}

		/// Overridden to use custom sorting model;
		virtual void setModel(QAbstractItemModel *model) {
			m_proxyModel->setSourceModel(model);
			QTableView::setModel(m_proxyModel);
		}

	private Q_SLOTS:
		void onHeaderSectionrClicked(int column) {
			if (m_isSortingEnabled)
				m_proxyModel->sortColumn(column, (QApplication::keyboardModifiers() & m_modifier));
			//qDebug() << column << (Qt::SortOrder)m_proxyModel->columnSortOrder(column);
		}
		void onSortIndicatorChanged(int column, Qt::SortOrder order) {
			//if (m_isSortingEnabled)
				//m_proxyModel->sortColumn(column, (QApplication::keyboardModifiers() & m_modifier), (qint8)order);
			//qDebug() << column << horizontalHeader()->sortIndicatorSection() << horizontalHeader()->sortIndicatorOrder();
		}

		void setupHeader() {
			horizontalHeader()->setSortIndicatorShown(false);  // we provide our own indicators
			horizontalHeader()->setSectionsClickable(true);    // make sure to receive click events so we can sort
			connect(horizontalHeader(), &QHeaderView::sectionClicked, this, &MultisortTableView::onHeaderSectionrClicked);
			//connect(horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, &MultisortTableView::onSortIndicatorChanged);
		}

	private:
		// Sorting enable state
		bool m_isSortingEnabled;
		// ProxyModel to sorting columns
		MultiColumnProxyModel *m_proxyModel;
		// Modifier to handle multicolumn sorting
		Qt::KeyboardModifier m_modifier;

};
