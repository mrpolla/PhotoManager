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

// === Constants ===
namespace {
// Window settings
constexpr int DEFAULT_WIDTH = 800;
constexpr int DEFAULT_HEIGHT = 600;

// UI spacing
constexpr int LAYOUT_MARGIN = 10;
constexpr int LAYOUT_SPACING = 10;
constexpr int BUTTON_SPACING = 5;

// File size constants
constexpr qint64 BYTES_PER_KB = 1024;
constexpr qint64 BYTES_PER_MB = 1024 * 1024;
constexpr qint64 BYTES_PER_GB = 1024 * 1024 * 1024;

// Style sheets
const QString STYLE_SUMMARY_NORMAL = "font-weight: bold; padding: 10px; background-color: lightblue;";
const QString STYLE_SUMMARY_SUCCESS = "font-weight: bold; padding: 10px; background-color: lightgreen;";
const QString STYLE_SUMMARY_CHANGES = "font-weight: bold; padding: 10px; background-color: lightyellow;";
const QString STYLE_BUTTON_ACCEPT = "QPushButton { color: green; font-weight: bold; }";
const QString STYLE_BUTTON_DANGER = "QPushButton { color: red; }";

// Messages
const QString MSG_INITIAL = "Click 'Synchronize' to scan for changes...";
const QString MSG_SUCCESS = "✓ Project is up to date! Scanned %1 files.";
const QString MSG_CHANGES = "Found changes: %1";
const QString MSG_SCANNING = "Scanning: %1";

// Tab titles
const QString TAB_NEW_FILES = "New Files";
const QString TAB_MISSING_FILES = "Missing Files";
const QString TAB_MOVED_FILES = "Moved Files";

// Column headers
const QStringList HEADERS_NEW_FILES = {"File Name", "Path", "Size"};
const QStringList HEADERS_MISSING_FILES = {"File Name", "Last Known Path", "Date Added"};
const QStringList HEADERS_MOVED_FILES = {"File Name", "Old Path", "New Path", "Confidence"};

// File extensions for dialog
const QString IMAGE_FILTER = "Image Files (*.jpg *.jpeg *.png *.bmp *.gif *.tiff *.webp)";
}

// === Constructor ===

SyncDialog::SyncDialog(ProjectManager *projectManager, QWidget *parent)
    : QDialog(parent)
    , m_projectManager(projectManager)
{
    setupUI();
    connectSignals();

    setWindowTitle("Synchronize Project");
    resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
}

// === Public Methods ===

void SyncDialog::showSyncResults(const ProjectManager::SyncResult &result)
{
    m_lastResult = result;

    updateSummary(result);
    populateAllTabs(result);
    updateTabTitles(result);
    switchToRelevantTab(result);
}

// === Private Slots - Synchronization Events ===

void SyncDialog::onSyncProgress(int current, int total, const QString &currentFile)
{
    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);

    const QFileInfo fileInfo(currentFile);
    m_statusLabel->setText(MSG_SCANNING.arg(fileInfo.fileName()));

    QApplication::processEvents(); // Keep UI responsive
}

void SyncDialog::onSyncCompleted(const ProjectManager::SyncResult &result)
{
    m_progressBar->setVisible(false);
    m_statusLabel->clear();
    m_syncButton->setEnabled(true);

    showSyncResults(result);
}

// === Private Slots - User Actions ===

void SyncDialog::startSynchronization()
{
    m_syncButton->setEnabled(false);
    m_projectManager->synchronizeProject();
}

void SyncDialog::acceptAllMoves()
{
    setAllMovesCheckState(Qt::Checked);

    QMessageBox::information(this, "Moves Accepted",
                             QString("Accepted %1 file moves.").arg(m_lastResult.movedFiles.size()));
}

void SyncDialog::rejectAllMoves()
{
    setAllMovesCheckState(Qt::Unchecked);

    QMessageBox::information(this, "Moves Rejected",
                             "All detected moves have been rejected.");
}

