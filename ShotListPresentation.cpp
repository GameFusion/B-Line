#include "ShotListPresentation.h"
#include <QTreeWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QActionGroup>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTimer>
#include <QPixmapCache>
#include <QAccessible>

namespace {
class ShotRowDelegate : public QStyledItemDelegate {
public:
    explicit ShotRowDelegate(QObject *parent) : QStyledItemDelegate(parent) {}
    int mode = 1;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        if (mode == 2) return QStyledItemDelegate::sizeHint(option,index).expandedTo(QSize(0,30));
        const auto kind = index.data(ShotListRoles::Kind).toString();
        return QSize(140, kind == "shot" ? (mode == 1 ? 82 : 48) : (kind == "panel" ? 34 : 30));
    }
    void paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i) const override {
        if (mode == 2 || i.column() != 0) { QStyledItemDelegate::paint(p,o,i); return; }
        const QString kind=i.data(ShotListRoles::Kind).toString();
        const bool selected=o.state & QStyle::State_Selected;
        p->save();p->setRenderHint(QPainter::Antialiasing);
        QRect r=o.rect.adjusted(3,3,-4,-3);
        QColor bg=selected ? o.palette.highlight().color() : o.palette.alternateBase().color();
        if (o.state & QStyle::State_MouseOver) bg=bg.lighter(112);
        if (kind=="shot" || selected) { p->setPen(Qt::NoPen);p->setBrush(bg);p->drawRoundedRect(r,6,6); }
        const QColor fg=selected ? o.palette.highlightedText().color() : o.palette.text().color();
        r.adjust(9,0,-9,0);
        if (mode==1 && kind=="shot") {
            const QRect imageRect(r.left(),r.center().y()-27,88,54);
            p->fillRect(imageRect, QColor(24,27,33));
            const QString file=i.data(ShotListRoles::Thumbnail).toString();
            QPixmap thumbnail;
            if (!file.isEmpty() && !QPixmapCache::find(file,&thumbnail)) {
                thumbnail=QPixmap(file).scaled(176,108,Qt::KeepAspectRatio,Qt::SmoothTransformation);
                if (!thumbnail.isNull()) QPixmapCache::insert(file,thumbnail);
            }
            if (!thumbnail.isNull()) {
                QSize size=thumbnail.size().scaled(imageRect.size(),Qt::KeepAspectRatio);
                p->drawPixmap(QRect(imageRect.center()-QPoint(size.width()/2,size.height()/2),size),thumbnail);
            } else { p->setPen(QColor(153,162,177));p->drawText(imageRect,Qt::AlignCenter,"No preview"); }
            r.setLeft(imageRect.right()+12);
        }
        p->setPen(fg);
        QFont title=o.font;title.setBold(kind=="shot" || kind.isEmpty());p->setFont(title);
        const QString name=i.data().toString();
        QRect titleRect=r;
        if (kind=="shot") titleRect.setHeight(r.height()/2+2);
        p->drawText(titleRect,Qt::AlignVCenter|Qt::AlignLeft,QFontMetrics(title).elidedText(name,Qt::ElideRight,r.width()));
        if (kind=="shot") {
            QFont secondary=o.font;secondary.setPointSizeF(qMax(8.0,secondary.pointSizeF()-1));p->setFont(secondary);
            QColor muted=fg;muted.setAlpha(180);p->setPen(muted);
            QString text=i.data(ShotListRoles::Duration).toString();
            const auto summary=i.data(ShotListRoles::Summary).toString();
            if (!summary.isEmpty()) text+="  ·  "+summary.simplified();
            QRect sub=r;sub.setTop(titleRect.bottom());
            p->drawText(sub,Qt::AlignVCenter|Qt::AlignLeft,QFontMetrics(secondary).elidedText(text,Qt::ElideRight,r.width()));
        }
        if (o.state & QStyle::State_HasFocus) {p->setBrush(Qt::NoBrush);p->setPen(o.palette.highlight().color().lighter());p->drawRoundedRect(o.rect.adjusted(1,1,-2,-2),6,6);}
        p->restore();
    }
};
}
ShotListPresentation::ShotListPresentation(QTreeWidget *tree) : QObject(tree),m_tree(tree),m_toolbar(new QWidget(tree->parentWidget())) {
    auto *row=new QHBoxLayout(m_toolbar);row->setContentsMargins(6,7,6,7);row->setSpacing(6);
    m_search=new QLineEdit(m_toolbar);m_search->setObjectName("shotListSearch");m_search->setPlaceholderText(tr("Find a shot…"));m_search->setClearButtonEnabled(true);m_search->setMinimumWidth(50);row->addWidget(m_search,1);
    auto *filter=new QToolButton(m_toolbar);filter->setObjectName("shotListFilter");filter->setToolTip(tr("Filter and view options"));filter->setAccessibleName(tr("Filter Shot List"));filter->setPopupMode(QToolButton::InstantPopup);
    QPixmap icon(20,20);icon.fill(Qt::transparent);{QPainter p(&icon);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(tree->palette().text().color(),1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawPolyline(QPolygonF{QPointF(2,4),QPointF(18,4),QPointF(12,11),QPointF(12,16),QPointF(8,18),QPointF(8,11),QPointF(2,4)});}filter->setIcon(QIcon(icon));row->addWidget(filter);
    auto *menu=new QMenu(filter);filter->setMenu(menu);auto *group=new QActionGroup(menu);
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "B-Line", "Storyboard");m_mode=qBound(0,settings.value("shotList/view",1).toInt(),2);
    const QStringList modes{tr("Compact"),tr("Thumbnails"),tr("Detailed")};
    for(int mode=0;mode<modes.size();++mode) {auto *a=menu->addAction(modes[mode]);a->setCheckable(true);a->setChecked(mode==m_mode);group->addAction(a);connect(a,&QAction::triggered,this,[this,mode]{m_mode=mode;QSettings(QSettings::defaultFormat(), QSettings::UserScope, "B-Line", "Storyboard").setValue("shotList/view",mode);applyView();});}
    menu->addSeparator();
    m_panels=menu->addAction(tr("Show panels"));m_panels->setCheckable(true);m_panels->setChecked(settings.value("shotList/panels",true).toBool());
    m_dialogue=menu->addAction(tr("Show dialogue details"));m_dialogue->setCheckable(true);m_dialogue->setChecked(settings.value("shotList/dialogue",false).toBool());
    for(auto *a:{m_panels,m_dialogue}) connect(a,&QAction::toggled,this,[this]{QSettings s(QSettings::defaultFormat(), QSettings::UserScope, "B-Line", "Storyboard");s.setValue("shotList/panels",m_panels->isChecked());s.setValue("shotList/dialogue",m_dialogue->isChecked());refresh();});
    auto *columns=menu->addMenu(tr("Detailed columns"));
    tree->setHeaderLabels({tr("Name"),tr("Type"),tr("Transition"),tr("Camera"),tr("Lighting"),tr("Audio"),tr("Description"),tr("Frames"),tr("Time of day"),tr("Restore"),tr("FX"),tr("Notes"),tr("Intent")});
    for(int col=1;col<tree->columnCount();++col) {auto *a=columns->addAction(tree->headerItem()->text(col));a->setCheckable(true);a->setData(col);a->setChecked(settings.value(QString("shotList/column%1").arg(col),col==1||col==3||col==6||col==7).toBool());m_columns<<a;connect(a,&QAction::toggled,this,[this,a,col](bool on){QSettings(QSettings::defaultFormat(), QSettings::UserScope, "B-Line", "Storyboard").setValue(QString("shotList/column%1").arg(col),on);applyView();});}
    menu->addSeparator();menu->addAction(tr("Expand all"),tree,&QTreeWidget::expandAll);menu->addAction(tr("Collapse all"),tree,&QTreeWidget::collapseAll);
    tree->setItemDelegate(new ShotRowDelegate(tree));tree->setIndentation(14);tree->setMouseTracking(true);tree->setWordWrap(false);tree->setTextElideMode(Qt::ElideRight);tree->setUniformRowHeights(false);tree->setFrameShape(QFrame::NoFrame);tree->setAnimated(false);
    // Qt 6.10's collapse path removes visible rows without invalidating its
    // accessibility child cache. UIA can then revisit a previous sibling forever.
    // Reset synchronously, before selection/focus queries can traverse stale rows.
    connect(tree, &QTreeView::collapsed, this, [tree] {
        QAccessibleTableModelChangeEvent event(tree, QAccessibleTableModelChangeEvent::ModelReset);
        QAccessible::updateAccessibility(&event);
    });
    connect(m_search,&QLineEdit::textChanged,this,[this]{refresh();});
    // Model insertions are coalesced after population, preserving existing items/IDs.
    auto *refreshTimer=new QTimer(this);refreshTimer->setSingleShot(true);
    connect(refreshTimer,&QTimer::timeout,this,[this]{refresh();});
    connect(tree->model(),&QAbstractItemModel::rowsInserted,this,[refreshTimer]{refreshTimer->start(0);});
    applyView();
}
void ShotListPresentation::applyView() {
    static_cast<ShotRowDelegate*>(m_tree->itemDelegate())->mode=m_mode;
    m_tree->setHeaderHidden(m_mode!=2);
    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->header()->setStretchLastSection(false);
    for(auto *a:m_columns)m_tree->setColumnHidden(a->data().toInt(),m_mode!=2||!a->isChecked());
    m_tree->header()->setSectionResizeMode(0,m_mode==2?QHeaderView::Interactive:QHeaderView::Stretch);
    if(m_mode==2){m_tree->setColumnWidth(0,190);for(auto *a:m_columns)m_tree->setColumnWidth(a->data().toInt(),a->data().toInt()==6?250:110);}
    m_tree->doItemsLayout();refresh();
}
bool ShotListPresentation::filterItem(QTreeWidgetItem *item,const QString &query,bool parentMatches) {
    const auto kind=item->data(0,ShotListRoles::Kind).toString();
    const bool allowed=(kind!="panel"||m_panels->isChecked())&&(kind!="dialogue"||m_dialogue->isChecked());
    bool matches=parentMatches||query.isEmpty();
    for(int c=0;c<item->columnCount()&&!matches;++c)matches=item->text(c).contains(query,Qt::CaseInsensitive);
    bool childMatches=false;for(int i=0;i<item->childCount();++i)childMatches=filterItem(item->child(i),query,matches)||childMatches;
    const bool visible=allowed&&(matches||childMatches);item->setHidden(!visible);
    if(visible&&!query.isEmpty()&&childMatches)item->setExpanded(true);
    return visible;
}
void ShotListPresentation::refresh() {
    const QString query=m_search->text().trimmed();
    for(int i=0;i<m_tree->topLevelItemCount();++i)filterItem(m_tree->topLevelItem(i),query,false);
    m_tree->viewport()->update();
}
