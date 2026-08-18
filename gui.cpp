#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QToolButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QProgressBar>
#include <QApplication>
#include <QClipboard>
#include <QStyle>
#include <QSizePolicy>
#include <QFontDatabase>
#include <QPalette>
#include <QTimer>
#include <QMetaObject>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QIcon>

#include <atomic>
#include <thread>
#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <cstring>
#include <iostream>
#include <streambuf>
#include <exception>

// -----------------------------------------------------------------------------
// Embed the CLI implementation.
// -----------------------------------------------------------------------------

#define main vmfpropmerger_cli_main
#include "src/main.cpp"
#undef main

#include "src/utils.cpp"
#include "src/options.cpp"
#include "src/vmf.cpp"
#include "src/smd.cpp"
#include "src/vpk.cpp"
#include "src/transform.cpp"

using namespace vmfpropmerger;

namespace {

// -----------------------------------------------------------------------------
// Application constants
// -----------------------------------------------------------------------------

constexpr int MINIMUM_WIDTH  = 1050;
constexpr int MINIMUM_HEIGHT = 700;

constexpr int DEFAULT_TRIANGLES = 12000;
constexpr int DEFAULT_CONVEX    = 4096;

constexpr double DEFAULT_SCALE = 1.0;
constexpr double DEFAULT_FPS   = 1.0;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

std::string narrow(const QString& value)
{
    return value.toUtf8().toStdString();
}

QString widen(const std::string& value)
{
    return QString::fromUtf8(value.c_str());
}

QStringList splitLines(const QString& text)
{
    QStringList result;

    const QStringList lines =
        text.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                   Qt::SkipEmptyParts);

    for (QString line : lines) {
        line = line.trimmed();

        if (!line.isEmpty())
            result.push_back(line);
    }

    return result;
}

QString quoteCommandArgument(const QString& input)
{
    QString result = QStringLiteral("\"");

    for (const QChar c : input) {
        if (c == QLatin1Char('"'))
            result += QStringLiteral("\\\"");

        else if (c == QLatin1Char('\\'))
            result += QStringLiteral("\\\\");

        else
            result += c;
    }

    result += QStringLiteral("\"");

    return result;
}

// -----------------------------------------------------------------------------
// Stream redirection
//
// The CLI implementation writes to std::cout/std::cerr. This stream buffer
// forwards output back to the Qt GUI thread safely.
// -----------------------------------------------------------------------------

class GuiStreamBuffer final : public std::streambuf
{
public:
    explicit GuiStreamBuffer(std::function<void(const QString&)> callback)
        : m_callback(std::move(callback))
    {
    }

protected:
    int overflow(int ch = EOF) override
    {
        if (ch == EOF) {
            flushPending();
            return 0;
        }

        m_pending.push_back(static_cast<char>(ch));

        if (ch == '\n' || m_pending.size() >= 4096)
            flushPending();

        return ch;
    }

    std::streamsize xsputn(const char* data,
                           std::streamsize count) override
    {
        for (std::streamsize i = 0; i < count; ++i)
            overflow(static_cast<unsigned char>(data[i]));

        return count;
    }

    int sync() override
    {
        flushPending();
        return 0;
    }

private:
    void flushPending()
    {
        if (m_pending.empty())
            return;

        const QString message = QString::fromUtf8(
            m_pending.data(),
            static_cast<int>(m_pending.size())
        );

        m_pending.clear();

        if (m_callback)
            m_callback(message);
    }

private:
    std::string m_pending;
    std::function<void(const QString&)> m_callback;
};

// -----------------------------------------------------------------------------
// Main window
// -----------------------------------------------------------------------------

class MainWindow final : public QMainWindow
{
public:
    MainWindow()
    {
        setWindowTitle(
            QStringLiteral("Reactive Drop — VMF Prop Merger")
        );

        setMinimumSize(MINIMUM_WIDTH, MINIMUM_HEIGHT);
        resize(1400, 900);

        buildInterface();
        applyTheme();
        setupRequiredValidation();
        resetDefaults();
        
        statusBar()->showMessage(
            QStringLiteral("Ready")
        );
    }

    ~MainWindow() override
    {
        if (m_worker.joinable())
            m_worker.join();
    }

protected:
    void closeEvent(QCloseEvent* event) override
    {
        if (m_running.load()) {
            const auto result = QMessageBox::warning(
                this,
                QStringLiteral("Merge in progress"),
                QStringLiteral(
                    "A merge operation is currently running.\n\n"
                    "Closing the application may leave external tools "
                    "such as Crowbar or StudioMDL running.\n\n"
                    "Close anyway?"
                ),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );

            if (result != QMessageBox::Yes) {
                event->ignore();
                return;
            }
        }

        event->accept();
    }

private:


    void setupRequiredValidation()
    {
        connect(
            m_vmfEdit,
            &QLineEdit::textChanged,
            this,
            [this](const QString&) {
                markRequired(
                    m_vmfEdit,
                    !m_vmfEdit->text().trimmed().isEmpty()
                );
            }
        );

        connect(
            m_gameEdit,
            &QLineEdit::textChanged,
            this,
            [this](const QString&) {
                markRequired(
                    m_gameEdit,
                    !m_gameEdit->text().trimmed().isEmpty()
                );
            }
        );

        connect(
            m_crowbarEdit,
            &QLineEdit::textChanged,
            this,
            [this](const QString&) {
                markRequired(
                    m_crowbarEdit,
                    !m_crowbarEdit->text().trimmed().isEmpty()
                );
            }
        );
    }
    // -------------------------------------------------------------------------
    // Interface construction
    // -------------------------------------------------------------------------

