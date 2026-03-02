#include "sidepanel.h"

namespace Perf
{

SidePanel::SidePanel(QWidget *parent)
    : QWidget(parent)
    , m_scrollArea(new QScrollArea(this))
    , m_container(new QWidget)
    , m_containerLayout(new QVBoxLayout(this->m_container))
{
    // Container inside the scroll area
    this->m_containerLayout->setContentsMargins(0, 0, 0, 0);
    this->m_containerLayout->setSpacing(1);
    this->m_containerLayout->addStretch(1);      // pushes items to the top

    this->m_scrollArea->setWidget(this->m_container);
    this->m_scrollArea->setWidgetResizable(true);
    this->m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->m_scrollArea->setFrameShape(QFrame::NoFrame);

    // Dark background matching SidePanelItem
    QPalette pal = this->m_container->palette();
    pal.setColor(QPalette::Window, QColor(0x12, 0x12, 0x1a));
    this->m_container->setPalette(pal);
    this->m_container->setAutoFillBackground(true);

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(this->m_scrollArea);
    this->setLayout(outerLayout);

    this->setMinimumWidth(150);
    this->setMaximumWidth(200);
}

int SidePanel::addItem(SidePanelItem *item)
{
    const int index = this->m_items.size();
    this->m_items.append(item);

    // Insert before the trailing stretch
    const int stretchIdx = this->m_containerLayout->count() - 1;
    this->m_containerLayout->insertWidget(stretchIdx, item);

    connect(item, &SidePanelItem::clicked, this, [this, index]()
    {
        this->setCurrentIndex(index);
    });

    // Auto-select the first item added
    if (index == 0)
        this->setCurrentIndex(0);

    return index;
}

void SidePanel::setCurrentIndex(int index)
{
    if (index < 0 || index >= this->m_items.size())
        return;
    if (index == this->m_currentIndex)
        return;

    // Deselect previous
    if (this->m_currentIndex >= 0 && this->m_currentIndex < this->m_items.size())
        this->m_items.at(this->m_currentIndex)->setSelected(false);

    this->m_currentIndex = index;
    this->m_items.at(index)->setSelected(true);

    emit this->currentChanged(index);
}

SidePanelItem *SidePanel::itemAt(int index) const
{
    if (index < 0 || index >= this->m_items.size())
        return nullptr;
    return this->m_items.at(index);
}

} // namespace Perf
