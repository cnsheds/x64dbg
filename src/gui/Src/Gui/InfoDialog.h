#ifndef INFODIALOG_H
#define INFODIALOG_H

#include <QDialog>
#include "Imports.h"
#include "Bridge.h"
#include "RvaInfo.h"

namespace Ui
{
    class InfoDialog;
}
class CPUWidget;


class InfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InfoDialog(QWidget* parent = 0);
    ~InfoDialog();
    void SetCpuWidget(CPUWidget*);
    void paintEvent(QPaintEvent* event);
    void disconnectCpuWidget();

signals:
    void showSelectInfo(uint64 addr, int nWidget);


protected:
    void mouseMoveEvent(QMouseEvent* e);
    void mouseReleaseEvent(QMouseEvent*);
    void mousePressEvent(QMouseEvent* e);
    void wheelEvent(QWheelEvent* event);


    void SetButtonStyle(QPushButton* button, QString imgsrc, int CutSec);
    void UpdateInfo(uint64 value);
    rstring GetVariant(uint64 value);

    bool eventFilter(QObject *obj, QEvent *ev);

private slots:
    void showSelectInfoSlot(uint64 addr, int nWidget);
    void dbgStateChanged(DBGSTATE state);

    void on_btn_close_clicked();
    void on_checkHex_toggled(bool checked);
    void on_checkBE_toggled(bool checked);
    void SaveRVAName();
    void setDbgMainModule(QString file);

private:
    CRvaInfo m_rvaInfo;
    Ui::InfoDialog* ui;
    bool mDragWindow;
    QPoint mMousePoint;
    uint64 m_lastValue;
    int m_initAlpha;

    CPUWidget* pCpuWidget;
    bool m_bTime64;
    bool m_bInt64;
    boolean m_bHex16Value;
    int mcontPaintEvent;
    QString mDbgBaseFilename;
    QString mDbgModuleRVAFilename;
};

#endif // INFODIALOG_H
