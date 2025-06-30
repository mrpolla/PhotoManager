#ifndef IMAGEGRIDWIDGET_H
#define IMAGEGRIDWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QStringList>
#include <QSet>

/**
 * @brief Clickable thumbnail label widget
 *
 * A QLabel that emits clicked signals and displays image thumbnails
 * with hover effects and tooltips.
 */
class ClickableLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ClickableLabel(const QString &imagePath, QWidget *parent = nullptr);

    /**
     * @brief Get the associated image path
     * @return Path to the image file
     */
    const QString& imagePath() const { return m_imagePath; }

signals:
    /**
     * @brief Emitted when the label is clicked
     * @param imagePath Path to the associated image
     */
    void clicked(const QString &imagePath);

protected:
    /**
     * @brief Handle mouse press events
     * @param event Mouse press event
     */
    void mousePressEvent(QMouseEvent *event) override;

private:
    /**
     * @brief Setup thumbnail label properties
     */
    void setupThumbnailLabel();

    QString m_imagePath;  ///< Associated image file path
};

// Forward declaration
class ThumbnailService;

/**
 * @brief Grid widget for displaying image thumbnails
 *
 * Provides efficient thumbnail display with:
 * - Lazy loading of thumbnails
 * - Grid layout management
 * - Progress tracking
 * - Performance limits for large folders
 */
class ImageGridWidget : public QScrollArea
{
    Q_OBJECT

public:
    explicit ImageGridWidget(ThumbnailService *thumbnailService, QWidget *parent = nullptr);

    // === Main Functionality ===

    /**
     * @brief Load and display images from a folder
     * @param folderPath Path to folder containing images
     */
    void loadImagesFromFolder(const QString &folderPath);

    /**
     * @brief Clear all displayed images
     */
    void clearImages();

    // === Configuration ===

    /**
     * @brief Set thumbnail size
     * @param size Thumbnail size in pixels
     */
    void setThumbnailSize(int size);

    /**
     * @brief Set maximum number of images to load per folder
     * @param maxImages Maximum images to display
     */
    void setMaxImagesPerLoad(int maxImages);

    // === Information ===

    /**
     * @brief Get total number of images in current folder
     * @return Total image count
     */
    int imageCount() const { return m_currentImages.size(); }

    /**
     * @brief Get number of thumbnails loaded so far
     * @return Number of loaded thumbnails
     */
    int loadedCount() const { return m_loadedCount; }

    /**
     * @brief Get current folder path
     * @return Path to currently displayed folder
     */
    QString currentFolder() const { return m_currentFolder; }

    /**
     * @brief Check if loading is in progress
     * @return True if thumbnails are being loaded
     */
    bool isLoading() const { return !m_pendingImages.isEmpty(); }

signals:
    /**
     * @brief Emitted when an image thumbnail is clicked
     * @param imagePath Path to the clicked image
     */
    void imageClicked(const QString &imagePath);

    /**
     * @brief Emitted when loading starts
     * @param totalImages Total number of images to load
     */
    void loadingStarted(int totalImages);

    /**
     * @brief Emitted during loading progress
     * @param loaded Number of thumbnails loaded
     * @param total Total number of thumbnails
     */
    void loadingProgress(int loaded, int total);

    /**
     * @brief Emitted when loading is complete
     * @param totalImages Total number of images loaded
     */
    void loadingFinished(int totalImages);

private slots:
    /**
     * @brief Handle thumbnail click events
     * @param imagePath Path to clicked image
     */
    void onThumbnailClicked(const QString &imagePath);

    /**
     * @brief Handle thumbnail ready events from service
     * @param imagePath Path to image
     * @param thumbnail Generated thumbnail
     */
    void onThumbnailReady(const QString &imagePath, const QPixmap &thumbnail);

private:
    // === UI Management ===

    /**
     * @brief Initialize the user interface
     */
    void setupUI();

    /**
     * @brief Connect signals between components
     */
    void connectSignals();

    /**
     * @brief Create the thumbnail grid layout
     */
    void createThumbnailGrid();

    /**
     * @brief Show placeholder message
     * @param message Message to display
     */
    void showPlaceholder(const QString &message);

    /**
     * @brief Get supported image file extensions
     * @return List of supported file patterns
     */
    QStringList getSupportedExtensions() const;

    // === Thumbnail Management ===

    /**
     * @brief Create a clickable thumbnail widget
     * @param imagePath Path to image file
     * @return Configured ClickableLabel widget
     */
    ClickableLabel* createThumbnailWidget(const QString &imagePath);

    /**
     * @brief Calculate grid dimensions based on viewport
     */
    void calculateGridDimensions();

    /**
     * @brief Add thumbnail to the grid layout
     * @param imagePath Image file path
     * @param thumbnail Thumbnail pixmap
     */
    void addThumbnailToGrid(const QString &imagePath, const QPixmap &thumbnail);

    // === State Management ===

    /**
     * @brief Reset widget state for new folder
     */
    void resetState();

    /**
     * @brief Reset grid position counters
     */
    void resetGridPosition();

    /**
     * @brief Advance to next grid position
     */
    void advanceGridPosition();

    /**
     * @brief Increment loaded count and emit progress
     */
    void incrementLoadedCount();

    // === Image Processing ===

    /**
     * @brief Scan folder for supported image files
     * @param folderPath Path to scan
     * @return List of image file paths
     */
    QStringList scanForImages(const QString &folderPath) const;

    /**
     * @brief Prepare and limit image list for loading
     * @param imageFiles Raw list of image files
     */
    void prepareImageList(const QStringList &imageFiles);

    /**
     * @brief Start the thumbnail loading process
     */
    void startLoading();

    /**
     * @brief Check if image is already added to grid
     * @param imagePath Path to check
     * @return True if image already in grid
     */
    bool isImageAlreadyInGrid(const QString &imagePath) const;

    // === Service Reference ===

    ThumbnailService *m_thumbnailService;  ///< Thumbnail generation service

    // === Current State ===

    QString m_currentFolder;               ///< Currently displayed folder
    QStringList m_currentImages;           ///< List of images in current folder
    QStringList m_pendingImages;           ///< Images waiting for thumbnails
    QSet<QString> m_addedImages;           ///< Images already added to grid (prevents duplicates)
    int m_loadedCount;                     ///< Number of loaded thumbnails

    // === Configuration ===

    int m_thumbnailSize;                   ///< Size of thumbnails in pixels
    int m_maxImagesPerLoad;               ///< Maximum images to load per folder
    int m_gridColumns;                    ///< Number of columns in grid

    // === UI Components ===

    QWidget *m_gridWidget;                ///< Container widget for grid
    QGridLayout *m_gridLayout;            ///< Grid layout manager
    int m_currentRow;                     ///< Current row in grid
    int m_currentCol;                     ///< Current column in grid
};

#endif // IMAGEGRIDWIDGET_H
