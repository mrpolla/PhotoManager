#include "zoomableimagelabel.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <qmath.h>

ZoomableImageLabel::ZoomableImageLabel(QWidget *parent)
    : QLabel(parent)
    , m_scaleFactor(1.0)
    , m_dragging(false)
    , m_scrollArea(nullptr)
{
    setAlignment(Qt::AlignCenter);
    setMinimumSize(300, 300);
    setStyleSheet("background-color: white;");

    // Find the scroll area parent (will be set when added to scroll area)
    QWidget *p = parent;
    while (p && !m_scrollArea) {
        m_scrollArea = qobject_cast<QScrollArea*>(p);
        p = p->parentWidget();
    }
}

void ZoomableImageLabel::setImagePixmap(const QPixmap &pixmap)
{
    m_originalPixmap = pixmap;
    m_scaleFactor = 1.0;

    if (!pixmap.isNull()) {
        fitToWindow();
    } else {
        setPixmap(QPixmap());
        setText("Select an image");
    }
}

void ZoomableImageLabel::resetZoom()
{
    m_scaleFactor = 1.0;
    updateDisplayedImage();
    emit zoomChanged(m_scaleFactor);
}

void ZoomableImageLabel::fitToWindow()
{
    if (m_originalPixmap.isNull()) return;

    // Find scroll area if not found yet
    if (!m_scrollArea) {
        QWidget *p = parentWidget();
        while (p && !m_scrollArea) {
            m_scrollArea = qobject_cast<QScrollArea*>(p);
            p = p->parentWidget();
        }
    }

    if (!m_scrollArea) return;

    QSize availableSize = m_scrollArea->viewport()->size();
    QSize imageSize = m_originalPixmap.size();

    // Calculate scale factor to fit image in available space
    double scaleX = static_cast<double>(availableSize.width()) / imageSize.width();
    double scaleY = static_cast<double>(availableSize.height()) / imageSize.height();
    m_scaleFactor = qMin(scaleX, scaleY);

    // Don't scale up small images beyond 100%
    m_scaleFactor = qMin(m_scaleFactor, 1.0);

    updateDisplayedImage();
    emit zoomChanged(m_scaleFactor);
}

void ZoomableImageLabel::wheelEvent(QWheelEvent *event)
{
    if (m_originalPixmap.isNull()) {
        QLabel::wheelEvent(event);
        return;
    }

    // Zoom in/out with mouse wheel
    const double zoomInFactor = 1.15;
    const double zoomOutFactor = 1.0 / zoomInFactor;

    double scaleFactor;
    if (event->angleDelta().y() > 0) {
        // Zoom in
        scaleFactor = zoomInFactor;
    } else {
        // Zoom out
        scaleFactor = zoomOutFactor;
    }

    scaleImage(scaleFactor);
    event->accept();
}

void ZoomableImageLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_originalPixmap.isNull()) {
        m_dragging = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    } else {
        QLabel::mousePressEvent(event);
    }
}

void ZoomableImageLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && m_scrollArea) {
        QPoint delta = event->pos() - m_lastPanPoint;
        m_lastPanPoint = event->pos();

        // Pan the image by adjusting scroll bar values
        QScrollBar *hScrollBar = m_scrollArea->horizontalScrollBar();
        QScrollBar *vScrollBar = m_scrollArea->verticalScrollBar();

        hScrollBar->setValue(hScrollBar->value() - delta.x());
        vScrollBar->setValue(vScrollBar->value() - delta.y());

        event->accept();
    } else {
        QLabel::mouseMoveEvent(event);
    }
}

void ZoomableImageLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
    } else {
        QLabel::mouseReleaseEvent(event);
    }
}

void ZoomableImageLabel::scaleImage(double factor)
{
    double newScaleFactor = m_scaleFactor * factor;

    // Clamp to min/max zoom levels
    newScaleFactor = qMax(MIN_SCALE_FACTOR, qMin(MAX_SCALE_FACTOR, newScaleFactor));

    if (newScaleFactor != m_scaleFactor) {
        m_scaleFactor = newScaleFactor;
        updateDisplayedImage();
        emit zoomChanged(m_scaleFactor);
    }
}

void ZoomableImageLabel::updateDisplayedImage()
{
    if (m_originalPixmap.isNull()) return;

    QSize newSize = m_originalPixmap.size() * m_scaleFactor;

    if (m_scaleFactor == 1.0) {
        // Show original image
        setPixmap(m_originalPixmap);
    } else {
        // Scale the image
        QPixmap scaledPixmap = m_originalPixmap.scaled(newSize,
                                                       Qt::KeepAspectRatio,
                                                       Qt::SmoothTransformation);
        setPixmap(scaledPixmap);
    }

    resize(newSize);
}
