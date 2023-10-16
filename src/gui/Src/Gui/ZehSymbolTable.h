#pragma once

#include "AbstractStdTable.h"
#include <QMutex>

class ZehSymbolTable : public AbstractStdTable
{
    Q_OBJECT
public:
    ZehSymbolTable(QWidget* parent = nullptr);

    QString getCellContent(duint r, duint c) override;
    void updateColors() override;
    bool isValidIndex(duint r, duint c) override;
    void sortRows(duint column, bool ascending) override;

    friend class SymbolView;
    friend class SearchListViewSymbols;
    friend class SymbolSearchList;

protected:
    QColor getSpecialColor(int r, int c);
private:
    std::vector<duint> mModules;
    std::vector<SYMBOLPTR> mData;
    QMutex mMutex;

    //Caching of translations to fix a bottleneck
    QString trImport;
    QString trExport;
    QString trSymbol;
    QColor  mBkColorOfImport;
    enum
    {
        ColAddr,
        ColType,
        ColOrdinal,
        ColDecorated,
        ColUndecorated
    };

    QString symbolInfoString(const SYMBOLINFO* info, duint c);
};
