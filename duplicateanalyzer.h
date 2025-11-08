#ifndef DUPLICATEANALYZER_H
#define DUPLICATEANALYZER_H

#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QSplitter>
#include <QHeaderView>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QFileInfo>

class ProjectManager;
class FolderManager;

/**
 * @brief Analyzer for detecting duplicate folders with various criteria
 *
 * Provides comprehensive folder duplicate detection including:
 * - Quick comparison (file size + image dimensions)
 * - Deep comparison (file size + image dimensions + partial hash)
 * - Multiple duplicate types detection
 * - IDE-style issue reporting with detailed descriptions
 */
class DuplicateAnalyzer : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Comparison mode for analysis
     */
    enum class ComparisonMode {
        Quick,      ///< Fast: File size + image dimensions only
        Deep        ///< Thorough: File size + image dimensions + partial hash
    };

    /**
     * @brief Types of duplicate folder issues
     */
    enum class DuplicateType {
        ExactComplete,      ///< Exact duplicate including all files and subfolders
        ExactFilesOnly,     ///< Exact duplicate of files only (ignoring folder structure)
        PartialDuplicate    ///< 90%+ file overlap
    };

    /**
     * @brief Duplicate folder issue information
     */
    struct DuplicateIssue {
        DuplicateType type;           ///< Type of duplicate detected
        QString primaryFolder;        ///< Primary folder path
        QString duplicateFolder;      ///< Duplicate folder path
        double similarity;            ///< Similarity percentage (0.0 - 1.0)
        int totalFiles;              ///< Total files in comparison
        int duplicateFiles;          ///< Number of duplicate files
        qint64 wastedSpace;          ///< Wasted disk space in bytes
        QString description;         ///< Human-readable issue description
        QString severity;            ///< Issue severity level
    };

    explicit DuplicateAnalyzer(ProjectManager *projectManager,
                               FolderManager *folderManager,
                               QWidget *parent = nullptr);

    /**
     * @brief Start analyzing folders for duplicates
     * @param mode Comparison mode (Quick or Deep)
     */
    void startAnalysis(ComparisonMode mode);

    /**
     * @brief Clear all analysis results
     */
    void clearResults();

    /**
     * @brief Get current analysis results
     * @return List of duplicate issues found
     */
    const QList<DuplicateIssue>& getResults() const { return m_duplicateIssues; }

    /**
     * @brief Get current comparison mode
     * @return Current comparison mode
     */
    ComparisonMode currentMode() const { return m_currentMode; }

signals:
    /**
     * @brief Emitted when analysis starts
     * @param totalFolders Total number of folders to analyze
     * @param mode Analysis mode being used
     */
    void analysisStarted(int totalFolders, ComparisonMode mode);

    /**
     * @brief Emitted during analysis progress
     * @param current Current progress
     * @param total Total items to process
     * @param currentFolder Currently processing folder
     */
    void analysisProgress(int current, int total, const QString &currentFolder);

    /**
     * @brief Emitted when analysis completes
     * @param issuesFound Number of duplicate issues found
     * @param mode Analysis mode that was used
     */
    void analysisCompleted(int issuesFound, ComparisonMode mode);

    /**
     * @brief Emitted when user wants to show folder in tree
     * @param folderPath Path to folder to highlight
     */
    void showFolderInTree(const QString &folderPath);

private slots:
    /**
     * @brief Handle issue selection in the tree
     */
    void onIssueSelected();

    /**
     * @brief Show primary folder in explorer tree
     */
    void showPrimaryFolder();

    /**
     * @brief Show duplicate folder in explorer tree
     */
    void showDuplicateFolder();

    /**
     * @brief Open primary folder in Windows Explorer
     */
    void openPrimaryInExplorer();

    /**
     * @brief Open duplicate folder in Windows Explorer
     */
    void openDuplicateInExplorer();

    /**
     * @brief Refresh the analysis with current mode
     */
    void refreshAnalysis();

