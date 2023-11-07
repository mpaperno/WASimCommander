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

#pragma once

#include <QLineEdit>
#include <vector>

class QTimer;
class QKeyEvent;

class FilterLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit FilterLineEdit(QWidget* parent, std::vector<FilterLineEdit*>* filters = nullptr, int columnnum = 0);

    // Override methods for programmatically changing the value of the line edit
    void clear();
    void setText(const QString& text);

Q_SIGNALS:
    void delayedTextChanged(int column, QString text);
    void filterFocused();

protected:
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;

private Q_SLOTS:
    void delayedSignalTimerTriggered();
    void setFilterHelper(const QString& filterOperator, const QString& operatorSuffix = QString(), bool clearCurrent = true);
    void showContextMenu(const QPoint &pos);

private:
    std::vector<FilterLineEdit*>* filterList;
    int columnNumber;
    QTimer* delaySignalTimer;
    QString lastValue;

};
