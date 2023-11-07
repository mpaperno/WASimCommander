/*
This file is part of the WASimCommander project.
https://github.com/mpaperno/WASimCommander

Original version of multi-column filtering from <https://pastebin.com/7yUnvW39>; Public domain; Modifications applied.
Original version multi-column sorting from <https://github.com/dimkanovikov/MultisortTableView>; GPL v3 license; Modifications applied.

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

#include <QSortFilterProxyModel>
#include <QRegExp>
#include <QDebug>
#include "AlphanumComparer.h"

class MultiColumnProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT

	public:
		using QSortFilterProxyModel::QSortFilterProxyModel;

		// Reimplemented to show sort icon and order as part of the heading title.
		QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override
		{
			if (orientation == Qt::Horizontal && role == Qt::DisplayRole && m_sortedColumns.contains(section)) {
				const QString header =
					sourceModel()->headerData(section, orientation).toString().append(m_sortedColumns.value(section).sortOrder == Qt::AscendingOrder ? "   ▲" : "   ▼");
				if (m_sortedColumns.count() == 1)
					return header;
				return header + ' ' + toSuperScript(m_sortedColumnsOrder.indexOf(section) + 1);
			}
			return QSortFilterProxyModel::headerData(section, orientation, role);
		}

		void setUseAlphanumericSortingAlgo(bool use = true)  { m_useAlphanumSort = use; }
		bool usingAlphanumericSortingAlgo() const { return m_useAlphanumSort; }

		/// Return sort order as `Qt::SortOrder` enum value of the given column index, or -1 if the column isn't sorted.
		int columnSortOrder (int column) const {
			return m_sortedColumns.contains(column) ? m_sortedColumns.value(column).sortOrder : -1;
		}

		/// Return column's position in the list of sorted columns, or -1 if the column isn't sorted.
		int columnSortPosition (int column) const {
			return m_sortedColumnsOrder.indexOf(column);
		}

		/// Return count of sorted columns
		int sortedColumnsCount () const {
			return m_sortedColumnsOrder.count();
		}

	public Q_SLOTS:

		// Filtering

		virtual void setRegExpFilter(int col, const QRegExp& matcher, int role = Qt::DisplayRole)
		{
			if (matcher.isEmpty() || !matcher.isValid())
				return removeFilterFromColumn(col, role);
			m_filters[(static_cast<qint64>(col) << 32) | static_cast<qint64>(role)] = matcher;
			//qDebug() << col << matcher.pattern() << role;
			invalidateFilter();
		}

		virtual void setStringPatternFilter(int col, QString pattern, int role)
		{
			if (pattern.isEmpty()) {
				removeFilterFromColumn(col);
			}
			else if (pattern.startsWith('/') && pattern.endsWith('/') /*&& pattern.length() > 2*/) {
				setRegExpFilter(col, QRegExp(pattern.mid(1, pattern.length() - 2), Qt::CaseInsensitive, QRegExp::RegExp));
			}
			else {
				if (!pattern.contains('*') && !pattern.contains('?') && !pattern.contains(']'))
					pattern.prepend('*').append('*');
				setRegExpFilter(col, QRegExp(pattern, Qt::CaseInsensitive, QRegExp::WildcardUnix));
			}
		}

		virtual void setDisplayRolePatternFilter(int col, QString pattern) {
			setStringPatternFilter(col, pattern, Qt::DisplayRole);
		}

		virtual void clearFilters()
		{
			m_filters.clear();
			invalidateFilter();
		}

		virtual void removeFilterFromColumn(int col, int role)
		{
			m_filters.remove((static_cast<qint64>(col) << 32) | static_cast<qint64>(role));
			invalidateFilter();
		}

		virtual void removeFilterFromColumn(int col)
		{
			for (auto i = m_filters.begin(); i != m_filters.end();) {
				if ((i.key() >> 32) == col)
					i = m_filters.erase(i);
				else
					++i;
			}
			invalidateFilter();
		}

		// Sorting

		/// Sort by `column` number. If `isModifierPressed` is `true` and the column is not in the current sort list
		/// then it is appended to the list using default Ascending order. If already in the list then the order is reversed.
		/// If sorted a 3rd time, the column is removed from the list (assuming there is more than one).
		/// The modifier is essentially ignored if only one column is being sorted on anyway.
		/// If `isModifierPressed` is `false` then any other currently sorted columns are cleared and this column's order is either set to Ascending or is reversed,
		/// depending on if it was already in the sort list or not.
		/// If `order` is specified as `Qt::AscendingOrder` or `Qt::DescendingOrder`, then the order of sorting is forced instead of being reversed.
		virtual void sortColumn(int column, bool isModifierPressed = false, qint8 order = -1)
		{
			if (isModifierPressed) {
				if (m_sortedColumns.contains(column)) {
					// remove sorted columns on 3rd click, but not if it's the last one.
					if (m_sortedColumns.count() > 1 && m_sortedColumns.value(column).activations >= 2)
						removeSortedColumn(column);
					else if (order == -1)
						changeSortDirection(column, 2);
					else
						setSortDirection(column, (Qt::SortOrder)order, 2);
				}
				else {
					// append new sorted column
					addSortedColumn(column, order == -1 ? Qt::AscendingOrder : (Qt::SortOrder)order);
				}
			}
			else {
				if (m_sortedColumns.contains(column) && order == -1) {
					if (m_sortedColumns.count() == 1) {
						// just change direction
						changeSortDirection(column, 1);
					}
					else {
						// need to remove all other columns first; save this column's order so it can be reversed afterwards.
						Qt::SortOrder ord = reverseOrder(m_sortedColumns.value(column).sortOrder);
						clearSortedColumns();
						addSortedColumn(column, ord);
					}
				}
				else {
					// column wasn't currently sorted on so just clear the rest and add the new one with default order
					clearSortedColumns();
					addSortedColumn(column, order == -1 ? Qt::AscendingOrder : (Qt::SortOrder)order);
				}
			}
			multisort();
		}

		/// Sorts by a single column in given order.
		virtual void sortByColumn(int column, Qt::SortOrder order) {
			clearSortedColumns();
			addSortedColumn(column, order);
			multisort();
		}

	protected:
		bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const override
		{
			for (int i = 0; i < sourceModel()->columnCount(source_parent); ++i) {
				const QModelIndex currntIndex = sourceModel()->index(source_row, i, source_parent);
				for (auto regExpIter = m_filters.constBegin(); regExpIter != m_filters.constEnd(); ++regExpIter) {
					if (static_cast<qint32>(regExpIter.key() >> 32) == i) {
						if (regExpIter.value().indexIn(currntIndex.data(static_cast<qint32>(regExpIter.key() & ((1i64 << 32) - 1))).toString().trimmed()) < 0)
							return false;
					}
				}
			}
			return true;
		}

		bool lessThan(const QModelIndex & left, const QModelIndex & right) const override {
			if (m_useAlphanumSort)
				return AlphanumComparer::lessThan(sourceModel()->data(left).toString(), sourceModel()->data(right).toString());
			return QSortFilterProxyModel::lessThan(left, right);
		}

		virtual void multisort() {
			// Perform the actual sort using superclass method in reverse order of currently sorted columns.
			for(auto col = m_sortedColumnsOrder.crbegin(); col != m_sortedColumnsOrder.crend(); ++col)
				sort(*col, m_sortedColumns.value(*col).sortOrder);
		}

	private:
		void addSortedColumn(int col, Qt::SortOrder order = Qt::AscendingOrder) {
			m_sortedColumns.insert(col, { order, 1 });
			m_sortedColumnsOrder.append(col);
		}

		void removeSortedColumn(int col) {
			m_sortedColumns.remove(col);
			m_sortedColumnsOrder.removeAll(col);
		}

		inline void clearSortedColumns() {
			m_sortedColumns.clear();
			m_sortedColumnsOrder.clear();
		}

		void changeSortDirection(int col, quint8 activations = 1) {
			m_sortedColumns.insert(col, { reverseOrder(m_sortedColumns.value(col).sortOrder), activations });
		}

		void setSortDirection(int col, Qt::SortOrder order, quint8 activations = 1) {
			m_sortedColumns.insert(col, { order, activations });
		}

	private:
		QHash<qint64, QRegExp> m_filters {};

		struct SortTrack {
			Qt::SortOrder sortOrder = Qt::AscendingOrder;
			quint8 activations = 1;
		};
		QHash<int, SortTrack> m_sortedColumns;
		QVector<int> m_sortedColumnsOrder;
		bool m_useAlphanumSort = true;

		static Qt::SortOrder reverseOrder(Qt::SortOrder ord) {
			return ord == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
		}

		static QString toSuperScript(int number) {
			QString n = QString::number(number);
			for (int i=0, e=n.length(); i < e; ++i) {
				switch (n[i].toLatin1()) {
					case '1': n.replace(i, 1, "¹"); break;
					case '2': n.replace(i, 1, "²"); break;
					case '3': n.replace(i, 1, "³"); break;
					case '4': n.replace(i, 1, "⁴"); break;
					case '5': n.replace(i, 1, "⁵"); break;
					case '6': n.replace(i, 1, "⁶"); break;
					case '7': n.replace(i, 1, "⁷"); break;
					case '8': n.replace(i, 1, "⁸"); break;
					case '9': n.replace(i, 1, "⁹"); break;
					case '0': n.replace(i, 1, "⁰"); break;
				}
			}
			return n;
		}
};
