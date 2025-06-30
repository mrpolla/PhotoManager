#include "projectmanager.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QImageReader>
#include <QDebug>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>

ProjectManager::ProjectManager(QObject *parent)
    : QObject(parent)
{
    // Define supported image extensions
    m_supportedExtensions << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp"
                          << "*.gif" << "*.tiff" << "*.tif" << "*.webp"
                          << "*.raw" << "*.cr2" << "*.nef" << "*.arw";
}

ProjectManager::~ProjectManager()
{
    closeProject();
}

bool ProjectManager::createProject(const QString &projectPath, const QString &projectName)
{
    closeProject();

    // Create project directory
    QDir projectDir;
    if (!projectDir.mkpath(projectPath)) {
        qWarning() << "Failed to create project directory:" << projectPath;
        return false;
    }

    m_projectPath = projectPath;
    m_projectName = projectName;

    // Create database
    QString dbPath = projectPath + "/catalog.db";
    m_database = QSqlDatabase::addDatabase("QSQLITE", "project_db");
    m_database.setDatabaseName(dbPath);

    if (!m_database.open()) {
        qWarning() << "Failed to create database:" << m_database.lastError().text();
        return false;
    }

    if (!createTables()) {
        closeProject();
        return false;
    }

    // Save project metadata
    QJsonObject projectInfo;
    projectInfo["name"] = projectName;
    projectInfo["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    projectInfo["version"] = "1.0";

    QJsonDocument doc(projectInfo);
    QFile projectFile(projectPath + "/project.json");
    if (projectFile.open(QIODevice::WriteOnly)) {
        projectFile.write(doc.toJson());
        projectFile.close();
    }

    emit projectOpened(projectName);
    return true;
}

bool ProjectManager::openProject(const QString &projectPath)
{
    closeProject();

    // Check if project exists
    QString dbPath = projectPath + "/catalog.db";
    QString projectFile = projectPath + "/project.json";

    if (!QFile::exists(dbPath) || !QFile::exists(projectFile)) {
        qWarning() << "Project files not found in:" << projectPath;
        return false;
    }

    // Load project metadata
    QFile file(projectFile);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        m_projectName = obj["name"].toString();
        file.close();
    }

    m_projectPath = projectPath;

    // Open database
    m_database = QSqlDatabase::addDatabase("QSQLITE", "project_db");
    m_database.setDatabaseName(dbPath);

    if (!m_database.open()) {
        qWarning() << "Failed to open database:" << m_database.lastError().text();
        return false;
    }

    // Check if migration is needed
    migrateDatabase();

    emit projectOpened(m_projectName);
    return true;
}

bool ProjectManager::closeProject()
{
    if (m_database.isOpen()) {
        m_database.close();
        QSqlDatabase::removeDatabase("project_db");
        emit projectClosed();
    }

    m_projectPath.clear();
    m_projectName.clear();
    return true;
}

void ProjectManager::saveProject()
{
    // Project is automatically saved to database
    // This could trigger additional backup operations
}

bool ProjectManager::createTables()
{
    QSqlQuery query(m_database);

    // Project folders table
    if (!query.exec(
            "CREATE TABLE project_folders ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "folder_path TEXT UNIQUE NOT NULL,"
            "date_added DATETIME DEFAULT CURRENT_TIMESTAMP"
            ")")) {
        qWarning() << "Failed to create project_folders table:" << query.lastError().text();
        return false;
    }

    // Images table
    if (!query.exec(
            "CREATE TABLE images ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "file_path TEXT UNIQUE NOT NULL,"
            "file_name TEXT NOT NULL,"
            "file_hash TEXT NOT NULL,"
            "file_size INTEGER NOT NULL,"
            "date_modified DATETIME NOT NULL,"
            "date_imported DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "width INTEGER,"
            "height INTEGER,"
            "status TEXT DEFAULT 'ok',"
            "user_status TEXT DEFAULT '',"
            "rating INTEGER DEFAULT 0,"
            "tags TEXT DEFAULT ''"
            ")")) {
        qWarning() << "Failed to create images table:" << query.lastError().text();
        return false;
    }

    // Create indices for performance
    query.exec("CREATE INDEX idx_images_path ON images(file_path)");
    query.exec("CREATE INDEX idx_images_hash ON images(file_hash)");
    query.exec("CREATE INDEX idx_images_status ON images(status)");

    return true;
}

void ProjectManager::migrateDatabase()
{
    // Future: Handle database schema migrations
}

void ProjectManager::addFolder(const QString &folderPath)
{
    if (!m_database.isOpen()) return;

    QSqlQuery query(m_database);
    query.prepare("INSERT OR IGNORE INTO project_folders (folder_path) VALUES (?)");
    query.addBindValue(folderPath);

    if (!query.exec()) {
        qWarning() << "Failed to add folder:" << query.lastError().text();
    }
}

