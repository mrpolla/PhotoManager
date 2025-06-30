#ifndef THUMBNAILSERVICE_H
#define THUMBNAILSERVICE_H

#include <QObject>
#include <QPixmap>
#include <QHash>
#include <QString>
#include <QTimer>

/**
 * @brief Service for generating, caching, and managing image thumbnails
 *
 * Provides efficient thumbnail generation with both memory and disk caching.
 * Supports asynchronous thumbnail loading and automatic cache cleanup.
 */
class ThumbnailService : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailService(QObject *parent = nullptr);
    ~ThumbnailService();

    // === Core Functionality ===

    /**
     * @brief Get thumbnail for an image file
     * @param imagePath Path to the source image
     * @param size Thumbnail size (default: 120px)
     * @return Thumbnail pixmap, or null pixmap if failed
     */
    QPixmap getThumbnail(const QString &imagePath, int size = 120);

    /**
     * @brief Preload thumbnails for multiple images
     * @param imagePaths List of image paths to preload
     * @param size Thumbnail size (default: 120px)
     */
    void preloadThumbnails(const QStringList &imagePaths, int size = 120);

    // === Cache Management ===

    /**
     * @brief Clear all cached thumbnails (memory and disk)
     */
    void clearCache();

    /**
     * @brief Set custom cache directory
     * @param directory Path to cache directory
     */
    void setCacheDirectory(const QString &directory);

    /**
     * @brief Set maximum number of thumbnails in memory cache
     * @param maxItems Maximum items to keep in memory
     */
    void setMaxMemoryCache(int maxItems);

    /**
     * @brief Set maximum disk cache size
     * @param maxSizeMB Maximum cache size in megabytes
     */
    void setMaxDiskCacheSize(int maxSizeMB);

    // === Configuration ===

    /**
     * @brief Set default thumbnail size
     * @param size Default size in pixels
     */
    void setThumbnailSize(int size);

    /**
     * @brief Get current default thumbnail size
     * @return Default thumbnail size in pixels
     */
    int getThumbnailSize() const { return m_defaultThumbnailSize; }

    // === Statistics ===

    /**
     * @brief Get number of thumbnails in memory cache
     * @return Number of cached items
     */
    int memoryCacheSize() const { return m_memoryCache.size(); }

    /**
     * @brief Get cache directory path
     * @return Path to cache directory
     */
    QString cacheDirectory() const { return m_cacheDirectory; }

    /**
     * @brief Calculate total disk cache size
     * @return Cache size in bytes
     */
    qint64 diskCacheSize() const;

signals:
    /**
     * @brief Emitted when a thumbnail is ready
     * @param imagePath Source image path
     * @param thumbnail Generated thumbnail
     */
    void thumbnailReady(const QString &imagePath, const QPixmap &thumbnail);

    /**
     * @brief Emitted when cache is cleared
     */
    void cacheCleared();

    /**
     * @brief Emitted during preload operations
     * @param loaded Number of thumbnails loaded
     * @param total Total number to load
     */
    void preloadProgress(int loaded, int total);

private slots:
    /**
     * @brief Periodic cleanup of old cache files
     */
    void cleanupOldCache();

private:
    // === Cache Operations ===

    /**
     * @brief Generate unique cache key for image and size
     * @param imagePath Source image path
     * @param size Thumbnail size
     * @return Unique cache key
     */
    QString getCacheKey(const QString &imagePath, int size) const;

    /**
     * @brief Create thumbnail from source image
     * @param imagePath Source image path
     * @param size Thumbnail size
     * @return Generated thumbnail
     */
    QPixmap createThumbnail(const QString &imagePath, int size);

    /**
     * @brief Get full path for disk cache file
     * @param cacheKey Cache key
     * @return Full file path
     */
    QString getDiskCachePath(const QString &cacheKey) const;

    /**
     * @brief Load thumbnail from disk cache
     * @param cacheKey Cache key
     * @return Cached thumbnail or null if not found
     */
    QPixmap loadFromDiskCache(const QString &cacheKey) const;

    /**
     * @brief Save thumbnail to disk cache
     * @param cacheKey Cache key
     * @param thumbnail Thumbnail to save
     */
    void saveToDiskCache(const QString &cacheKey, const QPixmap &thumbnail);

    /**
     * @brief Clean up memory cache when it exceeds limits
     */
    void cleanupMemoryCache();

    /**
     * @brief Initialize cache directory structure
     */
    void initializeCacheDirectory();

    /**
     * @brief Setup the cleanup timer
     */
    void setupCleanupTimer();

    /**
     * @brief Clear all files from disk cache
     */
    void clearDiskCache();

    // === Data Members ===

    QHash<QString, QPixmap> m_memoryCache;    ///< In-memory thumbnail cache
    QString m_cacheDirectory;                 ///< Disk cache directory path
    int m_maxMemoryCache;                     ///< Maximum items in memory cache
    int m_maxDiskCacheSizeMB;                ///< Maximum disk cache size (MB)
    int m_defaultThumbnailSize;              ///< Default thumbnail size
    QTimer *m_cleanupTimer;                  ///< Timer for periodic cache cleanup
};

#endif // THUMBNAILSERVICE_H
