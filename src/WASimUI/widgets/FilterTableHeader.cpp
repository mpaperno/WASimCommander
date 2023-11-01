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

#include "FilterTableHeader.h"
#include "FilterLineEdit.h"

#include <QApplication>
#include <QTableView>
#include <QScrollBar>

FilterTableHeader::FilterTableHeader(QTableView* parent) :
		QHeaderView(Qt::Horizontal, parent)
{
		// Do some connects: Basically just resize and reposition the input widgets whenever anything changes
		connect(this, &FilterTableHeader::sectionResized, this, &FilterTableHeader::adjustPositions);
		connect(this, &FilterTableHeader::sectionClicked, this, &FilterTableHeader::adjustPositions);
		connect(parent->horizontalScrollBar(), &QScrollBar::valueChanged, this, &FilterTableHeader::adjustPositions);
		connect(parent->verticalScrollBar(), &QScrollBar::valueChanged, this, &FilterTableHeader::adjustPositions);
}

void FilterTableHeader::generateFilters(int number)
{
		// Delete all the current filter widgets
		qDeleteAll(filterWidgets);
		filterWidgets.clear();

		// And generate a bunch of new ones
		for(int i=0; i < number; ++i)
		{
				FilterLineEdit* l = new FilterLineEdit(this, &filterWidgets, i);
				l->setVisible(m_filtersVisible);
				// Set as focus proxy the first non-row-id visible filter-line.
				if(!i)
						setFocusProxy(l);
				connect(l, &FilterLineEdit::delayedTextChanged, this, &FilterTableHeader::inputChanged);
				connect(l, &FilterLineEdit::filterFocused, this, &FilterTableHeader::filterFocused);
				filterWidgets.push_back(l);
		}

		// Position them correctly
		updateGeometries();
}

QSize FilterTableHeader::sizeHint() const
{
		// For the size hint just take the value of the standard implementation and add the height of a input widget to it if necessary
		QSize s = QHeaderView::sizeHint();
		if(m_filtersVisible && filterWidgets.size())
				s.setHeight(s.height() + filterWidgets.at(0)->sizeHint().height() + 4); // The 4 adds just adds some extra space
		return s;
}

void FilterTableHeader::updateGeometries()
{
		// If there are any input widgets add a viewport margin to the header to generate some empty space for them which is not affected by scrolling
		if(m_filtersVisible && filterWidgets.size())
				setViewportMargins(0, 0, 0, filterWidgets.at(0)->sizeHint().height());
		else
				setViewportMargins(0, 0, 0, 0);

		// Now just call the parent implementation and reposition the input widgets
		QHeaderView::updateGeometries();
		adjustPositions();
}

void FilterTableHeader::adjustPositions()
{
	// The two adds some extra space between the header label and the input widget
	const int y = QHeaderView::sizeHint().height() + 2;
	// Loop through all widgets
	for(int i=0, e = (int)filterWidgets.size(); i < e; ++i) {
		// Get the current widget, move it and resize it
		QWidget* w = filterWidgets.at((size_t)i);
		if (QApplication::layoutDirection() == Qt::RightToLeft)
				w->move(width() - (sectionPosition(i) + sectionSize(i) - offset()), y);
		else
				w->move(sectionPosition(i) - offset(), y);
		w->resize(sectionSize(i), w->sizeHint().height());
	}
}

void FilterTableHeader::inputChanged(int col, const QString& new_value)
{
	//adjustPositions();
	// Just get the column number and the new value and send them to anybody interested in filter changes
	emit filterChanged(col, new_value);
}

void FilterTableHeader::clearFilters()
{
	for(FilterLineEdit* filterLineEdit : filterWidgets)
			filterLineEdit->clear();
}

void FilterTableHeader::setFilter(int column, const QString& value)
{
	if(column < filterWidgets.size())
			filterWidgets.at(column)->setText(value);
}

QString FilterTableHeader::filterValue(int column) const
{
	return filterWidgets[column]->text();
}

void FilterTableHeader::setFocusColumn(int column)
{
	if(column < filterWidgets.size())
			filterWidgets.at(column)->setFocus(Qt::FocusReason::TabFocusReason);
}

void FilterTableHeader::setFiltersVisible(bool visible)
{
	if (m_filtersVisible == visible)
		return;
	m_filtersVisible = visible;
	for(FilterLineEdit* filterLineEdit : filterWidgets)
		filterLineEdit->setVisible(visible);
	if (!visible)
		clearFilters();
	updateGeometries();
}