void SyncDialog::locateMissingFile()
{
    QTreeWidgetItem *selectedItem = m_missingFilesTree->currentItem();
    if (!selectedItem) {
        QMessageBox::information(this, "No Selection",
                                 "Please select a missing file to locate.");
        return;
    }

    const QString missingPath = selectedItem->text(1);
    const QString fileName = selectedItem->text(0);

    const QString newPath = QFileDialog::getOpenFileName(
        this,
        QString("Locate: %1").arg(fileName),
        QString(),
        IMAGE_FILTER
        );

    if (!newPath.isEmpty()) {
        // TODO: Update database with new path via ProjectManager
        QMessageBox::information(this, "File Located",
                                 QString("Updated path for %1").arg(fileName));
    }
}

void SyncDialog::removeMissingFiles()
{
    const QList<QTreeWidgetItem*> selectedItems = m_missingFilesTree->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "No Selection",
                                 "Please select missing files to remove.");
        return;
    }

    const int result = QMessageBox::question(
        this, "Remove Missing Files",
        QString("Remove %1 missing file(s) from project?\n\n"
                "This will permanently delete them from the project database.")
            .arg(selectedItems.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
        );

    if (result == QMessageBox::Yes) {
        // TODO: Remove from database via ProjectManager
        QMessageBox::information(this, "Files Removed",
                                 QString("Removed %1 missing files from project.")
                                     .arg(selectedItems.size()));
    }
}

// === Private Methods - UI Setup ===

void SyncDialog::setupUI()
{
    createMainLayout();
}

void SyncDialog::createMainLayout()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(LAYOUT_MARGIN, LAYOUT_MARGIN, LAYOUT_MARGIN, LAYOUT_MARGIN);
    mainLayout->setSpacing(LAYOUT_SPACING);

    createSummarySection(mainLayout);
    createProgressSection(mainLayout);
    createTabsSection(mainLayout);
    createButtonsSection(mainLayout);
}

void SyncDialog::createSummarySection(QVBoxLayout *mainLayout)
{
    m_summaryLabel = new QLabel(MSG_INITIAL);
    m_summaryLabel->setStyleSheet(STYLE_SUMMARY_NORMAL);
    mainLayout->addWidget(m_summaryLabel);
}

void SyncDialog::createProgressSection(QVBoxLayout *mainLayout)
{
    QHBoxLayout *progressLayout = new QHBoxLayout;

    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);

    m_statusLabel = new QLabel;

    progressLayout->addWidget(m_progressBar);
    progressLayout->addWidget(m_statusLabel);

    mainLayout->addLayout(progressLayout);
}

void SyncDialog::createTabsSection(QVBoxLayout *mainLayout)
{
    m_tabWidget = new QTabWidget;

    // Create tabs
    createNewFilesTab();
    createMissingFilesTab();
    createMovedFilesTab();

    mainLayout->addWidget(m_tabWidget);
}

void SyncDialog::createButtonsSection(QVBoxLayout *mainLayout)
{
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(BUTTON_SPACING);

    m_syncButton = new QPushButton("Synchronize");
    m_syncButton->setStyleSheet(STYLE_BUTTON_ACCEPT);

    m_closeButton = new QPushButton("Close");

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_syncButton);
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);
}

void SyncDialog::connectSignals()
{
    // Project manager signals
    connect(m_projectManager, &ProjectManager::syncProgress,
            this, &SyncDialog::onSyncProgress);
    connect(m_projectManager, &ProjectManager::syncCompleted,
            this, &SyncDialog::onSyncCompleted);

    // Button signals
    connect(m_syncButton, &QPushButton::clicked, this, &SyncDialog::startSynchronization);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_acceptMovesButton, &QPushButton::clicked, this, &SyncDialog::acceptAllMoves);
    connect(m_rejectMovesButton, &QPushButton::clicked, this, &SyncDialog::rejectAllMoves);
    connect(m_locateButton, &QPushButton::clicked, this, &SyncDialog::locateMissingFile);
    connect(m_removeMissingButton, &QPushButton::clicked, this, &SyncDialog::removeMissingFiles);
}