    void buildInterface()
    {
        auto* central = new QWidget(this);
        central->setObjectName(QStringLiteral("centralWidget"));

        auto* rootLayout = new QVBoxLayout(central);
        rootLayout->setContentsMargins(18, 16, 18, 12);
        rootLayout->setSpacing(12);

        // Header --------------------------------------------------------------

        auto* header = new QFrame(central);
        header->setObjectName(QStringLiteral("header"));

        auto* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(18, 15, 18, 15);
        headerLayout->setSpacing(14);

        auto* titleColumn = new QVBoxLayout();
        titleColumn->setSpacing(3);

        auto* title = new QLabel(
            QStringLiteral("VMF Prop Merger"),
            header
        );

        title->setObjectName(QStringLiteral("appTitle"));

        auto* subtitle = new QLabel(
            QStringLiteral(
                "Reactive Drop • merge prop_dynamic geometry "
                "using the full CLI pipeline"
            ),
            header
        );

        subtitle->setObjectName(QStringLiteral("appSubtitle"));

        titleColumn->addWidget(title);
        titleColumn->addWidget(subtitle);

        headerLayout->addLayout(titleColumn);
        headerLayout->addStretch();

        auto* version = new QLabel(
            QStringLiteral("Made by: vq"),
            header
        );

        version->setObjectName(QStringLiteral("versionBadge"));

        headerLayout->addWidget(version);

        rootLayout->addWidget(header);

        // Main splitter -------------------------------------------------------

        auto* splitter = new QSplitter(Qt::Horizontal, central);
        splitter->setObjectName(QStringLiteral("mainSplitter"));
        splitter->setChildrenCollapsible(false);
        splitter->setMinimumWidth(1040);

        // Left: configuration -----------------------------------------------

        auto* optionsScroll = new QScrollArea(splitter);
        optionsScroll->setWidgetResizable(true);
        optionsScroll->setFrameShape(QFrame::NoFrame);
        optionsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        // Prevent the configuration panel from collapsing underneath the output panel.
        optionsScroll->setMinimumWidth(620);
        optionsScroll->setSizePolicy(
            QSizePolicy::MinimumExpanding,
            QSizePolicy::Expanding
        );
        auto* optionsContainer = new QWidget();
        optionsContainer->setObjectName(
            QStringLiteral("optionsContainer")
        );
        optionsContainer->setMinimumWidth(600);

        auto* optionsLayout = new QVBoxLayout(optionsContainer);
        optionsLayout->setContentsMargins(4, 4, 10, 4);
        optionsLayout->setSpacing(12);

        createRequiredSection(optionsLayout);
        createToolchainSection(optionsLayout);
        createOutputSection(optionsLayout);
        createFilterSection(optionsLayout);
        createTransformSection(optionsLayout);
        createCompileSection(optionsLayout);
        createAdvancedSection(optionsLayout);

        optionsLayout->addStretch();

        optionsScroll->setWidget(optionsContainer);

        // Right: output -------------------------------------------------------

        auto* outputPanel = new QFrame(splitter);
        outputPanel->setObjectName(QStringLiteral("outputPanel"));

        outputPanel->setMinimumWidth(420);
        outputPanel->setSizePolicy(
            QSizePolicy::MinimumExpanding,
            QSizePolicy::Expanding
        );

        auto* outputLayout = new QVBoxLayout(outputPanel);
        outputLayout->setContentsMargins(0, 0, 0, 0);
        outputLayout->setSpacing(8);

        auto* outputHeader = new QHBoxLayout();

        auto* outputTitle = new QLabel(
            QStringLiteral("Build Output"),
            outputPanel
        );

        outputTitle->setObjectName(QStringLiteral("sectionTitle"));

        outputHeader->addWidget(outputTitle);
        outputHeader->addStretch();

        m_clearButton = new QPushButton(
            QStringLiteral("Clear"),
            outputPanel
        );

        m_clearButton->setObjectName(
            QStringLiteral("secondaryButton")
        );

        m_clearButton->setToolTip(
            QStringLiteral("Clear the build output")
        );

        connect(
            m_clearButton,
            &QPushButton::clicked,
            this,
            [this]() {
                m_log->clear();
            }
        );

        outputHeader->addWidget(m_clearButton);

        m_copyCommandButton = new QPushButton(
            QStringLiteral("Copy Command"),
            outputPanel
        );

        m_copyCommandButton->setObjectName(
            QStringLiteral("secondaryButton")
        );

        connect(
            m_copyCommandButton,
            &QPushButton::clicked,
            this,
            [this]() {
                QApplication::clipboard()->setText(
                    makeCommandLine()
                );

                statusBar()->showMessage(
                    QStringLiteral("CLI command copied to clipboard"),
                    2500
                );
            }
        );

        outputHeader->addWidget(m_copyCommandButton);

        outputLayout->addLayout(outputHeader);

        m_log = new QPlainTextEdit(outputPanel);
        m_log->setObjectName(QStringLiteral("console"));
        m_log->setReadOnly(true);
        m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_log->setPlaceholderText(
            QStringLiteral(
                "CLI output will appear here..."
            )
        );

        QFont consoleFont =
            QFontDatabase::systemFont(QFontDatabase::FixedFont);

        consoleFont.setPointSize(10);
        m_log->setFont(consoleFont);

        outputLayout->addWidget(m_log, 1);

        // Output status -------------------------------------------------------

        auto* outputInfo = new QLabel(
            QStringLiteral(
                "The GUI runs the same embedded CLI implementation "
                "used by the command-line application."
            ),
            outputPanel
        );

        outputInfo->setObjectName(
            QStringLiteral("outputHint")
        );

        outputInfo->setWordWrap(true);

        outputLayout->addWidget(outputInfo);

        splitter->addWidget(optionsScroll);
        splitter->addWidget(outputPanel);

        // Responsive 60/40-ish split.
        splitter->setStretchFactor(0, 6);
        splitter->setStretchFactor(1, 4);

        splitter->setSizes({
            780,
            520
        });

        rootLayout->addWidget(splitter, 1);

        // Bottom action bar ---------------------------------------------------

        auto* actionBar = new QFrame(central);
        actionBar->setObjectName(QStringLiteral("actionBar"));

        auto* actionLayout = new QHBoxLayout(actionBar);
        actionLayout->setContentsMargins(12, 9, 12, 9);
        actionLayout->setSpacing(8);

        m_statusLabel = new QLabel(
            QStringLiteral("Ready"),
            actionBar
        );

        m_statusLabel->setObjectName(
            QStringLiteral("statusLabel")
        );

        actionLayout->addWidget(m_statusLabel);
        actionLayout->addStretch();

        m_resetButton = new QPushButton(
            QStringLiteral("Reset"),
            actionBar
        );

        m_resetButton->setObjectName(
            QStringLiteral("secondaryButton")
        );

        connect(
            m_resetButton,
            &QPushButton::clicked,
            this,
            [this]() {
                resetDefaults();
                statusBar()->showMessage(
                    QStringLiteral("Options reset"),
                    2000
                );
            }
        );

        actionLayout->addWidget(m_resetButton);

        m_runButton = new QPushButton(
            QStringLiteral("MERGE"),
            actionBar
        );

        m_runButton->setObjectName(
            QStringLiteral("primaryButton")
        );

        m_runButton->setMinimumWidth(130);
        m_runButton->setMinimumHeight(38);

        connect(
            m_runButton,
            &QPushButton::clicked,
            this,
            [this]() {
                startRun();
            }
        );

        actionLayout->addWidget(m_runButton);

        rootLayout->addWidget(actionBar);

        setCentralWidget(central);
    }

