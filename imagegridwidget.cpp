#include "imagegridwidget.h"
#include "thumbnailservice.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QMouseEvent>

// === Constants ===
namespace {
constexpr int DEFAULT_THUMBNAIL_SIZE = 120;
constexpr int DEFAULT_MAX_IMAGES = 100;
constexpr int DEFAULT_GRID_COLUMNS = 4;
constexpr int THUMBNAIL_MARGIN = 4;
constexpr int GRID_SPACING = 5;
constexpr int MIN_GRID_COLUMNS = 1;
constexpr int COLUMN_SPACING = 10;

const QString PLACEHOLDER_STYLE = "font-size: 14px; color: gray; padding: 20px;";
const QString THUMBNAIL_STYLE = "border: 1px solid lightgray; margin: 2px; background-color: white;";

const QString MSG_SELECT_FOLDER = "Select a folder to view images";
const QString MSG_NO_FOLDER = "No folder selected";
const QString MSG_NO_IMAGES = "No images found in this folder";
}

// === ClickableLabel Implementation ===

ClickableLabel::ClickableLabel(const QString &imagePath, QWidget *parent)
    : QLabel(parent)
    , m_imagePath(imagePath)
{
    setupThumbnailLabel();
}

void ClickableLabel::setupThumbnailLabel()
{
    setCursor(Qt::PointingHandCursor);
    setStyleSheet(THUMBNAIL_STYLE);
    setAlignment(Qt::AlignCenter);

    // Set tooltip with filename
    const QFileInfo fileInfo(m_imagePath);
    setToolTip(fileInfo.fileName());
}

void ClickableLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_imagePath);
    }
    QLabel::mousePressEvent(event);
}

// === ImageGridWidget Implementation ===

ImageGridWidget::ImageGridWidget(ThumbnailService *thumbnailService, QWidget *parent)
    : QScrollArea(parent)
    , m_thumbnailService(thumbnailService)
    , m_loadedCount(0)
    , m_thumbnailSize(DEFAULT_THUMBNAIL_SIZE)
    , m_maxImagesPerLoad(DEFAULT_MAX_IMAGES)
    , m_gridColumns(DEFAULT_GRID_COLUMNS)
    , m_gridWidget(nullptr)
    , m_gridLayout(nullptr)
    , m_currentRow(0)
    , m_currentCol(0)
{
    setupUI();
    connectSignals();
}

// === Public Methods ===

void ImageGridWidget::loadImagesFromFolder(const QString &folderPath)
{
    resetState();
    m_currentFolder = folderPath;

    if (folderPath.isEmpty()) {
        showPlaceholder(MSG_NO_FOLDER);
        return;
    }

    const QStringList imageFiles = scanForImages(folderPath);
    if (imageFiles.isEmpty()) {
        showPlaceholder(MSG_NO_IMAGES);
        emit loadingFinished(0);
        return;
    }

    prepareImageList(imageFiles);
    startLoading();
}

void ImageGridWidget::clearImages()
{
    resetState();
    showPlaceholder(MSG_SELECT_FOLDER);
}

void ImageGridWidget::setThumbnailSize(int size)
{
    m_thumbnailSize = qMax(16, size);
    if (m_thumbnailService) {
        m_thumbnailService->setThumbnailSize(m_thumbnailSize);
    }
}

void ImageGridWidget::setMaxImagesPerLoad(int maxImages)
{
    m_maxImagesPerLoad = qMax(1, maxImages);
}

// === Private Slots ===

void ImageGridWidget::onThumbnailClicked(const QString &imagePath)
{
    emit imageClicked(imagePath);
}

void ImageGridWidget::onThumbnailReady(const QString &imagePath, const QPixmap &thumbnail)
{
    // Only process if this image is part of current loading session
    if (!m_pendingImages.contains(imagePath)) {
        return;
    }

    // Check if this image was already added to avoid duplicates
    if (!isImageAlreadyInGrid(imagePath)) {
        addThumbnailToGrid(imagePath, thumbnail);
    }

    m_pendingImages.removeAll(imagePath);

    // Check if loading is complete
    if (m_pendingImages.isEmpty()) {
        emit loadingFinished(m_loadedCount);
    }
}

bool ImageGridWidget::isImageAlreadyInGrid(const QString &imagePath) const
{
    return m_addedImages.contains(imagePath);
}

// === Private Methods - UI Management ===

void ImageGridWidget::setupUI()
{
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    showPlaceholder(MSG_SELECT_FOLDER);
}

void ImageGridWidget::connectSignals()
{
    if (m_thumbnailService) {
        connect(m_thumbnailService, &ThumbnailService::thumbnailReady,
                this, &ImageGridWidget::onThumbnailReady);
    }
}

void ImageGridWidget::createThumbnailGrid()
{
    // Create new grid widget and layout
    m_gridWidget = new QWidget;
    m_gridLayout = new QGridLayout(m_gridWidget);
    m_gridLayout->setSpacing(GRID_SPACING);
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    calculateGridDimensions();
    resetGridPosition();

    // Set the widget to scroll area
    setWidget(m_gridWidget);
}

