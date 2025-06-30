#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QScrollArea>
#include <QLabel>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QSettings>
#include <QDir>
#include <QMouseEvent>
#include <QStatusBar>
#include <QProgressBar>
#include <QTimer>
#include <QHash>
#include <QPixmap>
#include <QStandardPaths>

QT_BEGIN_NAMESPACE
class QTreeWidget;
class QScrollArea;
class QLabel;
QT_END_NAMESPACE

// ClickableLabel class
class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    ClickableLabel(const QString &imagePath, QWidget *parent = nullptr);

signals:
    void clicked(const QString &imagePath);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QString m_imagePath;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void addFolder();
    void onFolderSelected();
    void onImageClicked(const QString &imagePath);
    void loadNextThumbnail();

private:
    void loadSubfolders(QTreeWidgetItem *parentItem, const QString &path);
    void loadImagesInGrid(const QString &folderPath);
    void saveSettings();
    void loadSettings();
    void updateStatus(const QString &message);
    void clearStatus();
    QString getCacheKey(const QString &imagePath);
    QPixmap getOrCreateThumbnail(const QString &imagePath);
    void initializeThumbnailCache();

    QTreeWidget *treeWidget;
    QScrollArea *scrollArea;
    QLabel *imageLabel;
    QPushButton *addFolderButton;
    QSettings *settings;
    QStatusBar *m_statusBar;
    QProgressBar *progressBar;
    QTimer *statusTimer;

    // Caching
    QHash<QString, QPixmap> thumbnailCache;
    QString cacheDirectory;
    int thumbnailSize;

    // Async loading
    QStringList pendingImages;
    QTimer *loadTimer;
    QWidget *currentImageWidget;
    QGridLayout *currentGridLayout;
    int currentRow, currentCol, maxColumns;
};

#endif // MAINWINDOW_H
