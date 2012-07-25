/***************************************************************************
 *   Copyright (C) 2009 by Rafael Fernández López <ereslibre@kde.org>      *
 *                                                                         *
 * This library is free software; you can redistribute it and/or           *
 * modify it under the terms of the GNU Library General Public             *
 * License version 2 as published by the Free Software Foundation.         *
 *                                                                         *
 * This library is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       *
 * Library General Public License for more details.                        *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

// this code is taken from SystemSettings/icons/CategoryDrawer.{h,cpp}
// Rafael agreet to relicense it under LGPLv2 or LGPLv3, just as we need it,
// see: http://lists.kde.org/?l=kwrite-devel&m=133061943317199&w=2

#include "ConferenceBox.h"

#include <QPainter>
#include <QApplication>
#include <QStyleOption>
#include <QModelIndex>
#include <QDebug>

ConferenceBox::ConferenceBox()
{
    setLeftMargin ( 7 );
    setRightMargin( 7 );
}

void ConferenceBox::drawCategory(const QModelIndex &index,
                                 int sortRole,
                                 const QStyleOption &option,
                                 QPainter *painter) const
{
    Q_UNUSED( sortRole )
    Q_UNUSED( index    )

    painter->setRenderHint(QPainter::Antialiasing);

    const QRect optRect = option.rect;
    const bool leftToRight = painter->layoutDirection() == Qt::LeftToRight;

    //BEGIN: decoration gradient
    {
        QPainterPath path(optRect.bottomLeft());

        path.lineTo(QPoint(optRect.topLeft().x(), optRect.topLeft().y() - 3));
        const QPointF topLeft(optRect.topLeft());
        QRectF arc(topLeft, QSizeF(4, 4));
        path.arcTo(arc, 180, -90);
        path.lineTo(optRect.topRight());
        path.lineTo(optRect.bottomRight());
        path.lineTo(optRect.bottomLeft());

        QColor window(option.palette.window().color());
        const QColor base(option.palette.base().color());

        window.setAlphaF(option.state & QStyle::State_Selected?0.9:0.7);

        QColor window2(window);
        window2.setAlphaF(option.state & QStyle::State_Selected?0.4:0.2);

        QLinearGradient decoGradient1;
        if (leftToRight) {
            decoGradient1.setStart(optRect.topLeft());
            decoGradient1.setFinalStop(optRect.bottomLeft());
        } else {
            decoGradient1.setStart(optRect.topRight());
            decoGradient1.setFinalStop(optRect.bottomRight());
        }
        decoGradient1.setColorAt(0, window);
        decoGradient1.setColorAt(1, Qt::transparent);

        QLinearGradient decoGradient2;
        if (leftToRight) {
            decoGradient2.setStart(optRect.topLeft());
            decoGradient2.setFinalStop(optRect.topRight());
        } else {
            decoGradient2.setStart(optRect.topRight());
            decoGradient2.setFinalStop(optRect.topLeft());
        }
        decoGradient2.setColorAt(0, window2);
        decoGradient2.setColorAt(1, Qt::transparent);

        painter->fillPath(path, decoGradient1);
        painter->fillRect(optRect, decoGradient2);
    }
    //END: decoration gradient

    {
        QRect newOptRect(optRect);

        if (leftToRight) {
            newOptRect.translate(1, 1);
        } else {
            newOptRect.translate(-1, 1);
        }

        //BEGIN: inner top left corner
        {
            painter->save();
            painter->setPen(option.palette.base().color());
            QRectF arc;
            if (leftToRight) {
                const QPointF topLeft(newOptRect.topLeft());
                arc = QRectF(topLeft, QSizeF(4, 4));
                arc.translate(0.5, 0.5);
                painter->drawArc(arc, 1440, 1440);
            } else {
                QPointF topRight(newOptRect.topRight());
                topRight.rx() -= 4;
                arc = QRectF(topRight, QSizeF(4, 4));
                arc.translate(-0.5, 0.5);
                painter->drawArc(arc, 0, 1440);
            }
            painter->restore();
        }
        //END: inner top left corner

        //BEGIN: inner left vertical line
        {
            QPoint start;
            QPoint verticalGradBottom;
            if (leftToRight) {
                start = newOptRect.topLeft();
                verticalGradBottom = newOptRect.topLeft();
            } else {
                start = newOptRect.topRight();
                verticalGradBottom = newOptRect.topRight();
            }
            start.ry() += 3;
            verticalGradBottom.ry() += newOptRect.height() - 3;
            QLinearGradient gradient(start, verticalGradBottom);
            gradient.setColorAt(0, option.palette.base().color());
            gradient.setColorAt(1, Qt::transparent);
            painter->fillRect(QRect(start, QSize(1, newOptRect.height() - 3)), gradient);
        }
        //END: inner left vertical line

        //BEGIN: top inner horizontal line
        {
            QPoint start;
            QPoint horizontalGradTop;
            if (leftToRight) {
                start = newOptRect.topLeft();
                horizontalGradTop = newOptRect.topLeft();
                start.rx() += 3;
                horizontalGradTop.rx() += newOptRect.width() - 3;
            } else {
                start = newOptRect.topRight();
                horizontalGradTop = newOptRect.topRight();
                start.rx() -= 3;
                horizontalGradTop.rx() -= newOptRect.width() - 3;
            }
            QLinearGradient gradient(start, horizontalGradTop);
            gradient.setColorAt(0, option.palette.base().color());
            gradient.setColorAt(1, Qt::transparent);
            QSize rectSize;
            if (leftToRight) {
                rectSize = QSize(newOptRect.width() - 3, 1);
            } else {
                rectSize = QSize(-newOptRect.width() + 3, 1);
            }
            painter->fillRect(QRect(start, rectSize), gradient);
        }
        //END: top inner horizontal line
    }

    QColor outlineColor = option.palette.text().color();
    outlineColor.setAlphaF(0.35);

    //BEGIN: top left corner
    {
        painter->save();
        painter->setPen(outlineColor);
        QRectF arc;
        if (leftToRight) {
            const QPointF topLeft(optRect.topLeft());
            arc = QRectF(topLeft, QSizeF(4, 4));
            arc.translate(0.5, 0.5);
            painter->drawArc(arc, 1440, 1440);
        } else {
            QPointF topRight(optRect.topRight());
            topRight.rx() -= 4;
            arc = QRectF(topRight, QSizeF(4, 4));
            arc.translate(-0.5, 0.5);
            painter->drawArc(arc, 0, 1440);
        }
        painter->restore();
    }
    //END: top left corner

    //BEGIN: top right corner
    {
        painter->save();
        painter->setPen(outlineColor);
        QRectF arc;
        if (!leftToRight) {
            const QPointF topLeft(optRect.topLeft());
            arc = QRectF(topLeft, QSizeF(4, 4));
            arc.translate(0.5, 0.5);
            painter->drawArc(arc, 1440, 1440);
        } else {
            QPointF topRight(optRect.topRight());
            topRight.rx() -= 4;
            arc = QRectF(topRight, QSizeF(4, 4));
            arc.translate(-0.5, 0.5);
            painter->drawArc(arc, 0, 1440);
        }
        painter->restore();
    }
    //END: top right corner

    //BEGIN: left vertical line
    {
        QPoint start;
        QPoint verticalGradBottom;
        if (leftToRight) {
            start = optRect.topLeft();
            verticalGradBottom = optRect.topLeft();
        } else {
            start = optRect.topRight();
            verticalGradBottom = optRect.topRight();
        }
        start.ry() += 3;
        verticalGradBottom.ry() += optRect.height() - 3 + 200;
        painter->fillRect(QRect(start, QSize(1, optRect.height() - 21)), outlineColor);
    }
    //END: left vertical line

    //BEGIN: right vertical line
    {
        QPoint start;
        QPoint verticalGradBottom;
        if (!leftToRight) {
            start = optRect.topLeft();
            verticalGradBottom = optRect.topLeft();
        } else {
            start = optRect.topRight();
            verticalGradBottom = optRect.topRight();
        }
        start.ry() += 3;
        verticalGradBottom.ry() += optRect.height() - 3 + 200;
        painter->fillRect(QRect(start, QSize(1, optRect.height() - 21)), outlineColor);
    }
    //END: right vertical line

    //BEGIN: horizontal line
    {
        QPoint start;
        QPoint horizontalGradTop;
        if (leftToRight) {
            start = optRect.topLeft();
            horizontalGradTop = optRect.topLeft();
            start.rx() += 3;
            horizontalGradTop.rx() += optRect.width() - 3;
        } else {
            start = optRect.topRight();
            horizontalGradTop = optRect.topRight();
            start.rx() -= 3;
            horizontalGradTop.rx() -= optRect.width() - 3;
        }
        QLinearGradient gradient(start, horizontalGradTop);
        gradient.setColorAt(0, outlineColor);
        gradient.setColorAt(1, outlineColor);
        QSize rectSize;
        if (leftToRight) {
            rectSize = QSize(optRect.width() - 7, 1);
        } else {
            rectSize = QSize(-optRect.width() + 7, 1);
        }
        painter->fillRect(QRect(start, rectSize), gradient);
    }
    //END: horizontal line

    //BEGIN: draw text
//     {
//         const QString category = index.model()->data(index, Qt::DisplayRole).toString();
//         QRect textRect = QRect(option.rect.topLeft(), QSize(option.rect.width() - 2 - 3 - 3, height));
//         textRect.setTop(textRect.top() + 2 + 3 /* corner */);
//         textRect.setLeft(textRect.left() + 2 + 3 /* corner */ + 3 /* a bit of margin */);
//         painter->save();
//         painter->setFont(font);
//         QColor penColor(option.palette.text().color());
//         penColor.setAlphaF(0.6);
//         painter->setPen(penColor);
//         painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop, category);
//         painter->restore();
//     }
    //END: draw text
}


