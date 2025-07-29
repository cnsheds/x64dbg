#pragma once

#include "TypeWidget.h"
#include "Bridge.h"
#include "ActionHelpers.h"

class GotoDialog;

class StructWidget : public TypeWidget, public ActionHelper<StructWidget>
{
    Q_OBJECT

public:
    explicit StructWidget(QWidget* parent = nullptr);

private:
    MenuBuilder* mMenuBuilder = nullptr;
    GotoDialog* mGotoDialog = nullptr;
    SCRIPTTYPEINFO* mAutoCompleteInfo;

    duint selectedValue() const;
    void setupContextMenu();

private slots:
    void registerAutoComplete(SCRIPTTYPEINFO* info);
    void typeAddNodeSlot(void* parent, const TYPEDESCRIPTOR* type);
    void typeClearSlot();
    void typeVisitSlot(QString typeName, duint addr);
    void dbgStateChangedSlot(DBGSTATE state);
    void contextMenuRequestedSlot(const QPoint & pos);
    void followDumpSlot();
    void followValueDumpSlot();
    void followValueDisasmSlot();
    void clearSlot();
    void removeSlot();
    void displayTypeSlot();
    void loadJsonSlot();
    void parseFileSlot();
    void reloadTypeSlot();
    void copyColumnSlot();
};
