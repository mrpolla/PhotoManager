#include "mainwindow.h"
#include "imagegridwidget.h"
#include "foldermanager.h"
#include "zoomableimagelabel.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QStatusBar>
#include <QProgressBar>
#include <QTimer>
#include <QSettings>
#include <QMenuBar>
#include <QPixmap>
#include <QFileInfo>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Create thumbnail service first
    thumbnailService = new ThumbnailService(this);

    setupUI();
    loadSettings();
    updateStatus("Photo Manager ready");
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::setupUI()
{
    createMenuBar();
    createMainLayout();
    createStatusBar();
    connectSignals();

    setWindowTitle("Photo Manager");
    resize(1200, 800);
}

void MainWindow::createMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    // File Menu
    QMenu *fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("&Add Folder", QKeySequence::Open, this, &MainWindow::addFolder);
    fileMenu->addSeparator();
    fileMenu->addAction("&Clear Project", this, &MainWindow::clearProject);
    fileMenu->addSeparator();
    fileMenu->addAction("&Exit", QKeySequence::Quit, this, &QWidget::close);

    // View Menu
    QMenu *viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("&Refresh", QKeySequence::Refresh, this, &MainWindow::refreshCurrentFolder);
    viewMenu->addSeparator();
    viewMenu->addAction("&Expand All", this, [this]() { folderManager->expandAll(); });
    viewMenu->addAction("&Collapse All", this, [this]() { folderManager->collapseAll(); });
    viewMenu->addSeparator();
    viewMenu->addAction("&Clear Thumbnail Cache", this, [this]() {
        thumbnailService->clearCache();
        updateStatus("Thumbnail cache cleared");
    });

    // Project Menu
    QMenu *projectMenu = menuBar->addMenu("&Project");
    projectMenu->addAction("&Project Info", this, &MainWindow::showProjectInfo);
}

void MainWindow::createMainLayout()
{
    // Create central widget and main layout
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // Create splitter for resizable panels
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter);

    // Left Panel: Folder Tree with FolderManager
    QWidget *leftPanel = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    addFolderButton = new QPushButton("Add Folder");

    // Create tree widget and folder manager
    QTreeWidget *treeWidget = new QTreeWidget;
    folderManager = new FolderManager(treeWidget, this);

    leftLayout->addWidget(addFolderButton);
    leftLayout->addWidget(treeWidget);

    // Middle Panel: Image Grid
    imageGrid = new ImageGridWidget(thumbnailService);  // Pass thumbnail service

    // Right Panel: Zoomable Image Display
    imageScrollArea = new QScrollArea;
    imageScrollArea->setWidgetResizable(false);
    imageScrollArea->setAlignment(Qt::AlignCenter);
    imageScrollArea->setMinimumSize(300, 300);
    imageScrollArea->setStyleSheet("border: 1px solid gray; background-color: lightgray;");

    imageLabel = new ZoomableImageLabel;
    imageLabel->setText("Select an image");

    imageScrollArea->setWidget(imageLabel);

    // Connect zoom signal for status updates
    connect(imageLabel, &ZoomableImageLabel::zoomChanged,
            [this](double zoom) {
                updateStatus(QString("Zoom: %1%").arg(qRound(zoom * 100)));
            });

    // Add panels to splitter
    splitter->addWidget(leftPanel);
    splitter->addWidget(imageGrid);
    splitter->addWidget(imageScrollArea);  // Use scroll area
    splitter->setSizes({250, 500, 350});
}

void MainWindow::createStatusBar()
{
    m_statusBar = statusBar();
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    m_statusBar->addPermanentWidget(progressBar);
    m_statusBar->showMessage("Ready");

    statusTimer = new QTimer(this);
    statusTimer->setSingleShot(true);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::clearStatus);
}

void MainWindow::connectSignals()
{
    // UI signals
    connect(addFolderButton, &QPushButton::clicked, this, &MainWindow::addFolder);

    // FolderManager signals
    connect(folderManager, &FolderManager::folderSelected, this, &MainWindow::onFolderSelected);
    connect(folderManager, &FolderManager::folderAdded, this, &MainWindow::onFolderAdded);

    // ImageGridWidget signals
    connect(imageGrid, &ImageGridWidget::imageClicked, this, &MainWindow::onImageClicked);
    connect(imageGrid, &ImageGridWidget::loadingStarted, this, &MainWindow::onLoadingStarted);
    connect(imageGrid, &ImageGridWidget::loadingProgress, this, &MainWindow::onLoadingProgress);
    connect(imageGrid, &ImageGridWidget::loadingFinished, this, &MainWindow::onLoadingFinished);
}

