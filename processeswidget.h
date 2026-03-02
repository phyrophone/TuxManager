#ifndef PROCESSESWIDGET_H
#define PROCESSESWIDGET_H

#include "os/processmodel.h"
#include "os/processfilterproxy.h"
#include "os/processhelper.h"

#include <QTimer>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class ProcessesWidget;
}
QT_END_NAMESPACE

class ProcessesWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit ProcessesWidget(QWidget *parent = nullptr);
        ~ProcessesWidget();

    private slots:
        void onTimerTick();
        void onRefreshRateChanged(int comboIndex);
        void onHeaderContextMenu(const QPoint &pos);
        void onTableContextMenu(const QPoint &pos);
        void updateStatusBar();

    private:
        Ui::ProcessesWidget      *ui;
        Os::ProcessModel         *m_model;
        Os::ProcessFilterProxy   *m_proxy;
        QTimer                   *m_refreshTimer;

        void setupTable();
        void setupRefreshCombo();

        /// Collect PIDs of all currently selected rows.
        QList<pid_t> selectedPids() const;

        /// Send signal to all selected processes; show error dialog on failure.
        void sendSignalToSelected(int signal);

        /// Open renice dialog for all selected processes.
        void reniceSelected();
};

#endif // PROCESSESWIDGET_H
