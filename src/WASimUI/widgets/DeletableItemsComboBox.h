/*
DeletableItemsComboBox
https://github.com/mpaperno/WASimCommander

COPYRIGHT: (c)2022 Maxim Paperno; All Right Reserved.
Contact: http://www.WorldDesign.com/contact

LICENSE:

Commercial License Usage
Licensees holding valid commercial licenses may use this file in
accordance with the terms contained in a written agreement between
you and the copyright holder.

GNU General Public License Usage
Alternatively, this file may be used under the terms of the GNU
General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License is available at <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <QApplication>
#include <QAbstractItemView>
#include <QCompleter>
#include <QLineEdit>
#include <QMenu>

#include "DataComboBox.h"

class DeletableItemsComboBox : public DataComboBox
{
	Q_OBJECT
	Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
public:
	DeletableItemsComboBox(QWidget *p = nullptr) : DataComboBox(p)
	{
		DataComboBox::setEditable(true);
		setInsertPolicy(InsertAtTop);
		setSizeAdjustPolicy(AdjustToContents);
		//setMinimumContentsLength(25);
		setMaxVisibleItems(25);
		setCurrentIndex(-1);
		setToolTip(tr(
			"<p>Press enter after entering text to save it in the list for future selection.</p>"
			"<p>Saved items can be removed by right-clicking on them while the list is open.</p>"
		));

		m_defaultCompleter = completer();
		m_defaultCompleter->setParent(this);  // Set a parent so QLineEdit doesn't delete it when removing.
		m_completerFilter = m_defaultCompleter->filterMode();
		m_completerMode = m_defaultCompleter->completionMode();
		m_completerEnabled = true;

		setContextMenuPolicy(Qt::CustomContextMenu);
		connect(this, &DataComboBox::customContextMenuRequested, this, &DeletableItemsComboBox::showContextMenu);

		connect(lineEdit(), &QWidget::destroyed, this, &DeletableItemsComboBox::updateCompleterOption, Qt::UniqueConnection);

		connect(this, &DeletableItemsComboBox::editTextChanged, this, [this](const QString &txt) {
			if (txt.isEmpty())
				setCurrentIndex(-1);
		});
		connect(view(), &QAbstractItemView::pressed, [this](const QModelIndex &idx) {
			if (idx.isValid() && !idx.data(Qt::UserRole).isValid() && (QApplication::mouseButtons() & Qt::RightButton))
				model()->removeRow(idx.row());
		});

		// Completer options actions/menu
		QMenu *compMenu = new QMenu(this);
		auto addAct = [=](const QString &t, int role, int d, const QString &i) {
			QAction *a = compMenu->addAction(QIcon(i + ".glyph"), t);
			a->setCheckable(true);
			a->setData(int(d));
			a->setProperty("role", role);
			compMenu->addAction(a);
			return a;
		};
		addAct(tr("Disable Suggestions"), 0, 1, "not_interested");
		compMenu->addSeparator();
		addAct(tr("Match Starts With"), 1, (int)Qt::MatchStartsWith, "start")->setChecked(m_completerFilter == Qt::MatchStartsWith);
		addAct(tr("Match Contains"), 1, (int)Qt::MatchContains, "rotate=90/vertical_align_center")->setChecked(m_completerFilter == Qt::MatchContains);
		addAct(tr("Match Ends With"), 1, (int)Qt::MatchEndsWith, "rotate=180/start")->setChecked(m_completerFilter == Qt::MatchEndsWith);
		compMenu->addSeparator();
		addAct(tr("Show inline suggestions"), 2, (int)QCompleter::InlineCompletion, "Material Icons,9,5,50,1,1,0,0,0/title")->setChecked(m_completerMode == QCompleter::InlineCompletion);
		addAct(tr("Show pop-up suggestions"), 2, (int)QCompleter::PopupCompletion, "IcoMoon-Free/option")->setChecked(m_completerMode == QCompleter::PopupCompletion);
		addAct(tr("Show unfiltered pop-up suggestions"), 2, (int)QCompleter::UnfilteredPopupCompletion, "filter_list_off")->setChecked(m_completerMode == QCompleter::UnfilteredPopupCompletion);

		connect(compMenu, &QMenu::triggered, this, &DeletableItemsComboBox::onCompleterOptionAction);

		m_completerAction = new QAction(QIcon("manage_search.glyph"), tr("Typing Suggestions"), this);
		m_completerAction->setToolTip(tr("Set options for the suggestions while you type are determined and presented."));
		m_completerAction->setMenu(compMenu);
		addAction(compMenu->menuAction());
	}

	QAction *completerChoicesMenuAction() const { return m_completerAction->menu()->menuAction(); }
	QString placeholderText() const { return lineEdit() ? lineEdit()->placeholderText() : ""; }

	const QStringList editedItems() const
	{
		QStringList ret;
		if (!isEditable())
			return ret;
		for (int i = 0, e = count(); i < e; ++i) {
			if (!itemData(i).isValid() && !itemText(i).isEmpty())
				ret << itemText(i);
		}
		return ret;
	}

	QByteArray saveState() const
	{
		QByteArray state;
		QDataStream ds(&state, QIODevice::WriteOnly);
		const QStringList items = editedItems();
		ds << items.count();
		for (const QString &itm : items)
			ds << itm;
		ds << (int)m_completerFilter << (int)m_completerMode << (int)m_completerEnabled;
		return state;
	}

	bool restoreState(const QByteArray &state)
	{
		if (state.isEmpty())
			return false;

		QDataStream ds(state);
		// Restore any saved list items
		int itemCount;
		ds >> itemCount;
		if (itemCount > 0 && !ds.atEnd()) {
			QStringList items;
			items.reserve(itemCount);
			QString item;
			for (int i = 0; i < itemCount && !ds.atEnd(); ++i) {
				ds >> item;
				items << item;
			}
			insertEditedItems(items);
		}
		// Restore completer options if there are any
		if (!ds.atEnd()) {
			int tmp;
			ds >> tmp;
			if (tmp != (int)m_completerFilter)
				setCompleterFilterMode((Qt::MatchFlags)tmp);
			ds >> tmp;
			if (tmp != (int)m_completerMode)
				setCompleterCompletionMode((QCompleter::CompletionMode)tmp);
			if (!ds.atEnd()) {
				ds >> tmp;
				if ((bool)tmp != m_completerEnabled && !tmp)
					setCompleterDisabled();
			}
		}

		return true;
	}

public Q_SLOTS:

	void setClearButtonEnabled(bool enabled = true) {
		if (lineEdit()) lineEdit()->setClearButtonEnabled(enabled);
	}

	void setPlaceholderText(const QString &text) {
		if (lineEdit())
			lineEdit()->setPlaceholderText(text);
	}

	void setMaxLength(int length) {
		if (lineEdit())
			lineEdit()->setMaxLength(length);
	}

	void setCompleterOptionsButtonEnabled(bool en = true)
	{
		m_completerButtonEnabled = en;

		if (!lineEdit())
			return;

		if (!en)
			lineEdit()->removeAction(m_completerAction);
		else if (!lineEdit()->actions().contains(m_completerAction))
			lineEdit()->addAction(m_completerAction, QLineEdit::TrailingPosition);
	}

	void setEditable(bool on = true) {
		if (on != isEditable()) {
			DataComboBox::setEditable(on);
			onLineEditChange();
		}
	}

	void setLineEdit(QLineEdit *le) {
		DataComboBox::setLineEdit(le);
		onLineEditChange();
	}

	// Reimplemented to save the last set completer for toggling it on/off.
	// There's apparently no way to temporarily suspend a completer.
	void setCompleter(QCompleter *c, bool setOptionsFromCompleter = false)
	{
		if (c == m_defaultCompleter) {
			resetCompleter();
			return;
		}
		// Make sure the line editor doesn't own the completer otherwise it will delete it when removing.
		if (c && (!c->parent() || c->parent() == lineEdit()))
			c->setParent(this);
		m_customCompleter = c;
		if (m_completerEnabled || setOptionsFromCompleter) {
			m_completerEnabled = true;
			DataComboBox::setCompleter(c);
		}
		if (!c || setOptionsFromCompleter) {
			updateCompleterOption();
			return;
		}
		if ((c = completer())) {
			c->setFilterMode(m_completerFilter);
			c->setCompletionMode(m_completerMode);
		}
	}

	// Reset completer back to the QComboBox default one.
	void resetCompleter() {
		DataComboBox::setCompleter(m_defaultCompleter);
		updateCompleterOption();
	}

	// Sets the completer to either a custom one provided previously in `setCompleter()` or sets up the default completer.
	void setCompleterEnabled(bool enabled = true) {
		if (enabled) {
			DataComboBox::setCompleter(!!m_customCompleter ? m_customCompleter : m_defaultCompleter);
			updateCompleterOption();
		}
		else {
			setCompleterDisabled();
		}
	}

	void setCompleterDisabled(bool disabled = true) {
		if (disabled) {
			DataComboBox::setCompleter(nullptr);
			updateCompleterOption();
		}
		else {
			setCompleterEnabled();
		}
	}
	void setCompleterFilterMode(Qt::MatchFlags flags)
	{
		if (!completer())
			setCompleterEnabled();
		completer()->setFilterMode(flags);
		updateCompleterOption();
	}

	void setCompleterCompletionMode(QCompleter::CompletionMode mode)
	{
		if (!completer())
			setCompleterEnabled();
		completer()->setCompletionMode(mode);
		updateCompleterOption();
	}

	void setCompleterOptions(Qt::MatchFlags flags, QCompleter::CompletionMode mode, bool enableButton = true) {
		setCompleterFilterMode(flags);
		setCompleterCompletionMode(mode);
		setCompleterOptionsButtonEnabled(enableButton);
	}

	void insertEditedItems(const QStringList &items, InsertPolicy policy = NoInsert)
	{
		if (!isEditable() || insertPolicy() == NoInsert || !items.length())
			return;
		if (policy == NoInsert)
			policy = insertPolicy();

		const int currIdx = currentIndex();
		if (policy == InsertAlphabetically) {
			addItems(items);
			model()->sort(0);
		}
		else {
			int index = 0;
			switch (policy) {
				case InsertAtTop:
					break;
				case InsertAtBottom:
					index = count();
					break;
				case InsertAfterCurrent:
					index = qMax(0, currentIndex());
					break;
				case InsertBeforeCurrent:
					index = qMax(0, currentIndex() - 1);
					break;
			}
			QStringList sorted(items);
			std::sort(sorted.begin(), sorted.end());
			insertItems(index, sorted);
		}
		if (currIdx == -1)
			setCurrentIndex(currIdx);
	}

private Q_SLOTS:
	void onLineEditChange() {
		if (lineEdit()) {
			setCompleterOptionsButtonEnabled(m_completerButtonEnabled);
			connect(lineEdit(), &QWidget::destroyed, this, &DeletableItemsComboBox::updateCompleterOption, Qt::UniqueConnection);
		}
		updateCompleterOption();
	}

	void onCompleterOptionAction(QAction *act) {
		const int role = act->property("role").toInt();
		if (role == 1)
			setCompleterFilterMode((Qt::MatchFlags)act->data().toInt());
		else if (role == 2)
			setCompleterCompletionMode((QCompleter::CompletionMode)act->data().toInt());
		else
			setCompleterDisabled();
	}

	void updateCompleterOption()
	{
		if (completer()) {
			m_completerEnabled = true;
			m_completerFilter = completer()->filterMode();
			m_completerMode = completer()->completionMode();
		}
		else {
			m_completerEnabled = false;
			m_completerAction->setEnabled(!!lineEdit());
		}

		for (QAction *a : m_completerAction->menu()->actions()) {
			int role = a->property("role").toInt();
			int data = a->data().toInt();
			a->setChecked(
				(role == 0 && (!m_completerEnabled || !completer())) ||
				(role == 1 && data == m_completerFilter && m_completerEnabled) ||
				(role == 2 && data == m_completerMode && m_completerEnabled)
			);
		}
	}

	void showContextMenu(const QPoint &pos)
	{
		if (!lineEdit())
			return;
		QMenu* menu = lineEdit()->createStandardContextMenu();
		menu->addSeparator();
		menu->addAction(m_completerAction->menu()->menuAction());
		menu->exec(mapToGlobal(pos));
	}

private:
	QCompleter *m_defaultCompleter = nullptr;
	QCompleter *m_customCompleter = nullptr;
	QAction *m_completerAction = nullptr;
	Qt::MatchFlags m_completerFilter;
	QCompleter::CompletionMode m_completerMode;
	bool m_completerEnabled;
	bool m_completerButtonEnabled = false;
};

