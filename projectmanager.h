#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QStringList>
#include <QDateTime>
#include <QHash>

/**
 * @brief Manages photo projects with database storage and synchronization
 *
 * Provides comprehensive project management including:
 * - Project creation and loading
 * - Database schema management
 * - File synchronization and tracking
 * - Image metadata management
 * - Missing file detection
 */
class ProjectManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Image record structure for database storage
     */
    struct ImageRecord {
        int id;                      ///< Database record ID
        QString filePath;            ///< Full path to image file
        QString fileName;            ///< Image filename only
        QString fileHash;            ///< MD5 hash for duplicate detection
        qint64 fileSize;            ///< File size in bytes
        QDateTime dateModified;      ///< Last modification date
        QDateTime dateImported;      ///< Date added to project
        int width;                   ///< Image width in pixels
        int height;                  ///< Image height in pixels
        QString status;              ///< File status: "ok", "missing", "modified", "conflict"

        // User attributes
        QString userStatus;          ///< User status: "selected", "trash", "ok", etc.
        int rating;                  ///< User rating: 0-5 stars
        QString tags;                ///< Comma-separated tags
    };

    /**
     * @brief Synchronization result structure
     */
    struct SyncResult {
        QStringList newFiles;                              ///< Newly discovered files
        QStringList missingFiles;                          ///< Files that could not be found
        QStringList modifiedFiles;                         ///< Files that have been modified
        QList<QPair<QString, QString>> movedFiles;         ///< Moved files (old path, new path)
        int totalScanned;                                  ///< Total files scanned
    };

    explicit ProjectManager(QObject *parent = nullptr);
    ~ProjectManager();

    // === Project Operations ===

    /**
     * @brief Create a new project
     * @param projectPath Directory path for project
     * @param projectName Name of the project
     * @return True if project created successfully
     */
    bool createProject(const QString &projectPath, const QString &projectName);

    /**
     * @brief Open an existing project
     * @param projectPath Directory path containing project files
     * @return True if project opened successfully
     */
    bool openProject(const QString &projectPath);

    /**
     * @brief Close the current project
     * @return True if project closed successfully
     */
    bool closeProject();

    /**
     * @brief Save current project state
     */
    void saveProject();

    // === Project Information ===

    /**
     * @brief Check if a project is currently open
     * @return True if project is open
     */
    bool hasOpenProject() const { return m_database.isOpen(); }

    /**
     * @brief Get current project directory path
     * @return Path to project directory
     */
    QString currentProjectPath() const { return m_projectPath; }

    /**
     * @brief Get current project name
     * @return Project name
     */
    QString currentProjectName() const { return m_projectName; }

    // === Folder Management ===

    /**
     * @brief Add a folder to the project
     * @param folderPath Path to folder to add
     */
    void addFolder(const QString &folderPath);

    /**
     * @brief Remove a folder from the project
     * @param folderPath Path to folder to remove
     */
    void removeFolder(const QString &folderPath);

    /**
     * @brief Get list of all project folders
     * @return List of folder paths
     */
    QStringList getProjectFolders() const;

    // === Image Operations ===

    /**
     * @brief Get all images in a specific folder
     * @param folderPath Path to folder
     * @return List of image records
     */
    QList<ImageRecord> getImagesInFolder(const QString &folderPath) const;

    /**
     * @brief Get all images in the project
     * @return List of all image records
     */
    QList<ImageRecord> getAllImages() const;

    /**
     * @brief Get specific image record by file path
     * @param filePath Path to image file
     * @return Image record or empty record if not found
     */
    ImageRecord getImageRecord(const QString &filePath) const;

    /**
     * @brief Update image status in database
     * @param filePath Path to image file
     * @param status New status value
     */
    void updateImageStatus(const QString &filePath, const QString &status);

    // === Synchronization ===

    /**
     * @brief Synchronize project with filesystem
     * @return Synchronization results
     */
    SyncResult synchronizeProject();

    /**
     * @brief Get count of missing files
     * @return Number of missing files
     */
    int getMissingFileCount() const;

    /**
     * @brief Get total image count in project
     * @return Total number of images
     */
    int getTotalImageCount() const;

signals:
    /**
     * @brief Emitted when a project is opened
     * @param projectName Name of opened project
     */
    void projectOpened(const QString &projectName);

    /**
     * @brief Emitted when a project is closed
     */
    void projectClosed();

    /**
     * @brief Emitted when synchronization starts
     */
    void syncStarted();

    /**
     * @brief Emitted during synchronization progress
     * @param current Current progress count
     * @param total Total items to process
     * @param currentFile Currently processing file
     */
    void syncProgress(int current, int total, const QString &currentFile);

    /**
     * @brief Emitted when synchronization completes
     * @param result Synchronization results
     */
    void syncCompleted(const SyncResult &result);

    /**
     * @brief Emitted when image status changes
     * @param filePath Path to affected image
     * @param status New status
     */
    void imageStatusChanged(const QString &filePath, const QString &status);

