#ifndef AUTOCOMPLETE_LINEEDIT_H
#define AUTOCOMPLETE_LINEEDIT_H

#include "bridge/bridgemain.h"
#include <QLineEdit>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QCompleter>
#include <QComboBox>
#include <QStringListModel>

class AutoCompleteEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit AutoCompleteEdit(QWidget* parent = 0);
    ~AutoCompleteEdit();

    void keyPressEvent(QKeyEvent* event);
    void setFocus();
    bool focusNextPrevChild(bool next);
    void setAutoComplete(SCRIPTTYPEINFO* info);

public slots:
    void autoCompleteUpdate(const QString text);

    void fontsUpdated();

private:
    QCompleter* mCompleter;
    QStringListModel* mCompleterModel;
    QStringList mDefaultCompletions;
    SCRIPTTYPEINFO* infoAutoComplete;
};

#endif // AUTOCOMPLETE_LINEEDIT_H
