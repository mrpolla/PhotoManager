#include "duplicateanalyzer.h"
#include "projectmanager.h"
#include "foldermanager.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QHeaderView>
#include <QApplication>
#include <QStyle>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>
#include <QBrush>
#include <QDataStream>
#include <QDateTime>
#include <QThread>
#include <QImageReader>
#include <cstdio>

// === Constants ===
namespace {
// UI dimensions
constexpr int SPLITTER_LEFT_SIZE = 400;
constexpr int SPLITTER_RIGHT_SIZE = 300;
constexpr int TREE_ICON_SIZE = 16;
constexpr int BUTTON_MIN_WIDTH = 120;

// File size constants
constexpr qint64 BYTES_PER_KB = 1024;
constexpr qint64 BYTES_PER_MB = 1024 * 1024;
constexpr qint64 BYTES_PER_GB = 1024 * 1024 * 1024;

// Column indices for issues tree
constexpr int COL_SEVERITY = 0;
constexpr int COL_TYPE = 1;
constexpr int COL_DESCRIPTION = 2;
constexpr int COL_SIMILARITY = 3;
constexpr int COL_WASTED_SPACE = 4;

// Supported image extensions
const QStringList SUPPORTED_EXTENSIONS = {
    "jpg", "jpeg", "png", "bmp", "gif", "tiff", "tif", "webp", "raw", "cr2", "nef", "arw"
};

// Style sheets
const QString STYLE_DETAILS_TITLE = "font-weight: bold; font-size: 14px; padding: 5px; background-color: lightgray;";
const QString STYLE_DETAILS_LABEL = "padding: 3px; margin: 2px;";
const QString STYLE_BUTTON_PRIMARY = "QPushButton { font-weight: bold; color: blue; }";
const QString STYLE_BUTTON_SUCCESS = "QPushButton { color: green; }";
const QString STYLE_BUTTON_DANGER = "QPushButton { color: red; }";

// Messages
const QString MSG_NO_ISSUES = "No duplicate folder issues found";
const QString MSG_ANALYZING = "Analyzing: %1";
const QString MSG_COMPLETED = "Analysis completed: %1 issues found";
const QString MSG_NO_SELECTION = "Select an issue to view details";

// Issue type names
const QString TYPE_EXACT_COMPLETE = "Exact Complete Duplicate";
const QString TYPE_EXACT_FILES = "Exact Files Duplicate";
const QString TYPE_PARTIAL = "Partial Duplicate";

// Severity levels
const QString SEVERITY_HIGH = "High";
const QString SEVERITY_MEDIUM = "Medium";
const QString SEVERITY_LOW = "Low";
}

// === Constructor ===

DuplicateAnalyzer::DuplicateAnalyzer(ProjectManager *projectManager,
                                     FolderManager *folderManager,
                                     QWidget *parent)
    : QWidget(parent)
    , m_projectManager(projectManager)
    , m_folderManager(folderManager)
    , m_currentMode(ComparisonMode::Quick)
    , m_analysisRunning(false)
{
    setupUI();
    loadFolderContentCache();
}

// === Public Methods ===

void DuplicateAnalyzer::startAnalysis(ComparisonMode mode)
{
    qDebug() << "=== DuplicateAnalyzer::startAnalysis() called ===" << "Mode:" << (mode == ComparisonMode::Quick ? "Quick" : "Deep");

    m_currentMode = mode;
    clearResults();

    QStringList projectFolders = getProjectFolders();
    qDebug() << "Total folders found (including subfolders):" << projectFolders.size();
    for (const QString &folder : projectFolders) {
        qDebug() << "  -" << folder;
    }

    if (projectFolders.size() < 2) {
        qDebug() << "Not enough folders to compare - need at least 2 folders";
        QMessageBox::information(this, "Insufficient Folders",
                                 "Need at least 2 folders to compare.\n\n"
                                 "This includes all subfolders within your project folders.\n"
                                 "Add more folders or subfolders to your project and try again.");
        return;
    }

    qDebug() << "Emitting analysisStarted signal...";
    emit analysisStarted(projectFolders.size(), mode);

    qDebug() << "Calling performAnalysis()...";
    performAnalysis();
    qDebug() << "=== startAnalysis() completed ===";
}

void DuplicateAnalyzer::clearResults()
{
    m_duplicateIssues.clear();
    // Don't clear the folder content cache - keep it for performance
    // But we need to be careful: Quick mode cache won't have partial hashes
    updateIssuesTree();
    updateDetailsPanel();

    m_progressBar->setVisible(false);
    m_statusLabel->setText(QString("Ready to analyze (%1 mode)").arg(getModeName(m_currentMode)));
}

// === Private Slots ===

void DuplicateAnalyzer::onIssueSelected()
{
    updateDetailsPanel();

    // Enable action buttons when issue is selected
    bool hasSelection = getCurrentIssueItem() != nullptr;
    m_showPrimaryButton->setEnabled(hasSelection);
    m_showDuplicateButton->setEnabled(hasSelection);
    m_openPrimaryButton->setEnabled(hasSelection);
    m_openDuplicateButton->setEnabled(hasSelection);
}

void DuplicateAnalyzer::showPrimaryFolder()
{
    QTreeWidgetItem *item = getCurrentIssueItem();
    if (!item) return;

    int issueIndex = m_issuesTree->indexOfTopLevelItem(item);
    if (issueIndex >= 0 && issueIndex < m_duplicateIssues.size()) {
        const DuplicateIssue &issue = m_duplicateIssues[issueIndex];
        emit showFolderInTree(issue.primaryFolder);
    }
}

void DuplicateAnalyzer::showDuplicateFolder()
{
    QTreeWidgetItem *item = getCurrentIssueItem();
    if (!item) return;

    int issueIndex = m_issuesTree->indexOfTopLevelItem(item);
    if (issueIndex >= 0 && issueIndex < m_duplicateIssues.size()) {
        const DuplicateIssue &issue = m_duplicateIssues[issueIndex];
        emit showFolderInTree(issue.duplicateFolder);
    }
}

