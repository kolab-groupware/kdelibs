/*
 *   Copyright 2009 Marco Martin <notmart@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "focusindicator_p.h"

#include <QGraphicsSceneResizeEvent>

#include <plasma/theme.h>
#include <plasma/framesvg.h>

#include <plasma/animator.h>

namespace Plasma
{

FocusIndicator::FocusIndicator(QGraphicsWidget *parent, QString widget)
    : QGraphicsWidget(parent),
      m_parent(parent),
      m_background(0)
{
    setFlag(QGraphicsItem::ItemStacksBehindParent);
    setAcceptsHoverEvents(true);

    m_background = new Plasma::FrameSvg(this);
    m_background->setImagePath(widget);
    m_background->setElementPrefix("hover");
    m_background->setCacheAllRenderedFrames(true);

    m_fade = Animator::create(Animator::FadeAnimation, this);
    m_fade->setWidgetToAnimate(this);
    m_fade->setProperty("startOpacity", 0.0);
    m_fade->setProperty("targetOpacity", 1.0);

    parent->installEventFilter(this);
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), SLOT(syncGeometry()));
    syncGeometry();
}

FocusIndicator::~FocusIndicator()
{
    delete m_fade;
}

void FocusIndicator::setCustomGeometry(const QRectF &geometry)
{
    m_customGeometry = geometry;
    syncGeometry();
}

bool FocusIndicator::eventFilter(QObject *watched, QEvent *event)
{
    if (static_cast<QGraphicsWidget *>(watched) != m_parent) {
        return false;
    }

    if (!m_parent->hasFocus() && event->type() == QEvent::GraphicsSceneHoverEnter) {
        m_background->setElementPrefix("hover");
        m_fade->setProperty("startOpacity", 0.0);
        m_fade->setProperty("targetOpacity", 1.0);
        m_fade->start();
    } else if (!m_parent->hasFocus() && event->type() == QEvent::GraphicsSceneHoverLeave) {
        m_fade->setProperty("startOpacity", 1.0);
        m_fade->setProperty("targetOpacity", 0.0);
        m_fade->start();
    } else if (event->type() == QEvent::GraphicsSceneResize) {
        syncGeometry();
    } else if (event->type() == QEvent::FocusIn) {
        m_background->setElementPrefix("focus");
        m_fade->setProperty("startOpacity", 0.0);
        m_fade->setProperty("targetOpacity", 1.0);
        m_fade->start();
    } else if (!m_parent->isUnderMouse() && event->type() == QEvent::FocusOut) {
        m_fade->setProperty("startOpacity", 1.0);
        m_fade->setProperty("targetOpacity", 0.0);
        m_fade->start();
    }

    return false;
}

void FocusIndicator::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    m_background->resizeFrame(event->newSize());
}

void FocusIndicator::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    m_background->paintFrame(painter);
}

void FocusIndicator::syncGeometry()
{
    QRectF geom;
    if (!m_customGeometry.isNull()) {
        geom = m_customGeometry;
    } else {
        geom = m_parent->boundingRect();
    }

    qreal left, top, right, bottom;
    m_background->getMargins(left, top, right, bottom);
    setGeometry(QRectF(geom.topLeft() + QPointF(-left, -top), geom.size() + QSize(left+right, top+bottom)));
}

}

#include <focusindicator_p.moc>
