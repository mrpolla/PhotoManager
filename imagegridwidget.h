#ifndef IMAGEGRIDWIDGET_H
#define IMAGEGRIDWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QStringList>

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickableLabel(const QString &imagePath, QWidget *parent = nullptr);
    const QString& imagePath() const { return m_imagePath; }

signals:
    void clicked(const QString &imagePath);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QString m_imagePath;
};

class ThumbnailService;  // Forward declaration

class ImageGridWidget : public QScrollArea
{
    Q_OBJECT

public:
    explicit ImageGridWidget(ThumbnailService *thumbnailService, QWidget *parent = nullptr);

    // Main functionality
    void loadImagesFromFolder(const QString &folderPath);
    void clearImages();

    // Configuration
    void setThumbnailSize(int size);
    void setMaxImagesPerLoad(int maxImages);

    // Information
    int imageCount() const { return m_currentImages.size(); }
    int loadedCount() const { return m_loadedCount; }

signals:
    void imageClicked(const QString &imagePath);
    void loadingStarted(int totalImages);
    void loadingProgress(int loaded, int total);
    void loadingFinished(int totalImages);

private slots:
    void onThumbnailClicked(const QString &imagePath);
    void onThumbnailReady(const QString &imagePath, const QPixmap &thumbnail);

private:
    // UI creation
    void setupUI();
    void createThumbnailGrid();
    void showPlaceholder(const QString &message);

    // Thumbnail management
    ClickableLabel* createThumbnailWidget(const QString &imagePath);
    void calculateGridDimensions();
    void addThumbnailToGrid(const QString &imagePath, const QPixmap &thumbnail);

    // Service reference
    ThumbnailService *m_thumbnailService;

    // Current state
    QString m_currentFolder;
    QStringList m_currentImages;
    QStringList m_pendingImages;  // Images waiting for thumbnails
    int m_loadedCount;

    // Configuration
    int m_thumbnailSize;
    int m_maxImagesPerLoad;
    int m_gridColumns;

    // UI components
    QWidget *m_gridWidget;
    QGridLayout *m_gridLayout;
    int m_currentRow;
    int m_currentCol;
};

#endif // IMAGEGRIDWIDGET_H
