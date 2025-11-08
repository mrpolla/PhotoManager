#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QProgressBar>
#include <QTimer>
#include <QScrollArea>

class ImageGridWidget;
class FolderManager;
class ThumbnailService;
class ProjectManager;
class ZoomableImageLabel;
class QSplitter;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // === Project Management ===
    void newProject();
    void openProject();
    void openProject(const QString &projectPath);  // Overload for direct path
    void closeProject();
    void synchronizeProject();
    void showProjectInfo();
    void analyzeDuplicates();

    // === Folder Management ===
    void addFolder();
    void refreshCurrentFolder();

    // === Project Manager Signals ===
    void onProjectOpened(const QString &projectName);
    void onProjectClosed();

    // === FolderManager Signals ===
    void onFolderSelected(const QString &folderPath);
    void onFolderAdded(const QString &folderPath);

    // === ImageGridWidget Signals ===
    void onImageClicked(const QString &imagePath);
    void onLoadingStarted(int totalImages);
    void onLoadingProgress(int loaded, int total);
    void onLoadingFinished(int totalImages);

    // === Duplicate Analysis ===
    void onShowFolderInTree(const QString &folderPath);

private:
    // === UI Setup ===
    void setupUI();
    void createMenuBar();
    void createStatusBar();
    void createMainLayout();
    void connectSignals();

    // === Project Workflow ===
    void showWelcomeScreen();
    void hideWelcomeScreen();
    bool confirmProjectClose();
    void enableProjectActions(bool enabled);

    // === Image Display ===
    void displayFullImage(const QString &imagePath);

    // === Settings ===
    void saveSettings();
    void loadSettings();

    // === Status Management ===
    void updateStatus(const QString &message);
    void clearStatus();

    // === Window Management ===
    void updateWindowTitle();

    // === UI Components ===
    FolderManager *folderManager;
    ImageGridWidget *imageGrid;
    ZoomableImageLabel *imageLabel;
    QScrollArea *imageScrollArea;
    QPushButton *addFolderButton;
    QStatusBar *m_statusBar;
    QProgressBar *progressBar;
    QTimer *statusTimer;

    // === Welcome Screen ===
    QWidget *welcomeWidget;
    QPushButton *newProjectButton;
    QPushButton *openProjectButton;

    // === Services ===
    ThumbnailService *thumbnailService;
    ProjectManager *projectManager;

    // === Data ===
    QSettings *settings;
};

#endif // MAINWINDOW_H