void DuplicateAnalyzer::openPrimaryInExplorer()
{
    QTreeWidgetItem *item = getCurrentIssueItem();
    if (!item) return;

    int issueIndex = m_issuesTree->indexOfTopLevelItem(item);
    if (issueIndex >= 0 && issueIndex < m_duplicateIssues.size()) {
        const DuplicateIssue &issue = m_duplicateIssues[issueIndex];
        openFolderInExplorer(issue.primaryFolder);
    }
}

void DuplicateAnalyzer::openDuplicateInExplorer()
{
    QTreeWidgetItem *item = getCurrentIssueItem();
    if (!item) return;

    int issueIndex = m_issuesTree->indexOfTopLevelItem(item);
    if (issueIndex >= 0 && issueIndex < m_duplicateIssues.size()) {
        const DuplicateIssue &issue = m_duplicateIssues[issueIndex];
        openFolderInExplorer(issue.duplicateFolder);
    }
}

void DuplicateAnalyzer::refreshAnalysis()
{
    // Cancel current analysis if running
    if (m_analysisRunning) {
        m_analysisRunning = false;
        printf("\nCancelling current analysis...\n");
        fflush(stdout);

        // Wait a bit for cancellation to take effect
        QApplication::processEvents();
        QThread::msleep(100);
    }

    // Clear cache and start fresh analysis with current mode
    m_folderContentCache.clear();
    qDebug() << "Cache cleared for fresh analysis";
    startAnalysis(m_currentMode);
}

void DuplicateAnalyzer::resetAnalysisState()
{
    m_progressBar->setVisible(false);
    m_statusLabel->setText("Analysis cancelled");
    m_analysisRunning = false;

    printf("\nAnalysis state reset.\n");
    fflush(stdout);
}

// === Private Methods - UI Setup ===

void DuplicateAnalyzer::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);

    // Progress section at top
    createProgressSection();

    // Create splitter for issues list and details
    m_splitter = new QSplitter(Qt::Horizontal);

    // Create issues tree on the left
    createIssuesTree();

    // Create details panel on the right
    createDetailsPanel();

    m_splitter->addWidget(m_issuesTree);
    m_splitter->addWidget(m_detailsPanel);
    m_splitter->setSizes({SPLITTER_LEFT_SIZE, SPLITTER_RIGHT_SIZE});
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    m_mainLayout->addWidget(m_splitter);

    // Create action buttons at bottom
    createActionButtons();
}

void DuplicateAnalyzer::createIssuesTree()
{
    // Container widget for tree and count label
    QWidget *treeContainer = new QWidget;
    QVBoxLayout *treeLayout = new QVBoxLayout(treeContainer);
    treeLayout->setContentsMargins(0, 0, 0, 0);

    // Issues count label
    m_issuesCountLabel = new QLabel(MSG_NO_ISSUES);
    m_issuesCountLabel->setStyleSheet("font-weight: bold; padding: 5px;");
    treeLayout->addWidget(m_issuesCountLabel);

    // Create tree widget
    m_issuesTree = new QTreeWidget;
    m_issuesTree->setHeaderLabels({"Severity", "Type", "Description", "Similarity", "Wasted Space"});
    m_issuesTree->setRootIsDecorated(false);
    m_issuesTree->setAlternatingRowColors(true);
    m_issuesTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_issuesTree->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Set column widths
    m_issuesTree->header()->resizeSection(COL_SEVERITY, 80);
    m_issuesTree->header()->resizeSection(COL_TYPE, 150);
    m_issuesTree->header()->resizeSection(COL_DESCRIPTION, 300);
    m_issuesTree->header()->resizeSection(COL_SIMILARITY, 80);
    m_issuesTree->header()->resizeSection(COL_WASTED_SPACE, 100);

    // Enable sorting
    m_issuesTree->setSortingEnabled(true);

    connect(m_issuesTree, &QTreeWidget::itemSelectionChanged,
            this, &DuplicateAnalyzer::onIssueSelected);

    treeLayout->addWidget(m_issuesTree);
}

