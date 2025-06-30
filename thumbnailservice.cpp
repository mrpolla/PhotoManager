#include "thumbnailservice.h"
#include <QPixmap>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QTimer>
#include <QDebug>

ThumbnailService::ThumbnailService(QObject *parent)
    : QObject(parent)
    , m_maxMemoryCache(200)
    , m_maxDiskCacheSizeMB(500)  // 500MB default
    , m_defaultThumbnailSize(120)
{
    initializeCacheDirectory();

    // Setup cleanup timer (runs every 5 minutes)
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &ThumbnailService::cleanupOldCache);
    m_cleanupTimer->start(5 * 60 * 1000); // 5 minutes
}

ThumbnailService::~ThumbnailService()
{
    // Optional: save cache statistics or cleanup
}

void ThumbnailService::initializeCacheDirectory()
{
    m_cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
    + "/PhotoManager/thumbnails";
    QDir().mkpath(m_cacheDirectory);
}

QPixmap ThumbnailService::getThumbnail(const QString &imagePath, int size)
{
    if (size <= 0) size = m_defaultThumbnailSize;

    QString cacheKey = getCacheKey(imagePath, size);

    // Check memory cache first
    if (m_memoryCache.contains(cacheKey)) {
        return m_memoryCache[cacheKey];
    }

    // Check disk cache
    QPixmap diskCached = loadFromDiskCache(cacheKey);
    if (!diskCached.isNull()) {
        // Add to memory cache
        m_memoryCache[cacheKey] = diskCached;
        cleanupMemoryCache();
        return diskCached;
    }

    // Create new thumbnail
    QPixmap thumbnail = createThumbnail(imagePath, size);
    if (!thumbnail.isNull()) {
        // Save to both caches
        m_memoryCache[cacheKey] = thumbnail;
        saveToDiskCache(cacheKey, thumbnail);
        cleanupMemoryCache();

        emit thumbnailReady(imagePath, thumbnail);
    }

    return thumbnail;
}

void ThumbnailService::preloadThumbnails(const QStringList &imagePaths, int size)
{
    if (size <= 0) size = m_defaultThumbnailSize;

    int loaded = 0;
    for (const QString &imagePath : imagePaths) {
        getThumbnail(imagePath, size);
        loaded++;

        if (loaded % 10 == 0) { // Emit progress every 10 images
            emit preloadProgress(loaded, imagePaths.size());
        }
    }

    emit preloadProgress(loaded, imagePaths.size());
}

void ThumbnailService::clearCache()
{
    m_memoryCache.clear();

    // Clear disk cache
    QDir cacheDir(m_cacheDirectory);
    QStringList files = cacheDir.entryList(QStringList() << "*.png", QDir::Files);
    for (const QString &file : files) {
        cacheDir.remove(file);
    }

    emit cacheCleared();
}

void ThumbnailService::setCacheDirectory(const QString &directory)
{
    m_cacheDirectory = directory;
    QDir().mkpath(m_cacheDirectory);
}

void ThumbnailService::setMaxMemoryCache(int maxItems)
{
    m_maxMemoryCache = maxItems;
    cleanupMemoryCache();
}

void ThumbnailService::setMaxDiskCacheSize(int maxSizeMB)
{
    m_maxDiskCacheSizeMB = maxSizeMB;
}

void ThumbnailService::setThumbnailSize(int size)
{
    m_defaultThumbnailSize = size;
}

qint64 ThumbnailService::diskCacheSize() const
{
    QDir cacheDir(m_cacheDirectory);
    QStringList files = cacheDir.entryList(QStringList() << "*.png", QDir::Files);

    qint64 totalSize = 0;
    for (const QString &file : files) {
        QFileInfo fileInfo(cacheDir.absoluteFilePath(file));
        totalSize += fileInfo.size();
    }

    return totalSize;
}

QString ThumbnailService::getCacheKey(const QString &imagePath, int size) const
{
    QFileInfo fileInfo(imagePath);
    QString key = QString("%1_%2_%3_%4")
                      .arg(fileInfo.fileName())
                      .arg(fileInfo.size())
                      .arg(fileInfo.lastModified().toSecsSinceEpoch())
                      .arg(size);

    return QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex();
}

QPixmap ThumbnailService::createThumbnail(const QString &imagePath, int size)
{
    QPixmap originalPixmap(imagePath);
    if (originalPixmap.isNull()) {
        return QPixmap();
    }

    return originalPixmap.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QString ThumbnailService::getDiskCachePath(const QString &cacheKey) const
{
    return m_cacheDirectory + "/" + cacheKey + ".png";
}

QPixmap ThumbnailService::loadFromDiskCache(const QString &cacheKey) const
{
    QString filePath = getDiskCachePath(cacheKey);
    if (QFile::exists(filePath)) {
        return QPixmap(filePath);
    }
    return QPixmap();
}

void ThumbnailService::saveToDiskCache(const QString &cacheKey, const QPixmap &thumbnail)
{
    QString filePath = getDiskCachePath(cacheKey);
    thumbnail.save(filePath, "PNG");
}

void ThumbnailService::cleanupMemoryCache()
{
    if (m_memoryCache.size() > m_maxMemoryCache) {
        // Remove oldest entries (simple approach - clear half)
        auto it = m_memoryCache.begin();
        int toRemove = m_memoryCache.size() / 2;
        for (int i = 0; i < toRemove && it != m_memoryCache.end(); ++i) {
            it = m_memoryCache.erase(it);
        }
    }
}

void ThumbnailService::cleanupOldCache()
{
    // Check disk cache size and clean up if needed
    qint64 currentSize = diskCacheSize();
    qint64 maxSize = static_cast<qint64>(m_maxDiskCacheSizeMB) * 1024 * 1024;

    if (currentSize > maxSize) {
        QDir cacheDir(m_cacheDirectory);
        QFileInfoList files = cacheDir.entryInfoList(QStringList() << "*.png",
                                                     QDir::Files,
                                                     QDir::Time);

        // Remove oldest files until we're under the limit
        qint64 removedSize = 0;
        for (const QFileInfo &fileInfo : files) {
            if (currentSize - removedSize <= maxSize) break;

            removedSize += fileInfo.size();
            QFile::remove(fileInfo.absoluteFilePath());
        }
    }
}
