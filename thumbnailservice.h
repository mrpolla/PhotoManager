#ifndef THUMBNAILSERVICE_H
#define THUMBNAILSERVICE_H

#include <QObject>
#include <QPixmap>
#include <QHash>
#include <QString>
#include <QTimer>

class ThumbnailService : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailService(QObject *parent = nullptr);
    ~ThumbnailService();

    // Main functionality
    QPixmap getThumbnail(const QString &imagePath, int size = 120);
    void preloadThumbnails(const QStringList &imagePaths, int size = 120);

    // Cache management
    void clearCache();
    void setCacheDirectory(const QString &directory);
    void setMaxMemoryCache(int maxItems);
    void setMaxDiskCacheSize(int maxSizeMB);

    // Configuration
    void setThumbnailSize(int size);
    int getThumbnailSize() const { return m_defaultThumbnailSize; }

    // Statistics
    int memoryCacheSize() const { return m_memoryCache.size(); }
    QString cacheDirectory() const { return m_cacheDirectory; }
    qint64 diskCacheSize() const;

signals:
    void thumbnailReady(const QString &imagePath, const QPixmap &thumbnail);
    void cacheCleared();
    void preloadProgress(int loaded, int total);

private slots:
    void cleanupOldCache();

private:
    // Cache operations
    QString getCacheKey(const QString &imagePath, int size) const;
    QPixmap createThumbnail(const QString &imagePath, int size);
    QString getDiskCachePath(const QString &cacheKey) const;
    QPixmap loadFromDiskCache(const QString &cacheKey) const;
    void saveToDiskCache(const QString &cacheKey, const QPixmap &thumbnail);
    void cleanupMemoryCache();
    void initializeCacheDirectory();

    // Data members
    QHash<QString, QPixmap> m_memoryCache;
    QString m_cacheDirectory;
    int m_maxMemoryCache;
    int m_maxDiskCacheSizeMB;
    int m_defaultThumbnailSize;
    QTimer *m_cleanupTimer;
};

#endif // THUMBNAILSERVICE_H