    // -------------------------------------------------------------------------
    // Required configuration
    // -------------------------------------------------------------------------

    void createRequiredSection(QVBoxLayout* parent)
    {
        auto* group = new QGroupBox(
            QStringLiteral("Required Configuration"),
            this
        );

        group->setObjectName(
            QStringLiteral("requiredGroup")
        );

        auto* layout = new QGridLayout(group);
        layout->setContentsMargins(14, 18, 14, 14);
        layout->setHorizontalSpacing(8);
        layout->setVerticalSpacing(9);

        m_vmfEdit = createPathEdit(
            layout,
            0,
            QStringLiteral("VMF file"),
            QStringLiteral("Input .vmf file")
        );

        addBrowseButton(
            layout,
            0,
            m_vmfEdit,
            QStringLiteral("VMF files (*.vmf);;All files (*.*)"),
            false
        );

        m_gameEdit = createPathEdit(
            layout,
            1,
            QStringLiteral("Game root"),
            QStringLiteral("Reactive Drop installation directory")
        );

        addBrowseButton(
            layout,
            1,
            m_gameEdit,
            QString(),
            true
        );

        m_crowbarEdit = createPathEdit(
            layout,
            2,
            QStringLiteral("Crowbar"),
            QStringLiteral("Path to Crowbar executable")
        );

        addBrowseButton(
            layout,
            2,
            m_crowbarEdit,
            QStringLiteral("Executables (*.exe);;All files (*.*)"),
            false
        );

        parent->addWidget(group);
    }

    // -------------------------------------------------------------------------
    // Toolchain
    // -------------------------------------------------------------------------

    void createToolchainSection(QVBoxLayout* parent)
    {
        auto* group = new QGroupBox(
            QStringLiteral("Toolchain"),
            this
        );

        auto* layout = new QGridLayout(group);
        layout->setContentsMargins(14, 18, 14, 14);
        layout->setHorizontalSpacing(8);
        layout->setVerticalSpacing(9);

        m_studiomdlEdit = createPathEdit(
            layout,
            0,
            QStringLiteral("StudioMDL"),
            QStringLiteral("Optional StudioMDL executable")
        );

        addBrowseButton(
            layout,
            0,
            m_studiomdlEdit,
            QStringLiteral("Executables (*.exe);;All files (*.*)"),
            false
        );

        m_workEdit = createPathEdit(
            layout,
            1,
            QStringLiteral("Work directory"),
            QStringLiteral("Optional temporary working directory")
        );

        addBrowseButton(
            layout,
            1,
            m_workEdit,
            QString(),
            true
        );

        parent->addWidget(group);
    }

    // -------------------------------------------------------------------------
    // Output
    // -------------------------------------------------------------------------

    void createOutputSection(QVBoxLayout* parent)
    {
        auto* group = new QGroupBox(
            QStringLiteral("Output"),
            this
        );

        auto* layout = new QGridLayout(group);
        layout->setContentsMargins(14, 18, 14, 14);
        layout->setHorizontalSpacing(8);
        layout->setVerticalSpacing(9);

        m_outputDirEdit = createPathEdit(
            layout,
            0,
            QStringLiteral("Output directory"),
            QStringLiteral("Optional destination directory")
        );

        addBrowseButton(
            layout,
            0,
            m_outputDirEdit,
            QString(),
            true
        );

        m_outputVmfEdit = createPathEdit(
            layout,
            1,
            QStringLiteral("Output VMF"),
            QStringLiteral("Optional modified VMF output path")
        );

        addBrowseButton(
            layout,
            1,
            m_outputVmfEdit,
            QStringLiteral("VMF files (*.vmf);;All files (*.*)"),
            false
        );

        m_modelNameEdit = createLineEdit(
            layout,
            2,
            QStringLiteral("Model name"),
            QStringLiteral("Optional compiled model name")
        );

        m_targetNameEdit = createLineEdit(
            layout,
            3,
            QStringLiteral("Target name"),
            QStringLiteral("Optional targetname")
        );

        parent->addWidget(group);
    }

    // -------------------------------------------------------------------------
    // Filtering
    // -------------------------------------------------------------------------

    void createFilterSection(QVBoxLayout* parent)
    {
        auto* group = new QGroupBox(
            QStringLiteral("Model Filtering"),
            this
        );

        auto* layout = new QFormLayout(group);
        layout->setContentsMargins(14, 18, 14, 14);
        layout->setHorizontalSpacing(12);
        layout->setVerticalSpacing(10);

        m_excludeEdit = new QPlainTextEdit(group);
        m_excludeEdit->setObjectName(QStringLiteral("multiLineEdit"));
        m_excludeEdit->setPlaceholderText(
            QStringLiteral(
                "One model pattern per line\n"
                "Example: models/props/large_*.mdl"
            )
        );
        m_excludeEdit->setMinimumHeight(70);
        m_excludeEdit->setMaximumHeight(110);

        layout->addRow(
            makeFieldLabel(QStringLiteral("Exclude models")),
            m_excludeEdit
        );

        m_includeEdit = new QPlainTextEdit(group);
        m_includeEdit->setObjectName(QStringLiteral("multiLineEdit"));
        m_includeEdit->setPlaceholderText(
            QStringLiteral(
                "Optional allow-list\n"
                "One model pattern per line"
            )
        );
        m_includeEdit->setMinimumHeight(70);
        m_includeEdit->setMaximumHeight(110);

        layout->addRow(
            makeFieldLabel(QStringLiteral("Include models")),
            m_includeEdit
        );

        auto* excludeFileRow = new QWidget(group);
        auto* excludeFileLayout =
            new QHBoxLayout(excludeFileRow);

        excludeFileLayout->setContentsMargins(0, 0, 0, 0);
        excludeFileLayout->setSpacing(8);

        m_excludeFileEdit = new QLineEdit(excludeFileRow);
        m_excludeFileEdit->setPlaceholderText(
            QStringLiteral("Optional exclude file")
        );

        excludeFileLayout->addWidget(
            m_excludeFileEdit,
            1
        );

        auto* browse = new QPushButton(
            QStringLiteral("Browse"),
            excludeFileRow
        );

        connect(
            browse,
            &QPushButton::clicked,
            this,
            [this]() {
                const QString file =
                    QFileDialog::getOpenFileName(
                        this,
                        QStringLiteral("Select exclude file"),
                        QString(),
                        QStringLiteral(
                            "Text files (*.txt);;"
                            "All files (*.*)"
                        )
                    );

                if (!file.isEmpty())
                    m_excludeFileEdit->setText(file);
            }
        );

        excludeFileLayout->addWidget(browse);

        layout->addRow(
            makeFieldLabel(QStringLiteral("Exclude file")),
            excludeFileRow
        );

        parent->addWidget(group);
    }

