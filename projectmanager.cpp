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

// === Constants ===
namespace {
const QString DB_CONNECTION_NAME = "project_db";
const QString DB_FILENAME = "catalog.db";
const QString PROJECT_FILENAME = "project.json";
const QString PROJECT_VERSION = "1.0";

// Database table names
const QString TABLE_FOLDERS = "project_folders";
const QString TABLE_IMAGES = "images";

// Image status values
const QString STATUS_OK = "ok";
const QString STATUS_MISSING = "missing";
const QString STATUS_MODIFIED = "modified";
const QString STATUS_CONFLICT = "conflict";

// Default values
const int DEFAULT_RATING = 0;
const QString DEFAULT_USER_STATUS = "";
const QString DEFAULT_TAGS = "";
}

// === Constructor & Destructor ===

ProjectManager::ProjectManager(QObject *parent)
    : QObject(parent)
{
    initializeSupportedExtensions();
}

ProjectManager::~ProjectManager()
{
    closeProject();
}

// === Project Operations ===

bool ProjectManager::createProject(const QString &projectPath, const QString &projectName)
{
    closeProject();

    if (!createProjectDirectory(projectPath)) {
        return false;
    }

    m_projectPath = projectPath;
    m_projectName = projectName;

    if (!initializeDatabase()) {
        closeProject();
        return false;
    }

    if (!createTables()) {
        closeProject();
        return false;
    }

    if (!saveProjectMetadata()) {
        closeProject();
        return false;
    }

    emit projectOpened(projectName);
    return true;
}

bool ProjectManager::openProject(const QString &projectPath)
{
    closeProject();

    if (!validateProjectDirectory(projectPath)) {
        return false;
    }

    if (!loadProjectMetadata(projectPath)) {
        return false;
    }

    m_projectPath = projectPath;

    if (!initializeDatabase()) {
        return false;
    }

    migrateDatabase();

    emit projectOpened(m_projectName);
    return true;
}

bool ProjectManager::closeProject()
{
    if (m_database.isOpen()) {
        m_database.close();
        QSqlDatabase::removeDatabase(DB_CONNECTION_NAME);
        emit projectClosed();
    }

    resetProjectState();
    return true;
}

void ProjectManager::saveProject()
{
    // Project is automatically saved to database
    // This could trigger additional backup operations in the future
}

// === Folder Management ===

void ProjectManager::addFolder(const QString &folderPath)
{
    if (!m_database.isOpen() || folderPath.isEmpty()) {
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QString("INSERT OR IGNORE INTO %1 (folder_path) VALUES (?)").arg(TABLE_FOLDERS));
    query.addBindValue(folderPath);

    if (!query.exec()) {
        qWarning() << "Failed to add folder:" << query.lastError().text();
    }
}

void ProjectManager::removeFolder(const QString &folderPath)
{
    if (!m_database.isOpen()) {
        return;
    }

    QSqlQuery query(m_database);

    // Remove folder from project folders
    query.prepare(QString("DELETE FROM %1 WHERE folder_path = ?").arg(TABLE_FOLDERS));
    query.addBindValue(folderPath);
    query.exec();

    // Mark images in this folder as missing
    query.prepare(QString("UPDATE %1 SET status = ? WHERE file_path LIKE ?").arg(TABLE_IMAGES));
    query.addBindValue(STATUS_MISSING);
    query.addBindValue(folderPath + "%");
    query.exec();
}

QStringList ProjectManager::getProjectFolders() const
{
    QStringList folders;
    if (!m_database.isOpen()) {
        return folders;
    }

    QSqlQuery query(QString("SELECT folder_path FROM %1 ORDER BY date_added").arg(TABLE_FOLDERS), m_database);
    while (query.next()) {
        folders.append(query.value(0).toString());
    }

    return folders;
}

// === Image Operations ===

QList<ProjectManager::ImageRecord> ProjectManager::getImagesInFolder(const QString &folderPath) const
{
    QList<ImageRecord> images;
    if (!m_database.isOpen()) {
        return images;
    }

    QSqlQuery query(m_database);
    query.prepare(QString("SELECT * FROM %1 WHERE file_path LIKE ? ORDER BY file_name").arg(TABLE_IMAGES));
    query.addBindValue(folderPath + "%");
    query.exec();

    while (query.next()) {
        images.append(createImageRecordFromQuery(query));
    }

    return images;
}