void DuplicateAnalyzer::createDetailsPanel()
{
    m_detailsPanel = new QWidget;
    QVBoxLayout *detailsLayout = new QVBoxLayout(m_detailsPanel);

    // Title
    m_detailsTitle = new QLabel(MSG_NO_SELECTION);
    m_detailsTitle->setStyleSheet(STYLE_DETAILS_TITLE);
    m_detailsTitle->setWordWrap(true);
    detailsLayout->addWidget(m_detailsTitle);

    // Comparison mode label
    m_modeLabel = new QLabel;
    m_modeLabel->setStyleSheet(STYLE_DETAILS_LABEL);
    m_modeLabel->setWordWrap(true);
    detailsLayout->addWidget(m_modeLabel);

    // Details labels
    m_primaryFolderLabel = new QLabel;
    m_primaryFolderLabel->setStyleSheet(STYLE_DETAILS_LABEL);
    m_primaryFolderLabel->setWordWrap(true);
    m_primaryFolderLabel->setTextFormat(Qt::RichText);
    detailsLayout->addWidget(m_primaryFolderLabel);

    m_duplicateFolderLabel = new QLabel;
    m_duplicateFolderLabel->setStyleSheet(STYLE_DETAILS_LABEL);
    m_duplicateFolderLabel->setWordWrap(true);
    m_duplicateFolderLabel->setTextFormat(Qt::RichText);
    detailsLayout->addWidget(m_duplicateFolderLabel);

    m_similarityLabel = new QLabel;
    m_similarityLabel->setStyleSheet(STYLE_DETAILS_LABEL);
    detailsLayout->addWidget(m_similarityLabel);

    m_filesCountLabel = new QLabel;
    m_filesCountLabel->setStyleSheet(STYLE_DETAILS_LABEL);
    detailsLayout->addWidget(m_filesCountLabel);

    m_wastedSpaceLabel = new QLabel;
    m_wastedSpaceLabel->setStyleSheet(STYLE_DETAILS_LABEL);
    detailsLayout->addWidget(m_wastedSpaceLabel);

    m_severityLabel = new QLabel;
    m_severityLabel->setStyleSheet(STYLE_DETAILS_LABEL);
    detailsLayout->addWidget(m_severityLabel);

    // Action buttons in details panel
    QVBoxLayout *detailsButtonLayout = new QVBoxLayout;
    detailsButtonLayout->setSpacing(5);

    m_showPrimaryButton = new QPushButton("Show Primary in Tree");
    m_showPrimaryButton->setEnabled(false);
    m_showPrimaryButton->setStyleSheet(STYLE_BUTTON_PRIMARY);
    connect(m_showPrimaryButton, &QPushButton::clicked,
            this, &DuplicateAnalyzer::showPrimaryFolder);
    detailsButtonLayout->addWidget(m_showPrimaryButton);

    m_showDuplicateButton = new QPushButton("Show Duplicate in Tree");
    m_showDuplicateButton->setEnabled(false);
    m_showDuplicateButton->setStyleSheet(STYLE_BUTTON_PRIMARY);
    connect(m_showDuplicateButton, &QPushButton::clicked,
            this, &DuplicateAnalyzer::showDuplicateFolder);
    detailsButtonLayout->addWidget(m_showDuplicateButton);

    m_openPrimaryButton = new QPushButton("Open Primary Folder");
    m_openPrimaryButton->setEnabled(false);
    connect(m_openPrimaryButton, &QPushButton::clicked,
            this, &DuplicateAnalyzer::openPrimaryInExplorer);
    detailsButtonLayout->addWidget(m_openPrimaryButton);

    m_openDuplicateButton = new QPushButton("Open Duplicate Folder");
    m_openDuplicateButton->setEnabled(false);
    connect(m_openDuplicateButton, &QPushButton::clicked,
            this, &DuplicateAnalyzer::openDuplicateInExplorer);
    detailsButtonLayout->addWidget(m_openDuplicateButton);

    detailsLayout->addLayout(detailsButtonLayout);
    detailsLayout->addStretch();
}

void DuplicateAnalyzer::createActionButtons()
{
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(10);

    m_refreshButton = new QPushButton("Refresh Analysis");
    m_refreshButton->setStyleSheet(STYLE_BUTTON_SUCCESS);
    connect(m_refreshButton, &QPushButton::clicked, this, &DuplicateAnalyzer::refreshAnalysis);
    buttonLayout->addWidget(m_refreshButton);

    buttonLayout->addStretch();

    m_mainLayout->addLayout(buttonLayout);
}

void DuplicateAnalyzer::createProgressSection()
{
    QVBoxLayout *progressLayout = new QVBoxLayout;

    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(true);
    progressLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Ready to analyze");
    m_statusLabel->setStyleSheet("padding: 5px; color: #555;");
    progressLayout->addWidget(m_statusLabel);

    m_mainLayout->addLayout(progressLayout);
}

// === Private Methods - Analysis Core ===

void DuplicateAnalyzer::performAnalysis()
{
    m_analysisRunning = true;
    m_filesAnalyzed = 0;
    m_foldersScanned = 0;

    QStringList projectFolders = getProjectFolders();
    if (projectFolders.isEmpty()) {
        resetAnalysisState();
        return;
    }

    // Show progress
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText(QString("Starting %1 analysis...").arg(getModeName(m_currentMode)));

    // Phase 1 (0-10%): Count files that need analysis
    qDebug() << "\n=== Phase 1: Counting uncached files ===";
    m_totalFilesToAnalyze = 0;
    m_totalFoldersToScan = 0;

    for (const QString &folder : projectFolders) {
        if (!m_analysisRunning) {
            resetAnalysisState();
            return;
        }

        if (!m_folderContentCache.contains(folder)) {
            m_totalFoldersToScan++;
            int fileCount = countFilesInFolder(folder);
            m_totalFilesToAnalyze += fileCount;
            qDebug() << "Folder needs scanning:" << folder << "with" << fileCount << "files";
        } else {
            qDebug() << "Folder already cached:" << folder;
        }

        m_progressBar->setValue(5);
        QApplication::processEvents();
    }

    qDebug() << "Total files to analyze:" << m_totalFilesToAnalyze;
    qDebug() << "Total folders to scan:" << m_totalFoldersToScan;

    // Phase 2 (10-70%): Analyze folder contents
    qDebug() << "\n=== Phase 2: Analyzing folder contents ===";
    m_progressBar->setValue(10);
    
    if (m_totalFilesToAnalyze > 0) {
        m_statusLabel->setText(QString("%1 analysis: Processing %2 files...")
                              .arg(getModeName(m_currentMode))
                              .arg(m_totalFilesToAnalyze));
        printf("\nStarting file analysis (%s mode):\n", getModeName(m_currentMode).toLocal8Bit().constData());
        fflush(stdout);
    }

    analyzeFolderPairs();

    if (!m_analysisRunning) {
        resetAnalysisState();
        return;
    }

    // Phase 3 (70-100%): Compare folder pairs and finalize
    qDebug() << "\n=== Phase 3: Finalizing results ===";
    m_progressBar->setValue(100);
    printf("\n\nAnalysis complete!\n");
    fflush(stdout);

    // Save cache for future runs
    saveFolderContentCache();

    // Update UI
    updateIssuesTree();
    m_statusLabel->setText(QString("%1 complete: %2 issues found")
                          .arg(MSG_COMPLETED.arg(m_duplicateIssues.size()))
                          .arg(getModeName(m_currentMode)));

    m_progressBar->setVisible(false);
    m_analysisRunning = false;

    emit analysisCompleted(m_duplicateIssues.size(), m_currentMode);
}

