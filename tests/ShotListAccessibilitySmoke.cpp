#include "../ShotListPresentation.h"
#include <QAccessible>
#include <QApplication>
#include <QCoreApplication>
#include <QSet>
#include <QSettings>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <cstdio>

// Bound traversal so the regression fails instead of hanging a UIA client.
static bool checkAccessibleRows(QTreeWidget &tree, const char *stage)
{
    auto *accessible = QAccessible::queryAccessibleInterface(&tree);
    if (!accessible) return false;
    const int count = accessible->childCount();
    for (int i = 0; i < count; ++i) {
        auto *child = accessible->child(i);
        if (!child || accessible->indexOfChild(child) != i) {
            fprintf(stderr, "FAIL %s: accessible child %d maps back to %d\n",
                    stage, i, child ? accessible->indexOfChild(child) : -999);
            return false;
        }
    }
    QSet<QAccessible::Id> visited;
    int index = 0;
    while (index < count) {
        auto *child = accessible->child(index);
        if (!child || !child->isValid() || child->state().invisible) {
            ++index;
            continue;
        }
        const auto id = QAccessible::uniqueId(child);
        if (visited.contains(id)) {
            fprintf(stderr, "FAIL %s: accessibility sibling traversal cycles\n", stage);
            return false;
        }
        visited.insert(id);
        index = accessible->indexOfChild(child) + 1;
    }
    return true;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settings.path());
    QWidget host;
    QTreeWidget tree(&host);
    tree.resize(600, 650);
    ShotListPresentation presentation(&tree);
    auto *scene6 = new QTreeWidgetItem(&tree, {"SCENE_006"});
    auto *scene8 = new QTreeWidgetItem(&tree, {"SCENE_008"});
    for (auto *scene : {scene6, scene8}) {
        for (int i = 0; i < 6; ++i) {
            auto *shot = new QTreeWidgetItem(scene, {QString("SHOT_%1").arg(i)});
            shot->setData(0, ShotListRoles::Kind, "shot");
            auto *panel = new QTreeWidgetItem(shot, {"PANEL_001"});
            panel->setData(0, ShotListRoles::Kind, "panel");
        }
        scene->setExpanded(true);
    }
    host.resize(650, 700);
    host.show();
    QCoreApplication::processEvents();
    tree.doItemsLayout();
    QAccessible::setActive(true);
    if (!checkAccessibleRows(tree, "expanded")) return 1;
    scene6->setExpanded(false);
    // Inspect before another event-loop pass: UIA can query synchronously here.
    if (!checkAccessibleRows(tree, "SCENE_006 collapsed")) return 1;
    tree.setCurrentItem(scene8->child(2));
    if (!checkAccessibleRows(tree, "SCENE_008 selected")) return 1;
    for (int i = 0; i < 10; ++i) {
        scene6->setExpanded(true);
        if (!checkAccessibleRows(tree, "re-expanded")) return 1;
        scene6->setExpanded(false);
        if (!checkAccessibleRows(tree, "re-collapsed")) return 1;
    }
    tree.collapseAll();
    QCoreApplication::processEvents();
    if (!checkAccessibleRows(tree, "collapse all")) return 1;
    tree.expandAll();
    QCoreApplication::processEvents();
    if (!checkAccessibleRows(tree, "expand all")) return 1;
    printf("ShotListAccessibilitySmoke: PASS (collapse, later-scene selection, repeated traversal, collapse/expand all)\n");
    return 0;
}
