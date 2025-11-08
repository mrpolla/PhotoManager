#include "mainwindow.h"
#include "imagegridwidget.h"
#include "foldermanager.h"
#include "zoomableimagelabel.h"
#include "projectmanager.h"
#include "syncdialog.h"
#include "duplicatedialog.h"
#include "thumbnailservice.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , welcomeWidget(nullptr)
{
    // Create services
    thumbnailService = new ThumbnailService(this);
    projectManager = new ProjectManager(this);

    setupUI();
    connectSignals();
    loadSettings();
    updateWindowTitle();

    // Show welcome screen if no project is open
    if (!projectManager->hasOpenProject()) {
        showWelcomeScreen();
        updateStatus("Welcome to Photo Manager - Create or open a project to begin");
    } else {
        updateStatus("Project loaded successfully");
    }
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

    setWindowTitle("Photo Manager");
    resize(1200, 800);
}

void MainWindow::createMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    // File Menu
    QMenu *fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("&New Project...", QKeySequence::New, this, &MainWindow::newProject);
    fileMenu->addAction("&Open Project...", QKeySequence::Open, this, [this]() { openProject(); });
    fileMenu->addSeparator();
    fileMenu->addAction("&Add Folder", QKeySequence("Ctrl+F"), this, &MainWindow::addFolder);
    fileMenu->addSeparator();
    fileMenu->addAction("&Close Project", QKeySequence("Ctrl+W"), this, &MainWindow::closeProject);
    fileMenu->addSeparator();
    fileMenu->addAction("&Exit", QKeySequence::Quit, this, &QWidget::close);

    // Project Menu
    QMenu *projectMenu = menuBar->addMenu("&Project");
    projectMenu->addAction("&Synchronize...", QKeySequence::Refresh, this, &MainWindow::synchronizeProject);
    projectMenu->addSeparator();
    projectMenu->addAction("&Analyze Duplicates...", QKeySequence("Ctrl+D"), this, &MainWindow::analyzeDuplicates);
    projectMenu->addSeparator();
    projectMenu->addAction("&Project Info", this, &MainWindow::showProjectInfo);

    // View Menu
    QMenu *viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("&Expand All", this, [this]() {
        if (folderManager) folderManager->expandAll();
    });
    viewMenu->addAction("&Collapse All", this, [this]() {
        if (folderManager) folderManager->collapseAll();
    });
    viewMenu->addSeparator();
    viewMenu->addAction("&Refresh Current Folder", QKeySequence("F5"), this, &MainWindow::refreshCurrentFolder);
    viewMenu->addSeparator();
    viewMenu->addAction("&Clear Thumbnail Cache", this, [this]() {
        if (thumbnailService) {
            thumbnailService->clearCache();
            updateStatus("Thumbnail cache cleared");
        }
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
    addFolderButton->setEnabled(false); // Disabled until project is open

    // Create tree widget and folder manager
    QTreeWidget *treeWidget = new QTreeWidget;
    folderManager = new FolderManager(treeWidget, this);

    leftLayout->addWidget(addFolderButton);
    leftLayout->addWidget(treeWidget);

    // Middle Panel: Image Grid
    imageGrid = new ImageGridWidget(thumbnailService);

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
    splitter->addWidget(imageScrollArea);
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

    // ProjectManager signals
    connect(projectManager, &ProjectManager::projectOpened, this, &MainWindow::onProjectOpened);
    connect(projectManager, &ProjectManager::projectClosed, this, &MainWindow::onProjectClosed);

    // FolderManager signals
    connect(folderManager, &FolderManager::folderSelected, this, &MainWindow::onFolderSelected);
    connect(folderManager, &FolderManager::folderAdded, this, &MainWindow::onFolderAdded);

    // ImageGridWidget signals
    connect(imageGrid, &ImageGridWidget::imageClicked, this, &MainWindow::onImageClicked);
    connect(imageGrid, &ImageGridWidget::loadingStarted, this, &MainWindow::onLoadingStarted);
    connect(imageGrid, &ImageGridWidget::loadingProgress, this, &MainWindow::onLoadingProgress);
    connect(imageGrid, &ImageGridWidget::loadingFinished, this, &MainWindow::onLoadingFinished);
}

// ===== PROJECT MANAGEMENT =====

void MainWindow::newProject()
{
    if (projectManager->hasOpenProject() && !confirmProjectClose()) {
        return;
    }

    QString projectDir = QFileDialog::getExistingDirectory(this, "Select Project Location");
    if (projectDir.isEmpty()) return;

    bool ok;
    QString projectName = QInputDialog::getText(this, "New Project",
                                                "Project Name:",
                                                QLineEdit::Normal,
                                                "My Photo Project", &ok);
    if (!ok || projectName.isEmpty()) return;

    // Create project directory path
    QString projectPath = projectDir + "/" + projectName;

    if (projectManager->createProject(projectPath, projectName)) {
        updateStatus("Created project: " + projectName);
        hideWelcomeScreen();
    } else {
        QMessageBox::warning(this, "Error", "Failed to create project.");
    }
}

void MainWindow::openProject()
{
    if (projectManager->hasOpenProject() && !confirmProjectClose()) {
        return;
    }

    QString projectPath = QFileDialog::getExistingDirectory(this, "Open Project");
    if (projectPath.isEmpty()) return;

    openProject(projectPath);
}

void MainWindow::openProject(const QString &projectPath)
{
    if (projectManager->openProject(projectPath)) {
        hideWelcomeScreen();
        updateStatus("Opened project: " + projectManager->currentProjectName());
    } else {
        QMessageBox::warning(this, "Error",
                             "Failed to open project. Please ensure the folder contains a valid PhotoManager project.");
    }
}

void MainWindow::closeProject()
{
    if (!projectManager->hasOpenProject()) {
        return;
    }

    if (!confirmProjectClose()) {
        return;
    }

    projectManager->closeProject();
    showWelcomeScreen();
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

void MainWindow::analyzeDuplicates()
{
    // Check if we have an open project
    if (!projectManager->hasOpenProject()) {
        QMessageBox::information(this, "No Project",
                                 "Please open a project before analyzing duplicates.");
        return;
    }

    // Check if we have any folders in the project
    QStringList projectFolders = projectManager->getProjectFolders();
    if (projectFolders.isEmpty()) {
        QMessageBox::information(this, "No Folders",
                                 "No folders found in your project.\n\n"
                                 "Add folders to your project using File → Add Folder.");
        return;
    }

    // Note: The duplicate analyzer will recursively scan all subfolders
    // So even with 1 top-level folder containing multiple subfolders, analysis will work
    updateStatus("Opening duplicate analysis...");

    // Create and show the duplicate analysis dialog
    DuplicateDialog *dialog = new DuplicateDialog(projectManager, folderManager, this);

    // Connect the show folder signal to our handler
    connect(dialog, &DuplicateDialog::showFolderInTree,
            this, &MainWindow::onShowFolderInTree);

    // Show the dialog
    dialog->exec();

    // Clean up
    delete dialog;

    updateStatus("Duplicate analysis completed");
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
        QMessageBox::information(this, "No Project",
                                 "No project is currently open.\n\n"
                                 "Create a new project or open an existing one to view project information.");
    }
}

// ===== PROJECT WORKFLOW =====

void MainWindow::showWelcomeScreen()
{
    if (welcomeWidget) {
        return; // Already showing
    }

    welcomeWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(welcomeWidget);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel("Welcome to Photo Manager");
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; margin: 20px;");
    titleLabel->setAlignment(Qt::AlignCenter);

    QLabel *descLabel = new QLabel("Create a new project or open an existing one to get started");
    descLabel->setStyleSheet("font-size: 14px; color: gray; margin: 10px;");
    descLabel->setAlignment(Qt::AlignCenter);

    newProjectButton = new QPushButton("Create New Project");
    newProjectButton->setStyleSheet("QPushButton { font-size: 14px; padding: 10px 20px; }");
    connect(newProjectButton, &QPushButton::clicked, this, [this]() { newProject(); });

    openProjectButton = new QPushButton("Open Existing Project");
    openProjectButton->setStyleSheet("QPushButton { font-size: 14px; padding: 10px 20px; }");
    connect(openProjectButton, &QPushButton::clicked, this, [this]() { openProject(); });

    layout->addWidget(titleLabel);
    layout->addWidget(descLabel);
    layout->addSpacing(20);
    layout->addWidget(newProjectButton);
    layout->addWidget(openProjectButton);

    // Replace the image grid with welcome screen
    QWidget *centralWidget = this->centralWidget();
    QHBoxLayout *mainLayout = qobject_cast<QHBoxLayout*>(centralWidget->layout());
    if (mainLayout) {
        QSplitter *splitter = qobject_cast<QSplitter*>(mainLayout->itemAt(0)->widget());
        if (splitter) {
            splitter->replaceWidget(1, welcomeWidget);
            imageGrid->setVisible(false);
        }
    }

    enableProjectActions(false);
}

void MainWindow::hideWelcomeScreen()
{
    if (!welcomeWidget) {
        return; // Not showing
    }

    // Replace welcome screen with image grid
    QWidget *centralWidget = this->centralWidget();
    QHBoxLayout *mainLayout = qobject_cast<QHBoxLayout*>(centralWidget->layout());
    if (mainLayout) {
        QSplitter *splitter = qobject_cast<QSplitter*>(mainLayout->itemAt(0)->widget());
        if (splitter) {
            splitter->replaceWidget(1, imageGrid);
            imageGrid->setVisible(true);
        }
    }

    welcomeWidget->deleteLater();
    welcomeWidget = nullptr;

    enableProjectActions(true);
}

bool MainWindow::confirmProjectClose()
{
    if (!projectManager->hasOpenProject()) {
        return true;
    }

    QMessageBox::StandardButton result = QMessageBox::question(this, "Close Project",
                                                               "Close the current project?\n\n"
                                                               "Any unsaved changes will be lost.",
                                                               QMessageBox::Yes | QMessageBox::No,
                                                               QMessageBox::No);
    return result == QMessageBox::Yes;
}

void MainWindow::enableProjectActions(bool enabled)
{
    addFolderButton->setEnabled(enabled);

    // Enable/disable menu actions
    QList<QAction*> actions = menuBar()->actions();
    for (QAction *action : actions) {
        if (action->text() == "&Project") {
            action->menu()->setEnabled(enabled);
            break;
        }
    }
}

// ===== PROJECT MANAGER SIGNALS =====

void MainWindow::onProjectOpened(const QString &projectName)
{
    // Load project folders into folder manager
    QStringList projectFolders = projectManager->getProjectFolders();
    folderManager->clearAllFolders();

    for (const QString &folder : projectFolders) {
        folderManager->addFolder(folder);
    }

    updateWindowTitle();
    enableProjectActions(true);
    updateStatus("Project opened: " + projectName);
}

void MainWindow::onProjectClosed()
{
    folderManager->clearAllFolders();
    imageGrid->clearImages();
    imageLabel->setImagePixmap(QPixmap());
    updateWindowTitle();
    enableProjectActions(false);
    updateStatus("Project closed");
}

// ===== FOLDER MANAGEMENT =====

void MainWindow::addFolder()
{
    // Check if we have an open project
    if (!projectManager->hasOpenProject()) {
        QMessageBox::information(this, "No Project",
                                 "Please create or open a project before adding folders.\n\n"
                                 "Use File → New Project or File → Open Project.");
        return;
    }

    updateStatus("Opening folder dialog...");
    QString folderPath = QFileDialog::getExistingDirectory(this, "Select Folder");
    if (!folderPath.isEmpty()) {
        // Add to both project database and folder manager
        projectManager->addFolder(folderPath);
        folderManager->addFolder(folderPath);

        updateStatus("Folder added to project successfully");
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

void MainWindow::onFolderSelected(const QString &folderPath)
{
    // Let ImageGridWidget handle the image loading
    imageGrid->loadImagesFromFolder(folderPath);

    // Save last opened folder
    if (settings) {
        settings->setValue("lastFolder", folderPath);
    }

    updateStatus("Loading folder: " + QDir(folderPath).dirName());
}

void MainWindow::onFolderAdded(const QString &folderPath)
{
    updateStatus("Added folder: " + QDir(folderPath).dirName());
}

void MainWindow::onShowFolderInTree(const QString &folderPath)
{
    if (!folderManager) {
        return;
    }

    // Find the folder in the tree and select it
    QTreeWidget *treeWidget = findChild<QTreeWidget*>();
    if (!treeWidget) {
        return;
    }

    // Try to find and expand to the folder
    QTreeWidgetItemIterator it(treeWidget);
    while (*it) {
        QTreeWidgetItem *item = *it;
        QString itemPath = item->data(0, Qt::UserRole).toString();

        if (itemPath == folderPath) {
            // Found the item - select and expand it
            treeWidget->setCurrentItem(item);
            treeWidget->scrollToItem(item);

            // Expand parent items to make sure it's visible
            QTreeWidgetItem *parent = item->parent();
            while (parent) {
                parent->setExpanded(true);
                parent = parent->parent();
            }

            // Also select the folder for image loading
            onFolderSelected(folderPath);

            updateStatus(QString("Showing folder: %1").arg(QFileInfo(folderPath).fileName()));

            // Bring main window to front
            raise();
            activateWindow();

            return;
        }
        ++it;
    }

    // If we get here, folder wasn't found in tree
    updateStatus("Folder not found in project tree");
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

    // Save current project path instead of legacy folder system
    if (projectManager && projectManager->hasOpenProject()) {
        settings->setValue("lastProjectPath", projectManager->currentProjectPath());
    }
}

void MainWindow::loadSettings()
{
    settings = new QSettings("PhotoManager", "PhotoManager", this);

    restoreGeometry(settings->value("windowGeometry").toByteArray());
    restoreState(settings->value("windowState").toByteArray());

    // Load last opened project instead of legacy folder system
    QString lastProjectPath = settings->value("lastProjectPath").toString();
    if (!lastProjectPath.isEmpty() && QDir(lastProjectPath).exists()) {
        // Delay project loading to ensure UI is ready
        QTimer::singleShot(100, [this, lastProjectPath]() {
            openProject(lastProjectPath);
        });
    }
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

void MainWindow::updateWindowTitle()
{
    QString title = "Photo Manager";

    if (projectManager && projectManager->hasOpenProject()) {
        title += " - " + projectManager->currentProjectName();
    }

    setWindowTitle(title);
}
