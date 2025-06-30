#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QStringList>
#include <QDateTime>
#include <QHash>

class ProjectManager : public QObject
{
    Q_OBJECT

public:
    struct ImageRecord {
        int id;
        QString filePath;
        QString fileName;
        QString fileHash;
        qint64 fileSize;
        QDateTime dateModified;
        QDateTime dateImported;
        int width;
        int height;
        QString status;          // "ok", "missing", "modified", "conflict"

        // Future: User attributes
        QString userStatus;      // "selected", "trash", "ok", etc.
        int rating;              // 0-5 stars
        QString tags;            // Comma-separated tags
    };

    struct SyncResult {
        QStringList newFiles;
        QStringList missingFiles;
        QStringList modifiedFiles;
        QList<QPair<QString, QString>> movedFiles;  // old path, new path
        int totalScanned;
    };

    explicit ProjectManager(QObject *parent = nullptr);
    ~ProjectManager();

    // Project operations
    bool createProject(const QString &projectPath, const QString &projectName);
    bool openProject(const QString &projectPath);
    bool closeProject();
    void saveProject();

    // Project info
    bool hasOpenProject() const { return m_database.isOpen(); }
    QString currentProjectPath() const { return m_projectPath; }
    QString currentProjectName() const { return m_projectName; }

    // Folder management
    void addFolder(const QString &folderPath);
    void removeFolder(const QString &folderPath);
    QStringList getProjectFolders() const;

    // Image operations
    QList<ImageRecord> getImagesInFolder(const QString &folderPath) const;
    QList<ImageRecord> getAllImages() const;
    ImageRecord getImageRecord(const QString &filePath) const;
    void updateImageStatus(const QString &filePath, const QString &status);

    // Synchronization
    SyncResult synchronizeProject();
    int getMissingFileCount() const;
    int getTotalImageCount() const;

signals:
    void projectOpened(const QString &projectName);
    void projectClosed();
    void syncStarted();
    void syncProgress(int current, int total, const QString &currentFile);
    void syncCompleted(const SyncResult &result);
    void imageStatusChanged(const QString &filePath, const QString &status);

private:
    // Database operations
    bool initializeDatabase();
    bool createTables();
    void migrateDatabase();

    // File operations
    QString calculateFileHash(const QString &filePath) const;
    void scanFolder(const QString &folderPath, QStringList &foundFiles) const;
    ImageRecord createImageRecord(const QString &filePath) const;
    void updateImageRecord(const ImageRecord &record);

    // Sync operations
    QStringList findNewFiles() const;
    QStringList findMissingFiles() const;
    QStringList findModifiedFiles() const;
    QList<QPair<QString, QString>> detectMovedFiles(const QStringList &missing, const QStringList &newFiles) const;

    // Data members
    QSqlDatabase m_database;
    QString m_projectPath;
    QString m_projectName;
    QStringList m_supportedExtensions;
};

#endif // PROJECTMANAGER_H
