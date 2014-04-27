/*
 *  HDRMerge - HDR exposure merging software.
 *  Copyright 2012 Javier Celaya
 *  jcelaya@gmail.com
 *
 *  This file is part of HDRMerge.
 *
 *  HDRMerge is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  HDRMerge is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with HDRMerge. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <iostream>
#include "PreviewWidget.hpp"
#include <QImage>
#include <QPainter>
#include <QFuture>
#include <QtConcurrentRun>
#include <QApplication>
#include <QBitmap>
using namespace hdrmerge;


PreviewWidget::PreviewWidget(QWidget * parent) : QWidget(parent),
paintAction(ADD_PIXELS), layer(0), radius(5) {
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}


void PreviewWidget::setImageStack (ImageStack* s) {
    stack.reset(s);
    pixmap.reset();
    QtConcurrent::run(this, &PreviewWidget::render, 0, 0, stack->getWidth(), stack->getHeight());
}


void PreviewWidget::setGamma(float g) {
    g = 1.0f / g;
    for (int i = 0; i < 65536; i++)
        gamma[i] = (int)std::floor(65536.0f * std::pow(i / 65536.0f, g)) >> 8;
}


QSize PreviewWidget::sizeHint() const {
    return pixmap.get() ? pixmap->size() : QSize(0, 0);
}


void PreviewWidget::paintEvent(QPaintEvent * event) {
    if (pixmap.get()) {
        QPainter painter(this);
        painter.drawPixmap(0, 0, *pixmap);
    }
}


QRgb PreviewWidget::rgb(int col, int row) const {
    int v = gamma[(int) stack->value(col, row)];
    int color = stack->getImageAt(col, row);
    QRgb pixel;
    switch (color) {
        case 0:
            pixel = qRgb(v*7/10, v, v*7/10); break;
        case 1:
            pixel = qRgb(v*7/10, v*7/10, v); break;
        case 2:
            pixel = qRgb(v, v*7/10, v*7/10); break;
        default:
            pixel = qRgb(v, v, v); break;
    }
}


void PreviewWidget::render(int minx, int miny, int maxx, int maxy) {
    if (!stack.get()) return;
    QRect area = QRect(0, 0, stack->getWidth(), stack->getHeight()).intersected(QRect(minx, miny, maxx - minx, maxy - miny));
    if (area.isEmpty()) return;
    area.getCoords(&minx, &miny, &maxx, &maxy);
    QImage image(area.width() - 1, area.height() - 1, QImage::Format_RGB32);
    for (int row = miny; row < maxy; row++) {
        QRgb * scanLine = reinterpret_cast<QRgb *>(image.scanLine(row - miny));
        for (int col = minx; col < maxx; col++) {
            *scanLine++ = rgb(col, row);
        }
    }
    QMetaObject::invokeMethod(this, "paintImage", Qt::AutoConnection,
                              Q_ARG(int, minx), Q_ARG(int, miny), Q_ARG(const QImage &, image));
}


void PreviewWidget::paintImage(int x, int y, const QImage & image) {
    if (!pixmap.get()) {
        pixmap.reset(new QPixmap);
        *pixmap = QPixmap::fromImage(image);
        resize(pixmap->size());
    } else {
        QPainter painter(pixmap.get());
        painter.drawImage(x, y, image);
    }
    update(x, y, image.width(), image.height());
}


QPixmap PreviewWidget::createCursor(bool plus) {
    QPixmap cursor(radius*2 + 1, radius*2 + 1);
    cursor.fill(Qt::white);
    {
        QPainter painter(&cursor);
        painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(0, 0, radius*2, radius*2);
        painter.drawLine(radius - 2, radius, radius + 2, radius);
        if (plus)
            painter.drawLine(radius, radius - 2, radius, radius + 2);
    }
    cursor.setMask(cursor.createMaskFromColor(Qt::white));
    return cursor;
}


void PreviewWidget::toggleAddPixelsTool(bool toggled) {
    if (toggled) {
        setCursor(QCursor(createCursor(true), radius, radius));
        paintAction = ADD_PIXELS;
    }
}


void PreviewWidget::toggleRmPixelsTool(bool toggled) {
    if (toggled) {
        setCursor(QCursor(createCursor(false), radius, radius));
        paintAction = REMOVE_PIXELS;
    }
}


void PreviewWidget::paintPixels(int x, int y) {
    if (!stack.get()) return;
    if (paintAction == ADD_PIXELS) {
        stack->addPixels(layer, x, y, radius);
        render(x - radius, y - radius, x + radius + 1, y + radius + 1);
    } else {
        stack->removePixels(layer, x, y, radius);
        render(x - radius, y - radius, x + radius + 1, y + radius + 1);
    }
}