private:
    // === UI Setup ===
    void setupUI();
    void createIssuesTree();
    void createDetailsPanel();
    void createActionButtons();
    void createProgressSection();

    // === Analysis Core ===
    void performAnalysis();
    void analyzeFolderPairs();
    void compareFolders(const QString &folder1, const QString &folder2);

    // === Folder Content Analysis ===
    
    /**
     * @brief File information for comparison
     */
    struct FileInfo {
        qint64 fileSize;           ///< File size in bytes
        int imageWidth;            ///< Image width in pixels
        int imageHeight;           ///< Image height in pixels
        QString partialHash;       ///< Partial hash (first 16KB + last 16KB) - only for deep mode
    };

    /**
     * @brief Folder content structure
     */
    struct FolderContent {
        QStringList allFiles;                      ///< All files in folder (relative paths)
        QStringList allSubfolders;                 ///< All subfolders (relative paths)
        QHash<QString, FileInfo> fileInfo;         ///< File path -> FileInfo mapping
        qint64 totalSize;                          ///< Total size in bytes
    };

    FolderContent analyzeFolderContent(const QString &folderPath);
    void scanFolderRecursive(const QString &folderPath,
                             const QString &basePath,
                             FolderContent &content);
    
    FileInfo analyzeFile(const QString &filePath);
    QSize readImageDimensions(const QString &filePath);
    QString calculatePartialHash(const QString &filePath);
    
    int countFilesInFolder(const QString &folderPath);
    void updateFileProgress();

    // === Duplicate Detection Methods ===
    bool isExactCompleteDuplicate(const FolderContent &folder1,
                                  const FolderContent &folder2);
    bool isExactFilesOnlyDuplicate(const FolderContent &folder1,
                                   const FolderContent &folder2);
    double calculateFileSimilarity(const FolderContent &folder1,
                                   const FolderContent &folder2);
    
    bool areFilesIdentical(const FileInfo &file1, const FileInfo &file2);

    // === Results Management ===
    void addDuplicateIssue(const DuplicateIssue &issue);
    void updateIssuesTree();
    void updateDetailsPanel();
    QString formatIssueDescription(const DuplicateIssue &issue);
    QString formatFileSize(qint64 bytes);
    QIcon getSeverityIcon(const QString &severity);
    QString getTypeDisplayName(DuplicateType type);
    QString getTypeDescription(DuplicateType type);
    QString getModeName(ComparisonMode mode);

    // === Cache Management ===
    void saveFolderContentCache();
    void loadFolderContentCache();
    QString getCacheKey(const QString &folderPath) const;
    bool isFolderContentCacheValid(const QString &folderPath, const FolderContent &content) const;
    void resetAnalysisState();

    // === Utility Methods ===
    QStringList getProjectFolders();
    void collectSubfoldersRecursive(const QString &parentPath, QStringList &folderList);
    void openFolderInExplorer(const QString &folderPath);
    QString getRelativePath(const QString &fullPath, const QString &basePath);
    QTreeWidgetItem* getCurrentIssueItem();

    // === UI Components ===
    QVBoxLayout *m_mainLayout;
    QSplitter *m_splitter;

    // Issues list
    QTreeWidget *m_issuesTree;
    QLabel *m_issuesCountLabel;

    // Details panel
    QWidget *m_detailsPanel;
    QLabel *m_detailsTitle;
    QLabel *m_primaryFolderLabel;
    QLabel *m_duplicateFolderLabel;
    QLabel *m_similarityLabel;
    QLabel *m_filesCountLabel;
    QLabel *m_wastedSpaceLabel;
    QLabel *m_severityLabel;
    QLabel *m_modeLabel;

    // Action buttons
    QPushButton *m_showPrimaryButton;
    QPushButton *m_showDuplicateButton;
    QPushButton *m_openPrimaryButton;
    QPushButton *m_openDuplicateButton;
    QPushButton *m_refreshButton;

    // Progress section
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;

    // === Data Members ===
    ProjectManager *m_projectManager;
    FolderManager *m_folderManager;
    QList<DuplicateIssue> m_duplicateIssues;
    QHash<QString, FolderContent> m_folderContentCache;
    ComparisonMode m_currentMode;

    // === Analysis progress tracking ===
    int m_totalFilesToAnalyze;
    int m_filesAnalyzed;
    int m_totalFoldersToScan;
    int m_foldersScanned;
    bool m_analysisRunning;

    // === Constants ===
    static constexpr double PARTIAL_DUPLICATE_THRESHOLD = 0.90; // 90%
    static constexpr int PROGRESS_UPDATE_INTERVAL = 5;
    static constexpr qint64 PARTIAL_HASH_SIZE = 16384; // 16 KB
};

#endif // DUPLICATEANALYZER_H
