#include "mainwindow.h"
#include "imagegridwidget.h"
#include "foldermanager.h"
#include "zoomableimagelabel.h"
#include "projectmanager.h"
#include "syncdialog.h"
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
#include <QInputDialog>
#include <QLineEdit>
#include <QSqlDatabase>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Create services
    thumbnailService = new ThumbnailService(this);
    projectManager = new ProjectManager(this);  // Add this line

    setupUI();
    loadSettings();
    updateStatus("Photo Manager ready - Create or open a project to begin");
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
    fileMenu->addAction("&New Project...", this, &MainWindow::newProject);
    fileMenu->addAction("&Open Project...", this, &MainWindow::openProject);
    fileMenu->addSeparator();
    fileMenu->addAction("&Add Folder", QKeySequence::Open, this, &MainWindow::addFolder);
    fileMenu->addSeparator();
    fileMenu->addAction("&Close Project", this, &MainWindow::closeProject);
    fileMenu->addSeparator();
    fileMenu->addAction("&Exit", QKeySequence::Quit, this, &QWidget::close);

    // Project Menu
    QMenu *projectMenu = menuBar->addMenu("&Project");
    projectMenu->addAction("&Synchronize...", QKeySequence::Refresh, this, &MainWindow::synchronizeProject);
    projectMenu->addSeparator();
    projectMenu->addAction("&Project Info", this, &MainWindow::showProjectInfo);

    // View Menu
    QMenu *viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("&Expand All", this, [this]() { folderManager->expandAll(); });
    viewMenu->addAction("&Collapse All", this, [this]() { folderManager->collapseAll(); });
    viewMenu->addSeparator();
    viewMenu->addAction("&Clear Thumbnail Cache", this, [this]() {
        thumbnailService->clearCache();
        updateStatus("Thumbnail cache cleared");
    });
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
    QMessageBox::StandardButton result = QMessageBox::question(this, "Clear Project",
                                                               "Clear all folders from the current project?\n\n"
                                                               "This will not delete any files from disk.",
                                                               QMessageBox::Yes | QMessageBox::No,
                                                               QMessageBox::No);

    if (result == QMessageBox::Yes) {
        folderManager->clearAllFolders();
        imageGrid->clearImages();
        imageLabel->setImagePixmap(QPixmap());  // Use setImagePixmap instead of setPixmap
        updateStatus("Project cleared");
    }
}

void MainWindow::newProject()
{
    QString projectPath = QFileDialog::getExistingDirectory(this, "Select Project Location");
    if (projectPath.isEmpty()) return;

    bool ok;
    QString projectName = QInputDialog::getText(this, "New Project",
                                                "Project Name:",
                                                QLineEdit::Normal,
                                                "My Photo Project", &ok);
    if (!ok || projectName.isEmpty()) return;

    QString fullProjectPath = projectPath + "/" + projectName + ".photoproj";

    if (projectManager->createProject(fullProjectPath, projectName)) {
        updateStatus("Created project: " + projectName);
    } else {
        QMessageBox::warning(this, "Error", "Failed to create project.");
    }
}

void MainWindow::openProject()
{
    QString projectPath = QFileDialog::getExistingDirectory(this, "Open Project");
    if (projectPath.isEmpty()) return;

    if (projectManager->openProject(projectPath)) {
        // Load project folders into folder manager
        QStringList projectFolders = projectManager->getProjectFolders();
        folderManager->clearAllFolders();

        for (const QString &folder : projectFolders) {
            folderManager->addFolder(folder);
        }

        updateStatus("Opened project: " + projectManager->currentProjectName());
    } else {
        QMessageBox::warning(this, "Error", "Failed to open project.");
    }
}

void MainWindow::closeProject()
{
    if (projectManager->hasOpenProject()) {
        projectManager->closeProject();
        folderManager->clearAllFolders();
        imageGrid->clearImages();
        imageLabel->setImagePixmap(QPixmap());
        updateStatus("Project closed");
    }
}

void MainWindow::synchronizeProject()
{
    if (!projectManager->hasOpenProject()) {
        QMessageBox::information(this, "No Project", "Please open a project first.");
        return;
    }

    SyncDialog dialog(projectManager, this);
    dialog.exec();
}

void MainWindow::showProjectInfo()
{
    if (projectManager && projectManager->hasOpenProject()) {
        QString info = QString("Project: %1\n"
                               "Location: %2\n"
                               "Total Images: %3\n"
                               "Missing Files: %4\n"
                               "Folders: %5")
                           .arg(projectManager->currentProjectName())
                           .arg(projectManager->currentProjectPath())
                           .arg(projectManager->getTotalImageCount())
                           .arg(projectManager->getMissingFileCount())
                           .arg(projectManager->getProjectFolders().size());

        QMessageBox::information(this, "Project Information", info);
    } else {
        QStringList projectFolders = folderManager->getAllFolderPaths();

        QString info = QString("Current session contains %1 folder(s):\n\n").arg(projectFolders.size());

        for (const QString &folder : projectFolders) {
            QFileInfo folderInfo(folder);
            info += QString("• %1\n  %2\n\n").arg(folderInfo.baseName()).arg(folder);
        }

        if (projectFolders.isEmpty()) {
            info = "No folders in current session.\n\nUse 'Add Folder' to add folders.";
        }

        QMessageBox::information(this, "Session Information", info);
    }
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
