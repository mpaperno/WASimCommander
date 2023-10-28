// Originally from https://github.com/dimkanovikov/MultisortTableView  Licensed under  GPL v3

#include <QHeaderView>
#include <QIcon>
#include <QApplication>
#include <QProxyStyle>

#include "MultisortTableView.h"
#include "AlphanumSortProxyModel.h"


class HeaderProxyStyle : public QProxyStyle
{
public:
	using QProxyStyle::QProxyStyle;
	void HeaderProxyStyle::drawControl(ControlElement el, const QStyleOption *opt, QPainter *p, const QWidget *w) const override
	{
		// Header label?
		if (el == CE_HeaderLabel) {
			if (QStyleOptionHeader *header = qstyleoption_cast<QStyleOptionHeader *>(const_cast<QStyleOption*>(opt))) {
				if (!header->icon.isNull())
					header->direction = header->direction == Qt::RightToLeft ? Qt::LeftToRight : Qt::RightToLeft;
			}
		}
		QProxyStyle::drawControl(el, opt, p, w);
	}

	int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const {
		if (metric == PM_SmallIconSize)
			return 14;
		return QProxyStyle::pixelMetric(metric, option, widget);
	}
};


MultisortTableView::MultisortTableView ( QWidget *parent ) :
    QTableView ( parent ),
		m_isSortingEnabled(false),
		m_proxyModel(new AlphanumSortProxyModel(this)),
		m_modifier(Qt::ControlModifier)
{
    // Default icons
		setSortIcons(QIcon(QStringLiteral("scale=1.5/arrow_drop_up.glyph")), QIcon(QStringLiteral("scale=1.5/arrow_drop_down.glyph")));
    horizontalHeader()->setDefaultAlignment( Qt::AlignHCenter | Qt::AlignTop );
		HeaderProxyStyle *proxy = new HeaderProxyStyle(QApplication::style());
		proxy->setParent(horizontalHeader());
		horizontalHeader()->setStyle(proxy);
		horizontalHeader()->setSortIndicatorShown(false);
		QTableView::setSortingEnabled(false);

    // Handler to sorting table
    connect(horizontalHeader(), &QHeaderView::sectionClicked, this, &MultisortTableView::headerClicked);
}

// Set icons to decorate sorted table headers
void MultisortTableView::setSortIcons ( QIcon ascIcon, QIcon descIcon )
{
    m_proxyModel->setSortIcons( ascIcon, descIcon );
}

// Set key modifier to handle multicolumn sorting
void MultisortTableView::setModifier ( Qt::KeyboardModifier modifier )
{
    m_modifier = modifier;
}


// Reimplemented to self handling of sorting enable state
void MultisortTableView::setSortingEnabled( bool enable )
{
    m_isSortingEnabled = enable;
}

// Reimplemented to use AlphanumSortProxyModel
void MultisortTableView::setModel( QAbstractItemModel *model )
{
    if ( model ) {
        m_proxyModel->setSourceModel( model );
        QTableView::setModel( m_proxyModel );
    }
}

// Handler to sort table
void MultisortTableView::headerClicked ( int column )
{
    if ( m_isSortingEnabled ) {
        bool isModifierPressed = QApplication::keyboardModifiers() & m_modifier;
        m_proxyModel->sortColumn( column, isModifierPressed );
    }
}
