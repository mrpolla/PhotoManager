#ifndef SYNCDIALOG_H
#define SYNCDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTabWidget>
#include "projectmanager.h"

// Forward declarations
class QVBoxLayout;

/**
 * @brief Dialog for project synchronization with detailed results
 *
 * Provides a comprehensive interface for:
 * - Running project synchronization
 * - Displaying sync results in organized tabs
 * - Managing detected file changes (new, missing, moved)
 * - User interaction with sync results
 */
class SyncDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SyncDialog(ProjectManager *projectManager, QWidget *parent = nullptr);

    /**
     * @brief Display synchronization results
     * @param result Sync results from ProjectManager
     */
    void showSyncResults(const ProjectManager::SyncResult &result);

private slots:
    // === Synchronization Events ===

    /**
     * @brief Handle sync progress updates
     * @param current Current progress count
     * @param total Total items to process
     * @param currentFile Currently processing file
     */
    void onSyncProgress(int current, int total, const QString &currentFile);

    /**
     * @brief Handle sync completion
     * @param result Synchronization results
     */
    void onSyncCompleted(const ProjectManager::SyncResult &result);

    // === User Actions ===

    /**
     * @brief Start synchronization process
     */
    void startSynchronization();

    /**
     * @brief Accept all detected file moves
     */
    void acceptAllMoves();

    /**
     * @brief Reject all detected file moves
     */
    void rejectAllMoves();

    /**
     * @brief Locate a missing file manually
     */
    void locateMissingFile();

    /**
     * @brief Remove selected missing files from project
     */
    void removeMissingFiles();

private:
    // === UI Setup ===

    /**
     * @brief Initialize the user interface
     */
    void setupUI();

    /**
     * @brief Create the main layout and components
     */
    void createMainLayout();

    /**
     * @brief Create the summary section
     * @param mainLayout Parent layout to add to
     */
    void createSummarySection(QVBoxLayout *mainLayout);

    /**
     * @brief Create the progress section
     * @param mainLayout Parent layout to add to
     */
    void createProgressSection(QVBoxLayout *mainLayout);

    /**
     * @brief Create the tabbed results section
     * @param mainLayout Parent layout to add to
     */
    void createTabsSection(QVBoxLayout *mainLayout);

    /**
     * @brief Create the action buttons section
     * @param mainLayout Parent layout to add to
     */
    void createButtonsSection(QVBoxLayout *mainLayout);

    /**
     * @brief Connect all signal-slot relationships
     */
    void connectSignals();

    /**
     * @brief Create the new files tab
     */
    void createNewFilesTab();

    /**
     * @brief Create the missing files tab
     */
    void createMissingFilesTab();

    /**
     * @brief Create the moved files tab
     */
    void createMovedFilesTab();

    /**
     * @brief Populate all result tabs
     * @param result Synchronization results
     */
    void populateAllTabs(const ProjectManager::SyncResult &result);

    /**
     * @brief Set check state for all move items
     * @param state Check state to set
     */
    void setAllMovesCheckState(Qt::CheckState state);

    // === Results Display ===

    /**
     * @brief Populate the new files tab
     * @param newFiles List of new file paths
     */
    void populateNewFilesTab(const QStringList &newFiles);

    /**
     * @brief Populate the missing files tab
     * @param missingFiles List of missing file paths
     */
    void populateMissingFilesTab(const QStringList &missingFiles);

    /**
     * @brief Populate the moved files tab
     * @param movedFiles List of moved file pairs
     */
    void populateMovedFilesTab(const QList<QPair<QString, QString>> &movedFiles);

    /**
     * @brief Update the summary display
     * @param result Synchronization results
     */
    void updateSummary(const ProjectManager::SyncResult &result);

    /**
     * @brief Update tab titles with counts
     * @param result Synchronization results
     */
    void updateTabTitles(const ProjectManager::SyncResult &result);

    /**
     * @brief Switch to the most relevant tab
     * @param result Synchronization results
     */
    void switchToRelevantTab(const ProjectManager::SyncResult &result);

    // === Helper Methods ===

    /**
     * @brief Create a tree widget with standard configuration
     * @param headers Column headers
     * @return Configured QTreeWidget
     */
    QTreeWidget* createStandardTreeWidget(const QStringList &headers);

    /**
     * @brief Add file item to tree widget
     * @param tree Target tree widget
     * @param filePath File path
     * @param iconType Icon type to use
     * @return Created tree item
     */
    QTreeWidgetItem* addFileItem(QTreeWidget *tree, const QString &filePath,
                                 QStyle::StandardPixmap iconType);

    /**
     * @brief Format file size for display
     * @param bytes Size in bytes
     * @return Formatted size string
     */
    QString formatFileSize(qint64 bytes) const;

    /**
     * @brief Get file confidence level string
     * @param oldPath Original file path
     * @param newPath New file path
     * @return Confidence level description
     */
    QString getConfidenceLevel(const QString &oldPath, const QString &newPath) const;

    // === Data Members ===

    ProjectManager *m_projectManager;                ///< Project manager reference

    // === UI Components - Main Layout ===

    QLabel *m_summaryLabel;                         ///< Summary information display
    QProgressBar *m_progressBar;                    ///< Synchronization progress
    QLabel *m_statusLabel;                          ///< Current status message
    QTabWidget *m_tabWidget;                        ///< Results tabs container

    // === UI Components - Tab Contents ===

    QTreeWidget *m_newFilesTree;                    ///< New files display
    QTreeWidget *m_missingFilesTree;                ///< Missing files display
    QTreeWidget *m_movedFilesTree;                  ///< Moved files display

    // === UI Components - Action Buttons ===

    QPushButton *m_syncButton;                      ///< Start synchronization
    QPushButton *m_closeButton;                     ///< Close dialog
    QPushButton *m_acceptMovesButton;               ///< Accept all moves
    QPushButton *m_rejectMovesButton;               ///< Reject all moves
    QPushButton *m_locateButton;                    ///< Locate missing file
    QPushButton *m_removeMissingButton;             ///< Remove missing files

    // === State ===

    ProjectManager::SyncResult m_lastResult;        ///< Last synchronization result
};

#endif // SYNCDIALOG_H
