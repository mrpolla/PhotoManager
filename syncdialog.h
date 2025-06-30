#ifndef SYNCDIALOG_H
#define SYNCDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTabWidget>
#include "projectmanager.h"

class SyncDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SyncDialog(ProjectManager *projectManager, QWidget *parent = nullptr);

    void showSyncResults(const ProjectManager::SyncResult &result);

private slots:
    void onSyncProgress(int current, int total, const QString &currentFile);
    void onSyncCompleted(const ProjectManager::SyncResult &result);
    void acceptAllMoves();
    void rejectAllMoves();
    void locateMissingFile();
    void removeMissingFiles();

private:
    void setupUI();
    void populateNewFilesTab(const QStringList &newFiles);
    void populateMissingFilesTab(const QStringList &missingFiles);
    void populateMovedFilesTab(const QList<QPair<QString, QString>> &movedFiles);
    void updateSummary(const ProjectManager::SyncResult &result);

    ProjectManager *m_projectManager;

    // UI Components
    QTabWidget *m_tabWidget;
    QLabel *m_summaryLabel;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_syncButton;
    QPushButton *m_closeButton;

    // Tab contents
    QTreeWidget *m_newFilesTree;
    QTreeWidget *m_missingFilesTree;
    QTreeWidget *m_movedFilesTree;

    // Action buttons
    QPushButton *m_acceptMovesButton;
    QPushButton *m_rejectMovesButton;
    QPushButton *m_locateButton;
    QPushButton *m_removeMissingButton;

    ProjectManager::SyncResult m_lastResult;
};

#endif // SYNCDIALOG_H
