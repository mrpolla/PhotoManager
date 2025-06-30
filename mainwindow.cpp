#include "mainwindow.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QTreeWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QTreeWidgetItem>
#include <QStatusBar>
#include <QProgressBar>
#include <QTimer>
#include <QSettings>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>

ClickableLabel::ClickableLabel(const QString &imagePath, QWidget *parent)
    : QLabel(parent), m_imagePath(imagePath) {
    setCursor(Qt::PointingHandCursor);
    setStyleSheet("border: 1px solid lightgray; margin: 2px;");
}

void ClickableLabel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_imagePath);  // This should emit the signal
    }
    QLabel::mousePressEvent(event);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), thumbnailSize(120)
{
    // Initialize settings and cache
    settings = new QSettings("PhotoManager", "PhotoManager", this);
    initializeThumbnailCache();

    // Create central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Create layout
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // Create splitter
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter);

    // Create left panel (folder tree)
    QWidget *leftPanel = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    addFolderButton = new QPushButton("Add Folder");
    treeWidget = new QTreeWidget;
    treeWidget->setHeaderLabel("Folders");
    leftLayout->addWidget(addFolderButton);
    leftLayout->addWidget(treeWidget);

    // Create middle panel (scroll area)
    scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    QLabel *placeholderLabel = new QLabel("Select a folder");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    scrollArea->setWidget(placeholderLabel);

    // Create right panel (image display)
    imageLabel = new QLabel;
    imageLabel->setText("Select an image");
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setMinimumSize(300, 300);
    imageLabel->setStyleSheet("border: 1px solid gray;");

    // Add panels to splitter
    splitter->addWidget(leftPanel);
    splitter->addWidget(scrollArea);
    splitter->addWidget(imageLabel);

    // Create status bar
    m_statusBar = this->statusBar();
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    m_statusBar->addPermanentWidget(progressBar);
    m_statusBar->showMessage("Ready");

    // Setup timer
    statusTimer = new QTimer(this);
    statusTimer->setSingleShot(true);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::clearStatus);

    // Connect signals
    connect(addFolderButton, &QPushButton::clicked, this, &MainWindow::addFolder);
    connect(treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onFolderSelected);

    setWindowTitle("Photo Manager");
    resize(1200, 800);

    updateStatus("Application started - cache ready");
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::initializeThumbnailCache()
{
    cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/PhotoManager";
    QDir().mkpath(cacheDirectory);
    updateStatus("Cache directory: " + cacheDirectory);
}

QString MainWindow::getCacheKey(const QString &imagePath)
{
    QFileInfo fileInfo(imagePath);
    QString key = QString("%1_%2_%3").arg(fileInfo.fileName())
                      .arg(fileInfo.lastModified().toSecsSinceEpoch())
                      .arg(thumbnailSize);
    return QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex();
}

QPixmap MainWindow::getOrCreateThumbnail(const QString &imagePath)
{
    // Test with a simple approach first
    QPixmap originalPixmap(imagePath);
    if (!originalPixmap.isNull()) {
        return originalPixmap.scaled(thumbnailSize, thumbnailSize,
                                     Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
    }
    return QPixmap();
}

void MainWindow::addFolder()
{
    updateStatus("Opening folder dialog...");
    QString folderPath = QFileDialog::getExistingDirectory(this, "Select Folder");
    if (!folderPath.isEmpty()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget);
        item->setText(0, QDir(folderPath).dirName());
        item->setData(0, Qt::UserRole, folderPath);
        treeWidget->addTopLevelItem(item);
        updateStatus("Folder added successfully");
    } else {
        updateStatus("No folder selected");
    }
}