void DuplicateAnalyzer::analyzeFolderPairs()
{
    QStringList projectFolders = getProjectFolders();
    int totalPairs = (projectFolders.size() * (projectFolders.size() - 1)) / 2;
    int pairsAnalyzed = 0;

    // Compare each pair of folders
    for (int i = 0; i < projectFolders.size(); ++i) {
        if (!m_analysisRunning) return;

        for (int j = i + 1; j < projectFolders.size(); ++j) {
            if (!m_analysisRunning) return;

            compareFolders(projectFolders[i], projectFolders[j]);

            pairsAnalyzed++;
            // Update progress (70-100% range)
            int progress = 70 + (pairsAnalyzed * 30) / qMax(1, totalPairs);
            m_progressBar->setValue(progress);
            QApplication::processEvents();
        }
    }
}

void DuplicateAnalyzer::compareFolders(const QString &folder1, const QString &folder2)
{
    // Skip if folders are the same
    if (folder1 == folder2) {
        return;
    }

    // Get or create folder content analysis
    if (!m_folderContentCache.contains(folder1)) {
        m_folderContentCache[folder1] = analyzeFolderContent(folder1);
    }
    if (!m_folderContentCache.contains(folder2)) {
        m_folderContentCache[folder2] = analyzeFolderContent(folder2);
    }

    const FolderContent &content1 = m_folderContentCache[folder1];
    const FolderContent &content2 = m_folderContentCache[folder2];

    // Skip empty folders
    if (content1.allFiles.isEmpty() && content2.allFiles.isEmpty()) {
        return;
    }

    // Check for exact complete duplicate (files + folder structure)
    if (isExactCompleteDuplicate(content1, content2)) {
        DuplicateIssue issue;
        issue.type = DuplicateType::ExactComplete;
        issue.primaryFolder = folder1;
        issue.duplicateFolder = folder2;
        issue.similarity = 1.0;
        issue.totalFiles = content1.allFiles.size();
        issue.duplicateFiles = content1.allFiles.size();
        issue.wastedSpace = qMin(content1.totalSize, content2.totalSize);
        issue.severity = SEVERITY_HIGH;
        issue.description = formatIssueDescription(issue);

        addDuplicateIssue(issue);
        return; // Don't check other types if exact match found
    }

    // Check for exact files-only duplicate
    if (isExactFilesOnlyDuplicate(content1, content2)) {
        DuplicateIssue issue;
        issue.type = DuplicateType::ExactFilesOnly;
        issue.primaryFolder = folder1;
        issue.duplicateFolder = folder2;
        issue.similarity = 1.0;
        issue.totalFiles = content1.allFiles.size();
        issue.duplicateFiles = content1.allFiles.size();
        issue.wastedSpace = qMin(content1.totalSize, content2.totalSize);
        issue.severity = SEVERITY_MEDIUM;
        issue.description = formatIssueDescription(issue);

        addDuplicateIssue(issue);
        return;
    }

    // Check for partial duplicate (90%+ similarity)
    double similarity = calculateFileSimilarity(content1, content2);
    if (similarity >= PARTIAL_DUPLICATE_THRESHOLD) {
        DuplicateIssue issue;
        issue.type = DuplicateType::PartialDuplicate;
        issue.primaryFolder = folder1;
        issue.duplicateFolder = folder2;
        issue.similarity = similarity;
        issue.totalFiles = qMax(content1.allFiles.size(), content2.allFiles.size());
        issue.duplicateFiles = qRound(similarity * issue.totalFiles);
        issue.wastedSpace = qRound(similarity * qMin(content1.totalSize, content2.totalSize));
        issue.severity = SEVERITY_LOW;
        issue.description = formatIssueDescription(issue);

        addDuplicateIssue(issue);
    }
}

// === Private Methods - Folder Content Analysis ===

DuplicateAnalyzer::FolderContent DuplicateAnalyzer::analyzeFolderContent(const QString &folderPath)
{
    qDebug() << "Analyzing folder content for:" << folderPath;

    FolderContent content;
    content.totalSize = 0;

    QDir dir(folderPath);
    if (!dir.exists()) {
        qDebug() << "Folder does not exist:" << folderPath;
        return content;
    }

    qDebug() << "Starting recursive scan...";
    scanFolderRecursive(folderPath, folderPath, content);

    qDebug() << "Folder analysis complete:" << folderPath;
    qDebug() << "  Files found:" << content.allFiles.size();
    qDebug() << "  Subfolders found:" << content.allSubfolders.size();
    qDebug() << "  Total size:" << content.totalSize << "bytes";

    return content;
}

void DuplicateAnalyzer::scanFolderRecursive(const QString &folderPath,
                                            const QString &basePath,
                                            FolderContent &content)
{
    static int scanDepth = 0;
    scanDepth++;
    QString indent = QString("  ").repeated(scanDepth);

    qDebug() << indent << "Scanning folder:" << folderPath;

    QDir dir(folderPath);
    if (!dir.exists()) {
        qDebug() << indent << "Folder does not exist!";
        scanDepth--;
        return;
    }

    // Scan files
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    int imageFileCount = 0;
    for (const QString &fileName : files) {
        if (!m_analysisRunning) {
            scanDepth--;
            return;
        }

        QString fullPath = dir.absoluteFilePath(fileName);
        QFileInfo fileInfo(fullPath);

        QString extension = fileInfo.suffix().toLower();

        // Only include image files
        if (SUPPORTED_EXTENSIONS.contains(extension)) {
            imageFileCount++;
            m_filesAnalyzed++;

            QString relativePath = getRelativePath(fullPath, basePath);
            content.allFiles.append(relativePath);

            // Analyze file based on current mode
            FileInfo info = analyzeFile(fullPath);
            content.fileInfo[relativePath] = info;
            content.totalSize += info.fileSize;

            // Update progress after EVERY file - force UI update
            updateFileProgress();
        }
    }

    // Scan subfolders
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString &subDirName : subDirs) {
        if (!m_analysisRunning) {
            scanDepth--;
            return;
        }

        QString subDirPath = dir.absoluteFilePath(subDirName);
        QString relativeSubDir = getRelativePath(subDirPath, basePath);

        content.allSubfolders.append(relativeSubDir);

        // Recursively scan subdirectory
        scanFolderRecursive(subDirPath, basePath, content);
    }

    scanDepth--;
}

