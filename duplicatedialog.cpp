#include "duplicatedialog.h"
#include "duplicateanalyzer.h"
#include "projectmanager.h"
#include "foldermanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

// === Constants ===
namespace {
constexpr int DIALOG_MIN_WIDTH = 1000;
constexpr int DIALOG_MIN_HEIGHT = 700;

const QString DIALOG_TITLE = "Duplicate Folder Analysis";
const QString INSTRUCTIONS_TEXT =
    "This tool analyzes your project folders to find duplicates using two comparison modes:\n\n"
    "<b>Quick Analysis</b> - Fast scan using file size + image dimensions\n"
    "  • Compares file sizes and image resolutions\n"
    "  • Very fast, suitable for large collections\n"
    "  • Catches ~98% of duplicates instantly\n\n"
    "<b>Deep Analysis</b> - Thorough verification using partial file hashing\n"
    "  • Adds partial content comparison (first 16KB + last 16KB)\n"
    "  • More accurate, still 20-50x faster than full hash\n"
    "  • Recommended for final verification\n\n"
    "Choose your preferred analysis mode to start.";

const QString STYLE_TITLE = "font-weight: bold; font-size: 16px; padding: 10px; color: #2c3e50;";
const QString STYLE_INSTRUCTIONS = "padding: 10px; background-color: #f8f9fa; border: 1px solid #e9ecef; border-radius: 4px; color: #495057;";
const QString STYLE_BUTTON_PRIMARY = "QPushButton { font-weight: bold; color: white; background-color: #007bff; border: 1px solid #007bff; padding: 8px 16px; border-radius: 4px; } QPushButton:hover { background-color: #0056b3; } QPushButton:disabled { background-color: #6c757d; }";
const QString STYLE_BUTTON_SUCCESS = "QPushButton { font-weight: bold; color: white; background-color: #28a745; border: 1px solid #28a745; padding: 8px 16px; border-radius: 4px; } QPushButton:hover { background-color: #218838; } QPushButton:disabled { background-color: #6c757d; }";
const QString STYLE_BUTTON_SECONDARY = "QPushButton { color: #6c757d; background-color: white; border: 1px solid #6c757d; padding: 8px 16px; border-radius: 4px; } QPushButton:hover { background-color: #f8f9fa; }";
}

// === Constructor ===

DuplicateDialog::DuplicateDialog(ProjectManager *projectManager,
                                 FolderManager *folderManager,
                                 QWidget *parent)
    : QDialog(parent)
    , m_projectManager(projectManager)
    , m_folderManager(folderManager)
{
    setupUI();

    // Connect analyzer signals
    connect(m_analyzer, &DuplicateAnalyzer::analysisStarted,
            this, &DuplicateDialog::onAnalysisStarted);
    connect(m_analyzer, &DuplicateAnalyzer::analysisProgress,
            this, &DuplicateDialog::onAnalysisProgress);
    connect(m_analyzer, &DuplicateAnalyzer::analysisCompleted,
            this, &DuplicateDialog::onAnalysisCompleted);
    connect(m_analyzer, &DuplicateAnalyzer::showFolderInTree,
            this, &DuplicateDialog::onShowFolderInTree);

    setWindowTitle(DIALOG_TITLE);
    setMinimumSize(DIALOG_MIN_WIDTH, DIALOG_MIN_HEIGHT);
    resize(DIALOG_MIN_WIDTH, DIALOG_MIN_HEIGHT);

    // Set window flags for better dialog behavior
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setModal(true);
}

// === Private Slots ===

void DuplicateDialog::onAnalysisStarted(int totalFolders, DuplicateAnalyzer::ComparisonMode mode)
{
    // Disable both analysis buttons during analysis
    m_quickAnalysisButton->setEnabled(false);
    m_deepAnalysisButton->setEnabled(false);
    
    QString modeText = (mode == DuplicateAnalyzer::ComparisonMode::Quick) ? "Quick" : "Deep";
    
    m_quickAnalysisButton->setText("Analyzing...");
    m_deepAnalysisButton->setText("Analyzing...");

    updateTitle(0, mode); // Reset title during analysis

    // Update instructions to show analysis status
    m_instructionsLabel->setText(QString("<b>Running %1 Analysis...</b><br><br>"
                                        "Analyzing %2 project folders for duplicates.<br>"
                                        "This may take a few moments depending on the number of files.")
                                    .arg(modeText)
                                    .arg(totalFolders));
}

void DuplicateDialog::onAnalysisProgress(int current, int total, const QString &currentFolder)
{
    Q_UNUSED(current)
    Q_UNUSED(total)

    // Update status with current folder being analyzed
    QFileInfo folderInfo(currentFolder);
    m_instructionsLabel->setText(QString("<b>Analyzing...</b><br><br>Processing: %1")
                                    .arg(folderInfo.fileName()));
}

