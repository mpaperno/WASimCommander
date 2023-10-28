// Originally from https://github.com/dimkanovikov/MultisortTableView  Licensed under  GPL v3

#ifndef ALPHANUMSORTPROXYMODEL_H
#define ALPHANUMSORTPROXYMODEL_H

#include <QSortFilterProxyModel>
#include "AlphanumComparer.h"
#include "ColumnsSorter.h"


class AlphanumSortProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit AlphanumSortProxyModel( QObject *parent = 0 ) :
        QSortFilterProxyModel( parent ) { }

    // Reimplemented to show order of column in common sorting if needed
    // and sorting icon of column
    virtual QVariant headerData ( int section,
                                  Qt::Orientation orientation,
                                  int role = Qt::DisplayRole ) const
    {
        if( orientation == Qt::Horizontal ) {
            switch ( role )
            {
                //case  Qt::DisplayRole: {
                //    QString header = sourceModel()->headerData( section, orientation ).toString();
                //    if ( m_columnSorter.columnsCount() > 1 &&
                //         m_columnSorter.columnOrder( section ) >= 0 ) {
                //        header.insert(0, QString::number( m_columnSorter.columnOrder( section ) + 1 ) + ": " );
                //    }
                //    return header;
                //}
                case Qt::DecorationRole:
                    return m_columnSorter.columnIcon( section );
                default:
                    return QSortFilterProxyModel::headerData(section, orientation, role);
            }
        }
        // Row number
        return section + 1;
    }

    // Sort column
    void sortColumn ( int column, bool isModifierPressed = false )
    {
        m_columnSorter.sortColumn( column, isModifierPressed );

        // "count-1" becouse indexes are started from zero value
        for( int i = m_columnSorter.columnsCount()-1; i >= 0; --i) {
            int col = m_columnSorter.columnIndex( i );
            sort( col, m_columnSorter.columnSortOrder( col ) );
        }
    }

    // Set icons to decorate sorted table headers
    void setSortIcons( QIcon ascIcon, QIcon descIcon )
    {
        m_columnSorter.setIcons( ascIcon, descIcon );
    }


protected:
    // Reimplemented to use alphanum sorting
    virtual bool lessThan ( const QModelIndex & left,
                            const QModelIndex & right ) const
    {
        QVariant leftData  = sourceModel()->data(left),
                 rightData = sourceModel()->data(right);

        return AlphanumComparer::lessThan( leftData.toString(),
                                           rightData.toString() );
    }

private:
    ColumnsSorter m_columnSorter;

};

#endif // ALPHANUMSORTPROXYMODEL_H