DuplicateAnalyzer::FileInfo DuplicateAnalyzer::analyzeFile(const QString &filePath)
{
    FileInfo info;
    
    QFileInfo fileInfo(filePath);
    info.fileSize = fileInfo.size();
    
    // Read image dimensions (quick)
    QSize dimensions = readImageDimensions(filePath);
    info.imageWidth = dimensions.width();
    info.imageHeight = dimensions.height();
    
    // Calculate partial hash only in Deep mode
    if (m_currentMode == ComparisonMode::Deep) {
        info.partialHash = calculatePartialHash(filePath);
    }
    
    return info;
}

QSize DuplicateAnalyzer::readImageDimensions(const QString &filePath)
{
    QImageReader reader(filePath);
    QSize size = reader.size();
    
    if (!size.isValid()) {
        qDebug() << "Failed to read image dimensions for:" << filePath;
        return QSize(0, 0);
    }
    
    return size;
}

QString DuplicateAnalyzer::calculatePartialHash(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for partial hashing:" << filePath;
        return QString();
    }

    qint64 fileSize = file.size();
    QCryptographicHash hash(QCryptographicHash::Md5);

    // Read first 16KB
    QByteArray firstChunk = file.read(PARTIAL_HASH_SIZE);
    hash.addData(firstChunk);

    // Read last 16KB (if file is large enough)
    if (fileSize > PARTIAL_HASH_SIZE * 2) {
        file.seek(fileSize - PARTIAL_HASH_SIZE);
        QByteArray lastChunk = file.read(PARTIAL_HASH_SIZE);
        hash.addData(lastChunk);
    }

    return hash.result().toHex();
}

// === Private Methods - Duplicate Detection ===

bool DuplicateAnalyzer::isExactCompleteDuplicate(const FolderContent &folder1,
                                                 const FolderContent &folder2)
{
    // Must have same number of files and subfolders
    if (folder1.allFiles.size() != folder2.allFiles.size() ||
        folder1.allSubfolders.size() != folder2.allSubfolders.size()) {
        return false;
    }

    // All files must match (same relative paths)
    QStringList files1 = folder1.allFiles;
    QStringList files2 = folder2.allFiles;
    files1.sort();
    files2.sort();

    if (files1 != files2) {
        return false;
    }

    // Check file content matches using our comparison method
    for (const QString &relativePath : files1) {
        const FileInfo &info1 = folder1.fileInfo.value(relativePath);
        const FileInfo &info2 = folder2.fileInfo.value(relativePath);
        
        if (!areFilesIdentical(info1, info2)) {
            return false;
        }
    }

    // All subfolders must match
    QStringList subfolders1 = folder1.allSubfolders;
    QStringList subfolders2 = folder2.allSubfolders;
    subfolders1.sort();
    subfolders2.sort();

    return subfolders1 == subfolders2;
}

bool DuplicateAnalyzer::isExactFilesOnlyDuplicate(const FolderContent &folder1,
                                                  const FolderContent &folder2)
{
    // Must have same number of files
    if (folder1.allFiles.size() != folder2.allFiles.size()) {
        return false;
    }

    // Create multisets of file signatures (ignoring paths/folder structure)
    QMultiMap<QString, FileInfo> signatures1, signatures2;
    
    for (const QString &path : folder1.allFiles) {
        const FileInfo &info = folder1.fileInfo.value(path);
        QString signature = QString("%1x%2_%3").arg(info.imageWidth).arg(info.imageHeight).arg(info.fileSize);
        if (m_currentMode == ComparisonMode::Deep) {
            signature += "_" + info.partialHash;
        }
        signatures1.insert(signature, info);
    }
    
    for (const QString &path : folder2.allFiles) {
        const FileInfo &info = folder2.fileInfo.value(path);
        QString signature = QString("%1x%2_%3").arg(info.imageWidth).arg(info.imageHeight).arg(info.fileSize);
        if (m_currentMode == ComparisonMode::Deep) {
            signature += "_" + info.partialHash;
        }
        signatures2.insert(signature, info);
    }

    return signatures1.keys() == signatures2.keys() && !signatures1.isEmpty();
}

double DuplicateAnalyzer::calculateFileSimilarity(const FolderContent &folder1,
                                                  const FolderContent &folder2)
{
    if (folder1.allFiles.isEmpty() && folder2.allFiles.isEmpty()) {
        return 1.0;
    }

    if (folder1.allFiles.isEmpty() || folder2.allFiles.isEmpty()) {
        return 0.0;
    }

    // Get unique file signatures from both folders
    QSet<QString> signatures1, signatures2;
    
    for (const QString &path : folder1.allFiles) {
        const FileInfo &info = folder1.fileInfo.value(path);
        QString signature = QString("%1x%2_%3").arg(info.imageWidth).arg(info.imageHeight).arg(info.fileSize);
        if (m_currentMode == ComparisonMode::Deep) {
            signature += "_" + info.partialHash;
        }
        signatures1.insert(signature);
    }
    
    for (const QString &path : folder2.allFiles) {
        const FileInfo &info = folder2.fileInfo.value(path);
        QString signature = QString("%1x%2_%3").arg(info.imageWidth).arg(info.imageHeight).arg(info.fileSize);
        if (m_currentMode == ComparisonMode::Deep) {
            signature += "_" + info.partialHash;
        }
        signatures2.insert(signature);
    }

    // Calculate Jaccard similarity coefficient
    QSet<QString> intersection = signatures1;
    intersection.intersect(signatures2);

    QSet<QString> unionSet = signatures1;
    unionSet.unite(signatures2);

    if (unionSet.isEmpty()) {
        return 0.0;
    }

    return static_cast<double>(intersection.size()) / unionSet.size();
}

