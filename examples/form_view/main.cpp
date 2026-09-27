// qpb 1.1: the same model shown as a form and as a tree, filtered by a search
// box. Edits in either view appear in the other one at once.

#include <qpb/qpb.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto root = qpb::PropertyGroup::create("Project");
    auto& general = root->addGroup("General");
    general.addString("title", "Untitled").placeholder("Project title");
    general.addString("description", "First line\nSecond line").multiline();
    general.addEnum("status", {"Draft", "Review", "Final"}, 0);
    auto& build = root->addGroup("Build");
    build.addBool("optimize", true);
    build.addInt("jobs", 4).range(1, 64).displayName("Parallel jobs");
    build.addDouble("timeout", 30.0).range(1.0, 600.0).suffix(" s");
    build.addDirPath("outputDir", QString());
    qpb::PropertyModel model(std::move(root));

    // One filter for both views.
    qpb::PropertyFilterProxyModel filter;
    filter.setSourceModel(&model);

    QWidget window;
    auto* search = new QLineEdit;
    search->setPlaceholderText("Search properties");
    search->setClearButtonEnabled(true);
    QObject::connect(search, &QLineEdit::textChanged, &filter,
        &qpb::PropertyFilterProxyModel::setFilterFixedString);

    auto* form = new qpb::PropertyFormView;
    form->setModel(&filter);
    auto* tree = new qpb::PropertyTreeView;
    tree->setModel(&filter);

    auto* views = new QHBoxLayout;
    views->addWidget(form);
    views->addWidget(tree);
    auto* layout = new QVBoxLayout(&window);
    layout->addWidget(search);
    layout->addLayout(views);
    window.setWindowTitle("qpb form view");
    window.resize(820, 480);
    window.show();
    return app.exec();
}
