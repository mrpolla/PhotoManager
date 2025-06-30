#include "zoomableimagelabel.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <qmath.h>

// === Constants ===
namespace {
constexpr int MIN_WIDGET_SIZE = 300;
const QString DEFAULT_TEXT = "Select an image";
const QString BACKGROUND_STYLE = "background-color: white;";
}

// === Constructor ===

ZoomableImageLabel::ZoomableImageLabel(QWidget *parent)
    : QLabel(parent)
    , m_scaleFactor(1.0)
    , m_dragging(false)
    , m_scrollArea(nullptr)
{
    setupWidget();
    findScrollArea();
}

// === Public Methods ===

void ZoomableImageLabel::setImagePixmap(const QPixmap &pixmap)
{
    m_originalPixmap = pixmap;
    m_scaleFactor = 1.0;

    if (hasImage()) {
        fitToWindow();
    } else {
        clearDisplay();
    }
}

void ZoomableImageLabel::resetZoom()
{
    if (!hasImage()) {
        return;
    }

    m_scaleFactor = 1.0;
    updateDisplayedImage();
    emit zoomChanged(m_scaleFactor);
}

void ZoomableImageLabel::fitToWindow()
{
    if (!hasImage()) {
        return;
    }

    findScrollArea();
    if (!m_scrollArea) {
        return;
    }

    const QSize availableSize = m_scrollArea->viewport()->size();
    const QSize imageSize = m_originalPixmap.size();

    // Calculate scale factor to fit image within available space
    const double scaleX = static_cast<double>(availableSize.width()) / imageSize.width();
    const double scaleY = static_cast<double>(availableSize.height()) / imageSize.height();

    // Use the smaller scale to ensure image fits completely
    m_scaleFactor = qMin(scaleX, scaleY);

    // Don't scale small images beyond 100%
    m_scaleFactor = qMin(m_scaleFactor, 1.0);

    updateDisplayedImage();
    emit zoomChanged(m_scaleFactor);
}

// === Event Handlers ===

void ZoomableImageLabel::wheelEvent(QWheelEvent *event)
{
    if (!hasImage()) {
        QLabel::wheelEvent(event);
        return;
    }

    // Determine zoom direction and factor
    const double scaleFactor = (event->angleDelta().y() > 0)
                                   ? ZOOM_FACTOR
                                   : (1.0 / ZOOM_FACTOR);

    scaleImage(scaleFactor);
    event->accept();
}

void ZoomableImageLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && hasImage()) {
        startPanning(event->pos());
        event->accept();
    } else {
        QLabel::mousePressEvent(event);
    }
}

void ZoomableImageLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && m_scrollArea) {
        performPanning(event->pos());
        event->accept();
    } else {
        QLabel::mouseMoveEvent(event);
    }
}

void ZoomableImageLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        stopPanning();
        event->accept();
    } else {
        QLabel::mouseReleaseEvent(event);
    }
}

// === Private Methods ===

void ZoomableImageLabel::setupWidget()
{
    setAlignment(Qt::AlignCenter);
    setMinimumSize(MIN_WIDGET_SIZE, MIN_WIDGET_SIZE);
    setStyleSheet(BACKGROUND_STYLE);
    setText(DEFAULT_TEXT);
}

void ZoomableImageLabel::findScrollArea()
{
    if (m_scrollArea) {
        return; // Already found
    }

    QWidget *widget = parentWidget();
    while (widget && !m_scrollArea) {
        m_scrollArea = qobject_cast<QScrollArea*>(widget);
        widget = widget->parentWidget();
    }
}

void ZoomableImageLabel::scaleImage(double factor)
{
    const double newScaleFactor = qBound(
        MIN_SCALE_FACTOR,
        m_scaleFactor * factor,
        MAX_SCALE_FACTOR
        );

    if (!qFuzzyCompare(newScaleFactor, m_scaleFactor)) {
        m_scaleFactor = newScaleFactor;
        updateDisplayedImage();
        emit zoomChanged(m_scaleFactor);
    }
}

void ZoomableImageLabel::updateDisplayedImage()
{
    if (!hasImage()) {
        return;
    }

    if (qFuzzyCompare(m_scaleFactor, 1.0)) {
        // Show original image at 100% scale
        setPixmap(m_originalPixmap);
        resize(m_originalPixmap.size());
    } else {
        // Scale the image
        const QSize newSize = m_originalPixmap.size() * m_scaleFactor;
        const QPixmap scaledPixmap = m_originalPixmap.scaled(
            newSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            );

        setPixmap(scaledPixmap);
        resize(newSize);
    }
}

void ZoomableImageLabel::clearDisplay()
{
    setPixmap(QPixmap());
    setText(DEFAULT_TEXT);
    resize(minimumSize());
}

void ZoomableImageLabel::startPanning(const QPoint &startPoint)
{
    m_dragging = true;
    m_lastPanPoint = startPoint;
    setCursor(Qt::ClosedHandCursor);
}

void ZoomableImageLabel::performPanning(const QPoint &currentPoint)
{
    const QPoint delta = currentPoint - m_lastPanPoint;
    m_lastPanPoint = currentPoint;

    // Update scroll bar positions
    QScrollBar *hScrollBar = m_scrollArea->horizontalScrollBar();
    QScrollBar *vScrollBar = m_scrollArea->verticalScrollBar();

    hScrollBar->setValue(hScrollBar->value() - delta.x());
    vScrollBar->setValue(vScrollBar->value() - delta.y());
}

void ZoomableImageLabel::stopPanning()
{
    m_dragging = false;
    setCursor(Qt::ArrowCursor);
}