// === Private Methods - Tab Creation ===

void SyncDialog::createNewFilesTab()
{
    QWidget *newFilesWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(newFilesWidget);

    layout->addWidget(new QLabel("New files found in project folders:"));

    m_newFilesTree = createStandardTreeWidget(HEADERS_NEW_FILES);
    layout->addWidget(m_newFilesTree);

    m_tabWidget->addTab(newFilesWidget, TAB_NEW_FILES);
}

void SyncDialog::createMissingFilesTab()
{
    QWidget *missingFilesWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(missingFilesWidget);

    layout->addWidget(new QLabel("Files that could not be found:"));

    m_missingFilesTree = createStandardTreeWidget(HEADERS_MISSING_FILES);
    m_missingFilesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(m_missingFilesTree);

    // Action buttons for missing files
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    m_locateButton = new QPushButton("Locate Selected File");
    m_removeMissingButton = new QPushButton("Remove Missing Files");
    m_removeMissingButton->setStyleSheet(STYLE_BUTTON_DANGER);

    buttonLayout->addWidget(m_locateButton);
    buttonLayout->addWidget(m_removeMissingButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    m_tabWidget->addTab(missingFilesWidget, TAB_MISSING_FILES);
}

void SyncDialog::createMovedFilesTab()
{
    QWidget *movedFilesWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(movedFilesWidget);

    layout->addWidget(new QLabel("Files that appear to have been moved:"));

    m_movedFilesTree = createStandardTreeWidget(HEADERS_MOVED_FILES);
    m_movedFilesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(m_movedFilesTree);

    // Action buttons for moved files
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    m_acceptMovesButton = new QPushButton("Accept All Moves");
    m_acceptMovesButton->setStyleSheet(STYLE_BUTTON_ACCEPT);
    m_rejectMovesButton = new QPushButton("Reject All Moves");

    buttonLayout->addWidget(m_acceptMovesButton);
    buttonLayout->addWidget(m_rejectMovesButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    m_tabWidget->addTab(movedFilesWidget, TAB_MOVED_FILES);
}

// === Private Methods - Results Display ===

void SyncDialog::populateAllTabs(const ProjectManager::SyncResult &result)
{
    populateNewFilesTab(result.newFiles);
    populateMissingFilesTab(result.missingFiles);
    populateMovedFilesTab(result.movedFiles);
}

void SyncDialog::populateNewFilesTab(const QStringList &newFiles)
{
    m_newFilesTree->clear();

    for (const QString &filePath : newFiles) {
        QTreeWidgetItem *item = addFileItem(m_newFilesTree, filePath, QStyle::SP_FileIcon);

        const QFileInfo fileInfo(filePath);
        item->setText(2, formatFileSize(fileInfo.size()));
    }
}

void SyncDialog::populateMissingFilesTab(const QStringList &missingFiles)
{
    m_missingFilesTree->clear();

    for (const QString &filePath : missingFiles) {
        QTreeWidgetItem *item = addFileItem(m_missingFilesTree, filePath, QStyle::SP_MessageBoxWarning);

        item->setText(2, "Unknown"); // Would need database query for actual date
    }
}

void SyncDialog::populateMovedFilesTab(const QList<QPair<QString, QString>> &movedFiles)
{
    m_movedFilesTree->clear();

    for (const auto &move : movedFiles) {
        const QFileInfo fileInfo(move.second); // new path
        QTreeWidgetItem *item = new QTreeWidgetItem(m_movedFilesTree);

        item->setText(0, fileInfo.fileName());
        item->setText(1, move.first);  // old path
        item->setText(2, move.second); // new path
        item->setText(3, getConfidenceLevel(move.first, move.second));

        item->setToolTip(1, move.first);
        item->setToolTip(2, move.second);
        item->setIcon(0, style()->standardIcon(QStyle::SP_ArrowRight));
        item->setCheckState(0, Qt::Checked); // Default to accepting moves
    }
}

void SyncDialog::updateSummary(const ProjectManager::SyncResult &result)
{
    QString summary;
    QString styleSheet;

    if (result.newFiles.isEmpty() && result.missingFiles.isEmpty() && result.movedFiles.isEmpty()) {
        summary = MSG_SUCCESS.arg(result.totalScanned);
        styleSheet = STYLE_SUMMARY_SUCCESS;
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

        summary = MSG_CHANGES.arg(changes.join(", "));
        styleSheet = STYLE_SUMMARY_CHANGES;
    }

    m_summaryLabel->setText(summary);
    m_summaryLabel->setStyleSheet(styleSheet);
}

void SyncDialog::updateTabTitles(const ProjectManager::SyncResult &result)
{
    m_tabWidget->setTabText(0, QString("%1 (%2)").arg(TAB_NEW_FILES).arg(result.newFiles.size()));
    m_tabWidget->setTabText(1, QString("%1 (%2)").arg(TAB_MISSING_FILES).arg(result.missingFiles.size()));
    m_tabWidget->setTabText(2, QString("%1 (%2)").arg(TAB_MOVED_FILES).arg(result.movedFiles.size()));
}

void SyncDialog::switchToRelevantTab(const ProjectManager::SyncResult &result)
{
    // Switch to most relevant tab - moved files need user attention first
    if (!result.movedFiles.isEmpty()) {
        m_tabWidget->setCurrentIndex(2); // Moved files
    } else if (!result.missingFiles.isEmpty()) {
        m_tabWidget->setCurrentIndex(1); // Missing files
    } else if (!result.newFiles.isEmpty()) {
        m_tabWidget->setCurrentIndex(0); // New files
    }
}

// === Private Methods - Helper Functions ===

QTreeWidget* SyncDialog::createStandardTreeWidget(const QStringList &headers)
{
    QTreeWidget *tree = new QTreeWidget;
    tree->setHeaderLabels(headers);
    tree->header()->setStretchLastSection(false);

    // Configure column resizing
    for (int i = 0; i < headers.size() - 1; ++i) {
        tree->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    tree->header()->setSectionResizeMode(headers.size() - 1, QHeaderView::Stretch);

    return tree;
}

QTreeWidgetItem* SyncDialog::addFileItem(QTreeWidget *tree, const QString &filePath,
                                         QStyle::StandardPixmap iconType)
{
    const QFileInfo fileInfo(filePath);
    QTreeWidgetItem *item = new QTreeWidgetItem(tree);

    item->setText(0, fileInfo.fileName());
    item->setText(1, filePath);
    item->setToolTip(1, filePath);
    item->setIcon(0, style()->standardIcon(iconType));

    return item;
}

QString SyncDialog::formatFileSize(qint64 bytes) const
{
    if (bytes >= BYTES_PER_GB) {
        return QString("%1 GB").arg(bytes / BYTES_PER_GB);
    } else if (bytes >= BYTES_PER_MB) {
        return QString("%1 MB").arg(bytes / BYTES_PER_MB);
    } else if (bytes >= BYTES_PER_KB) {
        return QString("%1 KB").arg(bytes / BYTES_PER_KB);
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}

QString SyncDialog::getConfidenceLevel(const QString &oldPath, const QString &newPath) const
{
    const QFileInfo oldInfo(oldPath);
    const QFileInfo newInfo(newPath);

    // High confidence if same filename
    if (oldInfo.fileName() == newInfo.fileName()) {
        return "High";
    }

    return "Medium";
}

void SyncDialog::setAllMovesCheckState(Qt::CheckState state)
{
    for (int i = 0; i < m_movedFilesTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_movedFilesTree->topLevelItem(i);
        item->setCheckState(0, state);
    }
}
