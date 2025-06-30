#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QProgressBar>
#include <QTimer>
#include "zoomableimagelabel.h"
#include "thumbnailservice.h"

class ImageGridWidget;
class FolderManager;
class ThumbnailService;
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
    void restoreLastFolder();

    // Status Management
    void updateStatus(const QString &message);
    void clearStatus();

    // UI Components
    FolderManager *folderManager;
    ImageGridWidget *imageGrid;
    //QLabel *imageLabel;
    ZoomableImageLabel *imageLabel;  // Change this line
    QScrollArea *imageScrollArea;    // Add this line
    QPushButton *addFolderButton;
    QStatusBar *m_statusBar;
    QProgressBar *progressBar;
    QTimer *statusTimer;

    // Services
    ThumbnailService *thumbnailService;  // Add this line here

    // Data
    QSettings *settings;
};

#endif // MAINWINDOW_H
