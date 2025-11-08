#ifndef DUPLICATEDIALOG_H
#define DUPLICATEDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include "duplicateanalyzer.h"

class ProjectManager;
class FolderManager;

/**
 * @brief Dialog for duplicate folder analysis and management
 *
 * Provides a modal dialog interface for:
 * - Running duplicate folder analysis (Quick or Deep mode)
 * - Displaying results in an organized manner
 * - Managing duplicate issues
 * - Integration with folder tree navigation
 */
class DuplicateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DuplicateDialog(ProjectManager *projectManager,
                             FolderManager *folderManager,
                             QWidget *parent = nullptr);

signals:
    /**
     * @brief Request to show folder in main application tree
     * @param folderPath Path to folder to show
     */
    void showFolderInTree(const QString &folderPath);

private slots:
    /**
     * @brief Handle analysis start
     * @param totalFolders Total folders to analyze
     * @param mode Analysis mode being used
     */
    void onAnalysisStarted(int totalFolders, DuplicateAnalyzer::ComparisonMode mode);

    /**
     * @brief Handle analysis progress
     * @param current Current progress
     * @param total Total items
     * @param currentFolder Currently processing folder
     */
    void onAnalysisProgress(int current, int total, const QString &currentFolder);

    /**
     * @brief Handle analysis completion
     * @param issuesFound Number of issues found
     * @param mode Analysis mode that was used
     */
    void onAnalysisCompleted(int issuesFound, DuplicateAnalyzer::ComparisonMode mode);

    /**
     * @brief Handle folder tree show requests
     * @param folderPath Path to show in tree
     */
    void onShowFolderInTree(const QString &folderPath);

    /**
     * @brief Close dialog
     */
    void closeDialog();

    /**
     * @brief Start quick analysis
     */
    void startQuickAnalysis();

    /**
     * @brief Start deep analysis
     */
    void startDeepAnalysis();

private:
    /**
     * @brief Setup the user interface
     */
    void setupUI();

    /**
     * @brief Create the header section
     */
    void createHeader();

    /**
     * @brief Create the action buttons
     */
    void createButtons();

    /**
     * @brief Update window title with results count
     * @param issueCount Number of issues found
     * @param mode Analysis mode
     */
    void updateTitle(int issueCount, DuplicateAnalyzer::ComparisonMode mode);

    /**
     * @brief Start analysis with specified mode
     * @param mode Comparison mode to use
     */
    void startAnalysis(DuplicateAnalyzer::ComparisonMode mode);

    // === UI Components ===
    QVBoxLayout *m_mainLayout;
    QLabel *m_titleLabel;
    QLabel *m_instructionsLabel;
    DuplicateAnalyzer *m_analyzer;
    QPushButton *m_quickAnalysisButton;
    QPushButton *m_deepAnalysisButton;
    QPushButton *m_closeButton;
    QPushButton *m_helpButton;

    // === Services ===
    ProjectManager *m_projectManager;
    FolderManager *m_folderManager;
};

#endif // DUPLICATEDIALOG_H