void ConferenceBox::drawBoxBottom(const QModelIndex &index, int sortRole, const QStyleOption &option, QPainter *painter) const {
   Q_UNUSED(index)
   Q_UNUSED(sortRole)
   painter->setRenderHint(QPainter::Antialiasing);
   QColor outlineColor = option.palette.text().color();
   outlineColor.setAlphaF(0.35);
   painter->setPen(outlineColor);

   //BEGIN: bottom horizontal line
   {
   QPoint bl = option.rect.bottomLeft();
   bl.setY(bl.y());
   bl.setX(0);

   painter->fillRect(QRect(bl, QSize(option.rect.width()+4,1)), outlineColor);
   }
   //END: bottom horizontal line

   //BEGIN: bottom right corner
   {
      QRectF arc;
      QPointF br(option.rect.bottomRight());
      br.setY(br.y()-4);
      br.setX(br.x()-12);
      arc = QRectF(br, QSizeF(4, 4));
      arc.translate(0.5, 0.5);
      painter->drawArc(arc, 4320, 1440);
   }
   //END: bottom right corner
}

int ConferenceBox::categoryHeight(const QModelIndex &index, const QStyleOption &option) const
{
   Q_UNUSED( index );
   Q_UNUSED( option );
   QFont font(QApplication::font());
   font.setBold(true);
   const QFontMetrics fontMetrics = QFontMetrics(font);

   return fontMetrics.height() + 2 + 12 /* vertical spacing */;
}

