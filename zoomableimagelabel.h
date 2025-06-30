#ifndef ZOOMABLEIMAGELABEL_H
#define ZOOMABLEIMAGELABEL_H

#include <QLabel>
#include <QPixmap>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollArea>

/**
 * @brief Image display widget with zoom and pan capabilities
 *
 * Provides interactive image viewing with:
 * - Mouse wheel zooming
 * - Click and drag panning
 * - Fit-to-window functionality
 * - Zoom level management
 */
class ZoomableImageLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ZoomableImageLabel(QWidget *parent = nullptr);

    // === Image Display ===

    /**
     * @brief Set the image to display
     * @param pixmap Image pixmap to display
     */
    void setImagePixmap(const QPixmap &pixmap);

    /**
     * @brief Reset zoom to 100% (1:1 scale)
     */
    void resetZoom();

    /**
     * @brief Fit image to window size
     */
    void fitToWindow();

    // === Information ===

    /**
     * @brief Get current zoom factor
     * @return Current zoom level (1.0 = 100%)
     */
    double getZoomFactor() const { return m_scaleFactor; }

    /**
     * @brief Check if an image is currently loaded
     * @return True if image is loaded
     */
    bool hasImage() const { return !m_originalPixmap.isNull(); }

signals:
    /**
     * @brief Emitted when zoom level changes
     * @param zoomFactor New zoom factor (1.0 = 100%)
     */
    void zoomChanged(double zoomFactor);

protected:
    // === Event Handlers ===

    /**
     * @brief Handle mouse wheel events for zooming
     * @param event Wheel event
     */
    void wheelEvent(QWheelEvent *event) override;

    /**
     * @brief Handle mouse press events for panning
     * @param event Mouse press event
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief Handle mouse move events for panning
     * @param event Mouse move event
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief Handle mouse release events for panning
     * @param event Mouse release event
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // === Zoom Operations ===

    /**
     * @brief Scale the image by a factor
     * @param factor Scale factor to apply
     */
    void scaleImage(double factor);

    /**
     * @brief Update the displayed image with current scale
     */
    void updateDisplayedImage();

    /**
     * @brief Initialize widget properties
     */
    void setupWidget();

    /**
     * @brief Find the parent scroll area
     */
    void findScrollArea();

    /**
     * @brief Clear the display and show default text
     */
    void clearDisplay();

    /**
     * @brief Start panning operation
     * @param startPoint Initial mouse position
     */
    void startPanning(const QPoint &startPoint);

    /**
     * @brief Perform panning operation
     * @param currentPoint Current mouse position
     */
    void performPanning(const QPoint &currentPoint);

    /**
     * @brief Stop panning operation
     */
    void stopPanning();

    // === Data Members ===

    QPixmap m_originalPixmap;        ///< Original full-size image
    double m_scaleFactor;            ///< Current zoom level (1.0 = 100%)
    bool m_dragging;                 ///< True when panning is active
    QPoint m_lastPanPoint;           ///< Last mouse position during pan
    QScrollArea *m_scrollArea;       ///< Parent scroll area for panning

    // === Constants ===

    static constexpr double MIN_SCALE_FACTOR = 0.1;   ///< Minimum zoom level (10%)
    static constexpr double MAX_SCALE_FACTOR = 5.0;   ///< Maximum zoom level (500%)
    static constexpr double ZOOM_FACTOR = 1.15;       ///< Zoom step size
};

#endif // ZOOMABLEIMAGELABEL_H