QList<ProjectManager::ImageRecord> ProjectManager::getAllImages() const
{
    QList<ImageRecord> images;
    if (!m_database.isOpen()) {
        return images;
    }

    QSqlQuery query(QString("SELECT * FROM %1 ORDER BY file_name").arg(TABLE_IMAGES), m_database);
    while (query.next()) {
        images.append(createImageRecordFromQuery(query));
    }

    return images;
}

ProjectManager::ImageRecord ProjectManager::getImageRecord(const QString &filePath) const
{
    ImageRecord record = {};
    if (!m_database.isOpen()) {
        return record;
    }

    QSqlQuery query(m_database);
    query.prepare(QString("SELECT * FROM %1 WHERE file_path = ?").arg(TABLE_IMAGES));
    query.addBindValue(filePath);

    if (query.exec() && query.next()) {
        record = createImageRecordFromQuery(query);
    }

    return record;
}

void ProjectManager::updateImageStatus(const QString &filePath, const QString &status)
{
    if (!m_database.isOpen()) {
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QString("UPDATE %1 SET status = ? WHERE file_path = ?").arg(TABLE_IMAGES));
    query.addBindValue(status);
    query.addBindValue(filePath);

    if (query.exec()) {
        emit imageStatusChanged(filePath, status);
    } else {
        qWarning() << "Failed to update image status:" << query.lastError().text();
    }
}

// === Synchronization ===

ProjectManager::SyncResult ProjectManager::synchronizeProject()
{
    if (!m_database.isOpen()) {
        return SyncResult();
    }

    emit syncStarted();

    SyncResult result = performSynchronization();

    emit syncCompleted(result);
    return result;
}

int ProjectManager::getMissingFileCount() const
{
    if (!m_database.isOpen()) {
        return 0;
    }

    QSqlQuery query(QString("SELECT COUNT(*) FROM %1 WHERE status = ?").arg(TABLE_IMAGES), m_database);
    query.addBindValue(STATUS_MISSING);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int ProjectManager::getTotalImageCount() const
{
    if (!m_database.isOpen()) {
        return 0;
    }

    QSqlQuery query(QString("SELECT COUNT(*) FROM %1").arg(TABLE_IMAGES), m_database);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

// === Private Methods - Database Operations ===

bool ProjectManager::initializeDatabase()
{
    const QString dbPath = m_projectPath + "/" + DB_FILENAME;
    m_database = QSqlDatabase::addDatabase("QSQLITE", DB_CONNECTION_NAME);
    m_database.setDatabaseName(dbPath);

    if (!m_database.open()) {
        qWarning() << "Failed to open database:" << m_database.lastError().text();
        return false;
    }

    return true;
}

bool ProjectManager::createTables()
{
    return createProjectFoldersTable() && createImagesTable() && createIndices();
}

void ProjectManager::migrateDatabase()
{
    // Future: Handle database schema migrations
    // Check version and apply necessary migrations
}

// === Private Methods - File Operations ===

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

void ProjectManager::scanFolder(const QString &folderPath, QStringList &foundFiles) const
{
    const QDir dir(folderPath);
    if (!dir.exists()) {
        return;
    }

    // Scan current directory for images
    const QStringList files = dir.entryList(m_supportedExtensions, QDir::Files);
    for (const QString &file : files) {
        foundFiles.append(dir.absoluteFilePath(file));
    }

    // Recursively scan subdirectories
    const QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &subDir : subDirs) {
        scanFolder(dir.absoluteFilePath(subDir), foundFiles);
    }
}

ProjectManager::ImageRecord ProjectManager::createImageRecord(const QString &filePath) const
{
    ImageRecord record = {};
    record.filePath = filePath;

    const QFileInfo fileInfo(filePath);
    record.fileName = fileInfo.fileName();
    record.fileSize = fileInfo.size();
    record.dateModified = fileInfo.lastModified();
    record.dateImported = QDateTime::currentDateTime();
    record.fileHash = calculateFileHash(filePath);
    record.status = STATUS_OK;
    record.rating = DEFAULT_RATING;
    record.userStatus = DEFAULT_USER_STATUS;
    record.tags = DEFAULT_TAGS;

    // Get image dimensions
    const QImageReader reader(filePath);
    if (reader.canRead()) {
        const QSize size = reader.size();
        record.width = size.width();
        record.height = size.height();
    }

    return record;
}

void ProjectManager::updateImageRecord(const ImageRecord &record)
{
    if (!m_database.isOpen()) {
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QString(
                      "INSERT OR REPLACE INTO %1 "
                      "(file_path, file_name, file_hash, file_size, date_modified, "
                      "date_imported, width, height, status, user_status, rating, tags) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
                      ).arg(TABLE_IMAGES));

    bindImageRecordToQuery(query, record);

    if (!query.exec()) {
        qWarning() << "Failed to update image record:" << query.lastError().text();
    }
}

// === Private Methods - Synchronization Operations ===

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
        query.prepare(QString("SELECT id FROM %1 WHERE file_path = ?").arg(TABLE_IMAGES));
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

    QSqlQuery query(QString("SELECT file_path FROM %1 WHERE status != ?").arg(TABLE_IMAGES), m_database);
    query.addBindValue(STATUS_MISSING);

    while (query.next()) {
        const QString filePath = query.value(0).toString();
        if (!QFile::exists(filePath)) {
            missingFiles.append(filePath);
        }
    }

    return missingFiles;
}

