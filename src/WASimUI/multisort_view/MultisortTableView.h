// Originally from https://github.com/dimkanovikov/MultisortTableView  Licensed under  GPL v3

#ifndef MULTISORTTABLEVIEW_H
#define MULTISORTTABLEVIEW_H

#include <QTableView>

class AlphanumSortProxyModel;
class QIcon;
class HeaderProxyStyle;

class MultisortTableView : public QTableView
{
    Q_OBJECT

public:
    explicit MultisortTableView ( QWidget *parent = 0 );

    // Set icons to decorate sorted table headers
    void setSortIcons ( QIcon ascIcon, QIcon descIcon );
    // Set key modifier to handle multicolumn sorting
    void setModifier ( Qt::KeyboardModifier modifier );

    virtual void setSortingEnabled ( bool enable );
    virtual void setModel ( QAbstractItemModel *model );

private:
    // Sorting enable state
    bool m_isSortingEnabled;
    // ProxyModel to sorting columns
    AlphanumSortProxyModel *m_proxyModel;
    // Modifier to handle multicolumn sorting
    Qt::KeyboardModifier m_modifier;

private slots:
    void headerClicked ( int column );
};

#endif // MULTISORTTABLEVIEW_H