void MainWindow::clearProject()
{
    int result = QMessageBox::question(this, "Clear Project",
                                       "Clear all folders from the current project?\n\n"
                                       "This will not delete any files from disk.",
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::No);

    if (result == QMessageBox::Yes) {
        folderManager->clearAllFolders();
        imageGrid->clearImages();
        imageLabel->setPixmap(QPixmap());
        imageLabel->setText("Select an image");
        updateStatus("Project cleared");
    }
}

void MainWindow::showProjectInfo()
{
    QStringList projectFolders = folderManager->getAllFolderPaths();

    QString info = QString("Project contains %1 folder(s):\n\n").arg(projectFolders.size());

    for (const QString &folder : projectFolders) {
        QFileInfo folderInfo(folder);
        info += QString("• %1\n  %2\n\n").arg(folderInfo.baseName()).arg(folder);
    }

    if (projectFolders.isEmpty()) {
        info = "No folders in current project.\n\nUse 'Add Folder' to add folders to your project.";
    }

    QMessageBox::information(this, "Project Information", info);
}

// ===== UI ACTIONS =====

void MainWindow::addFolder()
{
    updateStatus("Opening folder dialog...");
    QString folderPath = QFileDialog::getExistingDirectory(this, "Select Folder");
    if (!folderPath.isEmpty()) {
        folderManager->addFolder(folderPath);
        updateStatus("Folder added successfully");
    } else {
        updateStatus("No folder selected");
    }
}

void MainWindow::refreshCurrentFolder()
{
    QString currentFolder = folderManager->getCurrentFolderPath();
    if (!currentFolder.isEmpty()) {
        onFolderSelected(currentFolder);
    }
}

// ===== FOLDER MANAGEMENT =====

void MainWindow::onFolderSelected(const QString &folderPath)
{
    // Let ImageGridWidget handle the image loading
    imageGrid->loadImagesFromFolder(folderPath);

    // Save last opened folder
    settings->setValue("lastFolder", folderPath);
    updateStatus("Loading folder: " + QDir(folderPath).dirName());
}

void MainWindow::onFolderAdded(const QString &folderPath)
{
    updateStatus("Added folder: " + QDir(folderPath).dirName());
}

// ===== IMAGE HANDLING =====

void MainWindow::onImageClicked(const QString &imagePath)
{
    displayFullImage(imagePath);
}

void MainWindow::displayFullImage(const QString &imagePath)
{
    updateStatus("Loading full image...");

    QPixmap fullImage(imagePath);
    if (!fullImage.isNull()) {
        imageLabel->setImagePixmap(fullImage);

        QFileInfo fileInfo(imagePath);
        updateStatus(QString("Viewing: %1 (%2x%3)")
                         .arg(fileInfo.fileName())
                         .arg(fullImage.width())
                         .arg(fullImage.height()));
    } else {
        imageLabel->setImagePixmap(QPixmap());
        updateStatus("Could not load image: " + imagePath);
    }
}
// ===== LOADING PROGRESS =====

void MainWindow::onLoadingStarted(int totalImages)
{
    progressBar->setMaximum(totalImages);
    progressBar->setValue(0);
    progressBar->setVisible(true);
    updateStatus(QString("Loading %1 images...").arg(totalImages));
}

void MainWindow::onLoadingProgress(int loaded, int total)
{
    progressBar->setValue(loaded);
}

void MainWindow::onLoadingFinished(int totalImages)
{
    progressBar->setVisible(false);
    updateStatus(QString("Loaded %1 images").arg(totalImages));
}

// ===== SETTINGS MANAGEMENT =====

void MainWindow::saveSettings()
{
    if (!settings) return;

    settings->setValue("windowGeometry", saveGeometry());
    settings->setValue("windowState", saveState());

    // Save project folders
    folderManager->saveProject(settings);
}

void MainWindow::loadSettings()
{
    settings = new QSettings("PhotoManager", "PhotoManager", this);

    restoreGeometry(settings->value("windowGeometry").toByteArray());
    restoreState(settings->value("windowState").toByteArray());

    // Load project folders
    folderManager->loadProject(settings);
}

// ===== STATUS MANAGEMENT =====

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