    // -------------------------------------------------------------------------
    // Transform
    // -------------------------------------------------------------------------

    void createTransformSection(QVBoxLayout* parent)
    {
        auto* group = new QGroupBox(
            QStringLiteral("Transform"),
            this
        );

        auto* layout = new QGridLayout(group);
        layout->setContentsMargins(14, 18, 14, 14);
        layout->setHorizontalSpacing(8);
        layout->setVerticalSpacing(9);

        m_coordCombo = new QComboBox(group);

        m_coordCombo->addItems({
            QStringLiteral("-y,x,z"),
            QStringLiteral("x,y,z"),
            QStringLiteral("y,x,z"),
            QStringLiteral("-x,y,z"),
            QStringLiteral("x,-y,z"),
            QStringLiteral("x,y,-z")
        });

        layout->addWidget(
            makeFieldLabel(QStringLiteral("Coordinate map")),
            0, 0
        );

        layout->addWidget(
            m_coordCombo,
            0, 1, 1, 5
        );

        m_scaleSpin = new QDoubleSpinBox(group);
        m_scaleSpin->setRange(0.0001, 10000.0);
        m_scaleSpin->setDecimals(4);
        m_scaleSpin->setSingleStep(0.1);
        m_scaleSpin->setValue(DEFAULT_SCALE);

        layout->addWidget(
            makeFieldLabel(QStringLiteral("Global scale")),
            1, 0
        );

        layout->addWidget(
            m_scaleSpin,
            1, 1
        );

        m_offsetX = createDoubleSpinBox(group);
        m_offsetY = createDoubleSpinBox(group);
        m_offsetZ = createDoubleSpinBox(group);

        layout->addWidget(
            makeFieldLabel(QStringLiteral("Offset")),
            1, 2
        );

        layout->addWidget(
            m_offsetX,
            1, 3
        );

        layout->addWidget(
            m_offsetY,
            1, 4
        );

        layout->addWidget(
            m_offsetZ,
            1, 5
        );

        m_rotationCheck = new QCheckBox(
            QStringLiteral("Apply prop rotation"),
            group
        );

        m_rotationCheck->setChecked(true);

        layout->addWidget(
            m_rotationCheck,
            2, 0, 1, 3
        );

        m_normalsCheck = new QCheckBox(
            QStringLiteral("Transform normals"),
            group
        );

        m_normalsCheck->setChecked(true);

        layout->addWidget(
            m_normalsCheck,
            2, 3, 1, 3
        );

        parent->addWidget(group);
    }

    // -------------------------------------------------------------------------
    // Compile settings
    // -------------------------------------------------------------------------

    void createCompileSection(QVBoxLayout* parent)
    {
        auto* group = new QGroupBox(
            QStringLiteral("Compile Settings"),
            this
        );

        auto* layout = new QGridLayout(group);
        layout->setContentsMargins(14, 18, 14, 14);
        layout->setHorizontalSpacing(8);
        layout->setVerticalSpacing(9);

        m_trianglesSpin = new QSpinBox(group);
        m_trianglesSpin->setRange(1, 100000000);
        m_trianglesSpin->setValue(DEFAULT_TRIANGLES);

        m_convexSpin = new QSpinBox(group);
        m_convexSpin->setRange(1, 100000000);
        m_convexSpin->setValue(DEFAULT_CONVEX);

        m_surfaceEdit = new QLineEdit(group);
        m_surfaceEdit->setPlaceholderText(
            QStringLiteral("Optional surface property")
        );

        m_fpsSpin = new QDoubleSpinBox(group);
        m_fpsSpin->setRange(0.001, 1000.0);
        m_fpsSpin->setDecimals(3);
        m_fpsSpin->setValue(DEFAULT_FPS);

        layout->addWidget(
            makeFieldLabel(QStringLiteral("Max triangles")),
            0, 0
        );

        layout->addWidget(
            m_trianglesSpin,
            0, 1
        );

        layout->addWidget(
            makeFieldLabel(QStringLiteral("Max convex pieces")),
            0, 2
        );

        layout->addWidget(
            m_convexSpin,
            0, 3
        );

        layout->addWidget(
            makeFieldLabel(QStringLiteral("Surface prop")),
            1, 0
        );

        layout->addWidget(
            m_surfaceEdit,
            1, 1
        );

        layout->addWidget(
            makeFieldLabel(QStringLiteral("Sequence FPS")),
            1, 2
        );

        layout->addWidget(
            m_fpsSpin,
            1, 3
        );

        parent->addWidget(group);
    }

    // -------------------------------------------------------------------------
    // Advanced
    // -------------------------------------------------------------------------

    void createAdvancedSection(QVBoxLayout* parent)
    {
        auto* group = new QGroupBox(
            QStringLiteral("Advanced Options"),
            this
        );

        auto* layout = new QGridLayout(group);
        layout->setContentsMargins(14, 18, 14, 14);
        layout->setHorizontalSpacing(24);
        layout->setVerticalSpacing(8);

        m_keepWorkCheck = makeCheckBox(
            QStringLiteral("Keep work files")
        );

        m_noVpkCheck = makeCheckBox(
            QStringLiteral("Disable VPK search")
        );

        m_noVmfCheck = makeCheckBox(
            QStringLiteral("Don't write modified VMF")
        );

        m_keepOriginalCheck = makeCheckBox(
            QStringLiteral("Keep original props")
        );

        m_allowFailuresCheck = makeCheckBox(
            QStringLiteral("Allow partial failures")
        );

        m_dryRunCheck = makeCheckBox(
            QStringLiteral("Dry run")
        );

        m_verboseCheck = makeCheckBox(
            QStringLiteral("Verbose diagnostics")
        );

        m_quietCheck = makeCheckBox(
            QStringLiteral("Quiet CLI output")
        );

        layout->addWidget(m_keepWorkCheck,       0, 0);
        layout->addWidget(m_noVpkCheck,           0, 1);

        layout->addWidget(m_noVmfCheck,           1, 0);
        layout->addWidget(m_keepOriginalCheck,    1, 1);

        layout->addWidget(m_allowFailuresCheck,   2, 0);
        layout->addWidget(m_dryRunCheck,          2, 1);

        layout->addWidget(m_verboseCheck,         3, 0);
        layout->addWidget(m_quietCheck,           3, 1);

        parent->addWidget(group);
    }

