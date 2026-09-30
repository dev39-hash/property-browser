// A theme for the property browser with a style sheet (docs/SPEC.md section
// 5.8, qpb 1.6): one template, theme.qss, with @{token} placeholders and a
// dark and a light token table, switched at runtime. Standard selectors style
// Qt's widgets (and with them qpb's views and editors); qpb's hooks style what
// the property browser paints or builds itself.
//
//   qpb_example_custom_theme [dark|light|none]

#include <qpb/qpb.h>

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QVBoxLayout>

namespace {

using Tokens = QHash<QString, QString>;

// Graphite surfaces with an industrial orange accent.
const Tokens& darkTokens()
{
    static const Tokens tokens {
        {"bg.window", "#1a1c20"}, {"bg.surface", "#22252a"}, {"bg.alternate", "#262a30"},
        {"bg.elevated", "#2c3038"}, {"bg.group", "#2c3038"}, {"bg.disabled", "#1f2126"},
        {"border.default", "#3a3f48"}, {"border.strong", "#4a505a"}, {"border.disabled", "#2c3038"},
        {"text.primary", "#e0e4ea"}, {"text.muted", "#7a8898"}, {"text.disabled", "#4f5764"},
        {"text.on-accent", "#ffffff"}, {"accent.primary", "#e87c00"}, {"accent.bright", "#ffa040"},
        {"accent.pressed", "#b05a00"}, {"selection.bg", "#3d2e10"},
    };
    return tokens;
}

const Tokens& lightTokens()
{
    static const Tokens tokens {
        {"bg.window", "#f2f3f5"}, {"bg.surface", "#ffffff"}, {"bg.alternate", "#f7f8fa"},
        {"bg.elevated", "#e8eaee"}, {"bg.group", "#e8eaee"}, {"bg.disabled", "#edf0f3"},
        {"border.default", "#c8ccd4"}, {"border.strong", "#a8aeb8"}, {"border.disabled", "#dde0e5"},
        {"text.primary", "#1a1c20"}, {"text.muted", "#5a6878"}, {"text.disabled", "#9aa0aa"},
        {"text.on-accent", "#ffffff"}, {"accent.primary", "#c06400"}, {"accent.bright", "#a85800"},
        {"accent.pressed", "#8a4400"}, {"selection.bg", "#fde8cc"},
    };
    return tokens;
}

// theme.qss with every @{token} replaced by its value in tokens.
QString styleSheet(const Tokens& tokens)
{
    QFile file(QStringLiteral(":/custom_theme/theme.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QString sheet = QString::fromUtf8(file.readAll());
    static const QRegularExpression placeholder(QStringLiteral(R"(@\{([\w.-]+)\})"));
    QString result;
    qsizetype done = 0;
    for (const QRegularExpressionMatch& match : placeholder.globalMatch(sheet)) {
        result += sheet.mid(done, match.capturedStart() - done);
        result += tokens.value(match.captured(1), match.captured(0));
        done = match.capturedEnd();
    }
    return result + sheet.mid(done);
}

std::unique_ptr<qpb::PropertyGroup> createProperties()
{
    auto root = qpb::PropertyGroup::create("Camera");
    root->addString("name", "Main camera");
    root->addBool("enabled", true);
    auto& transform = root->addGroup("Transform");
    transform.addDouble("x", 0.0).range(-100.0, 100.0).suffix(" m");
    transform.addDouble("y", 0.0).range(-100.0, 100.0).suffix(" m");
    transform.addDouble("height", 1.6).suffix(" m").readOnly();
    auto& optics = root->addGroup("Optics");
    optics.addInt("fov", 60).range(10, 170).suffix(" deg");
    optics.addEnum("projection", {"Perspective", "Orthographic"}, 0);
    optics.addBool("hdr", false);
    auto& output = root->addGroup("Output");
    output.addFilePath("lut", {}).filter("LUT files (*.cube)");
    output.addDirPath("folder", {});
    output.addString("notes", {}).multiline().placeholder("Notes for the operator");
    output.addInt64("budget", qint64(4) << 30).suffix(" B");
    return root;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    qpb::PropertyModel model(createProperties());
    model.setValue("name", "Front camera"); // modified: bold, in the accent colour
    model.setValue("Optics/fov", 75);

    auto* theme = new QComboBox;
    theme->addItems({"Dark", "Light", "No style sheet"});
    auto* reset = new QPushButton("Reset all");
    QObject::connect(reset, &QPushButton::clicked, &model, &qpb::PropertyModel::resetAllToDefault);
    auto* bar = new QHBoxLayout;
    bar->addWidget(new QLabel("Theme:"));
    bar->addWidget(theme);
    bar->addStretch();
    bar->addWidget(reset);

    auto* tree = new qpb::PropertyTreeView;
    tree->setModel(&model);
    auto* form = new qpb::PropertyFormView;
    form->setModel(&model);
    auto* splitter = new QSplitter;
    splitter->addWidget(tree);
    splitter->addWidget(form);

    QWidget window;
    window.setWindowTitle("qpb custom theme");
    auto* layout = new QVBoxLayout(&window);
    layout->addLayout(bar);
    layout->addWidget(splitter);

    QObject::connect(theme, &QComboBox::currentIndexChanged, &app, [&app, tree](int index) {
        app.setStyleSheet(index == 0 ? styleSheet(darkTokens())
                : index == 1         ? styleSheet(lightTokens())
                                     : QString());
        if (index == 2) {
            // qproperty- values stay when a sheet is removed: back to the defaults.
            tree->setGroupBackground(QBrush());
            tree->setGroupForeground(QColor());
            tree->setModifiedForeground(QColor());
            tree->setReadOnlyForeground(QColor());
        }
    });
    const QString initial = app.arguments().value(1, "dark");
    theme->setCurrentIndex(initial == "light" ? 1 : initial == "none" ? 2 : 0);
    if (theme->currentIndex() == 0)
        app.setStyleSheet(styleSheet(darkTokens())); // no index change: apply it here

    window.resize(900, 540);
    window.show();
    return app.exec();
}
