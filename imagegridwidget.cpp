#include "imagegridwidget.h"
#include "thumbnailservice.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QMouseEvent>

// ClickableLabel Implementation (unchanged)
ClickableLabel::ClickableLabel(const QString &imagePath, QWidget *parent)
    : QLabel(parent), m_imagePath(imagePath)
{
    setCursor(Qt::PointingHandCursor);
    setStyleSheet("border: 1px solid lightgray; margin: 2px; background-color: white;");
    setAlignment(Qt::AlignCenter);

    // Set tooltip with filename
    QFileInfo fileInfo(imagePath);
    setToolTip(fileInfo.fileName());
}

void ClickableLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_imagePath);
    }
    QLabel::mousePressEvent(event);
}

// ImageGridWidget Implementation
ImageGridWidget::ImageGridWidget(ThumbnailService *thumbnailService, QWidget *parent)
    : QScrollArea(parent)
    , m_thumbnailService(thumbnailService)
    , m_loadedCount(0)
    , m_thumbnailSize(120)
    , m_maxImagesPerLoad(100)
    , m_gridColumns(4)
    , m_gridWidget(nullptr)
    , m_gridLayout(nullptr)
    , m_currentRow(0)
    , m_currentCol(0)
{
    setupUI();

    // Connect to thumbnail service signals
    if (m_thumbnailService) {
        connect(m_thumbnailService, &ThumbnailService::thumbnailReady,
                this, &ImageGridWidget::onThumbnailReady);
    }
}

void ImageGridWidget::setupUI()
{
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    showPlaceholder("Select a folder to view images");
}

void ImageGridWidget::loadImagesFromFolder(const QString &folderPath)
{
    m_currentFolder = folderPath;
    m_currentImages.clear();
    m_pendingImages.clear();
    m_loadedCount = 0;

    if (folderPath.isEmpty()) {
        showPlaceholder("No folder selected");
        return;
    }

    // Get image files
    QDir dir(folderPath);
    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif"
            << "*.tiff" << "*.tif" << "*.webp";

    QStringList imageFiles = dir.entryList(filters, QDir::Files, QDir::Name);

    if (imageFiles.isEmpty()) {
        showPlaceholder("No images found in this folder");
        emit loadingFinished(0);
        return;
    }

    // Convert to absolute paths
    for (const QString &fileName : imageFiles) {
        m_currentImages.append(dir.absoluteFilePath(fileName));
    }

    // Limit images for performance
    if (m_currentImages.size() > m_maxImagesPerLoad) {
        m_currentImages = m_currentImages.mid(0, m_maxImagesPerLoad);
    }

    emit loadingStarted(m_currentImages.size());
    createThumbnailGrid();
}

void ImageGridWidget::createThumbnailGrid()
{
    // Create new grid widget
    m_gridWidget = new QWidget;
    m_gridLayout = new QGridLayout(m_gridWidget);
    m_gridLayout->setSpacing(5);
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    calculateGridDimensions();

    // Reset grid position
    m_currentRow = 0;
    m_currentCol = 0;

    // Set the widget to scroll area immediately
    setWidget(m_gridWidget);

    // Request thumbnails for all images
    m_pendingImages = m_currentImages;

    if (m_thumbnailService) {
        // Get thumbnails (this will trigger onThumbnailReady for each one)
        for (const QString &imagePath : m_currentImages) {
            QPixmap thumbnail = m_thumbnailService->getThumbnail(imagePath, m_thumbnailSize);
            if (!thumbnail.isNull()) {
                // Thumbnail was in cache - add immediately
                addThumbnailToGrid(imagePath, thumbnail);
            }
            // If thumbnail wasn't cached, onThumbnailReady will be called when it's ready
        }
    }

    // If all thumbnails were cached, we're done
    if (m_loadedCount >= m_currentImages.size()) {
        emit loadingFinished(m_loadedCount);
    }
}

void ImageGridWidget::onThumbnailReady(const QString &imagePath, const QPixmap &thumbnail)
{
    // Only process if this image is part of current loading session
    if (!m_pendingImages.contains(imagePath)) {
        return;
    }

    addThumbnailToGrid(imagePath, thumbnail);
    m_pendingImages.removeAll(imagePath);

    // Check if we're done loading
    if (m_pendingImages.isEmpty()) {
        emit loadingFinished(m_loadedCount);
    }
}

void ImageGridWidget::addThumbnailToGrid(const QString &imagePath, const QPixmap &thumbnail)
{
    if (!m_gridLayout || thumbnail.isNull()) {
        return;
    }

    ClickableLabel *thumbnailLabel = createThumbnailWidget(imagePath);
    if (thumbnailLabel) {
        thumbnailLabel->setPixmap(thumbnail);

        m_gridLayout->addWidget(thumbnailLabel, m_currentRow, m_currentCol);

        m_currentCol++;
        if (m_currentCol >= m_gridColumns) {
            m_currentCol = 0;
            m_currentRow++;
        }

        m_loadedCount++;
        emit loadingProgress(m_loadedCount, m_currentImages.size());
    }
}

void ImageGridWidget::calculateGridDimensions()
{
    // Calculate columns based on viewport width
    int availableWidth = viewport()->width();
    m_gridColumns = qMax(1, availableWidth / (m_thumbnailSize + 10));
}

ClickableLabel* ImageGridWidget::createThumbnailWidget(const QString &imagePath)
{
    ClickableLabel *thumbnailLabel = new ClickableLabel(imagePath);
    thumbnailLabel->setFixedSize(m_thumbnailSize + 4, m_thumbnailSize + 4);

    connect(thumbnailLabel, &ClickableLabel::clicked,
            this, &ImageGridWidget::onThumbnailClicked);

    return thumbnailLabel;
}

void ImageGridWidget::onThumbnailClicked(const QString &imagePath)
{
    emit imageClicked(imagePath);
}

void ImageGridWidget::clearImages()
{
    m_currentImages.clear();
    m_pendingImages.clear();
    m_loadedCount = 0;
    m_currentFolder.clear();

    showPlaceholder("Select a folder to view images");
}

void ImageGridWidget::showPlaceholder(const QString &message)
{
    QWidget *placeholderWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(placeholderWidget);

    QLabel *placeholderLabel = new QLabel(message);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setStyleSheet("font-size: 14px; color: gray; padding: 20px;");

    layout->addWidget(placeholderLabel);
    setWidget(placeholderWidget);
}

void ImageGridWidget::setThumbnailSize(int size)
{
    m_thumbnailSize = size;
    if (m_thumbnailService) {
        m_thumbnailService->setThumbnailSize(size);
    }
}

void ImageGridWidget::setMaxImagesPerLoad(int maxImages)
{
    m_maxImagesPerLoad = maxImages;
}