    // -------------------------------------------------------------------------
    // Widget helpers
    // -------------------------------------------------------------------------

    QLabel* makeFieldLabel(const QString& text)
    {
        auto* label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("fieldLabel"));
        return label;
    }

    QLineEdit* createLineEdit(
        QGridLayout* layout,
        int row,
        const QString& labelText,
        const QString& placeholder)
    {
        auto* edit = new QLineEdit(this);

        edit->setPlaceholderText(placeholder);

        layout->addWidget(
            makeFieldLabel(labelText),
            row,
            0
        );

        layout->addWidget(
            edit,
            row,
            1,
            1,
            5
        );

        return edit;
    }

    QLineEdit* createPathEdit(
        QGridLayout* layout,
        int row,
        const QString& labelText,
        const QString& placeholder)
    {
        auto* edit = new QLineEdit(this);

        edit->setPlaceholderText(placeholder);

        layout->addWidget(
            makeFieldLabel(labelText),
            row,
            0
        );

        layout->addWidget(
            edit,
            row,
            1,
            1,
            4
        );

        return edit;
    }

    void addBrowseButton(
        QGridLayout* layout,
        int row,
        QLineEdit* edit,
        const QString& filter,
        bool folder)
    {
        auto* button = new QPushButton(
            folder
                ? QStringLiteral("Choose")
                : QStringLiteral("Browse"),
            this
        );

        button->setObjectName(
            QStringLiteral("secondaryButton")
        );

        layout->addWidget(
            button,
            row,
            5
        );

        connect(
            button,
            &QPushButton::clicked,
            this,
            [this, edit, filter, folder]() {

                QString path;

                if (folder) {
                    path = QFileDialog::getExistingDirectory(
                        this,
                        QStringLiteral("Choose directory"),
                        edit->text()
                    );
                } else {
                    path = QFileDialog::getOpenFileName(
                        this,
                        QStringLiteral("Choose file"),
                        edit->text(),
                        filter
                    );
                }

                if (!path.isEmpty())
                    edit->setText(path);
            }
        );
    }

    QDoubleSpinBox* createDoubleSpinBox(QWidget* parent)
    {
        auto* box = new QDoubleSpinBox(parent);

        box->setRange(-1000000.0, 1000000.0);
        box->setDecimals(3);
        box->setSingleStep(1.0);
        box->setValue(0.0);

        return box;
    }

    QCheckBox* makeCheckBox(const QString& text)
    {
        auto* check = new QCheckBox(text, this);
        return check;
    }

    // -------------------------------------------------------------------------
    // Theme
    //
    // This intentionally follows the visual language of modern JetBrains
    // IDEs rather than trying to reproduce their exact proprietary UI.
    // -----------------------------------------------------------------------------

    void applyTheme()
    {
        const QString style = QStringLiteral(R"(
            * {
                font-family: "Segoe UI";
                font-size: 10pt;
            }

            QMainWindow,
            #centralWidget,
            #optionsContainer {
                background: #1f2024;
                color: #d7dae0;
            }

            #header {
                background: #25262b;
                border: 1px solid #34363d;
                border-radius: 8px;
            }

            #appTitle {
                color: #f0f1f3;
                font-size: 19pt;
                font-weight: 600;
            }

            #appSubtitle {
                color: #969aa4;
                font-size: 9pt;
            }

            #versionBadge {
                background: #303238;
                color: #aeb3bd;
                border: 1px solid #3b3e46;
                border-radius: 5px;
                padding: 5px 10px;
            }

            QGroupBox {
                background: #25262b;
                border: 1px solid #35373e;
                border-radius: 7px;
                margin-top: 11px;
                padding: 12px 10px 10px 10px;
                color: #e1e3e8;
                font-weight: 600;
            }

            QGroupBox::title {
                subcontrol-origin: margin;
                left: 12px;
                padding: 0 6px;
                color: #c9ccd3;
                background: #25262b;
            }

            #requiredGroup {
                border: 1px solid #3d506d;
            }

            #requiredGroup::title {
                color: #7aa7e8;
            }

            #fieldLabel {
                color: #aeb2bb;
                font-weight: 500;
                min-width: 115px;
            }

            QLineEdit,
            QPlainTextEdit,
            QTextEdit,
            QComboBox,
            QSpinBox,
            QDoubleSpinBox {
                background: #1d1f23;
                color: #dfe2e8;
                border: 1px solid #3b3e46;
                border-radius: 5px;
                padding: 6px 8px;
                selection-background-color: #365d91;
                selection-color: #ffffff;
            }

            QLineEdit:hover,
            QPlainTextEdit:hover,
            QComboBox:hover,
            QSpinBox:hover,
            QDoubleSpinBox:hover {
                border: 1px solid #555963;
            }

            QLineEdit:focus,
            QPlainTextEdit:focus,
            QComboBox:focus,
            QSpinBox:focus,
            QDoubleSpinBox:focus {
                border: 1px solid #4d82c4;
            }

            QLineEdit[required="true"] {
                border: 1px solid #4a5c78;
            }

            QComboBox {
                padding-right: 24px;
            }

            QComboBox QAbstractItemView {
                background: #25262b;
                color: #dfe2e8;
                border: 1px solid #464950;
                selection-background-color: #315d91;
            }

            QPushButton {
                background: #303238;
                color: #d9dce2;
                border: 1px solid #44474f;
                border-radius: 5px;
                padding: 7px 13px;
                min-height: 18px;
            }

            QPushButton:hover {
                background: #393b42;
                border-color: #565963;
            }

            QPushButton:pressed {
                background: #292b30;
            }

            QPushButton:disabled {
                background: #28292d;
                color: #686c75;
                border-color: #33353a;
            }

            #secondaryButton {
                min-width: 72px;
            }

            #primaryButton {
                background: #3574c7;
                color: white;
                border: 1px solid #4788df;
                font-weight: 600;
                letter-spacing: 0.5px;
                min-width: 110px;
            }

            #primaryButton:hover {
                background: #4081d8;
                border-color: #5c99e8;
            }

            #primaryButton:pressed {
                background: #2d63aa;
            }

            QCheckBox {
                color: #bfc3cb;
                spacing: 8px;
            }

            QCheckBox:hover {
                color: #e0e2e6;
            }

            QCheckBox::indicator {
                width: 15px;
                height: 15px;
                border-radius: 3px;
                border: 1px solid #555963;
                background: #1d1f23;
            }

            QCheckBox::indicator:checked {
                background: #3978ca;
                border-color: #4b8bdf;
            }

            #outputPanel {
                background: #202126;
                border: 1px solid #34363d;
                border-radius: 7px;
                padding: 8px;
            }

            #sectionTitle {
                color: #dfe1e6;
                font-size: 11pt;
                font-weight: 600;
            }

            #console {
                background: #17181b;
                color: #c8ccd3;
                border: 1px solid #34363d;
                border-radius: 5px;
                padding: 8px;
            }

            #outputHint {
                color: #777c87;
                font-size: 8.5pt;
            }

            #actionBar {
                background: #25262b;
                border: 1px solid #34363d;
                border-radius: 7px;
            }

            #statusLabel {
                color: #8f949f;
                padding-left: 4px;
            }

            QScrollArea {
                background: transparent;
                border: none;
            }

            QScrollBar:vertical {
                background: #1c1d21;
                width: 12px;
                margin: 2px;
            }

            QScrollBar::handle:vertical {
                background: #3c3f46;
                border-radius: 5px;
                min-height: 35px;
            }

            QScrollBar::handle:vertical:hover {
                background: #4a4e57;
            }

            QScrollBar::add-line:vertical,
            QScrollBar::sub-line:vertical {
                height: 0px;
            }

            QSplitter::handle {
                background: #1c1d21;
            }

            QSplitter::handle:hover {
                background: #3c6da7;
            }

            QStatusBar {
                background: #1b1c20;
                color: #858a94;
            }

            QToolTip {
                background: #303238;
                color: #e1e3e8;
                border: 1px solid #4a4d55;
                padding: 5px;
            }
        )");

        qApp->setStyleSheet(style);

        QPalette palette = qApp->palette();

        palette.setColor(
            QPalette::Window,
            QColor(QStringLiteral("#1f2024"))
        );

        palette.setColor(
            QPalette::WindowText,
            QColor(QStringLiteral("#d7dae0"))
        );

        palette.setColor(
            QPalette::Base,
            QColor(QStringLiteral("#1d1f23"))
        );

        palette.setColor(
            QPalette::Text,
            QColor(QStringLiteral("#dfe2e8"))
        );

        palette.setColor(
            QPalette::Button,
            QColor(QStringLiteral("#303238"))
        );

        palette.setColor(
            QPalette::ButtonText,
            QColor(QStringLiteral("#d9dce2"))
        );

        qApp->setPalette(palette);
    }

    // -------------------------------------------------------------------------
    // Build command line
    // -------------------------------------------------------------------------

    std::vector<std::string> buildArgs() const
    {
        std::vector<std::string> args;

        auto add = [&](
            const QLineEdit* edit,
            const char* option)
        {
            if (!edit)
                return;

            const QString value = edit->text().trimmed();

            if (!value.isEmpty()) {
                args.emplace_back(option);
                args.emplace_back(narrow(value));
            }
        };

        auto addCheck = [&](
            const QCheckBox* check,
            const char* option)
        {
            if (check && check->isChecked())
                args.emplace_back(option);
        };

        // Required options ---------------------------------------------------

        add(m_vmfEdit, "--vmf");
        add(m_gameEdit, "--game");
        add(m_crowbarEdit, "--crowbar");

        // Optional toolchain -------------------------------------------------

        add(m_studiomdlEdit, "--studiomdl");
        add(m_workEdit, "--work-dir");

        // Optional output ----------------------------------------------------

        add(m_outputDirEdit, "--output-dir");
        add(m_modelNameEdit, "--model-name");
        add(m_targetNameEdit, "--targetname");
        add(m_outputVmfEdit, "--output-vmf");

        // Optional filtering -------------------------------------------------

        for (const QString& line :
             splitLines(m_excludeEdit->toPlainText()))
        {
            args.emplace_back("--exclude-model");
            args.emplace_back(narrow(line));
        }

        for (const QString& line :
             splitLines(m_includeEdit->toPlainText()))
        {
            args.emplace_back("--include-model");
            args.emplace_back(narrow(line));
        }

        add(m_excludeFileEdit, "--exclude-file");

        // Coordinate map -----------------------------------------------------

        const QString coord =
            m_coordCombo->currentText().trimmed();

        if (!coord.isEmpty() &&
            coord != QStringLiteral("-y,x,z"))
        {
            args.emplace_back("--coord-map");
            args.emplace_back(narrow(coord));
        }

        // Transform ----------------------------------------------------------

        if (!qFuzzyCompare(
                m_scaleSpin->value(),
                DEFAULT_SCALE))
        {
            args.emplace_back("--global-scale");
            args.emplace_back(
                narrow(
                    QString::number(
                        m_scaleSpin->value(),
                        'g',
                        12
                    )
                )
            );
        }

        if (!qFuzzyIsNull(m_offsetX->value()) ||
            !qFuzzyIsNull(m_offsetY->value()) ||
            !qFuzzyIsNull(m_offsetZ->value()))
        {
            args.emplace_back("--offset");

            args.emplace_back(
                narrow(
                    QString::number(
                        m_offsetX->value(),
                        'g',
                        12
                    )
                )
            );

            args.emplace_back(
                narrow(
                    QString::number(
                        m_offsetY->value(),
                        'g',
                        12
                    )
                )
            );

            args.emplace_back(
                narrow(
                    QString::number(
                        m_offsetZ->value(),
                        'g',
                        12
                    )
                )
            );
        }

        // The CLI defaults are rotation=true and transformNormals=true.

        if (!m_rotationCheck->isChecked())
            args.emplace_back("--no-rotation");

        if (!m_normalsCheck->isChecked())
            args.emplace_back("--no-transform-normals");

        // Compile ------------------------------------------------------------

        if (m_trianglesSpin->value() != DEFAULT_TRIANGLES) {
            args.emplace_back("--max-triangles");
            args.emplace_back(
                narrow(
                    QString::number(
                        m_trianglesSpin->value()
                    )
                )
            );
        }

        if (m_convexSpin->value() != DEFAULT_CONVEX) {
            args.emplace_back("--max-convex-pieces");
            args.emplace_back(
                narrow(
                    QString::number(
                        m_convexSpin->value()
                    )
                )
            );
        }

        if (!m_surfaceEdit->text().trimmed().isEmpty()) {
            args.emplace_back("--surfaceprop");
            args.emplace_back(
                narrow(
                    m_surfaceEdit->text().trimmed()
                )
            );
        }

        if (!qFuzzyCompare(
                m_fpsSpin->value(),
                DEFAULT_FPS))
        {
            args.emplace_back("--sequence-fps");
            args.emplace_back(
                narrow(
                    QString::number(
                        m_fpsSpin->value(),
                        'g',
                        12
                    )
                )
            );
        }

        // Advanced -----------------------------------------------------------

        addCheck(m_keepWorkCheck, "--keep-work");
        addCheck(m_noVpkCheck, "--no-vpk");
        addCheck(m_noVmfCheck, "--no-vmf");
        addCheck(m_keepOriginalCheck, "--keep-original-props");
        addCheck(m_allowFailuresCheck, "--allow-failures");
        addCheck(m_dryRunCheck, "--dry-run");
        addCheck(m_verboseCheck, "--verbose");
        addCheck(m_quietCheck, "--quiet");

        return args;
    }

    // -------------------------------------------------------------------------
    // Command line preview
    // -------------------------------------------------------------------------

    QString makeCommandLine() const
    {
        const auto args = buildArgs();

        QString command =
            QStringLiteral("vmfpropmerger.exe");

        for (const std::string& argument : args) {
            command += QLatin1Char(' ');
            command += quoteCommandArgument(
                widen(argument)
            );
        }

        return command;
    }

    // -------------------------------------------------------------------------
    // Validation
    // -------------------------------------------------------------------------

    bool validateRequired()
    {
        const bool vmf =
            !m_vmfEdit->text().trimmed().isEmpty();

        const bool game =
            !m_gameEdit->text().trimmed().isEmpty();

        const bool crowbar =
            !m_crowbarEdit->text().trimmed().isEmpty();

        markRequired(m_vmfEdit, vmf);
        markRequired(m_gameEdit, game);
        markRequired(m_crowbarEdit, crowbar);

        if (vmf && game && crowbar)
            return true;

        QStringList missing;

        if (!vmf)
            missing << QStringLiteral("VMF file");

        if (!game)
            missing << QStringLiteral("Game root");

        if (!crowbar)
            missing << QStringLiteral("Crowbar");

        QMessageBox::warning(
            this,
            QStringLiteral("Required options missing"),
            QStringLiteral(
                "Please provide the following required options:\n\n"
            ) +
            QStringLiteral("• ") +
            missing.join(QStringLiteral("\n• "))
        );

        return false;
    }

    void markRequired(QLineEdit* edit, bool valid)
    {
        if (!edit)
            return;

        edit->setProperty("requiredValid", valid);

        if (valid) {
            edit->setStyleSheet(
                QStringLiteral(
                    "QLineEdit {"
                    " border: 1px solid #4caf50;"
                    " background: #1b241d;"
                    "}"
                    "QLineEdit:focus {"
                    " border: 1px solid #66bb6a;"
                    "}"
                )
            );
        }
        else {
            edit->setStyleSheet(
                QStringLiteral(
                    "QLineEdit {"
                    " border: 1px solid #c85c68;"
                    " background: #211b1e;"
                    "}"
                    "QLineEdit:focus {"
                    " border: 1px solid #df7380;"
                    "}"
                )
            );
        }
    }

    // -------------------------------------------------------------------------
    // Reset
    // -------------------------------------------------------------------------

    void resetDefaults()
    {
        m_studiomdlEdit->clear();
        m_workEdit->clear();

        m_excludeEdit->clear();
        m_includeEdit->clear();
        m_excludeFileEdit->clear();

        m_outputDirEdit->clear();
        m_outputVmfEdit->clear();

        m_modelNameEdit->clear();
        m_targetNameEdit->clear();

        m_coordCombo->setCurrentIndex(0);

        m_scaleSpin->setValue(DEFAULT_SCALE);

        m_offsetX->setValue(0.0);
        m_offsetY->setValue(0.0);
        m_offsetZ->setValue(0.0);

        m_trianglesSpin->setValue(
            DEFAULT_TRIANGLES
        );

        m_convexSpin->setValue(
            DEFAULT_CONVEX
        );

        m_surfaceEdit->clear();

        m_fpsSpin->setValue(DEFAULT_FPS);

        m_rotationCheck->setChecked(true);
        m_normalsCheck->setChecked(true);

        m_keepWorkCheck->setChecked(false);
        m_noVpkCheck->setChecked(false);
        m_noVmfCheck->setChecked(false);
        m_keepOriginalCheck->setChecked(false);
        m_allowFailuresCheck->setChecked(false);
        m_dryRunCheck->setChecked(false);
        m_verboseCheck->setChecked(false);
        m_quietCheck->setChecked(false);

        markRequired(
            m_vmfEdit,
            !m_vmfEdit->text().trimmed().isEmpty()
        );

        markRequired(
            m_gameEdit,
            !m_gameEdit->text().trimmed().isEmpty()
        );

        markRequired(
            m_crowbarEdit,
            !m_crowbarEdit->text().trimmed().isEmpty()
        );
    }

    // -------------------------------------------------------------------------
    // Run
    // -------------------------------------------------------------------------

    void startRun()
    {
        if (m_running.load())
            return;

        if (!validateRequired())
            return;

        const auto args = buildArgs();

        m_log->clear();

        appendLog(
            QStringLiteral(
                "============================================================\n"
            )
        );

        appendLog(
            QStringLiteral(
                "VMF Prop Merger\n"
            )
        );

        appendLog(
            QStringLiteral(
                "============================================================\n\n"
            )
        );

        appendLog(
            QStringLiteral("Command:\n")
        );

        appendLog(
            makeCommandLine() +
            QStringLiteral("\n\n")
        );

        setRunning(true);

        m_worker = std::thread(
            [this, args]() {
                runWorker(args);
            }
        );
    }

    void runWorker(
        const std::vector<std::string>& args)
    {
        std::vector<char*> argv;

        argv.reserve(args.size() + 1);

        std::string executable =
            "vmfpropmerger-gui";

        argv.push_back(
            executable.data()
        );

        for (const std::string& argument : args) {
            argv.push_back(
                const_cast<char*>(
                    argument.c_str()
                )
            );
        }

        int resultCode = 1;

        GuiStreamBuffer stdoutBuffer(
            [this](const QString& text) {
                QMetaObject::invokeMethod(
                    this,
                    [this, text]() {
                        appendLog(text);
                    },
                    Qt::QueuedConnection
                );
            }
        );

        GuiStreamBuffer stderrBuffer(
            [this](const QString& text) {
                QMetaObject::invokeMethod(
                    this,
                    [this, text]() {
                        appendError(text);
                    },
                    Qt::QueuedConnection
                );
            }
        );

        std::streambuf* oldOut =
            std::cout.rdbuf(&stdoutBuffer);

        std::streambuf* oldErr =
            std::cerr.rdbuf(&stderrBuffer);

        try {
            resultCode =
                vmfpropmerger_cli_main(
                    static_cast<int>(argv.size()),
                    argv.data()
                );
        }
        catch (const std::exception& e) {
            std::cerr
                << "GUI worker caught exception: "
                << e.what()
                << "\n";

            resultCode = 1;
        }
        catch (...) {
            std::cerr
                << "GUI worker caught an unknown exception.\n";

            resultCode = 1;
        }

        std::cout.flush();
        std::cerr.flush();

        stdoutBuffer.pubsync();
        stderrBuffer.pubsync();

        std::cout.rdbuf(oldOut);
        std::cerr.rdbuf(oldErr);

        QMetaObject::invokeMethod(
            this,
            [this, resultCode]() {
                finishRun(resultCode);
            },
            Qt::QueuedConnection
        );
    }

    void finishRun(int resultCode)
    {
        setRunning(false);

        if (resultCode == 0) {
            appendLog(
                QStringLiteral(
                    "\n============================================================\n"
                    "Completed successfully.\n"
                    "============================================================\n"
                )
            );

            setStatus(
                QStringLiteral("Completed successfully"),
                true
            );
        }
        else {
            appendLog(
                QStringLiteral(
                    "\n============================================================\n"
                    "Finished with errors.\n"
                    "============================================================\n"
                )
            );

            setStatus(
                QStringLiteral("Finished with errors"),
                false
            );
        }

        if (m_worker.joinable())
            m_worker.join();
    }

    void setRunning(bool running)
    {
        m_running.store(running);

        m_runButton->setEnabled(!running);
        m_resetButton->setEnabled(!running);
        m_copyCommandButton->setEnabled(!running);

        if (running) {
            m_runButton->setText(
                QStringLiteral("RUNNING…")
            );

            setStatus(
                QStringLiteral("Merge in progress…"),
                true
            );
        }
        else {
            m_runButton->setText(
                QStringLiteral("MERGE")
            );
        }
    }

    // -------------------------------------------------------------------------
    // Output
    // -------------------------------------------------------------------------

    void appendLog(const QString& text)
    {
        if (!m_log)
            return;

        m_log->moveCursor(
            QTextCursor::End
        );

        m_log->insertPlainText(text);

        m_log->moveCursor(
            QTextCursor::End
        );
    }

    void appendError(const QString& text)
    {
        appendLog(
            QStringLiteral("[stderr] ") +
            text
        );
    }

    void setStatus(
        const QString& text,
        bool normal)
    {
        if (m_statusLabel)
            m_statusLabel->setText(text);

        if (normal) {
            m_statusLabel->setStyleSheet(
                QStringLiteral(
                    "color: #8f949f;"
                )
            );
        }
        else {
            m_statusLabel->setStyleSheet(
                QStringLiteral(
                    "color: #df7884;"
                )
            );
        }

        statusBar()->showMessage(text);
    }