void ProjectManager::removeFolder(const QString &folderPath)
{
    if (!m_database.isOpen()) return;

    QSqlQuery query(m_database);

    // Remove folder
    query.prepare("DELETE FROM project_folders WHERE folder_path = ?");
    query.addBindValue(folderPath);
    query.exec();

    // Mark images in this folder as missing
    query.prepare("UPDATE images SET status = 'missing' WHERE file_path LIKE ?");
    query.addBindValue(folderPath + "%");
    query.exec();
}

QStringList ProjectManager::getProjectFolders() const
{
    QStringList folders;
    if (!m_database.isOpen()) return folders;

    QSqlQuery query("SELECT folder_path FROM project_folders ORDER BY date_added", m_database);
    while (query.next()) {
        folders.append(query.value(0).toString());
    }

    return folders;
}

QString ProjectManager::calculateFileHash(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(file.readAll());
    return hash.result().toHex();
}

ProjectManager::ImageRecord ProjectManager::createImageRecord(const QString &filePath) const
{
    ImageRecord record;
    record.filePath = filePath;

    QFileInfo fileInfo(filePath);
    record.fileName = fileInfo.fileName();
    record.fileSize = fileInfo.size();
    record.dateModified = fileInfo.lastModified();
    record.dateImported = QDateTime::currentDateTime();
    record.fileHash = calculateFileHash(filePath);
    record.status = "ok";
    record.rating = 0;

    // Get image dimensions
    QImageReader reader(filePath);
    if (reader.canRead()) {
        QSize size = reader.size();
        record.width = size.width();
        record.height = size.height();
    }

    return record;
}

ProjectManager::SyncResult ProjectManager::synchronizeProject()
{
    if (!m_database.isOpen()) {
        return SyncResult();
    }

    emit syncStarted();

    SyncResult result;

    // Find all files in project folders
    QStringList allFiles;
    QStringList projectFolders = getProjectFolders();

    int totalFolders = projectFolders.size();
    int currentFolder = 0;

    for (const QString &folderPath : projectFolders) {
        emit syncProgress(currentFolder++, totalFolders, folderPath);
        scanFolder(folderPath, allFiles);
    }

    result.totalScanned = allFiles.size();

    // Find new files
    result.newFiles = findNewFiles();

    // Find missing files
    result.missingFiles = findMissingFiles();

    // Find modified files
    result.modifiedFiles = findModifiedFiles();

    // Detect moved files
    result.movedFiles = detectMovedFiles(result.missingFiles, result.newFiles);

    // Add new files to database
    for (const QString &newFile : result.newFiles) {
        if (!result.movedFiles.contains(qMakePair(QString(), newFile))) {
            ImageRecord record = createImageRecord(newFile);
            updateImageRecord(record);
        }
    }

    // Update moved files
    for (const auto &move : result.movedFiles) {
        QSqlQuery query(m_database);
        query.prepare("UPDATE images SET file_path = ?, status = 'ok' WHERE file_path = ?");
        query.addBindValue(move.second);  // new path
        query.addBindValue(move.first);   // old path
        query.exec();

        // Remove from new/missing lists
        result.newFiles.removeAll(move.second);
        result.missingFiles.removeAll(move.first);
    }

    // Mark missing files
    for (const QString &missingFile : result.missingFiles) {
        QSqlQuery query(m_database);
        query.prepare("UPDATE images SET status = 'missing' WHERE file_path = ?");
        query.addBindValue(missingFile);
        query.exec();
    }

    emit syncCompleted(result);
    return result;
}

QStringList ProjectManager::findModifiedFiles() const
{
    QStringList modifiedFiles;
    if (!m_database.isOpen()) return modifiedFiles;

    QSqlQuery query("SELECT file_path, file_hash, file_size, date_modified FROM images WHERE status = 'ok'", m_database);
    while (query.next()) {
        QString filePath = query.value(0).toString();
        QString storedHash = query.value(1).toString();
        qint64 storedSize = query.value(2).toLongLong();
        QDateTime storedDate = query.value(3).toDateTime();

        if (QFile::exists(filePath)) {
            QFileInfo currentFile(filePath);

            // Quick check: if size or date changed, it's likely modified
            if (currentFile.size() != storedSize ||
                currentFile.lastModified() != storedDate) {

                // Verify with hash (slower but accurate)
                QString currentHash = calculateFileHash(filePath);
                if (!currentHash.isEmpty() && currentHash != storedHash) {
                    modifiedFiles.append(filePath);
                }
            }
        }
    }

    return modifiedFiles;
}

void ProjectManager::scanFolder(const QString &folderPath, QStringList &foundFiles) const
{
    QDir dir(folderPath);
    if (!dir.exists()) return;

    // Scan current directory
    QStringList files = dir.entryList(m_supportedExtensions, QDir::Files);
    for (const QString &file : files) {
        foundFiles.append(dir.absoluteFilePath(file));
    }

    // Scan subdirectories
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &subDir : subDirs) {
        scanFolder(dir.absoluteFilePath(subDir), foundFiles);
    }
}

