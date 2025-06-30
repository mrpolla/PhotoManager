#include "foldermanager.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDir>
#include <QFileInfo>
#include <QStyle>
#include <QSettings>
#include <QMenu>
#include <QAction>
#include <QMessageBox>

FolderManager::FolderManager(QTreeWidget *treeWidget, QObject *parent)
    : QObject(parent), m_treeWidget(treeWidget)
{
    setupTreeWidget();
    addContextMenu();

    // Define supported image extensions
    m_supportedExtensions << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp"
                          << "*.gif" << "*.tiff" << "*.tif" << "*.webp";
}

void FolderManager::setupTreeWidget()
{
    m_treeWidget->setHeaderLabel("Project Folders");
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_treeWidget, &QTreeWidget::itemClicked,
            this, &FolderManager::onItemClicked);

    // Add lazy loading connection
    connect(m_treeWidget, &QTreeWidget::itemExpanded,
            this, &FolderManager::onItemExpanded);
}

void FolderManager::addContextMenu()
{
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            [this](const QPoint &pos) {
                QTreeWidgetItem *item = m_treeWidget->itemAt(pos);
                if (!item) return;

                QMenu contextMenu;
                QAction *removeAction = contextMenu.addAction("Remove Folder");
                connect(removeAction, &QAction::triggered, [this]() {
                    removeSelectedFolder();
                });

                contextMenu.exec(m_treeWidget->mapToGlobal(pos));
            });
}

void FolderManager::addFolder(const QString &folderPath)
{
    if (folderPath.isEmpty() || !QDir(folderPath).exists()) {
        return;
    }

    // Check if folder already exists
    if (folderAlreadyExists(folderPath)) {
        return;
    }

    QFileInfo folderInfo(folderPath);
    QTreeWidgetItem *item = createFolderItem(folderPath, folderInfo.baseName());

    // LAZY LOADING: Only check if it has subfolders, don't load them yet
    if (hasSubfolders(folderPath)) {
        // Add a dummy child to show the expand arrow
        QTreeWidgetItem *dummyChild = new QTreeWidgetItem(item);
        dummyChild->setText(0, "Loading...");
        dummyChild->setData(0, Qt::UserRole, "DUMMY");
    }

    m_treeWidget->addTopLevelItem(item);
    // Don't expand by default anymore - let user expand when needed

    // Add to project folders list
    m_projectFolders.append(folderPath);

    emit folderAdded(folderPath);
}

void FolderManager::onItemExpanded(QTreeWidgetItem *item)
{
    // Check if this item has dummy children that need to be replaced with real ones
    if (item->childCount() == 1 &&
        item->child(0)->data(0, Qt::UserRole).toString() == "DUMMY") {

        // Remove dummy child
        delete item->takeChild(0);

        // Load real subfolders
        loadSubfoldersLazy(item);
    }
}

void FolderManager::loadSubfoldersLazy(QTreeWidgetItem *parentItem)
{
    QString folderPath = parentItem->data(0, Qt::UserRole).toString();
    QDir dir(folderPath);
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString &subDirName : subDirs) {
        QString subDirPath = dir.absoluteFilePath(subDirName);
        QTreeWidgetItem *subItem = createFolderItem(subDirPath, subDirName);

        // Check if this subfolder has its own subfolders
        if (hasSubfolders(subDirPath)) {
            // Add dummy child to show expand arrow
            QTreeWidgetItem *dummyChild = new QTreeWidgetItem(subItem);
            dummyChild->setText(0, "Loading...");
            dummyChild->setData(0, Qt::UserRole, "DUMMY");
        }

        parentItem->addChild(subItem);
    }
}

bool FolderManager::hasSubfolders(const QString &path) const
{
    QDir dir(path);
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    return !subDirs.isEmpty();
}