bool DuplicateAnalyzer::areFilesIdentical(const FileInfo &file1, const FileInfo &file2)
{
    // Quick comparison: file size and dimensions
    if (file1.fileSize != file2.fileSize ||
        file1.imageWidth != file2.imageWidth ||
        file1.imageHeight != file2.imageHeight) {
        return false;
    }
    
    // Deep comparison: also check partial hash
    if (m_currentMode == ComparisonMode::Deep) {
        if (file1.partialHash != file2.partialHash) {
            return false;
        }
    }
    
    return true;
}

// === Private Methods - Results Management ===

void DuplicateAnalyzer::addDuplicateIssue(const DuplicateIssue &issue)
{
    m_duplicateIssues.append(issue);
}

void DuplicateAnalyzer::updateIssuesTree()
{
    m_issuesTree->clear();

    if (m_duplicateIssues.isEmpty()) {
        m_issuesCountLabel->setText(MSG_NO_ISSUES);
        return;
    }

    m_issuesCountLabel->setText(QString("%1 duplicate folder issues found (%2 mode)")
                                    .arg(m_duplicateIssues.size())
                                    .arg(getModeName(m_currentMode)));

    for (const DuplicateIssue &issue : m_duplicateIssues) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_issuesTree);

        // Set item data
        item->setText(COL_SEVERITY, issue.severity);
        item->setText(COL_TYPE, getTypeDisplayName(issue.type));
        item->setText(COL_DESCRIPTION, issue.description);
        item->setText(COL_SIMILARITY, QString("%1%").arg(qRound(issue.similarity * 100)));
        item->setText(COL_WASTED_SPACE, formatFileSize(issue.wastedSpace));

        // Set severity icon
        item->setIcon(COL_SEVERITY, getSeverityIcon(issue.severity));

        // Set tooltips
        item->setToolTip(COL_DESCRIPTION, issue.description);
        item->setToolTip(COL_TYPE, getTypeDescription(issue.type));

        // Color coding by severity
        if (issue.severity == SEVERITY_HIGH) {
            item->setBackground(COL_SEVERITY, QBrush(QColor(255, 200, 200))); // Light red
        } else if (issue.severity == SEVERITY_MEDIUM) {
            item->setBackground(COL_SEVERITY, QBrush(QColor(255, 255, 200))); // Light yellow
        } else {
            item->setBackground(COL_SEVERITY, QBrush(QColor(200, 255, 200))); // Light green
        }
    }

    // Sort by severity (High first)
    m_issuesTree->sortItems(COL_SEVERITY, Qt::DescendingOrder);
}

void DuplicateAnalyzer::updateDetailsPanel()
{
    QTreeWidgetItem *item = getCurrentIssueItem();
    if (!item) {
        // No selection - show default message
        m_detailsTitle->setText(MSG_NO_SELECTION);
        m_modeLabel->clear();
        m_primaryFolderLabel->clear();
        m_duplicateFolderLabel->clear();
        m_similarityLabel->clear();
        m_filesCountLabel->clear();
        m_wastedSpaceLabel->clear();
        m_severityLabel->clear();
        return;
    }

    // Get issue details
    int issueIndex = m_issuesTree->indexOfTopLevelItem(item);
    if (issueIndex < 0 || issueIndex >= m_duplicateIssues.size()) {
        return;
    }

    const DuplicateIssue &issue = m_duplicateIssues[issueIndex];

    // Update details panel
    m_detailsTitle->setText(QString("Issue Details - %1").arg(getTypeDisplayName(issue.type)));
    
    m_modeLabel->setText(QString("<b>Analysis Mode:</b> %1").arg(getModeName(m_currentMode)));

    QFileInfo primaryInfo(issue.primaryFolder);
    QFileInfo duplicateInfo(issue.duplicateFolder);

    m_primaryFolderLabel->setText(QString("<b>Primary Folder:</b><br>%1<br><small>%2</small>")
                                      .arg(primaryInfo.fileName())
                                      .arg(issue.primaryFolder));

    m_duplicateFolderLabel->setText(QString("<b>Duplicate Folder:</b><br>%1<br><small>%2</small>")
                                        .arg(duplicateInfo.fileName())
                                        .arg(issue.duplicateFolder));

    m_similarityLabel->setText(QString("<b>Similarity:</b> %1%")
                                   .arg(qRound(issue.similarity * 100)));

    m_filesCountLabel->setText(QString("<b>Files:</b> %1 duplicates out of %2 total")
                                   .arg(issue.duplicateFiles)
                                   .arg(issue.totalFiles));

    m_wastedSpaceLabel->setText(QString("<b>Wasted Space:</b> %1")
                                    .arg(formatFileSize(issue.wastedSpace)));

    m_severityLabel->setText(QString("<b>Severity:</b> %1").arg(issue.severity));
}

QString DuplicateAnalyzer::formatIssueDescription(const DuplicateIssue &issue)
{
    QFileInfo primaryInfo(issue.primaryFolder);
    QFileInfo duplicateInfo(issue.duplicateFolder);

    QString desc;
    switch (issue.type) {
    case DuplicateType::ExactComplete:
        desc = QString("'%1' and '%2' are exact duplicates (same files and folder structure)")
                   .arg(primaryInfo.fileName())
                   .arg(duplicateInfo.fileName());
        break;
    case DuplicateType::ExactFilesOnly:
        desc = QString("'%1' and '%2' contain the same files in different folder structures")
                   .arg(primaryInfo.fileName())
                   .arg(duplicateInfo.fileName());
        break;
    case DuplicateType::PartialDuplicate:
        desc = QString("'%1' and '%2' have %3% file overlap")
                   .arg(primaryInfo.fileName())
                   .arg(duplicateInfo.fileName())
                   .arg(qRound(issue.similarity * 100));
        break;
    }

    return desc;
}

