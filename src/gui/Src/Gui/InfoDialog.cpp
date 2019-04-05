#include "InfoDialog.h"
#include "ui_InfoDialog.h"
#include "StringUtil.h"
#include <Configuration.h>
#include "MainWindow.h"
#include "CPUWidget.h"
#include "CPUMultiDump.h"
#include "CPUDump.h"
#include "CPUStack.h"

//=============================================================
InfoDialog::InfoDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::InfoDialog)
{
    m_bHex16Value = false;
    m_lastValue = 0;
    mcontPaintEvent = 0;
    m_initAlpha = 230;
    m_bTime64 = false;

    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint
                   | Qt::FramelessWindowHint
                   | Qt::MSWindowsFixedSizeDialogHint);

    adjustSize();

    QPalette pl = palette();
    pl.setColor(QPalette::Background, QColor(255, 255, 34, 180));    //   设置背景颜色为黑色，如果不设置默认为白色
    pl.setColor(QPalette::WindowText, QColor(0, 0, 0, 255));
    setPalette(pl);
    setAutoFillBackground(true);
    setWindowOpacity(0.9);     //   背景和元素都设置透明效果

    QString editSytle = "QLineEdit{background:transparent;border-width:1;border-style:outset";
    editSytle += ";border-color:rgba(0,0,0,90)}";

    ui->edit_rva->setStyleSheet(editSytle);
    ui->edit_module->setStyleSheet(editSytle);
    ui->edit_char->setStyleSheet(editSytle);
    ui->edit_int->setStyleSheet(editSytle);
    ui->edit_float->setStyleSheet(editSytle);
    ui->edit_double->setStyleSheet(editSytle);
    ui->edit_short->setStyleSheet(editSytle);
    ui->edit_timet->setStyleSheet(editSytle);
    ui->edit_variant->setStyleSheet(editSytle);
    ui->edit_cstr->setStyleSheet(editSytle);
    ui->edit_ustr->setStyleSheet(editSytle);
    ui->edit_utf8->setStyleSheet(editSytle);

    ui->label_timet->installEventFilter(this);

    connect(ui->edit_rva, SIGNAL(returnPressed()), this, SLOT(SaveRVAName()));
    connect(Bridge::getBridge(), SIGNAL(addRecentFile(QString)), this, SLOT(setDbgMainModule(QString)));

    SetButtonStyle(ui->btn_close, ":/icons/images/btn_close.png", 3);
    Config()->setupWindowPos(this);
}

InfoDialog::~InfoDialog()
{
    Config()->saveWindowPos(this);
    delete ui;
}

void InfoDialog::SetButtonStyle(QPushButton* button, QString imgsrc, int CutSec)
{
    int img_w = QPixmap(imgsrc).width();
    int img_h = QPixmap(imgsrc).height();
    int PicWidth = img_w / CutSec;
    button->setFixedSize(PicWidth, img_h);
    button->setStyleSheet(QString("QPushButton{border-width: 41px; border-image: url(%1)  0 0 0 %2 repeat  repeat;border-width: 0px; border-radius: 0px;}")
                          .append("QPushButton::hover{border-image: url(%1) 0 0 0 %3  repeat  repeat;}")
                          .append("QPushButton::pressed{border-image: url(%1) 0  0 0 %4 repeat  repeat;}")
                          .append("QPushButton::checked{border-image: url(%1) 0  0 0 %4 repeat  repeat;}")
                          .append("QPushButton::disabled{border-image: url(%1) 0  0 0 %5 repeat  repeat;}")
                          .arg(imgsrc).arg(0).arg(PicWidth * 1).arg(PicWidth * 2).arg(PicWidth * 3));
}

void InfoDialog::mousePressEvent(QMouseEvent* e)
{
    if(e->button() == Qt::LeftButton)
    {
        mMousePoint = e->globalPos() - this->pos();
        e->accept();
        mDragWindow = true;
    }
}

void InfoDialog::mouseReleaseEvent(QMouseEvent* e)
{
    mDragWindow = false;
}

void InfoDialog::mouseMoveEvent(QMouseEvent* e)
{
    if(mDragWindow && (e->buttons() && Qt::LeftButton))
    {
        this->move(e->globalPos() - mMousePoint);
        e->accept();
    }
}

void InfoDialog::wheelEvent(QWheelEvent* event)
{
    int zDelta = event->delta();
    if(m_initAlpha < 150 && zDelta < 0)
        return;
    if(m_initAlpha > 254 && zDelta > 0)
        return;
    m_initAlpha += zDelta / 40;
    setWindowOpacity((float)m_initAlpha / 255);
}

