#ifndef ZOOMABLEIMAGELABEL_H
#define ZOOMABLEIMAGELABEL_H

#include <QLabel>
#include <QPixmap>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollArea>

class ZoomableImageLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ZoomableImageLabel(QWidget *parent = nullptr);

    void setImagePixmap(const QPixmap &pixmap);
    void resetZoom();
    void fitToWindow();

    double getZoomFactor() const { return m_scaleFactor; }

signals:
    void zoomChanged(double zoomFactor);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void scaleImage(double factor);
    void updateDisplayedImage();

    QPixmap m_originalPixmap;
    double m_scaleFactor;
    bool m_dragging;
    QPoint m_lastPanPoint;
    QScrollArea *m_scrollArea;

    static constexpr double MIN_SCALE_FACTOR = 0.1;
    static constexpr double MAX_SCALE_FACTOR = 5.0;
};

#endif // ZOOMABLEIMAGELABEL_H