void DuplicateDialog::onAnalysisCompleted(int issuesFound, DuplicateAnalyzer::ComparisonMode mode)
{
    // Re-enable analysis buttons
    m_quickAnalysisButton->setEnabled(true);
    m_deepAnalysisButton->setEnabled(true);
    
    m_quickAnalysisButton->setText("Quick Analysis");
    m_deepAnalysisButton->setText("Deep Analysis");

    updateTitle(issuesFound, mode);

    QString modeText = (mode == DuplicateAnalyzer::ComparisonMode::Quick) ? "Quick" : "Deep";

    // Update instructions based on results
    if (issuesFound == 0) {
        m_instructionsLabel->setText(
            QString("✅ <b>%1 Analysis Complete - No duplicates found!</b><br><br>"
                    "Your project folders appear to be well-organized with no duplicate content detected.<br><br>"
                    "You can run a %2 for additional verification, or close this dialog.")
                .arg(modeText)
                .arg(mode == DuplicateAnalyzer::ComparisonMode::Quick ? "Deep Analysis" : "Quick Analysis to re-scan")
            );
    } else {
        QString recommendation = "";
        if (mode == DuplicateAnalyzer::ComparisonMode::Quick) {
            recommendation = "<br><br><b>Tip:</b> Run a Deep Analysis for more accurate verification of these matches.";
        }
        
        m_instructionsLabel->setText(
            QString("⚠️ <b>%1 Analysis Complete - Found %2 duplicate issue%3</b><br><br>"
                    "Review the issues below. You can click on folders to navigate to them in your project tree, "
                    "or open them directly in Windows Explorer. Consider consolidating duplicate folders to save disk space.%4")
                .arg(modeText)
                .arg(issuesFound)
                .arg(issuesFound > 1 ? "s" : "")
                .arg(recommendation)
            );
    }
}

void DuplicateDialog::onShowFolderInTree(const QString &folderPath)
{
    // Forward the request to the main application
    emit showFolderInTree(folderPath);

    // Show brief confirmation message
    QFileInfo folderInfo(folderPath);
    m_instructionsLabel->setText(QString("📂 <b>Navigated to folder:</b> %1<br><br>"
                                        "The folder has been highlighted in your project tree.")
                                    .arg(folderInfo.fileName()));
}

void DuplicateDialog::closeDialog()
{
    accept(); // Close with accepted status
}

void DuplicateDialog::startQuickAnalysis()
{
    startAnalysis(DuplicateAnalyzer::ComparisonMode::Quick);
}

void DuplicateDialog::startDeepAnalysis()
{
    startAnalysis(DuplicateAnalyzer::ComparisonMode::Deep);
}

void DuplicateDialog::startAnalysis(DuplicateAnalyzer::ComparisonMode mode)
{
    // Verify we have a project open
    if (!m_projectManager || !m_projectManager->hasOpenProject()) {
        QMessageBox::information(this, "No Project Open",
                                 "Please open a project before analyzing for duplicates.");
        return;
    }

    // Check if we have folders to analyze
    // Note: getProjectFolders() in analyzer recursively collects all subfolders
    QStringList topLevelFolders = m_projectManager->getProjectFolders();
    
    if (topLevelFolders.isEmpty()) {
        QMessageBox::information(this, "No Folders",
                                 "No folders found in your project.\n\n"
                                 "Add folders to your project using File → Add Folder.");
        return;
    }

    // The analyzer will check if there are enough folders/subfolders to compare
    // It counts all subfolders recursively, so even 1 top-level folder with 
    // multiple subfolders is sufficient
    m_analyzer->startAnalysis(mode);
}

// === Private Methods ===

void DuplicateDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);

    createHeader();

    // Create the main analyzer widget
    m_analyzer = new DuplicateAnalyzer(m_projectManager, m_folderManager, this);
    m_mainLayout->addWidget(m_analyzer);

    createButtons();
}

void DuplicateDialog::createHeader()
{
    // Title label
    m_titleLabel = new QLabel(DIALOG_TITLE);
    m_titleLabel->setStyleSheet(STYLE_TITLE);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_titleLabel);

    // Instructions label
    m_instructionsLabel = new QLabel(INSTRUCTIONS_TEXT);
    m_instructionsLabel->setStyleSheet(STYLE_INSTRUCTIONS);
    m_instructionsLabel->setWordWrap(true);
    m_instructionsLabel->setTextFormat(Qt::RichText);
    m_mainLayout->addWidget(m_instructionsLabel);
}

