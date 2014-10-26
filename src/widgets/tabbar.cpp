/*
 * tabbar.cpp
 * Copyright (C) 2013-2014  Ivan Romanov <drizt@land.ru>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "tabbar.h"

#include <QAbstractButton>
#include <QPixmap>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QMouseEvent>

class CloseButton : public QAbstractButton
{
	Q_OBJECT

public:
	CloseButton(QWidget *parent = 0);

	QSize sizeHint() const;
	inline QSize minimumSizeHint() const
		{ return sizeHint(); }
	void enterEvent(QEvent *event);
	void leaveEvent(QEvent *event);
	void paintEvent(QPaintEvent *event);
};

class TabBar::Private
{
public:
	TabBar *q;
	Private(TabBar *base)
	{
		q = base;
		tabsClosable = false;
		multiRow = false;
		balanseCloseButtons();
	}

	void layoutTabs();
	QSize tabSizeHint(QStyleOptionTabV3 tab) const;
	void balanseCloseButtons();

	QList<QStyleOptionTabV3> hackedTabs;
	QList<CloseButton*> closeButtons;
	bool tabsClosable;
	bool multiRow;
};

void TabBar::Private::layoutTabs()
{
	hackedTabs.clear();

	QTabBar::ButtonPosition closeSide = (QTabBar::ButtonPosition)q->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, q);
	int barWidth = q->width();
	for (int i = 0; i < q->count(); i++) {
		QStyleOptionTabV3 tab;
		q->initStyleOption(&tab, i);
		if (i == 0) {
			tab.rect.setLeft(0);
		}

		tab.position = QStyleOptionTab::Beginning;

		if (tabsClosable) {
			tab.rect.setWidth(tab.rect.width() + closeButtons.at(i)->size().width());
			if (closeSide == QTabBar::LeftSide) {
				tab.leftButtonSize = closeButtons.at(i)->size();
			}
			else {
				tab.rightButtonSize = closeButtons.at(i)->size();
			}
		}

		tab.rect.setSize(tabSizeHint(tab));

		QStyleOptionTabV3 prevTab;
		if (i > 0) {
			prevTab = hackedTabs.last();
		}

		if (i > 0 && prevTab.rect.right() + tab.rect.width() >= barWidth) {
			tab.rect.moveTo(0, prevTab.rect.bottom() - 1);
			if (prevTab.position == QStyleOptionTab::Middle) {
				prevTab.position = QStyleOptionTab::End;
			}
			else {
				prevTab.position = QStyleOptionTab::OnlyOneTab;
			}

			if (i == q->count() - 1) {
				tab.position = QStyleOptionTab::OnlyOneTab;
			}

			hackedTabs[i - 1] = prevTab;
		}
		else {
			if (q->count() == 1) {
				tab.position = QStyleOptionTab::OnlyOneTab;
			}
			else if (i == q->count() - 1) {
				tab.position = QStyleOptionTab::End;
				tab.rect.moveTo(prevTab.rect.right() + 1, prevTab.rect.top());
			}
			else if (i > 0) {
				tab.position = QStyleOptionTab::Middle;
				tab.rect.moveTo(prevTab.rect.right() + 1, prevTab.rect.top());
			}
		}

		hackedTabs << tab;
	}

	int rowStart = 0;

	// stretch filled or last row
	for (int i = 0; i < hackedTabs.size(); i++) {
		QStyleOptionTab::TabPosition position = hackedTabs.at(i).position;
		if (position != QStyleOptionTab::End && position != QStyleOptionTab::OnlyOneTab) {
			continue;
		}

		// stretch factor
		double sf = static_cast<float>(barWidth) / hackedTabs.at(i).rect.right();

		// don't strech the last row
		bool lastRow = i == hackedTabs.size() - 1;
		if (lastRow)
			sf = qMin(1.5, sf);

		for (int j = rowStart; j <= i; j++) {
			QStyleOptionTabV3 tab = hackedTabs.at(j);
			if (j > rowStart) {
				tab.rect.moveLeft(hackedTabs.at(j - 1).rect.right() + 1);
			}
			if (j < i) {
				tab.rect.setWidth(tab.rect.width() * sf);
			}
			else {
				if (!lastRow) {
					tab.rect.setRight(barWidth - 1);
				}
				else {
					int width = qMin(static_cast<int>(tab.rect.width() * sf), barWidth - 1);
					tab.rect.setWidth(width);
				}
			}
			hackedTabs[j] = tab;
		}

		rowStart = i + 1;
	}

	q->setMinimumSize(0, q->sizeHint().height());
	q->resize(q->sizeHint());
}

inline static bool verticalTabs(QTabBar::Shape shape)
{
	return shape == QTabBar::RoundedWest
		   || shape == QTabBar::RoundedEast
		   || shape == QTabBar::TriangularWest
		   || shape == QTabBar::TriangularEast;
}

QSize TabBar::Private::tabSizeHint(QStyleOptionTabV3 opt) const
{
	QSize iconSize = opt.iconSize;
	int hframe = q->style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, q);
	int vframe = q->style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, q);
	const QFontMetrics fm = q->fontMetrics();

	int maxWidgetHeight = qMax(opt.leftButtonSize.height(), opt.rightButtonSize.height());
	int maxWidgetWidth = qMax(opt.leftButtonSize.width(), opt.rightButtonSize.width());

	int widgetWidth = 0;
	int widgetHeight = 0;
	int padding = 0;
	if (!opt.leftButtonSize.isEmpty()) {
		padding += 4;
		widgetWidth += opt.leftButtonSize.width();
		widgetHeight += opt.leftButtonSize.height();
	}
	if (!opt.rightButtonSize.isEmpty()) {
		padding += 4;
		widgetWidth += opt.rightButtonSize.width();
		widgetHeight += opt.rightButtonSize.height();
	}
	if (!opt.icon.isNull())
		padding += 4;

	QSize csz;
	if (verticalTabs(q->shape())) {
		csz = QSize( qMax(maxWidgetWidth, qMax(fm.height(), iconSize.height())) + vframe,
					 fm.size(Qt::TextShowMnemonic, opt.text).width() + iconSize.width() + hframe + widgetHeight + padding);
	} else {
		csz = QSize(fm.size(Qt::TextShowMnemonic, opt.text).width() + iconSize.width() + hframe
					+ widgetWidth + padding,
					qMax(maxWidgetHeight, qMax(fm.height(), iconSize.height())) + vframe);
	}

	QSize retSize = q->style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, csz, q);
	return retSize;

}

void TabBar::Private::balanseCloseButtons()
{
	if (tabsClosable && multiRow) {
		while (closeButtons.isEmpty() || closeButtons.size() < q->count()) {
			CloseButton *cb = new CloseButton(q);
			closeButtons << cb;
			cb->show();
			connect(cb, SIGNAL(clicked()), q, SLOT(closeTab()));
		}

		while (closeButtons.size() > q->count()) {
			closeButtons.takeLast()->deleteLater();
		}
	}
	else {
		qDeleteAll(closeButtons);
		closeButtons.clear();
	}
}

TabBar::TabBar(QWidget *parent)
	: QTabBar(parent)
{
	d = new Private(this);
}

void TabBar::layoutTabs()
{
	if (d->multiRow) {
		d->layoutTabs();
	}
	update();
}

void TabBar::setMultiRow(bool b)
{
	if (b == d->multiRow) {
		return;
	}

	d->multiRow = b;

	if (b) {
		d->tabsClosable = QTabBar::tabsClosable();
		setElideMode(Qt::ElideNone);
		QTabBar::setTabsClosable(false);
	}
	else {
		QTabBar::setTabsClosable(d->tabsClosable);
		d->tabsClosable = false;
	}

	d->balanseCloseButtons();
	if (b) {
		// setUsesScrollButtons(false);
		layoutTabs();
	}
	else {
		d->hackedTabs.clear();
		update();
	}
}

void TabBar::setCurrentIndex(int index)
{
	if (!d->hackedTabs.isEmpty()) {
		int prev = currentIndex();
		if (prev > -1) {
			d->hackedTabs[prev].state = QStyle::State_Enabled;

			if (prev > 0)
				d->hackedTabs[prev - 1].selectedPosition = QStyleOptionTab::NotAdjacent;
			if (prev + 1 < d->hackedTabs.size())
				d->hackedTabs[prev + 1].selectedPosition = QStyleOptionTab::NotAdjacent;
		}
		d->hackedTabs[index].state = QStyle::State_Enabled | QStyle::State_Selected;

		if (index > 0)
			d->hackedTabs[index - 1].selectedPosition = QStyleOptionTab::NextIsSelected;
		if (index + 1 < d->hackedTabs.size())
			d->hackedTabs[index + 1].selectedPosition = QStyleOptionTab::PreviousIsSelected;

	}
	QTabBar::setCurrentIndex(index);
}

void TabBar::setTabText(int index, const QString & text)
{
	QTabBar::setTabText(index, text);
	layoutTabs();
}

void TabBar::setTabTextColor(int index, const QColor & color)
{
	QTabBar::setTabTextColor(index, color);
	layoutTabs();
}

void TabBar::setTabIcon(int index, const QIcon &icon)
{
	QTabBar::setTabIcon(index, icon);
	layoutTabs();
}

QRect TabBar::tabRect(int index) const
{
	if (d->multiRow) {
		if (index < d->hackedTabs.size() && index >= 0)
			return d->hackedTabs[index].rect;
		else
			return QRect();
	}
	else {
		return QTabBar::tabRect(index);
	}
}

QWidget *TabBar::tabButton(int index, ButtonPosition position) const
{
	ButtonPosition closeButtonPos = static_cast<ButtonPosition>(style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, this));
	if (!d->multiRow || position != closeButtonPos)
		return QTabBar::tabButton(index, position);

	Q_ASSERT(index < d->closeButtons.size());
	return index < d->closeButtons.size() ? d->closeButtons[index] : 0;
}

bool TabBar::multiRow() const
{
	return d->multiRow;
}

QSize TabBar::minimumSizeHint() const
{
	return QSize(0, sizeHint().height());
}

QSize TabBar::sizeHint() const
{
	// use own sizeHint only for single row mode
	if (!d->multiRow) {
		return QTabBar::sizeHint();
	}

	QList<QStyleOptionTabV3> tabs = d->hackedTabs;

	QRect rect;
	for (int i=0; i < tabs.size(); i++) {
		rect = rect.united(tabs.at(i).rect);
	}

	QSize size = rect.size();
	size.setWidth(width());
	return size;
}

QSize TabBar::tabSizeHint(int index) const
{
	// use own sizeHint only for single row mode
	if (!d->multiRow) {
		return QTabBar::tabSizeHint(index);
	}

	if (index < 0 || d->hackedTabs.size() <= index) {
		return QSize();
	}

	QStyleOptionTabV3 opt = d->hackedTabs.at(index);
	return d->tabSizeHint(opt);
}

void TabBar::setTabsClosable(bool b)
{
	if (!d->multiRow) {
		QTabBar::setTabsClosable(b);
		return;
	}

	if (d->tabsClosable == b) {
		return;
	}

	d->tabsClosable = b;
	d->balanseCloseButtons();
	d->layoutTabs();
}

bool TabBar::tabsClosable() const
{
	return d->tabsClosable;
}

void TabBar::paintEvent(QPaintEvent *event)
{
	// use own painting only for multi row mode
	if (!d->multiRow) {
		QTabBar::paintEvent(event);
		return;
	}

	QStylePainter p(this);
	QList<QStyleOptionTabV3> tabs = d->hackedTabs;
	int selected = currentIndex();
	int rowHeight = tabs[selected].rect.height();

	// There is some problems when tabs are painted not in first row.
	// Draw on a pixmap like a painting in the first row. Then move image
	// to real TabBar widget.
	QPixmap pixmap(width(), rowHeight);
	pixmap.fill(Qt::transparent);
	QStylePainter pp(&pixmap, this);
	bool drawSelected = false;

	if (shape() == QTabBar::RoundedNorth) {
		for (int i = 0; i < tabs.size(); ++i) {
			QStyleOptionTabV3 tab = tabs[i];
			if (i != selected) {
				tab.rect.moveBottom(rowHeight - 1);
				pp.drawControl(QStyle::CE_TabBarTab, tab);
			}
			else {
				drawSelected = true;
			}

			if (tab.position == QStyleOptionTab::End || tab.position == QStyleOptionTab::OnlyOneTab) {
				if (drawSelected) {
					// Draw current tab in the last order
					tab = tabs[selected];
					tab.rect.moveBottom(rowHeight - 1);
					pp.drawControl(QStyle::CE_TabBarTab, tab);
					drawSelected = false;
				}

				QRect rect(0, tabs[i].rect.top(), width(), rowHeight);
				p.drawItemPixmap(rect, Qt::AlignCenter, pixmap);
				pixmap.fill(Qt::transparent);
			}
		}
	}
	else {
		for (int i = tabs.size() - 1; i >= 0; --i) {
			QStyleOptionTabV3 tab = tabs[i];
			if (i != selected) {
				tab.rect.moveTop(0);
				pp.drawControl(QStyle::CE_TabBarTab, tab);
			}
			else {
				drawSelected = true;
			}

			if (tab.position == QStyleOptionTab::Beginning || tab.position == QStyleOptionTab::OnlyOneTab) {
				if (drawSelected) {
					// Draw current tab in the last order
					tab = tabs[selected];
					tab.rect.moveTop(0);
					pp.drawControl(QStyle::CE_TabBarTab, tab);
					drawSelected = false;
				}

				QRect rect(0, tabs[i].rect.top(), width(), rowHeight);
				p.drawItemPixmap(rect, Qt::AlignCenter, pixmap);
				pixmap.fill(Qt::transparent);
			}
		}
	}

	ButtonPosition closeSide = (QTabBar::ButtonPosition)style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, this);
	QStyle::SubElement se = (closeSide == LeftSide ? QStyle::SE_TabBarTabLeftButton : QStyle::SE_TabBarTabRightButton);
	if (d->tabsClosable) {
		for (int i = 0; i < tabs.size(); i++) {
			QStyleOptionTabV3 opt;
			opt = tabs.at(i);

			QRect rect = style()->subElementRect(se, &opt, this);
			rect.setTop(rect.top() + opt.rect.top());
			QPoint p = rect.topLeft();
			d->closeButtons.at(i)->move(p);
		}
	}
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
	if (!d->multiRow) {
		QTabBar::mousePressEvent(event);
		return;
	}

	if (event->button() != Qt::LeftButton) {
		event->ignore();
		return;
	}

	for (int i = 0; i < count(); i++) {
		if (d->hackedTabs.at(i).rect.contains(event->pos())) {
			setCurrentIndex(i);
			d->layoutTabs();

			// QStyleOptionTabV3 tab = d->hackedTabs.at(currentIndex());
			// tab.state = QStyle::State_Enabled;
			// d->hackedTabs[currentIndex()] = tab;
			// tab = d->hackedTabs.at(i);
			// tab.state = QStyle::State_Selected | QStyle::State_Enabled;
			// d->hackedTabs[i] = tab;
			// update();
		}
	}
}

void TabBar::tabInserted(int index)
{
	QTabBar::tabInserted(index);

	if (!d->multiRow) {
		return;
	}

	d->balanseCloseButtons();
	d->layoutTabs();
}

void TabBar::tabRemoved(int index)
{
	QTabBar::tabRemoved(index);

	if (!d->multiRow) {
		return;
	}

	d->balanseCloseButtons();
	d->layoutTabs();
}

void TabBar::closeTab()
{
	CloseButton *cb = qobject_cast<CloseButton*>(sender());
	int index = d->closeButtons.indexOf(cb);
	emit tabCloseRequested(index);
}

CloseButton::CloseButton(QWidget *parent)
	: QAbstractButton(parent)
{
	setFocusPolicy(Qt::NoFocus);
	setCursor(Qt::ArrowCursor);
	setToolTip(tr("Close Tab"));
	resize(sizeHint());
}

QSize CloseButton::sizeHint() const
{
	ensurePolished();
	int width = style()->pixelMetric(QStyle::PM_TabCloseIndicatorWidth, 0, this);
	int height = style()->pixelMetric(QStyle::PM_TabCloseIndicatorHeight, 0, this);
	return QSize(width, height);
}

void CloseButton::enterEvent(QEvent *event)
{
	if (isEnabled())
		update();
	QAbstractButton::enterEvent(event);
}

void CloseButton::leaveEvent(QEvent *event)
{
	if (isEnabled())
		update();
	QAbstractButton::leaveEvent(event);
}

void CloseButton::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	QStyleOption opt;
	opt.init(this);
	opt.state |= QStyle::State_AutoRaise;
	if (isEnabled() && underMouse() && !isChecked() && !isDown())
		opt.state |= QStyle::State_Raised;
	if (isChecked())
		opt.state |= QStyle::State_On;
	if (isDown())
		opt.state |= QStyle::State_Sunken;

	if (const TabBar *tb = qobject_cast<const TabBar*>(parent())) {
		int index = tb->currentIndex();
		QTabBar::ButtonPosition position = static_cast<QTabBar::ButtonPosition>(style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, tb));
		if (tb->tabButton(index, position) == this)
			opt.state |= QStyle::State_Selected;
	}

	style()->drawPrimitive(QStyle::PE_IndicatorTabClose, &opt, &p, this);
}

#include "tabbar.moc"