void InfoDialog::setDbgMainModule(QString filepath)
{
    mDbgBaseFilename = filepath;
    int pos = filepath.lastIndexOf("\\");
    if(pos != -1)
        mDbgBaseFilename = filepath.mid(pos + 1);

    mDbgModuleRVAFilename = QString("%1/db/%2.xml").arg(QCoreApplication::applicationDirPath(), mDbgBaseFilename);
}

void InfoDialog::SetCpuWidget(CPUWidget* cpuWidget)
{
    pCpuWidget = cpuWidget;

    RegistersView* pGeneralRegs = pCpuWidget->getRegisterWidget();
    connect((QObject*)pGeneralRegs, SIGNAL(showSelectInfo(uint64, int)), this, SLOT(showSelectInfoSlot(uint64, int)));
    CPUStack* pStackWidget = pCpuWidget->getStackWidget();
    connect((QObject*)pStackWidget, SIGNAL(showSelectInfo(uint64, int)), this, SLOT(showSelectInfoSlot(uint64, int)));

    CPUMultiDump* pMultiDump = pCpuWidget->getDumpWidget();
    for(int i = 0; i < pMultiDump->getMaxCPUTabs(); i++)
    {
        CPUDump* current = qobject_cast<CPUDump*>(pMultiDump->widget(i));
        connect((QObject*)current, SIGNAL(showSelectInfo(uint64, int)), this, SLOT(showSelectInfoSlot(uint64, int)));
    }
}

void InfoDialog::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    if(mcontPaintEvent == 0)
    {
        QString text("F");
        QFontMetrics fm = painter.fontMetrics();
        int charWidth = fm.width(text);
        int wndHeight = ui->edit_rva->height() * 11 + ui->widget_title->height() + 3;
        setFixedSize(charWidth * 35, wndHeight);
        mcontPaintEvent++;
    }
}

void InfoDialog::showSelectInfoSlot(uint64 addr, int nWidget)
{
    uint64 value = addr;
    if(nWidget == 1)    //Dump
        DbgMemRead(addr, &value, 8);
    else if(nWidget == 2)   //Stack
        DbgMemRead(addr, &value, sizeof(duint));

    UpdateInfo(value);
}

QString SignedToHex(int value)
{
    QString strret;
    if(value < 0)
    {
        int uvalue = abs(value);
        strret = "-";
        strret += QString().sprintf("%X", uvalue);
    }
    else
        strret = QString().sprintf("%X", value);

    return strret;
}

std::string WChar2Ansi(LPCWSTR pwszSrc)
{
    int nLen = WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, NULL, 0, NULL, NULL);

    if(nLen <= 0) return std::string("");

    char* pszDst = new char[nLen];
    if(NULL == pszDst) return std::string("");

    WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, pszDst, nLen, NULL, NULL);
    pszDst[nLen - 1] = 0;

    std::string strTemp(pszDst);
    delete[] pszDst;

    return strTemp;
}

void InfoDialog::SaveRVAName()
{
    saddr rva = (saddr)ui->edit_rva->userData(1980);
    if(rva == 0)
        return;

    QString strRVA = ui->edit_rva->text();
    QString strModule = ui->edit_module->text();
    if(strRVA.indexOf("[") != -1)
        return;
    int modulename_off = (int)ui->edit_module->userData(1980);
    if(modulename_off > 0)
        strModule = strModule.mid(modulename_off + 1);

    if(m_rvaInfo.AddRVAInfo(rva, strModule, strRVA))
    {
        m_rvaInfo.SaveRVAInfo(mDbgModuleRVAFilename);
        UpdateInfo(m_lastValue);
    }
}

