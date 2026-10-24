// qpb 1.9: what the handlers of the model's signals may do.
//
//   1. valueChanged / userValueChanged: react to values by path; a handler may change
//      the tree. Shape (Circle / Rectangle) rebuilds the Size group.
//   2. dataChanged: presentation only; the handler reads and logs.
//   3. A change of the tree because of dataChanged is deferred. Locked makes Size
//      read-only, which adds a note to the tree.
//   4. A value handler may delete the model. "Close document" shows a new one.

#include <qpb/qpb.h>

#include <QApplication>
#include <QMainWindow>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>

#include <memory>

namespace {

// The group that depends on the shape: a radius, or a width and a height.
void addSize(qpb::PropertyGroup& root, int shape, bool locked)
{
    auto& size = root.addGroup("Size");
    if (shape == 0) {
        size.addDouble("radius", 10.0).range(1.0, 100.0);
    } else {
        size.addDouble("width", 20.0).range(1.0, 100.0);
        size.addDouble("height", 10.0).range(1.0, 100.0);
    }
    if (locked)
        size.setReadOnly(true);
}

std::unique_ptr<qpb::PropertyGroup> buildDocument()
{
    auto root = qpb::PropertyGroup::create("Document");
    root->addEnum("Shape", {"Circle", "Rectangle"}, 0);
    root->addBool("Locked", false);
    addSize(*root, 0, false);
    root->addBool("CloseDocument", false).displayName("Close document");
    return root;
}

class Window : public QMainWindow
{
public:
    Window()
    {
        m_view = new qpb::PropertyTreeView;
        m_log = new QPlainTextEdit;
        m_log->setReadOnly(true);
        auto* splitter = new QSplitter(Qt::Vertical);
        splitter->addWidget(m_view);
        splitter->addWidget(m_log);
        setCentralWidget(splitter);

        openDocument();
        setWindowTitle("qpb model signals - Document[*]");
        resize(520, 520);
    }

    // The model must not be destroyed while the view shows it.
    ~Window() override
    {
        m_view->setModel(nullptr);
    }

private:
    void log(const QString& text)
    {
        m_log->appendPlainText(text);
    }

    // Everything connected to the model lives here, so a new document gets the
    // same handlers. The handlers capture only this window, which outlives every model.
    void openDocument()
    {
        m_model = std::make_unique<qpb::PropertyModel>(buildDocument());
        qpb::PropertyModel* model = m_model.get();

        // 1. Values are handled by path. A handler of valueChanged may change the
        // tree, here it replaces the whole Size group; the call that wrote the value
        // returns normally and later receivers still get the change.
        model->onValueChanged("Shape", this, [this](const QVariant& shape) {
            log(QStringLiteral("valueChanged: Shape = %1, rebuild Size").arg(shape.toInt()));
            qpb::PropertyGroup* root = m_model->root();
            const bool locked = sizeLocked();
            root->remove("Size");
            addSize(*root, shape.toInt(), locked);
        });
        model->onValueChanged("Locked", this, [this](const QVariant& locked) {
            // Changes a flag only, no row is added or removed.
            m_model->find("Size")->setReadOnly(locked.toBool());
        });

        // userValueChanged is what the user typed, not what the code wrote: it marks the
        // document as modified, and application writes such as setValue() do not reach it.
        connect(model, &qpb::PropertyModel::userValueChanged, this,
            [this](const QString& path) {
                log(QStringLiteral("userValueChanged: %1").arg(path));
                setWindowModified(true);
            });

        // 2. dataChanged is about presentation (flags, names, texts, the row of a
        // value): it only reads. Values are handled in valueChanged, by path.
        connect(model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex& first, const QModelIndex& last) {
                for (int row = first.row(); row <= last.row(); ++row) {
                    const qpb::Property* property
                        = m_model->propertyAt(m_model->index(row, 0, first.parent()));
                    if (property) {
                        log(QStringLiteral("dataChanged: %1").arg(property->path()));
                        statusBar()->showMessage(
                            QStringLiteral("%1 changed").arg(property->path()));
                    }
                }

                // 3. A change of the tree because of dataChanged is deferred. The views,
                // proxies and delegates that receive the same signal after this handler
                // would get indexes of rows that have moved. The queued call runs when
                // the emission is over and finds its way by path.
                if (sizeLocked() != (m_model->find("Note") != nullptr) && !m_syncQueued) {
                    m_syncQueued = true;
                    QMetaObject::invokeMethod(this, [this] { syncNote(); }, Qt::QueuedConnection);
                }
            });

        // 4. A value handler may delete the model, and the window then shows a new one.
        // Nothing of the old model is used after closeDocument() returns.
        model->onUserValueChanged("CloseDocument", this, [this](const QVariant& checked) {
            if (checked.toBool())
                closeDocument();
        });

        m_view->setModel(model);
        setWindowModified(false);
        log(QStringLiteral("new document"));
    }

    void closeDocument()
    {
        log(QStringLiteral("close document: deleting the model"));
        // The view is detached first: it may still use the index of the click after
        // the handler returns, and must not find a deleted model behind it.
        m_view->setModel(nullptr);
        m_model.reset();
        openDocument();
    }

    // Read from the tree, so it follows what dataChanged reports.
    bool sizeLocked() const
    {
        const qpb::Property* size = m_model->find("Size");
        return size && size->isReadOnly();
    }

    // The deferred half of pattern 3. The document may have been closed meanwhile.
    void syncNote()
    {
        m_syncQueued = false;
        if (!m_model || sizeLocked() == (m_model->find("Note") != nullptr))
            return;
        if (sizeLocked())
            m_model->root()->addString("Note", "Size is locked").readOnly();
        else
            m_model->root()->remove("Note");
        log(QStringLiteral("deferred: note %1").arg(sizeLocked() ? "added" : "removed"));
    }

    std::unique_ptr<qpb::PropertyModel> m_model;
    qpb::PropertyTreeView* m_view = nullptr;
    QPlainTextEdit* m_log = nullptr;
    bool m_syncQueued = false;
};

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Window window;
    window.show();
    return app.exec();
}
