#include "thumbnailservice.h"
#include <QPixmap>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QTimer>
#include <QDebug>

// === Constants ===
namespace {
constexpr int DEFAULT_MEMORY_CACHE_SIZE = 200;
constexpr int DEFAULT_DISK_CACHE_SIZE_MB = 500;
constexpr int DEFAULT_THUMBNAIL_SIZE = 120;
constexpr int CLEANUP_INTERVAL_MS = 5 * 60 * 1000; // 5 minutes
constexpr int PROGRESS_UPDATE_INTERVAL = 10;
}

// === Constructor & Destructor ===

ThumbnailService::ThumbnailService(QObject *parent)
    : QObject(parent)
    , m_maxMemoryCache(DEFAULT_MEMORY_CACHE_SIZE)
    , m_maxDiskCacheSizeMB(DEFAULT_DISK_CACHE_SIZE_MB)
    , m_defaultThumbnailSize(DEFAULT_THUMBNAIL_SIZE)
{
    initializeCacheDirectory();
    setupCleanupTimer();
}

ThumbnailService::~ThumbnailService()
{
    // Cache will be automatically cleaned up
}

// === Core Functionality ===

QPixmap ThumbnailService::getThumbnail(const QString &imagePath, int size)
{
    if (size <= 0) {
        size = m_defaultThumbnailSize;
    }

    const QString cacheKey = getCacheKey(imagePath, size);

    // 1. Check memory cache first (fastest)
    if (m_memoryCache.contains(cacheKey)) {
        return m_memoryCache[cacheKey];
    }

    // 2. Check disk cache (fast)
    QPixmap diskCached = loadFromDiskCache(cacheKey);
    if (!diskCached.isNull()) {
        // Add to memory cache for faster future access
        m_memoryCache[cacheKey] = diskCached;
        cleanupMemoryCache();
        return diskCached;
    }

    // 3. Create new thumbnail (slow)
    QPixmap thumbnail = createThumbnail(imagePath, size);
    if (!thumbnail.isNull()) {
        // Cache in both memory and disk
        m_memoryCache[cacheKey] = thumbnail;
        saveToDiskCache(cacheKey, thumbnail);
        cleanupMemoryCache();

        emit thumbnailReady(imagePath, thumbnail);
    }

    return thumbnail;
}

void ThumbnailService::preloadThumbnails(const QStringList &imagePaths, int size)
{
    if (size <= 0) {
        size = m_defaultThumbnailSize;
    }

    const int totalImages = imagePaths.size();
    int loadedCount = 0;

    for (const QString &imagePath : imagePaths) {
        getThumbnail(imagePath, size);
        loadedCount++;

        // Emit progress periodically to avoid UI flooding
        if (loadedCount % PROGRESS_UPDATE_INTERVAL == 0 || loadedCount == totalImages) {
            emit preloadProgress(loadedCount, totalImages);
        }
    }
}

// === Cache Management ===

void ThumbnailService::clearCache()
{
    // Clear memory cache
    m_memoryCache.clear();

    // Clear disk cache
    clearDiskCache();

    emit cacheCleared();
}

void ThumbnailService::setCacheDirectory(const QString &directory)
{
    m_cacheDirectory = directory;
    QDir().mkpath(m_cacheDirectory);
}

void ThumbnailService::setMaxMemoryCache(int maxItems)
{
    m_maxMemoryCache = qMax(1, maxItems);
    cleanupMemoryCache();
}

void ThumbnailService::setMaxDiskCacheSize(int maxSizeMB)
{
    m_maxDiskCacheSizeMB = qMax(1, maxSizeMB);
}

void ThumbnailService::setThumbnailSize(int size)
{
    m_defaultThumbnailSize = qMax(16, size);
}

// === Statistics ===

qint64 ThumbnailService::diskCacheSize() const
{
    const QDir cacheDir(m_cacheDirectory);
    const QStringList files = cacheDir.entryList(QStringList() << "*.png", QDir::Files);

    qint64 totalSize = 0;
    for (const QString &fileName : files) {
        const QFileInfo fileInfo(cacheDir.absoluteFilePath(fileName));
        totalSize += fileInfo.size();
    }

    return totalSize;
}