void ImageGridWidget::showPlaceholder(const QString &message)
{
    QWidget *placeholderWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(placeholderWidget);

    QLabel *placeholderLabel = new QLabel(message);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setStyleSheet(PLACEHOLDER_STYLE);

    layout->addWidget(placeholderLabel);
    setWidget(placeholderWidget);
}

QStringList ImageGridWidget::getSupportedExtensions() const
{
    return QStringList() << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp"
                         << "*.gif" << "*.tiff" << "*.tif" << "*.webp";
}

// === Private Methods - Thumbnail Management ===

ClickableLabel* ImageGridWidget::createThumbnailWidget(const QString &imagePath)
{
    ClickableLabel *thumbnailLabel = new ClickableLabel(imagePath);
    const int widgetSize = m_thumbnailSize + THUMBNAIL_MARGIN;
    thumbnailLabel->setFixedSize(widgetSize, widgetSize);

    connect(thumbnailLabel, &ClickableLabel::clicked,
            this, &ImageGridWidget::onThumbnailClicked);

    return thumbnailLabel;
}

void ImageGridWidget::calculateGridDimensions()
{
    const int availableWidth = viewport()->width();
    const int columnWidth = m_thumbnailSize + COLUMN_SPACING;
    m_gridColumns = qMax(MIN_GRID_COLUMNS, availableWidth / columnWidth);
}

void ImageGridWidget::addThumbnailToGrid(const QString &imagePath, const QPixmap &thumbnail)
{
    if (!m_gridLayout || thumbnail.isNull()) {
        return;
    }

    // Double-check we haven't already added this image
    if (isImageAlreadyInGrid(imagePath)) {
        return;
    }

    ClickableLabel *thumbnailLabel = createThumbnailWidget(imagePath);
    if (!thumbnailLabel) {
        return;
    }

    thumbnailLabel->setPixmap(thumbnail);
    m_gridLayout->addWidget(thumbnailLabel, m_currentRow, m_currentCol);

    // Track that this image was added
    m_addedImages.insert(imagePath);

    advanceGridPosition();
    incrementLoadedCount();
}

// === Private Methods - State Management ===

void ImageGridWidget::resetState()
{
    m_currentImages.clear();
    m_pendingImages.clear();
    m_addedImages.clear(); // Clear tracking of added images
    m_loadedCount = 0;
    m_currentFolder.clear();
}

void ImageGridWidget::resetGridPosition()
{
    m_currentRow = 0;
    m_currentCol = 0;
}

void ImageGridWidget::advanceGridPosition()
{
    m_currentCol++;
    if (m_currentCol >= m_gridColumns) {
        m_currentCol = 0;
        m_currentRow++;
    }
}

void ImageGridWidget::incrementLoadedCount()
{
    m_loadedCount++;
    emit loadingProgress(m_loadedCount, m_currentImages.size());
}

// === Private Methods - Image Processing ===

QStringList ImageGridWidget::scanForImages(const QString &folderPath) const
{
    const QDir dir(folderPath);
    if (!dir.exists()) {
        return QStringList();
    }

    const QStringList filters = getSupportedExtensions();
    const QStringList imageFiles = dir.entryList(filters, QDir::Files, QDir::Name);

    // Convert to absolute paths
    QStringList absolutePaths;
    for (const QString &fileName : imageFiles) {
        absolutePaths.append(dir.absoluteFilePath(fileName));
    }

    return absolutePaths;
}

void ImageGridWidget::prepareImageList(const QStringList &imageFiles)
{
    m_currentImages = imageFiles;

    // Limit images for performance
    if (m_currentImages.size() > m_maxImagesPerLoad) {
        m_currentImages = m_currentImages.mid(0, m_maxImagesPerLoad);
    }
}

void ImageGridWidget::startLoading()
{
    emit loadingStarted(m_currentImages.size());
    createThumbnailGrid();

    // Request thumbnails for all images
    m_pendingImages = m_currentImages;
    m_addedImages.clear(); // Track images already added to grid

    if (!m_thumbnailService) {
        emit loadingFinished(0);
        return;
    }

    // Request thumbnails from service
    for (const QString &imagePath : m_currentImages) {
        const QPixmap thumbnail = m_thumbnailService->getThumbnail(imagePath, m_thumbnailSize);
        if (!thumbnail.isNull()) {
            // Thumbnail was in cache - add immediately if not already added
            if (!isImageAlreadyInGrid(imagePath)) {
                addThumbnailToGrid(imagePath, thumbnail);
                m_pendingImages.removeAll(imagePath);
            }
        }
        // If thumbnail wasn't cached, onThumbnailReady will be called when ready
    }

    // If all thumbnails were cached, we're done
    if (m_pendingImages.isEmpty()) {
        emit loadingFinished(m_loadedCount);
    }
}