private:
    // -------------------------------------------------------------------------
    // Required
    // -------------------------------------------------------------------------

    QLineEdit* m_vmfEdit = nullptr;
    QLineEdit* m_gameEdit = nullptr;
    QLineEdit* m_crowbarEdit = nullptr;

    // -------------------------------------------------------------------------
    // Toolchain
    // -------------------------------------------------------------------------

    QLineEdit* m_studiomdlEdit = nullptr;
    QLineEdit* m_workEdit = nullptr;

    // -------------------------------------------------------------------------
    // Output
    // -------------------------------------------------------------------------

    QLineEdit* m_outputDirEdit = nullptr;
    QLineEdit* m_outputVmfEdit = nullptr;
    QLineEdit* m_modelNameEdit = nullptr;
    QLineEdit* m_targetNameEdit = nullptr;

    // -------------------------------------------------------------------------
    // Filtering
    // -------------------------------------------------------------------------

    QPlainTextEdit* m_excludeEdit = nullptr;
    QPlainTextEdit* m_includeEdit = nullptr;
    QLineEdit* m_excludeFileEdit = nullptr;

    // -------------------------------------------------------------------------
    // Transform
    // -------------------------------------------------------------------------

    QComboBox* m_coordCombo = nullptr;

    QDoubleSpinBox* m_scaleSpin = nullptr;
    QDoubleSpinBox* m_offsetX = nullptr;
    QDoubleSpinBox* m_offsetY = nullptr;
    QDoubleSpinBox* m_offsetZ = nullptr;

    QCheckBox* m_rotationCheck = nullptr;
    QCheckBox* m_normalsCheck = nullptr;

    // -------------------------------------------------------------------------
    // Compile
    // -------------------------------------------------------------------------

    QSpinBox* m_trianglesSpin = nullptr;
    QSpinBox* m_convexSpin = nullptr;
    QLineEdit* m_surfaceEdit = nullptr;
    QDoubleSpinBox* m_fpsSpin = nullptr;

    // -------------------------------------------------------------------------
    // Advanced
    // -------------------------------------------------------------------------

    QCheckBox* m_keepWorkCheck = nullptr;
    QCheckBox* m_noVpkCheck = nullptr;
    QCheckBox* m_noVmfCheck = nullptr;
    QCheckBox* m_keepOriginalCheck = nullptr;
    QCheckBox* m_allowFailuresCheck = nullptr;
    QCheckBox* m_dryRunCheck = nullptr;
    QCheckBox* m_verboseCheck = nullptr;
    QCheckBox* m_quietCheck = nullptr;

    // -------------------------------------------------------------------------
    // Output / actions
    // -------------------------------------------------------------------------

    QPlainTextEdit* m_log = nullptr;

    QLabel* m_statusLabel = nullptr;

    QPushButton* m_runButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QPushButton* m_copyCommandButton = nullptr;

    // -------------------------------------------------------------------------
    // Worker
    // -------------------------------------------------------------------------

    std::atomic<bool> m_running{false};
    std::thread m_worker;
};

// -----------------------------------------------------------------------------
// Application entry point
// -----------------------------------------------------------------------------

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName(
        QStringLiteral("Reactive Drop VMF Prop Merger")
    );

    QApplication::setApplicationDisplayName(
        QStringLiteral("Reactive Drop — VMF Prop Merger")
    );

    QApplication::setOrganizationName(
        QStringLiteral("Reactive Drop")
    );

    // Use the native Windows style while providing our own dark theme.
    app.setStyle(QStringLiteral("Fusion"));

    MainWindow window;

    window.show();

    return app.exec();
}

