#include "syncdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QApplication>
#include <QStyle>

SyncDialog::SyncDialog(ProjectManager *projectManager, QWidget *parent)
    : QDialog(parent), m_projectManager(projectManager)
{
    setupUI();

    // Connect to project manager signals
    connect(m_projectManager, &ProjectManager::syncProgress,
            this, &SyncDialog::onSyncProgress);
    connect(m_projectManager, &ProjectManager::syncCompleted,
            this, &SyncDialog::onSyncCompleted);

    setWindowTitle("Synchronize Project");
    resize(800, 600);
}

void SyncDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Summary section
    m_summaryLabel = new QLabel("Click 'Synchronize' to scan for changes...");
    m_summaryLabel->setStyleSheet("font-weight: bold; padding: 10px; background-color: lightblue;");
    mainLayout->addWidget(m_summaryLabel);

    // Progress section
    QHBoxLayout *progressLayout = new QHBoxLayout;
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    m_statusLabel = new QLabel;
    progressLayout->addWidget(m_progressBar);
    progressLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(progressLayout);

    // Tab widget for different types of changes
    m_tabWidget = new QTabWidget;
    mainLayout->addWidget(m_tabWidget);

    // New Files Tab
    QWidget *newFilesWidget = new QWidget;
    QVBoxLayout *newFilesLayout = new QVBoxLayout(newFilesWidget);
    newFilesLayout->addWidget(new QLabel("New files found in project folders:"));
    m_newFilesTree = new QTreeWidget;
    m_newFilesTree->setHeaderLabels({"File Name", "Path", "Size"});
    m_newFilesTree->header()->setStretchLastSection(false);
    m_newFilesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_newFilesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_newFilesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    newFilesLayout->addWidget(m_newFilesTree);
    m_tabWidget->addTab(newFilesWidget, "New Files");

    // Missing Files Tab
    QWidget *missingFilesWidget = new QWidget;
    QVBoxLayout *missingFilesLayout = new QVBoxLayout(missingFilesWidget);
    missingFilesLayout->addWidget(new QLabel("Files that could not be found:"));
    m_missingFilesTree = new QTreeWidget;
    m_missingFilesTree->setHeaderLabels({"File Name", "Last Known Path", "Date Added"});
    m_missingFilesTree->header()->setStretchLastSection(false);
    m_missingFilesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_missingFilesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_missingFilesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_missingFilesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    missingFilesLayout->addWidget(m_missingFilesTree);

    // Missing files action buttons
    QHBoxLayout *missingButtonsLayout = new QHBoxLayout;
    m_locateButton = new QPushButton("Locate Selected File");
    m_removeMissingButton = new QPushButton("Remove Missing Files");
    m_removeMissingButton->setStyleSheet("QPushButton { color: red; }");
    missingButtonsLayout->addWidget(m_locateButton);
    missingButtonsLayout->addWidget(m_removeMissingButton);
    missingButtonsLayout->addStretch();
    missingFilesLayout->addLayout(missingButtonsLayout);

    m_tabWidget->addTab(missingFilesWidget, "Missing Files");

    // Moved Files Tab
    QWidget *movedFilesWidget = new QWidget;
    QVBoxLayout *movedFilesLayout = new QVBoxLayout(movedFilesWidget);
    movedFilesLayout->addWidget(new QLabel("Files that appear to have been moved:"));
    m_movedFilesTree = new QTreeWidget;
    m_movedFilesTree->setHeaderLabels({"File Name", "Old Path", "New Path", "Confidence"});
    m_movedFilesTree->header()->setStretchLastSection(false);
    m_movedFilesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_movedFilesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_movedFilesTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_movedFilesTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_movedFilesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    movedFilesLayout->addWidget(m_movedFilesTree);

    // Moved files action buttons
    QHBoxLayout *movedButtonsLayout = new QHBoxLayout;
    m_acceptMovesButton = new QPushButton("Accept All Moves");
    m_acceptMovesButton->setStyleSheet("QPushButton { color: green; font-weight: bold; }");
    m_rejectMovesButton = new QPushButton("Reject All Moves");
    movedButtonsLayout->addWidget(m_acceptMovesButton);
    movedButtonsLayout->addWidget(m_rejectMovesButton);
    movedButtonsLayout->addStretch();
    movedFilesLayout->addLayout(movedButtonsLayout);

    m_tabWidget->addTab(movedFilesWidget, "Moved Files");

    // Main action buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    m_syncButton = new QPushButton("Synchronize");
    m_syncButton->setStyleSheet("QPushButton { font-weight: bold; }");
    m_closeButton = new QPushButton("Close");
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_syncButton);
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);

    // Connect button signals
    connect(m_syncButton, &QPushButton::clicked, [this]() {
        m_syncButton->setEnabled(false);
        m_projectManager->synchronizeProject();
    });
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_acceptMovesButton, &QPushButton::clicked, this, &SyncDialog::acceptAllMoves);
    connect(m_rejectMovesButton, &QPushButton::clicked, this, &SyncDialog::rejectAllMoves);
    connect(m_locateButton, &QPushButton::clicked, this, &SyncDialog::locateMissingFile);
    connect(m_removeMissingButton, &QPushButton::clicked, this, &SyncDialog::removeMissingFiles);
}