QString DuplicateAnalyzer::formatFileSize(qint64 bytes)
{
    if (bytes >= BYTES_PER_GB) {
        return QString("%1 GB").arg(bytes / (double)BYTES_PER_GB, 0, 'f', 2);
    } else if (bytes >= BYTES_PER_MB) {
        return QString("%1 MB").arg(bytes / (double)BYTES_PER_MB, 0, 'f', 2);
    } else if (bytes >= BYTES_PER_KB) {
        return QString("%1 KB").arg(bytes / (double)BYTES_PER_KB, 0, 'f', 2);
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}

QIcon DuplicateAnalyzer::getSeverityIcon(const QString &severity)
{
    if (severity == SEVERITY_HIGH) {
        return style()->standardIcon(QStyle::SP_MessageBoxCritical);
    } else if (severity == SEVERITY_MEDIUM) {
        return style()->standardIcon(QStyle::SP_MessageBoxWarning);
    } else {
        return style()->standardIcon(QStyle::SP_MessageBoxInformation);
    }
}

QString DuplicateAnalyzer::getTypeDisplayName(DuplicateType type)
{
    switch (type) {
    case DuplicateType::ExactComplete:
        return TYPE_EXACT_COMPLETE;
    case DuplicateType::ExactFilesOnly:
        return TYPE_EXACT_FILES;
    case DuplicateType::PartialDuplicate:
        return TYPE_PARTIAL;
    }
    return "Unknown";
}

QString DuplicateAnalyzer::getTypeDescription(DuplicateType type)
{
    switch (type) {
    case DuplicateType::ExactComplete:
        return "Folders are identical in every way - same files and same folder structure";
    case DuplicateType::ExactFilesOnly:
        return "Folders contain exactly the same image files, but organized differently";
    case DuplicateType::PartialDuplicate:
        return "Folders share 90% or more of their image files";
    }
    return "Unknown duplicate type";
}

QString DuplicateAnalyzer::getModeName(ComparisonMode mode)
{
    return (mode == ComparisonMode::Quick) ? "Quick" : "Deep";
}

// === Private Methods - Utility ===

QStringList DuplicateAnalyzer::getProjectFolders()
{
    if (!m_folderManager) {
        return QStringList();
    }
    
    // Get top-level project folders
    QStringList topLevelFolders = m_folderManager->getAllFolderPaths();
    QStringList allFolders;
    
    // Recursively collect all subfolders from each top-level folder
    for (const QString &topFolder : topLevelFolders) {
        // Add the top-level folder itself
        allFolders.append(topFolder);
        
        // Recursively collect all subfolders
        collectSubfoldersRecursive(topFolder, allFolders);
    }
    
    return allFolders;
}

void DuplicateAnalyzer::collectSubfoldersRecursive(const QString &parentPath, QStringList &folderList)
{
    QDir dir(parentPath);
    if (!dir.exists()) {
        return;
    }
    
    // Get all subdirectories
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    
    for (const QString &subDirName : subDirs) {
        QString subDirPath = dir.absoluteFilePath(subDirName);
        
        // Add this subfolder to the list
        folderList.append(subDirPath);
        
        // Recursively process this subfolder's subfolders
        collectSubfoldersRecursive(subDirPath, folderList);
    }
}

void DuplicateAnalyzer::openFolderInExplorer(const QString &folderPath)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
}

QString DuplicateAnalyzer::getRelativePath(const QString &fullPath, const QString &basePath)
{
    QDir baseDir(basePath);
    return baseDir.relativeFilePath(fullPath);
}

QTreeWidgetItem* DuplicateAnalyzer::getCurrentIssueItem()
{
    QList<QTreeWidgetItem*> selectedItems = m_issuesTree->selectedItems();
    return selectedItems.isEmpty() ? nullptr : selectedItems.first();
}

// === Cache Management Methods ===

void DuplicateAnalyzer::saveFolderContentCache()
{
    if (!m_projectManager || !m_projectManager->hasOpenProject()) {
        return;
    }

    QString projectPath = m_projectManager->currentProjectPath();
    QString cacheFilePath = projectPath + "/.folder_analysis_cache";

    QFile cacheFile(cacheFilePath);
    if (!cacheFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save folder content cache:" << cacheFilePath;
        return;
    }

    QDataStream stream(&cacheFile);
    stream.setVersion(QDataStream::Qt_6_0);

    // Write cache version and metadata
    stream << QString("FolderContentCache_v2.0"); // New version for FileInfo struct
    stream << static_cast<qint32>(m_currentMode);
    stream << static_cast<qint32>(m_folderContentCache.size());

    // Write each cached folder
    for (auto it = m_folderContentCache.constBegin(); it != m_folderContentCache.constEnd(); ++it) {
        const QString &folderPath = it.key();
        const FolderContent &content = it.value();

        // Get folder modification time for cache validation
        QFileInfo folderInfo(folderPath);
        QDateTime folderModTime = folderInfo.lastModified();

        stream << folderPath;
        stream << folderModTime;
        stream << content.allFiles;
        stream << content.allSubfolders;
        stream << content.totalSize;
        
        // Write FileInfo map
        stream << static_cast<qint32>(content.fileInfo.size());
        for (auto infoIt = content.fileInfo.constBegin(); infoIt != content.fileInfo.constEnd(); ++infoIt) {
            stream << infoIt.key();
            stream << infoIt.value().fileSize;
            stream << infoIt.value().imageWidth;
            stream << infoIt.value().imageHeight;
            stream << infoIt.value().partialHash;
        }
    }

    qDebug() << "Saved folder content cache with" << m_folderContentCache.size() << "entries";
}