QStringList ProjectManager::findNewFiles() const
{
    QStringList newFiles;
    QStringList allFiles;

    // Get all files in project folders
    for (const QString &folderPath : getProjectFolders()) {
        scanFolder(folderPath, allFiles);
    }

    // Check which files are not in database
    for (const QString &filePath : allFiles) {
        QSqlQuery query(m_database);
        query.prepare("SELECT id FROM images WHERE file_path = ?");
        query.addBindValue(filePath);
        query.exec();

        if (!query.next()) {
            newFiles.append(filePath);
        }
    }

    return newFiles;
}

QStringList ProjectManager::findMissingFiles() const
{
    QStringList missingFiles;

    QSqlQuery query("SELECT file_path FROM images WHERE status != 'missing'", m_database);
    while (query.next()) {
        QString filePath = query.value(0).toString();
        if (!QFile::exists(filePath)) {
            missingFiles.append(filePath);
        }
    }

    return missingFiles;
}

QList<QPair<QString, QString>> ProjectManager::detectMovedFiles(const QStringList &missing, const QStringList &newFiles) const
{
    QList<QPair<QString, QString>> movedFiles;

    for (const QString &missingFile : missing) {
        // Get hash of missing file from database
        QSqlQuery query(m_database);
        query.prepare("SELECT file_hash, file_size, file_name FROM images WHERE file_path = ?");
        query.addBindValue(missingFile);
        query.exec();

        if (query.next()) {
            QString missingHash = query.value(0).toString();
            qint64 missingSize = query.value(1).toLongLong();
            QString missingName = query.value(2).toString();

            // Look for matching file in new files
            for (const QString &newFile : newFiles) {
                QString newHash = calculateFileHash(newFile);
                QFileInfo newFileInfo(newFile);

                // Perfect match: same hash
                if (!missingHash.isEmpty() && newHash == missingHash) {
                    movedFiles.append(qMakePair(missingFile, newFile));
                    break;
                }

                // Likely match: same name and size
                if (newFileInfo.fileName() == missingName &&
                    newFileInfo.size() == missingSize) {
                    movedFiles.append(qMakePair(missingFile, newFile));
                    break;
                }
            }
        }
    }

    return movedFiles;
}

void ProjectManager::updateImageRecord(const ImageRecord &record)
{
    if (!m_database.isOpen()) return;

    QSqlQuery query(m_database);
    query.prepare(
        "INSERT OR REPLACE INTO images "
        "(file_path, file_name, file_hash, file_size, date_modified, "
        "date_imported, width, height, status, user_status, rating, tags) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );

    query.addBindValue(record.filePath);
    query.addBindValue(record.fileName);
    query.addBindValue(record.fileHash);
    query.addBindValue(record.fileSize);
    query.addBindValue(record.dateModified);
    query.addBindValue(record.dateImported);
    query.addBindValue(record.width);
    query.addBindValue(record.height);
    query.addBindValue(record.status);
    query.addBindValue(record.userStatus);
    query.addBindValue(record.rating);
    query.addBindValue(record.tags);

    if (!query.exec()) {
        qWarning() << "Failed to update image record:" << query.lastError().text();
    }
}

QList<ProjectManager::ImageRecord> ProjectManager::getImagesInFolder(const QString &folderPath) const
{
    QList<ImageRecord> images;
    if (!m_database.isOpen()) return images;

    QSqlQuery query(m_database);
    query.prepare("SELECT * FROM images WHERE file_path LIKE ? ORDER BY file_name");
    query.addBindValue(folderPath + "%");
    query.exec();

    while (query.next()) {
        ImageRecord record;
        record.id = query.value("id").toInt();
        record.filePath = query.value("file_path").toString();
        record.fileName = query.value("file_name").toString();
        record.fileHash = query.value("file_hash").toString();
        record.fileSize = query.value("file_size").toLongLong();
        record.dateModified = query.value("date_modified").toDateTime();
        record.dateImported = query.value("date_imported").toDateTime();
        record.width = query.value("width").toInt();
        record.height = query.value("height").toInt();
        record.status = query.value("status").toString();
        record.userStatus = query.value("user_status").toString();
        record.rating = query.value("rating").toInt();
        record.tags = query.value("tags").toString();

        images.append(record);
    }

    return images;
}

int ProjectManager::getMissingFileCount() const
{
    if (!m_database.isOpen()) return 0;

    QSqlQuery query("SELECT COUNT(*) FROM images WHERE status = 'missing'", m_database);
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int ProjectManager::getTotalImageCount() const
{
    if (!m_database.isOpen()) return 0;

    QSqlQuery query("SELECT COUNT(*) FROM images", m_database);
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}