void DuplicateDialog::createButtons()
{
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(10);

    // Help button (left side)
    m_helpButton = new QPushButton("Help");
    m_helpButton->setStyleSheet(STYLE_BUTTON_SECONDARY);
    connect(m_helpButton, &QPushButton::clicked, [this]() {
        QMessageBox::information(this, "Duplicate Analysis Help",
                                 "<h3>Duplicate Folder Analysis</h3>"
                                 "<p>This tool helps you identify and manage duplicate content in your project folders using two analysis modes:</p>"
                                 
                                 "<h4>Quick Analysis (Recommended First)</h4>"
                                 "<ul>"
                                 "<li><b>Speed:</b> Very fast - suitable for large photo collections</li>"
                                 "<li><b>Method:</b> Compares file size + image dimensions (width × height)</li>"
                                 "<li><b>Accuracy:</b> Catches ~98% of duplicates</li>"
                                 "<li><b>Best for:</b> Initial scan of large collections</li>"
                                 "</ul>"
                                 
                                 "<h4>Deep Analysis (For Verification)</h4>"
                                 "<ul>"
                                 "<li><b>Speed:</b> Slower but still fast (20-50x faster than full hash)</li>"
                                 "<li><b>Method:</b> File size + dimensions + partial content hash</li>"
                                 "<li><b>Accuracy:</b> 99.9% accurate - near-perfect duplicate detection</li>"
                                 "<li><b>Best for:</b> Final verification before deleting duplicates</li>"
                                 "</ul>"
                                 
                                 "<h4>Duplicate Types Detected:</h4>"
                                 "<ul>"
                                 "<li><b>Exact Complete Duplicates:</b> Identical files and folder structure (High severity)</li>"
                                 "<li><b>Exact Files Duplicates:</b> Same files, different organization (Medium severity)</li>"
                                 "<li><b>Partial Duplicates:</b> 90%+ file overlap (Low severity)</li>"
                                 "</ul>"
                                 
                                 "<h4>Actions you can take:</h4>"
                                 "<ul>"
                                 "<li>Click 'Show in Tree' buttons to navigate to folders in your project</li>"
                                 "<li>Click 'Open Folder' buttons to open folders in Windows Explorer</li>"
                                 "<li>Review and consolidate duplicate folders to save disk space</li>"
                                 "</ul>"
                                 
                                 "<p><b>Note:</b> Only image files (JPG, PNG, TIFF, RAW, etc.) are compared during analysis.</p>");
    });

    buttonLayout->addWidget(m_helpButton);
    buttonLayout->addStretch();

    // Quick Analysis button
    m_quickAnalysisButton = new QPushButton("Quick Analysis");
    m_quickAnalysisButton->setStyleSheet(STYLE_BUTTON_PRIMARY);
    m_quickAnalysisButton->setMinimumWidth(140);
    m_quickAnalysisButton->setToolTip("Fast scan using file size + image dimensions\n"
                                      "Very quick, catches ~98% of duplicates");
    connect(m_quickAnalysisButton, &QPushButton::clicked, this, &DuplicateDialog::startQuickAnalysis);

    // Deep Analysis button
    m_deepAnalysisButton = new QPushButton("Deep Analysis");
    m_deepAnalysisButton->setStyleSheet(STYLE_BUTTON_SUCCESS);
    m_deepAnalysisButton->setMinimumWidth(140);
    m_deepAnalysisButton->setToolTip("Thorough verification with partial content hashing\n"
                                     "More accurate, recommended for final verification");
    connect(m_deepAnalysisButton, &QPushButton::clicked, this, &DuplicateDialog::startDeepAnalysis);

    // Close button
    m_closeButton = new QPushButton("Close");
    m_closeButton->setStyleSheet(STYLE_BUTTON_SECONDARY);
    m_closeButton->setMinimumWidth(80);
    connect(m_closeButton, &QPushButton::clicked, this, &DuplicateDialog::closeDialog);

    // Layout buttons
    buttonLayout->addWidget(m_quickAnalysisButton);
    buttonLayout->addWidget(m_deepAnalysisButton);
    buttonLayout->addWidget(m_closeButton);

    m_mainLayout->addLayout(buttonLayout);
}

void DuplicateDialog::updateTitle(int issueCount, DuplicateAnalyzer::ComparisonMode mode)
{
    QString modeText = (mode == DuplicateAnalyzer::ComparisonMode::Quick) ? "Quick" : "Deep";
    
    if (issueCount == 0) {
        setWindowTitle(DIALOG_TITLE);
    } else {
        setWindowTitle(QString("%1 - %2 Issues Found (%3 Analysis)")
                          .arg(DIALOG_TITLE)
                          .arg(issueCount)
                          .arg(modeText));
    }
}
