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
    // UI Actions
    void addFolder();
    void refreshCurrentFolder();
    void clearProject();
    void newProject();
    void openProject();
    void closeProject();
    void synchronizeProject();
    void showProjectInfo();

    // FolderManager signals
    void onFolderSelected(const QString &folderPath);
    void onFolderAdded(const QString &folderPath);

    // ImageGridWidget signals
    void onImageClicked(const QString &imagePath);
    void onLoadingStarted(int totalImages);
    void onLoadingProgress(int loaded, int total);
    void onLoadingFinished(int totalImages);

private:
    // UI Setup
    void setupUI();
    void createMenuBar();
    void createStatusBar();
    void createMainLayout();
    void connectSignals();

    // Image Display
    void displayFullImage(const QString &imagePath);

    // Settings
    void saveSettings();
    void loadSettings();

    // Status Management
    void updateStatus(const QString &message);
    void clearStatus();

    // UI Components
    FolderManager *folderManager;
    ImageGridWidget *imageGrid;
    ZoomableImageLabel *imageLabel;
    QScrollArea *imageScrollArea;
    QPushButton *addFolderButton;
    QStatusBar *m_statusBar;
    QProgressBar *progressBar;
    QTimer *statusTimer;

    // Services
    ThumbnailService *thumbnailService;
    ProjectManager *projectManager;

    // Data
    QSettings *settings;
};

#endif // MAINWINDOW_H
