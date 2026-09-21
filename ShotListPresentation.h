#ifndef SHOT_LIST_PRESENTATION_H
#define SHOT_LIST_PRESENTATION_H
#include <QObject>
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QAction;
class QWidget;

namespace ShotListRoles {
constexpr int Kind = Qt::UserRole + 101;
constexpr int Summary = Qt::UserRole + 102;
constexpr int Duration = Qt::UserRole + 103;
constexpr int Thumbnail = Qt::UserRole + 104;
}
class ShotListPresentation : public QObject {
public:
    explicit ShotListPresentation(QTreeWidget *tree);
    QWidget *toolbar() const { return m_toolbar; }
    void refresh();
private:
    bool filterItem(QTreeWidgetItem *item, const QString &query, bool parentMatches);
    void applyView();
    QTreeWidget *m_tree;
    QWidget *m_toolbar;
    QLineEdit *m_search;
    QAction *m_panels;
    QAction *m_dialogue;
    int m_mode = 1;
    QList<QAction*> m_columns;
};
#endif
