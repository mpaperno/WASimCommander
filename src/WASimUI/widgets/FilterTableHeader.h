/*
This file is part of the WASimCommander project.
https://github.com/mpaperno/WASimCommander

Original version from <https://github.com/sqlitebrowser/sqlitebrowser>; Used under GPL v3 license; Modifications applied.
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

#ifndef FILTERTABLEHEADER_H
#define FILTERTABLEHEADER_H

#include <QHeaderView>
#include <vector>

class QTableView;
class FilterLineEdit;

class FilterTableHeader : public QHeaderView
{
		Q_OBJECT

public:
		explicit FilterTableHeader(QTableView* parent = nullptr);
		QSize sizeHint() const override;
		bool hasFilters() const { return (filterWidgets.size() > 0); }
		bool areFiltersVisible() const { return m_filtersVisible; }
		QString filterValue(int column) const;

	public Q_SLOTS:
		void generateFilters(int number);
		void adjustPositions();
		void clearFilters();
		void setFilter(int column, const QString& value);
		void setFocusColumn(int column);
		void setFiltersVisible(bool visible = true);

	Q_SIGNALS:
		void filterChanged(int column, QString value);
		void filterFocused();

	protected:
		void updateGeometries() override;

	private Q_SLOTS:
		void inputChanged(int col, const QString& new_value);

	private:
		std::vector<FilterLineEdit*> filterWidgets {};
		bool m_filtersVisible = false;
};

#endif
