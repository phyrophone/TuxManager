#ifndef PERF_SIDEPANEL_H
#define PERF_SIDEPANEL_H

#include "sidepanelitem.h"

#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace Perf
{

/// Left-hand side panel of the Performance tab.
/// Contains a vertical list of SidePanelItem entries with mutual-exclusive
/// selection.  Wraps itself in a QScrollArea so it gracefully handles many
/// entries (disk 0, disk 1, … ethernet 0, …) in the future.
class SidePanel : public QWidget
{
    Q_OBJECT

    public:
        explicit SidePanel(QWidget *parent = nullptr);

        /// Add a new item to the panel; returns the assigned index (0-based).
        int addItem(SidePanelItem *item);

        void setCurrentIndex(int index);
        int  currentIndex() const { return this->m_currentIndex; }

        SidePanelItem *itemAt(int index) const;
        int            count() const { return this->m_items.size(); }

    signals:
        void currentChanged(int index);

    private:
        QScrollArea          *m_scrollArea;
        QWidget              *m_container;
        QVBoxLayout          *m_containerLayout;
        QList<SidePanelItem *> m_items;
        int                   m_currentIndex { -1 };
};

} // namespace Perf

#endif // PERF_SIDEPANEL_H