void DuplicateAnalyzer::loadFolderContentCache()
{
    if (!m_projectManager || !m_projectManager->hasOpenProject()) {
        return;
    }

    QString projectPath = m_projectManager->currentProjectPath();
    QString cacheFilePath = projectPath + "/.folder_analysis_cache";

    QFile cacheFile(cacheFilePath);
    if (!cacheFile.exists() || !cacheFile.open(QIODevice::ReadOnly)) {
        qDebug() << "No folder content cache found or failed to open:" << cacheFilePath;
        return;
    }

    QDataStream stream(&cacheFile);
    stream.setVersion(QDataStream::Qt_6_0);

    // Read and validate cache version
    QString version;
    stream >> version;
    if (version != "FolderContentCache_v2.0") {
        qDebug() << "Invalid cache version:" << version << "- clearing cache";
        return;
    }

    // Read cache metadata
    qint32 cachedMode;
    stream >> cachedMode;

    // Read cache size
    qint32 cacheSize;
    stream >> cacheSize;

    int validEntries = 0;
    int invalidEntries = 0;

    // Read each cached folder
    for (int i = 0; i < cacheSize; ++i) {
        QString folderPath;
        QDateTime cachedModTime;
        FolderContent content;

        stream >> folderPath;
        stream >> cachedModTime;
        stream >> content.allFiles;
        stream >> content.allSubfolders;
        stream >> content.totalSize;
        
        // Read FileInfo map
        qint32 fileInfoSize;
        stream >> fileInfoSize;
        for (int j = 0; j < fileInfoSize; ++j) {
            QString filePath;
            FileInfo info;
            stream >> filePath;
            stream >> info.fileSize;
            stream >> info.imageWidth;
            stream >> info.imageHeight;
            stream >> info.partialHash;
            content.fileInfo[filePath] = info;
        }

        // Validate cache entry
        if (isFolderContentCacheValid(folderPath, content)) {
            QFileInfo folderInfo(folderPath);
            if (folderInfo.exists() && folderInfo.lastModified() <= cachedModTime.addSecs(1)) {
                m_folderContentCache[folderPath] = content;
                validEntries++;
            } else {
                invalidEntries++;
                qDebug() << "Cache entry invalid (folder modified):" << folderPath;
            }
        } else {
            invalidEntries++;
            qDebug() << "Cache entry invalid (content check failed):" << folderPath;
        }
    }

    qDebug() << "Loaded folder content cache:" << validEntries << "valid," << invalidEntries << "invalid entries";
}

QString DuplicateAnalyzer::getCacheKey(const QString &folderPath) const
{
    QFileInfo folderInfo(folderPath);
    return QString("%1_%2").arg(folderInfo.absoluteFilePath()).arg(folderInfo.lastModified().toSecsSinceEpoch());
}

bool DuplicateAnalyzer::isFolderContentCacheValid(const QString &folderPath, const FolderContent &content) const
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        return false;
    }

    // Basic sanity check - if folder has files but cache shows empty, it's invalid
    QStringList currentFiles = dir.entryList(QDir::Files, QDir::Name);
    bool hasImageFiles = false;
    for (const QString &fileName : currentFiles) {
        QFileInfo fileInfo(dir.absoluteFilePath(fileName));
        if (SUPPORTED_EXTENSIONS.contains(fileInfo.suffix().toLower())) {
            hasImageFiles = true;
            break;
        }
    }

    // If we found image files but cache is empty, or vice versa, cache is likely invalid
    if (hasImageFiles && content.allFiles.isEmpty()) {
        return false;
    }

    return true;
}

int DuplicateAnalyzer::countFilesInFolder(const QString &folderPath)
{
    int count = 0;
    QDir dir(folderPath);

    if (!dir.exists()) {
        return 0;
    }

    // Count files in current directory
    QStringList files = dir.entryList(QDir::Files, QDir::Name);
    for (const QString &fileName : files) {
        QFileInfo fileInfo(dir.absoluteFilePath(fileName));
        if (SUPPORTED_EXTENSIONS.contains(fileInfo.suffix().toLower())) {
            count++;
        }
    }

    // Recursively count files in subdirectories
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &subDirName : subDirs) {
        QString subDirPath = dir.absoluteFilePath(subDirName);
        count += countFilesInFolder(subDirPath);
    }

    return count;
}

void DuplicateAnalyzer::updateFileProgress()
{
    // Update progress and status during file analysis phase (10-70%)
    if (m_totalFilesToAnalyze > 0) {
        // Clamp values to prevent overflow
        int filesAnalyzed = qMin(m_filesAnalyzed, m_totalFilesToAnalyze);

        int fileProgress = 10 + (filesAnalyzed * 60) / m_totalFilesToAnalyze;
        fileProgress = qMin(fileProgress, 70); // Don't exceed 70% during this phase

        m_progressBar->setValue(fileProgress);

        // Calculate percentage safely
        int percentage = (filesAnalyzed * 100) / m_totalFilesToAnalyze;
        percentage = qMin(percentage, 100); // Cap at 100%

        // Update status with file count
        QString statusText = QString("%1 analysis: %2/%3 files (%4%)")
                                 .arg(getModeName(m_currentMode))
                                 .arg(filesAnalyzed)
                                 .arg(m_totalFilesToAnalyze)
                                 .arg(percentage);
        m_statusLabel->setText(statusText);

        // Console progress bar (like tqdm)
        int barWidth = 50;
        float progress = (float)filesAnalyzed / m_totalFilesToAnalyze;
        progress = qMin(progress, 1.0f); // Cap at 100%

        int pos = qRound(barWidth * progress);
        pos = qMin(pos, barWidth); // Make sure we don't exceed bar width

        QString progressBar = "[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) progressBar += "=";
            else if (i == pos && pos < barWidth) progressBar += ">";
            else progressBar += " ";
        }
        progressBar += QString("] %1/%2 (%3%)")
                           .arg(filesAnalyzed)
                           .arg(m_totalFilesToAnalyze)
                           .arg(percentage);

        // Print progress on same line (like tqdm)
        printf("\r%s", progressBar.toLocal8Bit().constData());
        fflush(stdout);

        // Force UI update EVERY file
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);

        // Also force repaint of progress bar and status
        m_progressBar->repaint();
        m_statusLabel->repaint();
    } else {
        // Fallback if no files to analyze
        m_statusLabel->setText("All folders are cached - no files to analyze");
        QApplication::processEvents();
        m_statusLabel->repaint();
    }
}
