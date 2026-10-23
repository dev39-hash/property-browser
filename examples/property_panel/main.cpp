// A reusable panel around PropertyTreeView: a search box above the tree and a
// description of the current property below it (PropertyPanel.h). Type in the
// search box to filter, click a property to read its tool tip.

#include "PropertyPanel.h"

#include <qpb/qpb.h>

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto root = qpb::PropertyGroup::create("Project");
    auto& general = root->addGroup("General");
    general.addString("title", "Untitled")
        .placeholder("Project title")
        .toolTip("Shown in the window title and in the recent projects list.");
    general.addString("author", "").toolTip("Name written into the header of generated files.");
    general.addEnum("status", {"Draft", "Review", "Final"}, 0)
        .toolTip("Only a Final project can be published.");
    auto& build = root->addGroup("Build");
    build.addBool("optimize", true).toolTip("Compile with optimizations. Slower to build.");
    build.addInt("jobs", 4)
        .range(1, 64)
        .displayName("Parallel jobs")
        .toolTip("Number of compiler processes started at the same time.");
    build.addDouble("timeout", 30.0)
        .range(1.0, 600.0)
        .suffix(" s")
        .toolTip("A build step that takes longer is stopped.");
    // No tool tip: the description shows the name alone.
    build.addDirPath("outputDir", QString());
    qpb::PropertyModel model(std::move(root));

    PropertyPanel panel;
    panel.setModel(&model);
    panel.setWindowTitle("qpb property panel");
    panel.resize(420, 480);
    panel.show();
    return app.exec();
}