QStringList ProjectManager::findModifiedFiles() const
{
    QStringList modifiedFiles;
    if (!m_database.isOpen()) {
        return modifiedFiles;
    }

    QSqlQuery query(QString("SELECT file_path, file_hash, file_size, date_modified FROM %1 WHERE status = ?").arg(TABLE_IMAGES), m_database);
    query.addBindValue(STATUS_OK);

    while (query.next()) {
        const QString filePath = query.value(0).toString();
        const QString storedHash = query.value(1).toString();
        const qint64 storedSize = query.value(2).toLongLong();
        const QDateTime storedDate = query.value(3).toDateTime();

        if (QFile::exists(filePath)) {
            const QFileInfo currentFile(filePath);

            // Quick check: if size or date changed, it's likely modified
            if (currentFile.size() != storedSize ||
                currentFile.lastModified() != storedDate) {

                // Verify with hash (slower but accurate)
                const QString currentHash = calculateFileHash(filePath);
                if (!currentHash.isEmpty() && currentHash != storedHash) {
                    modifiedFiles.append(filePath);
                }
            }
        }
    }

    return modifiedFiles;
}

QList<QPair<QString, QString>> ProjectManager::detectMovedFiles(const QStringList &missing, const QStringList &newFiles) const
{
    QList<QPair<QString, QString>> movedFiles;

    for (const QString &missingFile : missing) {
        // Get hash of missing file from database
        QSqlQuery query(m_database);
        query.prepare(QString("SELECT file_hash, file_size, file_name FROM %1 WHERE file_path = ?").arg(TABLE_IMAGES));
        query.addBindValue(missingFile);
        query.exec();

        if (query.next()) {
            const QString missingHash = query.value(0).toString();
            const qint64 missingSize = query.value(1).toLongLong();
            const QString missingName = query.value(2).toString();

            // Look for matching file in new files
            for (const QString &newFile : newFiles) {
                const QString newHash = calculateFileHash(newFile);
                const QFileInfo newFileInfo(newFile);

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

ProjectManager::SyncResult ProjectManager::performSynchronization()
{
    SyncResult result;

    // Scan all project folders
    QStringList allFiles;
    const QStringList projectFolders = getProjectFolders();

    for (int i = 0; i < projectFolders.size(); ++i) {
        const QString &folderPath = projectFolders.at(i);
        emit syncProgress(i, projectFolders.size(), folderPath);
        scanFolder(folderPath, allFiles);
    }

    result.totalScanned = allFiles.size();

    // Find changes
    result.newFiles = findNewFiles();
    result.missingFiles = findMissingFiles();
    result.modifiedFiles = findModifiedFiles();
    result.movedFiles = detectMovedFiles(result.missingFiles, result.newFiles);

    // Apply changes
    processNewFiles(result.newFiles, result.movedFiles);
    processMissingFiles(result.missingFiles, result.movedFiles);
    processModifiedFiles(result.modifiedFiles);
    processMovedFiles(result.movedFiles);

    return result;
}

// === Private Helper Methods ===

void ProjectManager::initializeSupportedExtensions()
{
    m_supportedExtensions << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp"
                          << "*.gif" << "*.tiff" << "*.tif" << "*.webp"
                          << "*.raw" << "*.cr2" << "*.nef" << "*.arw";
}

void ProjectManager::resetProjectState()
{
    m_projectPath.clear();
    m_projectName.clear();
}

bool ProjectManager::createProjectDirectory(const QString &projectPath)
{
    QDir projectDir;
    if (!projectDir.mkpath(projectPath)) {
        qWarning() << "Failed to create project directory:" << projectPath;
        return false;
    }
    return true;
}

bool ProjectManager::validateProjectDirectory(const QString &projectPath)
{
    const QString dbPath = projectPath + "/" + DB_FILENAME;
    const QString projectFile = projectPath + "/" + PROJECT_FILENAME;

    if (!QFile::exists(dbPath) || !QFile::exists(projectFile)) {
        qWarning() << "Project files not found in:" << projectPath;
        return false;
    }
    return true;
}

bool ProjectManager::loadProjectMetadata(const QString &projectPath)
{
    const QString projectFile = projectPath + "/" + PROJECT_FILENAME;
    QFile file(projectFile);

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject obj = doc.object();
    m_projectName = obj["name"].toString();

    return !m_projectName.isEmpty();
}

bool ProjectManager::saveProjectMetadata()
{
    QJsonObject projectInfo;
    projectInfo["name"] = m_projectName;
    projectInfo["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    projectInfo["version"] = PROJECT_VERSION;

    const QJsonDocument doc(projectInfo);
    QFile projectFile(m_projectPath + "/" + PROJECT_FILENAME);

    if (!projectFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    projectFile.write(doc.toJson());
    return true;
}

bool ProjectManager::createProjectFoldersTable()
{
    QSqlQuery query(m_database);
    return query.exec(QString(
                          "CREATE TABLE %1 ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "folder_path TEXT UNIQUE NOT NULL,"
                          "date_added DATETIME DEFAULT CURRENT_TIMESTAMP"
                          ")").arg(TABLE_FOLDERS));
}

bool ProjectManager::createImagesTable()
{
    QSqlQuery query(m_database);
    return query.exec(QString(
                          "CREATE TABLE %1 ("
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
                          ")").arg(TABLE_IMAGES));
}

bool ProjectManager::createIndices()
{
    QSqlQuery query(m_database);

    bool success = true;
    success &= query.exec(QString("CREATE INDEX idx_images_path ON %1(file_path)").arg(TABLE_IMAGES));
    success &= query.exec(QString("CREATE INDEX idx_images_hash ON %1(file_hash)").arg(TABLE_IMAGES));
    success &= query.exec(QString("CREATE INDEX idx_images_status ON %1(status)").arg(TABLE_IMAGES));

    return success;
}

ProjectManager::ImageRecord ProjectManager::createImageRecordFromQuery(const QSqlQuery &query) const
{
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
    return record;
}

void ProjectManager::bindImageRecordToQuery(QSqlQuery &query, const ImageRecord &record) const
{
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
}

void ProjectManager::processNewFiles(const QStringList &newFiles, const QList<QPair<QString, QString>> &movedFiles)
{
    for (const QString &newFile : newFiles) {
        // Skip if this file is part of a move operation
        bool isMovedFile = false;
        for (const auto &move : movedFiles) {
            if (move.second == newFile) {
                isMovedFile = true;
                break;
            }
        }

        if (!isMovedFile) {
            const ImageRecord record = createImageRecord(newFile);
            updateImageRecord(record);
        }
    }
}

void ProjectManager::processMissingFiles(const QStringList &missingFiles, const QList<QPair<QString, QString>> &movedFiles)
{
    for (const QString &missingFile : missingFiles) {
        // Skip if this file is part of a move operation
        bool isMovedFile = false;
        for (const auto &move : movedFiles) {
            if (move.first == missingFile) {
                isMovedFile = true;
                break;
            }
        }

        if (!isMovedFile) {
            updateImageStatus(missingFile, STATUS_MISSING);
        }
    }
}

void ProjectManager::processModifiedFiles(const QStringList &modifiedFiles)
{
    for (const QString &modifiedFile : modifiedFiles) {
        const ImageRecord record = createImageRecord(modifiedFile);
        updateImageRecord(record);
    }
}

void ProjectManager::processMovedFiles(const QList<QPair<QString, QString>> &movedFiles)
{
    for (const auto &move : movedFiles) {
        QSqlQuery query(m_database);
        query.prepare(QString("UPDATE %1 SET file_path = ?, status = ? WHERE file_path = ?").arg(TABLE_IMAGES));
        query.addBindValue(move.second);  // new path
        query.addBindValue(STATUS_OK);
        query.addBindValue(move.first);   // old path
        query.exec();
    }
}