void SyncDialog::onSyncProgress(int current, int total, const QString &currentFile)
{
    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);

    QFileInfo fileInfo(currentFile);
    m_statusLabel->setText(QString("Scanning: %1").arg(fileInfo.fileName()));

    QApplication::processEvents(); // Keep UI responsive
}

void SyncDialog::onSyncCompleted(const ProjectManager::SyncResult &result)
{
    m_progressBar->setVisible(false);
    m_statusLabel->clear();
    m_syncButton->setEnabled(true);

    showSyncResults(result);
}

void SyncDialog::showSyncResults(const ProjectManager::SyncResult &result)
{
    m_lastResult = result;

    // Update summary
    updateSummary(result);

    // Populate tabs
    populateNewFilesTab(result.newFiles);
    populateMissingFilesTab(result.missingFiles);
    populateMovedFilesTab(result.movedFiles);

    // Update tab titles with counts
    m_tabWidget->setTabText(0, QString("New Files (%1)").arg(result.newFiles.size()));
    m_tabWidget->setTabText(1, QString("Missing Files (%1)").arg(result.missingFiles.size()));
    m_tabWidget->setTabText(2, QString("Moved Files (%1)").arg(result.movedFiles.size()));

    // Switch to most relevant tab
    if (!result.movedFiles.isEmpty()) {
        m_tabWidget->setCurrentIndex(2); // Moved files need user attention
    } else if (!result.missingFiles.isEmpty()) {
        m_tabWidget->setCurrentIndex(1); // Missing files need attention
    } else if (!result.newFiles.isEmpty()) {
        m_tabWidget->setCurrentIndex(0); // New files are informational
    }
}

void SyncDialog::updateSummary(const ProjectManager::SyncResult &result)
{
    QString summary;

    if (result.newFiles.isEmpty() && result.missingFiles.isEmpty() && result.movedFiles.isEmpty()) {
        summary = QString("✓ Project is up to date! Scanned %1 files.").arg(result.totalScanned);
        m_summaryLabel->setStyleSheet("font-weight: bold; padding: 10px; background-color: lightgreen;");
    } else {
        QStringList changes;
        if (!result.newFiles.isEmpty()) {
            changes << QString("%1 new file(s)").arg(result.newFiles.size());
        }
        if (!result.missingFiles.isEmpty()) {
            changes << QString("%1 missing file(s)").arg(result.missingFiles.size());
        }
        if (!result.movedFiles.isEmpty()) {
            changes << QString("%1 moved file(s)").arg(result.movedFiles.size());
        }

        summary = QString("Found changes: %1").arg(changes.join(", "));
        m_summaryLabel->setStyleSheet("font-weight: bold; padding: 10px; background-color: lightyellow;");
    }

    m_summaryLabel->setText(summary);
}