void InfoDialog::UpdateInfo(uint64 value)
{
    duint rva = 0, base = 0;
    char String[260] = { 0 };
    wchar_t Unicode[260] = { 0 };

    QString showText;
    m_lastValue = value;
    DWORD _oDword = (DWORD)value;

    duint bShowInfoWindow = true;
    BridgeSettingGetUint("Gui", "ShowInfoWindow", &bShowInfoWindow);
    if(!bShowInfoWindow)
        return;

    if(!isVisible() && (GetAsyncKeyState(VK_CONTROL) & 0x8000))
    {
        m_rvaInfo.LoadRVAInfo(mDbgModuleRVAFilename);
        showNormal();
    }

    showText = ToHexString(value);
    ui->label_addr->setText(showText);

    base = DbgFunctions()->ModBaseFromAddr(value);

    ui->edit_module->setUserData(1980, (QObjectUserData*)0);
    char modname[MAX_MODULE_SIZE] = "";
    if(base && DbgFunctions()->ModNameFromAddr(value, modname, true))
    {
#ifdef _WIN64
        rva = value - base;
#else
        rva = _oDword - base;
#endif
        QString rvaName = m_rvaInfo.GetRVAname(rva, modname);
        if(rvaName.length() > 0)
        {
            ui->edit_module->setUserData(1980, (QObjectUserData*)rvaName.length());
            rvaName += "!";
            rvaName += modname;
            ui->edit_module->setText(rvaName);
            ui->edit_module->setCursorPosition(0);
        }
        else
            ui->edit_module->setText(QString(modname) + "");
    }
    else
        ui->edit_module->setText("");

    if(base)
#ifdef _WIN64
        showText.sprintf("%llX [%llX]", value - base, base);
#else
        showText.sprintf("%X [%X]", _oDword - base, base);
#endif
    else
        showText = ToHexString(rva);

    ui->edit_rva->setText(showText);
    ui->edit_rva->setUserData(1980, (QObjectUserData*)rva);

    memset(String, 0, sizeof(String));
    bool bread = false;
#ifdef _WIN64
    bread = DbgMemRead(value, String, 256);
#else
    bread = DbgMemRead(_oDword, String, 256);
#endif
    if(bread)
    {
        wcscpy_s(Unicode, 256, (wchar_t*)String);
        QString strUTF8 = QString(String).toUtf8();
        std::string czString = WChar2Ansi(Unicode);
        ui->edit_cstr->setText(String);
        ui->edit_ustr->setText(czString.c_str());
        ui->edit_utf8->setText(strUTF8);
    }
    else
    {
        ui->edit_cstr->setText("");
        ui->edit_ustr->setText("");
        ui->edit_utf8->setText("");
    }

    //////////////////////////////////////////////////////////////////////////
    DWORD _dword = (DWORD)value;
    if(ui->checkBE->isChecked())
    {
        value = _byteswap_uint64(value);
        _dword = _byteswap_ulong(_dword);
    }

    float _float;
    double _Double;
    ushort _word = (WORD)_dword;
    uchar _byte = (BYTE)_dword;
    _float = *(float*)&_dword;
    _Double = *(double*)&value;

    if(m_bHex16Value)
    {
        showText.sprintf("%s(%X)", SignedToHex((signed char)_byte).toLocal8Bit().constData(), _byte);
        ui->edit_char->setText(showText);
        showText.sprintf("%s(%X)", SignedToHex(_dword).toLocal8Bit().constData(), _dword);
        ui->edit_int->setText(showText);
        showText.sprintf("%.7g", _float);
        ui->edit_float->setText(showText);
        showText.sprintf("%.13g", _Double);
        ui->edit_double->setText(showText);
        showText.sprintf("%s(%X)", SignedToHex((signed short)_word).toLocal8Bit().constData(), _word);
        ui->edit_short->setText(showText);
    }
    else
    {
        showText.sprintf("%d(%u)", (signed char)_byte, _byte);
        ui->edit_char->setText(showText);
        showText.sprintf("%d(%u)", _dword, _dword);
        ui->edit_int->setText(showText);
        showText.sprintf("%.7g", _float);
        ui->edit_float->setText(showText);
        showText.sprintf("%.13g", _Double);
        ui->edit_double->setText(showText);
        showText.sprintf("%d(%u)", (signed short)_word, _word);
        ui->edit_short->setText(showText);
    }

    struct tm* ptm = nullptr;
    if (!m_bTime64)
    {
        ptm = _gmtime32((__time32_t*)&_dword);
        ui->label_timet->setText("time_t:");
    }else
    {
        ptm = _gmtime64((__time64_t*)&value);
        ui->label_timet->setText("time64:");
    }

    if(ptm)
    {
        /* format: YYYY/MM/DD HH24:MI:SS */
        showText.sprintf("%04d/%02d/%02d %02d:%02d:%02d",
                         1900 + ptm->tm_year, ptm->tm_mon, ptm->tm_mday,
                         ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
        ui->edit_timet->setText(showText);
    }
    else
        ui->edit_timet->setText("");

    ui->edit_variant->setText(GetVariant(value));
}

rstring InfoDialog::GetVariant(uint64 value)
{
    VARTYPE varType = (VARTYPE)value;
    rstring strRet;
    switch (varType & VT_TYPEMASK) {
    case VT_EMPTY:
        strRet = "VT_EMPTY";
        break;
    case VT_NULL:
        strRet = "VT_NULL";
        break;
    case VT_I2:
        strRet = "VT_I2";
        break;
    case VT_I4:
        strRet = "VT_I4";
        break;
    case VT_R4:
        strRet = "VT_R4";
        break;
    case VT_R8:
        strRet = "VT_R8";
        break;
    case VT_CY:
        strRet = "VT_CY";
        break;
    case VT_DATE:
        strRet = "VT_DATE";
        break;
    case VT_BSTR:
        strRet = "VT_BSTR";
        break;
    case VT_DISPATCH:
        strRet = "VT_DISPATCH";
        break;
    case VT_ERROR:
        strRet = "VT_ERROR";
        break;
    case VT_BOOL:
        strRet = "VT_BOOL";
        break;
    case VT_VARIANT:
        strRet = "VT_VARIANT";
        break;
    case VT_UNKNOWN:
        strRet = "VT_UNKNOWN";
        break;
    case VT_DECIMAL:
        strRet = "VT_DECIMAL";
        break;
    case VT_I1:
        strRet = "VT_I1";
        break;
    case VT_UI1:
        strRet = "VT_UI1";
        break;
    case VT_UI2:
        strRet = "VT_UI2";
        break;
    case VT_UI4:
        strRet = "VT_UI4";
        break;
    case VT_I8:
        strRet = "VT_I8";
        break;
    case VT_UI8:
        strRet = "VT_UI8";
        break;
    case VT_INT:
        strRet = "VT_INT";
        break;
    case VT_UINT:
        strRet = "VT_UINT";
        break;
    case VT_VOID:
        strRet = "VT_VOID";
        break;
    case VT_HRESULT:
        strRet = "VT_HRESULT";
        break;
    case VT_PTR:
        strRet = "VT_PTR";
        break;
    case VT_SAFEARRAY:
        strRet = "VT_SAFEARRAY";
        break;
    case VT_CARRAY:
        strRet = "VT_CARRAY";
        break;
    case VT_USERDEFINED:
        strRet = "VT_USERDEFINED";
        break;
    case VT_LPSTR:
        strRet = "VT_LPSTR";
        break;
    case VT_LPWSTR:
        strRet = "VT_LPWSTR";
        break;
    case VT_RECORD:
        strRet = "VT_RECORD";
        break;
    case VT_INT_PTR:
        strRet = "VT_INT_PTR";
        break;
    case VT_UINT_PTR:
        strRet = "VT_UINT_PTR";
        break;
    case VT_FILETIME:
        strRet = "VT_FILETIME";
        break;
    case VT_BLOB:
        strRet = "VT_BLOB";
        break;
    case VT_STREAM:
        strRet = "VT_STREAM";
        break;
    case VT_STORAGE:
        strRet = "VT_STORAGE";
        break;
    case VT_STREAMED_OBJECT:
        strRet = "VT_STREAMED_OBJECT";
        break;
    case VT_STORED_OBJECT:
        strRet = "VT_STORED_OBJECT";
        break;
    case VT_BLOB_OBJECT:
        strRet = "VT_BLOB_OBJECT";
        break;
    case VT_CF:
        strRet = "VT_CF";
        break;
    case VT_CLSID:
        strRet = "VT_CLSID";
        break;
    case VT_VERSIONED_STREAM:
        strRet = "VT_VERSIONED_STREAM";
        break;
    case VT_BSTR_BLOB:
        strRet = "VT_BSTR_BLOB";
        break;
    }

    if (!strRet.isEmpty())
    {
        switch (varType & 0xF000) {
        case VT_VECTOR:
            strRet.insert(0, "VT_VECTOR|");
            break;
        case VT_ARRAY:
            strRet.insert(0, "VT_ARRAY|");
            break;
        case VT_BYREF:
            strRet.insert(0, "VT_BYREF|");
            break;
        case VT_RESERVED:
            strRet.insert(0, "VT_RESERVED|");
            break;
        }
    }

    return strRet;
}

bool InfoDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->label_timet) {
        if (event->type() == QEvent::MouseButtonPress) {
            m_bTime64 = !m_bTime64;
            UpdateInfo(m_lastValue);
            return true;
        } else {
            return false;
        }
    } else {
        // pass the event on to the parent class
        return QDialog::eventFilter(obj, event);
    }
}

void InfoDialog::on_btn_close_clicked()
{
    hide();
}

void InfoDialog::on_checkHex_toggled(bool checked)
{
    m_bHex16Value = checked;
    UpdateInfo(m_lastValue);
}

void InfoDialog::on_checkBE_toggled(bool checked)
{
    UpdateInfo(m_lastValue);
}