private:
    // === Database Operations ===

    /**
     * @brief Initialize database connection and schema
     * @return True if successful
     */
    bool initializeDatabase();

    /**
     * @brief Create database tables
     * @return True if successful
     */
    bool createTables();

    /**
     * @brief Migrate database schema if needed
     */
    void migrateDatabase();

    // === File Operations ===

    /**
     * @brief Calculate MD5 hash of file
     * @param filePath Path to file
     * @return MD5 hash string
     */
    QString calculateFileHash(const QString &filePath) const;

    /**
     * @brief Recursively scan folder for image files
     * @param folderPath Path to scan
     * @param foundFiles Output list of found files
     */
    void scanFolder(const QString &folderPath, QStringList &foundFiles) const;

    /**
     * @brief Create image record from file
     * @param filePath Path to image file
     * @return Populated image record
     */
    ImageRecord createImageRecord(const QString &filePath) const;

    /**
     * @brief Update image record in database
     * @param record Image record to update
     */
    void updateImageRecord(const ImageRecord &record);

    // === Synchronization Operations ===

    /**
     * @brief Find new files not in database
     * @return List of new file paths
     */
    QStringList findNewFiles() const;

    /**
     * @brief Find missing files that no longer exist
     * @return List of missing file paths
     */
    QStringList findMissingFiles() const;

    /**
     * @brief Find modified files based on hash comparison
     * @return List of modified file paths
     */
    QStringList findModifiedFiles() const;

    /**
     * @brief Detect moved files by comparing hashes
     * @param missing List of missing files
     * @param newFiles List of new files
     * @return List of move pairs (old path, new path)
     */
    QList<QPair<QString, QString>> detectMovedFiles(const QStringList &missing, const QStringList &newFiles) const;

    // === Helper Methods ===

    /**
     * @brief Initialize supported file extensions list
     */
    void initializeSupportedExtensions();

    /**
     * @brief Reset project state variables
     */
    void resetProjectState();

    /**
     * @brief Create project directory structure
     * @param projectPath Path to create
     * @return True if successful
     */
    bool createProjectDirectory(const QString &projectPath);

    /**
     * @brief Validate project directory contains required files
     * @param projectPath Path to validate
     * @return True if valid
     */
    bool validateProjectDirectory(const QString &projectPath);

    /**
     * @brief Load project metadata from JSON file
     * @param projectPath Path to project
     * @return True if successful
     */
    bool loadProjectMetadata(const QString &projectPath);

    /**
     * @brief Save project metadata to JSON file
     * @return True if successful
     */
    bool saveProjectMetadata();

    /**
     * @brief Create project folders table
     * @return True if successful
     */
    bool createProjectFoldersTable();

    /**
     * @brief Create images table
     * @return True if successful
     */
    bool createImagesTable();

    /**
     * @brief Create database indices for performance
     * @return True if successful
     */
    bool createIndices();

    /**
     * @brief Create ImageRecord from database query result
     * @param query Query positioned on record
     * @return Populated ImageRecord
     */
    ImageRecord createImageRecordFromQuery(const QSqlQuery &query) const;

    /**
     * @brief Bind ImageRecord to prepared query
     * @param query Prepared query
     * @param record Record to bind
     */
    void bindImageRecordToQuery(QSqlQuery &query, const ImageRecord &record) const;

    /**
     * @brief Perform the synchronization process
     * @return Synchronization results
     */
    SyncResult performSynchronization();

    /**
     * @brief Process new files found during sync
     * @param newFiles List of new files
     * @param movedFiles List of moved files to exclude
     */
    void processNewFiles(const QStringList &newFiles, const QList<QPair<QString, QString>> &movedFiles);

    /**
     * @brief Process missing files found during sync
     * @param missingFiles List of missing files
     * @param movedFiles List of moved files to exclude
     */
    void processMissingFiles(const QStringList &missingFiles, const QList<QPair<QString, QString>> &movedFiles);

    /**
     * @brief Process modified files found during sync
     * @param modifiedFiles List of modified files
     */
    void processModifiedFiles(const QStringList &modifiedFiles);

    /**
     * @brief Process moved files found during sync
     * @param movedFiles List of moved files
     */
    void processMovedFiles(const QList<QPair<QString, QString>> &movedFiles);

    // === Data Members ===

    QSqlDatabase m_database;              ///< Project database connection
    QString m_projectPath;                ///< Path to project directory
    QString m_projectName;                ///< Project name
    QStringList m_supportedExtensions;    ///< Supported image file extensions
};

#endif // PROJECTMANAGER_H