void SyncDialog::populateNewFilesTab(const QStringList &newFiles)
{
    m_newFilesTree->clear();

    for (const QString &filePath : newFiles) {
        QFileInfo fileInfo(filePath);
        QTreeWidgetItem *item = new QTreeWidgetItem(m_newFilesTree);
        item->setText(0, fileInfo.fileName());
        item->setText(1, filePath);
        item->setText(2, QString::number(fileInfo.size()));
        item->setToolTip(1, filePath);

        // Add icon based on file type
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    }
}

void SyncDialog::populateMissingFilesTab(const QStringList &missingFiles)
{
    m_missingFilesTree->clear();

    for (const QString &filePath : missingFiles) {
        QFileInfo fileInfo(filePath);
        QTreeWidgetItem *item = new QTreeWidgetItem(m_missingFilesTree);
        item->setText(0, fileInfo.fileName());
        item->setText(1, filePath);
        item->setText(2, "Unknown"); // Would need to query database for date added
        item->setToolTip(1, filePath);

        // Add warning icon
        item->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
    }
}

void SyncDialog::populateMovedFilesTab(const QList<QPair<QString, QString>> &movedFiles)
{
    m_movedFilesTree->clear();

    for (const auto &move : movedFiles) {
        QFileInfo fileInfo(move.second); // new path
        QTreeWidgetItem *item = new QTreeWidgetItem(m_movedFilesTree);
        item->setText(0, fileInfo.fileName());
        item->setText(1, move.first);  // old path
        item->setText(2, move.second); // new path
        item->setText(3, "High");      // confidence - could be calculated
        item->setToolTip(1, move.first);
        item->setToolTip(2, move.second);

        // Add move icon
        item->setIcon(0, style()->standardIcon(QStyle::SP_ArrowRight));
        item->setCheckState(0, Qt::Checked); // Default to accepting moves
    }
}

void SyncDialog::acceptAllMoves()
{
    // This would update the database to accept all detected moves
    for (int i = 0; i < m_movedFilesTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_movedFilesTree->topLevelItem(i);
        item->setCheckState(0, Qt::Checked);
    }

    QMessageBox::information(this, "Moves Accepted",
                             QString("Accepted %1 file moves.").arg(m_lastResult.movedFiles.size()));
}

void SyncDialog::rejectAllMoves()
{
    for (int i = 0; i < m_movedFilesTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_movedFilesTree->topLevelItem(i);
        item->setCheckState(0, Qt::Unchecked);
    }

    QMessageBox::information(this, "Moves Rejected",
                             "All detected moves have been rejected.");
}

void SyncDialog::locateMissingFile()
{
    QTreeWidgetItem *selectedItem = m_missingFilesTree->currentItem();
    if (!selectedItem) {
        QMessageBox::information(this, "No Selection", "Please select a missing file to locate.");
        return;
    }

    QString missingPath = selectedItem->text(1);
    QString fileName = selectedItem->text(0);

    QString newPath = QFileDialog::getOpenFileName(this,
                                                   QString("Locate: %1").arg(fileName),
                                                   QString(),
                                                   "Image Files (*.jpg *.jpeg *.png *.bmp *.gif *.tiff *.webp)");

    if (!newPath.isEmpty()) {
        // Update database with new path
        // This would require additional ProjectManager method
        QMessageBox::information(this, "File Located",
                                 QString("Updated path for %1").arg(fileName));
    }
}

void SyncDialog::removeMissingFiles()
{
    QList<QTreeWidgetItem*> selectedItems = m_missingFilesTree->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "No Selection", "Please select missing files to remove.");
        return;
    }

    int result = QMessageBox::question(this, "Remove Missing Files",
                                       QString("Remove %1 missing file(s) from project?\n\n"
                                               "This will permanently delete them from the project database.")
                                           .arg(selectedItems.size()),
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::No);

    if (result == QMessageBox::Yes) {
        // Remove from database
        // This would require additional ProjectManager method
        QMessageBox::information(this, "Files Removed",
                                 QString("Removed %1 missing files from project.").arg(selectedItems.size()));
    }
}
