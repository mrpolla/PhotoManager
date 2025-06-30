#ifndef FOLDERMANAGER_H
#define FOLDERMANAGER_H

#include <QObject>
#include <QStringList>

class QTreeWidget;
class QTreeWidgetItem;
class QSettings;

class FolderManager : public QObject
{
    Q_OBJECT

public:
    explicit FolderManager(QTreeWidget *treeWidget, QObject *parent = nullptr);

    // Main operations
    void addFolder(const QString &folderPath);
    void removeSelectedFolder();
    void clearAllFolders();
    QString getCurrentFolderPath() const;
    QStringList getAllFolderPaths() const;

    // Project management
    void saveProject(QSettings *settings);
    void loadProject(QSettings *settings);

    // Tree operations
    void expandAll();
    void collapseAll();

    // Utility
    QStringList getImageFiles(const QString &folderPath) const;

signals:
    void folderSelected(const QString &folderPath);
    void folderAdded(const QString &folderPath);
    void folderRemoved(const QString &folderPath);
    void foldersCleared();

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onItemExpanded(QTreeWidgetItem *item);

private:
    void setupTreeWidget();
    void loadSubfolders(QTreeWidgetItem *parentItem, const QString &path, int depth = 0);
    void loadSubfoldersLazy(QTreeWidgetItem *parentItem);
    QTreeWidgetItem* createFolderItem(const QString &folderPath, const QString &displayName);
    QTreeWidgetItem* findItemByPath(const QString &path) const;
    bool folderAlreadyExists(const QString &folderPath) const;
    void addContextMenu();
    bool hasSubfolders(const QString &path) const;

    QTreeWidget *m_treeWidget;
    QStringList m_supportedExtensions;
    QStringList m_projectFolders;  // Track all project folders
    static const int MAX_SUBFOLDER_DEPTH = 5;
};

#endif // FOLDERMANAGER_H
