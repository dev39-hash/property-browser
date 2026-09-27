// qpb 1.2: edit the Q_PROPERTYs of application objects (QObjectPropertySource),
// with undo/redo (QUndoStack) and saving/loading as JSON (qpb::serialization).

#include <qpb/qpb.h>

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QMainWindow>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>

#include "Light.h"

namespace {

// One property edit. Consecutive edits of the same property merge.
class SetValueCommand : public QUndoCommand
{
public:
    SetValueCommand(qpb::PropertyModel& model, bool& applying, const QString& path,
        const QVariant& newValue, const QVariant& oldValue)
        : m_model(model)
        , m_applying(applying)
        , m_path(path)
        , m_new(newValue)
        , m_old(oldValue)
    {
        setText(QStringLiteral("Change %1").arg(path));
    }

    void undo() override
    {
        apply(m_old);
    }
    // The first redo() is the edit that already happened.
    void redo() override
    {
        if (!m_done)
            m_done = true;
        else
            apply(m_new);
    }
    int id() const override
    {
        return 1;
    }
    bool mergeWith(const QUndoCommand* other) override
    {
        const auto* next = static_cast<const SetValueCommand*>(other);
        if (next->m_path != m_path)
            return false;
        m_new = next->m_new;
        return true;
    }

private:
    void apply(const QVariant& value)
    {
        m_applying = true;
        m_model.setValue(m_path, value);
        m_applying = false;
    }

    qpb::PropertyModel& m_model;
    bool& m_applying;
    QString m_path;
    QVariant m_new;
    QVariant m_old;
    bool m_done = false;
};

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    Light key;
    key.setObjectName("key");
    Light fill;
    fill.setObjectName("fill");
    fill.setName("Fill light");
    fill.setIntensity(0.5);

    qpb::PropertyModel model(qpb::PropertyGroup::create("Scene"));
    qpb::QObjectPropertySource source(&model);
    source.setLiveReadOnlyProperties(true); // 1.3: "emitted" is live
    source.addObject(&key);
    source.addObject(&fill);

    // Every change made through the model becomes an undoable command, except
    // live values, which the objects maintain themselves.
    QUndoStack undoStack;
    bool applying = false;
    QObject::connect(&model, &qpb::PropertyModel::valueChanged,
        [&](const QString& path, const QVariant& newValue, const QVariant& oldValue) {
            if (!applying && !model.find(path)->isLive())
                undoStack.push(new SetValueCommand(model, applying, path, newValue, oldValue));
        });

    // The key light is on: it keeps emitting.
    QTimer emitting;
    QObject::connect(&emitting, &QTimer::timeout, [&key] { key.addEmitted(1'000'000); });
    emitting.start(500);

    QMainWindow window;
    auto* view = new qpb::PropertyTreeView;
    view->setModel(&model);
    window.setCentralWidget(view);

    QToolBar* toolBar = window.addToolBar("Edit");
    QAction* undo = undoStack.createUndoAction(&window);
    undo->setShortcut(QKeySequence::Undo);
    QAction* redo = undoStack.createRedoAction(&window);
    redo->setShortcut(QKeySequence::Redo);
    toolBar->addAction(undo);
    toolBar->addAction(redo);
    toolBar->addSeparator();
    toolBar->addAction("Save...", [&] {
        const QString file = QFileDialog::getSaveFileName(&window, {}, {}, "JSON (*.json)");
        QFile out(file);
        if (!file.isEmpty() && out.open(QIODevice::WriteOnly))
            out.write(QJsonDocument(qpb::serialization::toJson(*model.root())).toJson());
    });
    toolBar->addAction("Load...", [&] {
        const QString file = QFileDialog::getOpenFileName(&window, {}, {}, "JSON (*.json)");
        QFile in(file);
        if (file.isEmpty() || !in.open(QIODevice::ReadOnly))
            return;
        undoStack.beginMacro("Load");
        model.beginBatch();
        qpb::serialization::fromJson(*model.root(), QJsonDocument::fromJson(in.readAll()).object());
        model.endBatch();
        undoStack.endMacro();
    });

    window.setWindowTitle("qpb object editor");
    window.resize(460, 420);
    window.show();
    return app.exec();
}
