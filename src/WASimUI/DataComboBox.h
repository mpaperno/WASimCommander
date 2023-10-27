/*
DataComboBox
https://github.com/mpaperno/maxLibQt

COPYRIGHT: (c)2019 Maxim Paperno; All Right Reserved.
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

#ifndef _DATACOMBOBOX_H_
#define _DATACOMBOBOX_H_

#include <QComboBox>

/*!
	\brief The DataComboBox is a \c QComboBox with focus on the \c data() value of each item, vs. its \c index or \c text value.
	It adds a new \p currentData property, a \c setCurrentData() slot, and a \c currentDataChanged() signal.
	The default data role is \c Qt::UserRole but can be set with \c setDefaultRole(). The default data match flags for
	\c setCurrentData() (and the underlying \c QComboBox::findData() ) can be changed with \c setDefaultMatchFlags().

	Note that the \p currentData property is set as the \c USER (default) property for this class. Some Qt parts like item view
	delegates may use the \c USER property.
*/
class DataComboBox: public QComboBox
{
		Q_OBJECT
		//! Property for \c QComboBox::currentData() of the selected combo box item. \sa setCurrentData(), currentDataChanged()
		Q_PROPERTY(QVariant currentData READ currentData WRITE setCurrentData NOTIFY currentDataChanged USER true)
		//! The Role id to use for data operations unless otherwise specified. Default is \c Qt::UserRole. \sa setDefaultRole()
		Q_PROPERTY(int defaultRole READ defaultRole WRITE setDefaultRole)
		//! The default MatchFlags to use for \c setCurrentData() unless otherwise specified. \sa setDefaultMatchFlags()
		Q_PROPERTY(Qt::MatchFlags defaultMatchFlags READ defaultMatchFlags WRITE setDefaultMatchFlags)

	public:
		explicit DataComboBox(QWidget *parent = nullptr):
		  QComboBox(parent)
		{
			connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(onCurrentIndexChanged(int)));
		}

		//! The Role id to use for data operations unless otherwise specified. Default is \c Qt::UserRole. \sa setDefaultRole()
		inline int defaultRole() const { return m_role; }
		//! The default MatchFlags to use for \c setCurrentData() unless otherwise specified. \sa setDefaultMatchFlags()
		inline Qt::MatchFlags defaultMatchFlags() const { return m_matchFlags; }

		//! Same as the \c QComboBox version but if \a role is \c -1 (default) then the \p defaultRole is used.
		inline QVariant currentData(int role = -1) const
		{
			if (role < 0)
				role = m_role;
			return QComboBox::currentData(role);
		}

		//! Add items with names from \a texts, with corresponding data from \a datas list, using the specified \a role. If \a role is \c -1 (default) then the \p defaultRole is used.
		void addItems(const QStringList &texts, const QVariantList &datas, int role = -1)
		{
			if (role < 0)
				role = m_role;
			const int c = count();
			for (int i=0; i < texts.count(); ++i) {
				addItem(texts.at(i));
				setItemData(i + c, datas.value(i, texts.at(i)), role);
			}
		}

		//! Add items with corresponding data from \a items map, using the specified \a role. If \a role is \c -1 (default) then the \p defaultRole is used.
		void addItems(const QMap<QString, QVariant> &items, int role = -1) {
			addItems(items.keys(), items.values(), role);
		}

		//! Add items with names from \a texts, with corresponding data from \a datas list, using the specified \a role. If \a role is \c -1 (default) then the \p defaultRole is used.
		void addItems(const QStringList &texts, const QStringList &datas, int role = -1)
		{
			if (role < 0)
				role = m_role;
			const int c = count();
			for (int i=0; i < texts.count(); ++i) {
				addItem(texts.at(i));
				setItemData(i + c, datas.value(i, texts.at(i)), role);
			}
		}

		//! Add items with corresponding data from \a items map, using the specified \a role. If \a role is \c -1 (default) then the \p defaultRole is used.
		void addItems(const QMap<QString, QString> &items, int role = -1) {
			addItems(items.keys(), items.values(), role);
		}

		using QComboBox::addItems;  // bring back the superclass version

	public slots:
		//! Convenience slot for \c QComboBox::setCurrentIndex(findData(value, role, matchFlags))
		//! If \a role is \c -1 (default) then the \p defaultRole is used. If \a matchFlags is \c -1 (default) then the \p defaultMatchFlags are used.
		inline void setCurrentData(const QVariant &value, int role = -1, int matchFlags = -1)
		{
			if (role < 0)
				role = m_role;
			if (matchFlags < 0)
				matchFlags = m_matchFlags;
			setCurrentIndex(findData(value, role, Qt::MatchFlags(matchFlags)));
		}

		//! Set the Role id to use for data operations unless otherwise specified. Default is \c Qt::UserRole. \sa defaultRole()
		inline void setDefaultRole(int role) { m_role = role; }
		//! Set the default \c Qt::MatchFlags to use for \c setCurrentData() to \a flags. Default is \c {Qt::MatchExactly | Qt::MatchCaseSensitive} (same as \c QComboBox::findData() ). \sa defaultMatchFlags()
		inline void setDefaultMatchFlags(Qt::MatchFlags flags) { m_matchFlags = flags; }

	signals:
		//! Emitted whenever the \p currentIndex changes. The \a value is in the current \p defaultRole (\c Qt::UserRole unless set otherwise with \c setDefaultRole() )
		void currentDataChanged(const QVariant &value);

	protected slots:
		void onCurrentIndexChanged(int)
		{
			emit currentDataChanged(currentData(m_role));
		}

	protected:
		int m_role = Qt::UserRole;
		Qt::MatchFlags m_matchFlags = Qt::MatchExactly | Qt::MatchCaseSensitive;
};

#endif // _DATACOMBOBOX_H_
