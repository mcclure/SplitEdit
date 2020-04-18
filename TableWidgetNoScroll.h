#ifndef TABLEWIDGETNOSCROLL_H
#define TABLEWIDGETNOSCROLL_H

#include <QTableWidget>

class TableWidgetNoScroll : public QTableWidget {
	Q_OBJECT
public:
	using QTableWidget::QTableWidget;
	void wheelEvent(QWheelEvent* evt) {
		// If a QTableWidget is inside a larger scroll view, it swallows wheel events
		// To avoid this we pass through non-viable scrolls to superview
		// Currently this is determined only by NeverScroll status
		if (this->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff) {
			QWidget::wheelEvent(evt);
		} else {
			QTableWidget::wheelEvent(evt);
		}
	}
};

#endif