void MainWindow::onFolderSelected()
{
    QTreeWidgetItem *currentItem = treeWidget->currentItem();
    if (currentItem) {
        QString folderPath = currentItem->data(0, Qt::UserRole).toString();

        updateStatus("Loading images from folder...");

        // Create a new widget for the scroll area content
        QWidget *contentWidget = new QWidget;
        QGridLayout *gridLayout = new QGridLayout(contentWidget);
        gridLayout->setSpacing(5);

        // Get all image files from folder
        QDir dir(folderPath);
        QStringList filters;
        filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif" << "*.tiff" << "*.webp";
        QStringList imageFiles = dir.entryList(filters, QDir::Files);

        if (imageFiles.isEmpty()) {
            QLabel *noImagesLabel = new QLabel("No images found in this folder");
            noImagesLabel->setAlignment(Qt::AlignCenter);
            gridLayout->addWidget(noImagesLabel, 0, 0);
        } else {
            // Calculate columns based on scroll area width
            int columns = qMax(1, scrollArea->width() / (thumbnailSize + 10));

            // Add images in grid
            int row = 0, col = 0;
            int imageCount = 0;

            for (const QString &fileName : imageFiles) {
                // Limit to first 50 images for now (to avoid performance issues)
                if (imageCount >= 50) break;

                QString fullPath = dir.absoluteFilePath(fileName);
                QPixmap thumbnail = getOrCreateThumbnail(fullPath);

                if (!thumbnail.isNull()) {
                    // Create clickable thumbnail
                    ClickableLabel *thumbnailLabel = new ClickableLabel(fullPath);
                    thumbnailLabel->setPixmap(thumbnail);
                    thumbnailLabel->setAlignment(Qt::AlignCenter);
                    thumbnailLabel->setFixedSize(thumbnailSize + 4, thumbnailSize + 4);

                    // Connect click signal - let's make sure this works
                    connect(thumbnailLabel, &ClickableLabel::clicked, this, &MainWindow::onImageClicked);

                    // Add debug: change cursor and add tooltip
                    thumbnailLabel->setCursor(Qt::PointingHandCursor);
                    thumbnailLabel->setToolTip("Click to view: " + QFileInfo(fullPath).fileName());

                    gridLayout->addWidget(thumbnailLabel, row, col);

                    col++;
                    if (col >= columns) {
                        col = 0;
                        row++;
                    }
                    imageCount++;
                }
            }

            updateStatus(QString("Loaded %1 images from folder").arg(imageCount));
        }

        // Set the widget to scroll area
        scrollArea->setWidget(contentWidget);

        // Save last opened folder
        settings->setValue("lastFolder", folderPath);
    }
}

void MainWindow::updateStatus(const QString &message)
{
    if (m_statusBar) {
        m_statusBar->showMessage(message);
        statusTimer->start(3000);
    }
}

void MainWindow::clearStatus()
{
    if (m_statusBar) {
        m_statusBar->showMessage("Ready");
    }
}

void MainWindow::saveSettings()
{
    if (settings) {
        settings->setValue("windowGeometry", saveGeometry());
        settings->setValue("windowState", saveState());
    }
}

void MainWindow::loadSettings()
{
    if (settings) {
        restoreGeometry(settings->value("windowGeometry").toByteArray());
        restoreState(settings->value("windowState").toByteArray());
    }
}

void MainWindow::onImageClicked(const QString &imagePath)
{
    updateStatus("Loading full image...");

    QPixmap fullImage(imagePath);
    if (!fullImage.isNull()) {
        // Make sure imageLabel is properly set up
        imageLabel->setScaledContents(true);

        // Scale to fit the right panel while maintaining aspect ratio
        QPixmap scaledImage = fullImage.scaled(imageLabel->size(),
                                               Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation);
        imageLabel->setPixmap(scaledImage);
        imageLabel->setText(""); // Clear any text

        QFileInfo fileInfo(imagePath);
        updateStatus(QString("Viewing: %1 (%2x%3)")
                         .arg(fileInfo.fileName())
                         .arg(fullImage.width())
                         .arg(fullImage.height()));
    } else {
        imageLabel->setText("Could not load image");
        updateStatus("Could not load image: " + imagePath);
    }
}

// Empty implementations
void MainWindow::loadNextThumbnail() {}
void MainWindow::loadSubfolders(QTreeWidgetItem *parentItem, const QString &path) {}
void MainWindow::loadImagesInGrid(const QString &folderPath) {}
