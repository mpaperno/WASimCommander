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

#include "FilterLineEdit.h"

#include <QTimer>
#include <QKeyEvent>
#include <QMenu>
#include <QWhatsThis>

FilterLineEdit::FilterLineEdit(QWidget* parent, std::vector<FilterLineEdit*>* filters, int columnnum) :
    QLineEdit(parent),
    filterList(filters),
    columnNumber(columnnum)
{
    setPlaceholderText(tr("Filter"));
    setClearButtonEnabled(true);
    setProperty("column", columnnum);            // Store the column number for later use

    // Introduce a timer for delaying the signal triggered whenever the user changes the filter value.
    // The idea here is that the textChanged() event isn't connected to the update filter slot directly anymore
    // but instead there this timer mechanism in between: whenever the user changes the filter the delay timer
    // is (re)started. As soon as the user stops typing the timer has a chance to trigger and call the
    // delayedSignalTimerTriggered() method which then stops the timer and emits the delayed signal.
    delaySignalTimer = new QTimer(this);
    delaySignalTimer->setInterval(200);  // This is the milliseconds of not-typing we want to wait before triggering
    connect(this, &FilterLineEdit::textChanged, delaySignalTimer, QOverload<>::of(&QTimer::start));
    connect(delaySignalTimer, &QTimer::timeout, this, &FilterLineEdit::delayedSignalTimerTriggered);

    const QString help(tr(
      "<p>These input fields allow you to perform quick filters in the currently selected table.</p>"
      "All filters are case-insensitive. By default a search is performed anywhere in the column's value (<tt>*text*</tt>).<br/>"
      "The following operators are also supported:<br/>"
      "<table>"
      "<tr><td><tt>*</tt></td><td>Matches zero or more of any characters.</td></tr>"
      "<tr><td><tt>?</tt></td><td>Matches any single character.</td></tr>"
      "<tr><td><tt>\\*</tt> or <tt>\\?</tt></td><td>Matches a literal asterisk or question mark (\"escapes\" it).</td></tr>"
      "<tr><td><tt>[...]</tt></td><td>Sets of characters can be represented in square brackets, similar to full regular expressions.</td></tr>"
      "<tr><td><tt>/regexp/</tt></td><td>Values matching the regular expression between the slashes (<tt>/</tt>).</td></tr>"
      "<table>"
    ));

    setToolTip(help);
    setWhatsThis(help);

    // Immediately emit the delayed filter value changed signal if the user presses the enter or the return key or
    // the line edit widget loses focus
    connect(this, &FilterLineEdit::editingFinished, this, &FilterLineEdit::delayedSignalTimerTriggered);

    // Prepare for adding the What's This information and filter helper actions to the context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &FilterLineEdit::customContextMenuRequested, this, &FilterLineEdit::showContextMenu);
}

void FilterLineEdit::delayedSignalTimerTriggered()
{
    // Stop the timer first to avoid triggering in intervals
    delaySignalTimer->stop();

    // Only emit text changed signal if the text has actually changed in comparison to the last emitted signal. This is necessary
    // because this method is also called whenever the line edit loses focus and not only when its text has definitely been changed.
    if(text() != lastValue)
    {
        // Emit the delayed signal using the current value
        emit delayedTextChanged(columnNumber, text());

        // Remember this value for the next time
        lastValue = text();
    }
}

void FilterLineEdit::keyReleaseEvent(QKeyEvent* event)
{
    if (filterList) {
      if (event->key() == Qt::Key_Tab) {
          if (columnNumber < filterList->size() - 1)
              filterList->at((size_t)columnNumber + 1)->setFocus();
          else
              filterList->at(0)->setFocus();
          return;
      }
      else if (event->key() == Qt::Key_Backtab && columnNumber > 0) {
          filterList->at((size_t)columnNumber - 1)->setFocus();
          return;
      }
    }

    QLineEdit::keyReleaseEvent(event);
}

void FilterLineEdit::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);
    emit filterFocused();
}

void FilterLineEdit::clear()
{
    // When programmatically clearing the line edit's value make sure the effects are applied immediately, i.e.
    // bypass the delayed signal timer
    QLineEdit::clear();
    delayedSignalTimerTriggered();
}

void FilterLineEdit::setText(const QString& text)
{
    // When programmatically setting the line edit's value make sure the effects are applied immediately, i.e.
    // bypass the delayed signal timer
    QLineEdit::setText(text);
    delayedSignalTimerTriggered();
}

void FilterLineEdit::setFilterHelper(const QString& filterOperator, const QString& operatorSuffix, bool clearCurrent)
{
    const QString txt(clearCurrent ? "" : text());
    setText(txt + filterOperator + "?" + operatorSuffix);
    // Select the value for easy editing of the expression
    setSelection(filterOperator.length() + txt.length(), 1);
}

void FilterLineEdit::showContextMenu(const QPoint &pos)
{

    // This has to be created here, otherwise the set of enabled options would not update accordingly.
    QMenu* editContextMenu = createStandardContextMenu();
    editContextMenu->addSeparator();

    QMenu* filterMenu = editContextMenu->addMenu(tr("Set Filter Expression"));

    filterMenu->addAction(QIcon("help_outline.glyph"), tr("What's This?"), this, [&]() {
        QWhatsThis::showText(pos, whatsThis(), this);
    });
    filterMenu->addSeparator();
    filterMenu->addAction(tr("Starts with..."), this, [&]() { setFilterHelper(QString (""), QString("*")); });
    filterMenu->addAction(tr("... ends with"), this, [&]() { setFilterHelper(QString ("*"), QString(""), false); });
    filterMenu->addAction(tr("In range [...]"), this, [&]() { setFilterHelper(QString ("["), QString("]")); });
    filterMenu->addAction(tr("Regular expression..."), this, [&]() { setFilterHelper(QString ("/"), QString ("/")); });

    editContextMenu->exec(mapToGlobal(pos));
}