void FolderManager::removeSelectedFolder()
{
    QTreeWidgetItem *currentItem = m_treeWidget->currentItem();
    if (!currentItem) return;

    QString folderPath = currentItem->data(0, Qt::UserRole).toString();

    // Only allow removal of top-level project folders
    if (currentItem->parent() != nullptr) {
        QMessageBox::information(m_treeWidget, "Cannot Remove",
                                 "You can only remove top-level project folders.\n"
                                 "Select the main folder to remove it from the project.");
        return;
    }

    // Confirm removal
    QFileInfo folderInfo(folderPath);
    int result = QMessageBox::question(m_treeWidget, "Remove Folder",
                                       QString("Remove '%1' from project?\n\n"
                                               "This will not delete the actual folder from disk.")
                                           .arg(folderInfo.baseName()),
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::No);

    if (result == QMessageBox::Yes) {
        // Remove from tree
        m_treeWidget->takeTopLevelItem(m_treeWidget->indexOfTopLevelItem(currentItem));
        delete currentItem;

        // Remove from project folders list
        m_projectFolders.removeAll(folderPath);

        emit folderRemoved(folderPath);
    }
}

void FolderManager::clearAllFolders()
{
    m_treeWidget->clear();
    m_projectFolders.clear();
    emit foldersCleared();
}

QString FolderManager::getCurrentFolderPath() const
{
    QTreeWidgetItem *currentItem = m_treeWidget->currentItem();
    if (currentItem) {
        return currentItem->data(0, Qt::UserRole).toString();
    }
    return QString();
}

QStringList FolderManager::getAllFolderPaths() const
{
    return m_projectFolders;
}

void FolderManager::saveProject(QSettings *settings)
{
    settings->beginGroup("Project");
    settings->setValue("folders", m_projectFolders);
    settings->endGroup();
}

void FolderManager::loadProject(QSettings *settings)
{
    settings->beginGroup("Project");
    QStringList savedFolders = settings->value("folders").toStringList();
    settings->endGroup();

    // Clear current project
    clearAllFolders();

    // Load saved folders
    for (const QString &folderPath : savedFolders) {
        if (QDir(folderPath).exists()) {
            addFolder(folderPath);
        }
    }
}

void FolderManager::expandAll()
{
    m_treeWidget->expandAll();
}

void FolderManager::collapseAll()
{
    m_treeWidget->collapseAll();
}

QStringList FolderManager::getImageFiles(const QString &folderPath) const
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        return QStringList();
    }

    QStringList imageFiles = dir.entryList(m_supportedExtensions, QDir::Files, QDir::Name);

    // Convert to absolute paths
    QStringList absolutePaths;
    for (const QString &fileName : imageFiles) {
        absolutePaths.append(dir.absoluteFilePath(fileName));
    }

    return absolutePaths;
}

void FolderManager::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (item) {
        QString folderPath = item->data(0, Qt::UserRole).toString();
        emit folderSelected(folderPath);
    }
}

void FolderManager::loadSubfolders(QTreeWidgetItem *parentItem, const QString &path, int depth)
{
    // Prevent infinite recursion
    if (depth >= MAX_SUBFOLDER_DEPTH) {
        return;
    }

    QDir dir(path);
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString &subDirName : subDirs) {
        QString subDirPath = dir.absoluteFilePath(subDirName);
        QTreeWidgetItem *subItem = createFolderItem(subDirPath, subDirName);
        parentItem->addChild(subItem);

        // Recursively load subfolders
        loadSubfolders(subItem, subDirPath, depth + 1);
    }
}

QTreeWidgetItem* FolderManager::createFolderItem(const QString &folderPath, const QString &displayName)
{
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, displayName);
    item->setData(0, Qt::UserRole, folderPath);

    // Add folder icon
    item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_DirIcon));

    // Add tooltip with full path
    item->setToolTip(0, folderPath);

    return item;
}

QTreeWidgetItem* FolderManager::findItemByPath(const QString &path) const
{
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toString() == path) {
            return *it;
        }
        ++it;
    }
    return nullptr;
}

bool FolderManager::folderAlreadyExists(const QString &folderPath) const
{
    return m_projectFolders.contains(folderPath);
}