// === Private Slots ===

void ThumbnailService::cleanupOldCache()
{
    const qint64 currentSize = diskCacheSize();
    const qint64 maxSize = static_cast<qint64>(m_maxDiskCacheSizeMB) * 1024 * 1024;

    if (currentSize <= maxSize) {
        return; // No cleanup needed
    }

    const QDir cacheDir(m_cacheDirectory);
    QFileInfoList files = cacheDir.entryInfoList(
        QStringList() << "*.png",
        QDir::Files,
        QDir::Time | QDir::Reversed // Oldest first
        );

    // Remove oldest files until we're under the limit
    qint64 removedSize = 0;
    for (const QFileInfo &fileInfo : files) {
        if (currentSize - removedSize <= maxSize) {
            break;
        }

        removedSize += fileInfo.size();
        if (!QFile::remove(fileInfo.absoluteFilePath())) {
            qWarning() << "Failed to remove cache file:" << fileInfo.absoluteFilePath();
        }
    }

    if (removedSize > 0) {
        qDebug() << "Cleaned up" << removedSize << "bytes from thumbnail cache";
    }
}

// === Private Methods ===

void ThumbnailService::initializeCacheDirectory()
{
    m_cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
    + "/PhotoManager/thumbnails";

    if (!QDir().mkpath(m_cacheDirectory)) {
        qWarning() << "Failed to create cache directory:" << m_cacheDirectory;
    }
}

void ThumbnailService::setupCleanupTimer()
{
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &ThumbnailService::cleanupOldCache);
    m_cleanupTimer->start(CLEANUP_INTERVAL_MS);
}

QString ThumbnailService::getCacheKey(const QString &imagePath, int size) const
{
    const QFileInfo fileInfo(imagePath);
    const QString keyData = QString("%1_%2_%3_%4")
                                .arg(fileInfo.fileName())
                                .arg(fileInfo.size())
                                .arg(fileInfo.lastModified().toSecsSinceEpoch())
                                .arg(size);

    return QCryptographicHash::hash(keyData.toUtf8(), QCryptographicHash::Md5).toHex();
}

QPixmap ThumbnailService::createThumbnail(const QString &imagePath, int size)
{
    QPixmap originalPixmap(imagePath);
    if (originalPixmap.isNull()) {
        qWarning() << "Failed to load image for thumbnail:" << imagePath;
        return QPixmap();
    }

    return originalPixmap.scaled(
        size, size,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        );
}

QString ThumbnailService::getDiskCachePath(const QString &cacheKey) const
{
    return m_cacheDirectory + "/" + cacheKey + ".png";
}

QPixmap ThumbnailService::loadFromDiskCache(const QString &cacheKey) const
{
    const QString filePath = getDiskCachePath(cacheKey);
    if (!QFile::exists(filePath)) {
        return QPixmap();
    }

    QPixmap cached(filePath);
    if (cached.isNull()) {
        // Corrupted cache file - remove it
        QFile::remove(filePath);
    }

    return cached;
}

void ThumbnailService::saveToDiskCache(const QString &cacheKey, const QPixmap &thumbnail)
{
    const QString filePath = getDiskCachePath(cacheKey);
    if (!thumbnail.save(filePath, "PNG")) {
        qWarning() << "Failed to save thumbnail to cache:" << filePath;
    }
}

void ThumbnailService::cleanupMemoryCache()
{
    if (m_memoryCache.size() <= m_maxMemoryCache) {
        return; // No cleanup needed
    }

    // Simple cleanup: remove half of the items
    // A more sophisticated approach would use LRU
    const int itemsToRemove = m_memoryCache.size() / 2;
    auto it = m_memoryCache.begin();

    for (int i = 0; i < itemsToRemove && it != m_memoryCache.end(); ++i) {
        it = m_memoryCache.erase(it);
    }
}

void ThumbnailService::clearDiskCache()
{
    QDir cacheDir(m_cacheDirectory);  // Remove const
    const QStringList files = cacheDir.entryList(QStringList() << "*.png", QDir::Files);

    for (const QString &fileName : files) {
        if (!cacheDir.remove(fileName)) {
            qWarning() << "Failed to remove cache file:" << fileName;
        }
    }
}
