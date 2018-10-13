#include "AutoCompleteEdit.h"
#include "Bridge.h"
#include "Configuration.h"

AutoCompleteEdit::AutoCompleteEdit(QWidget* parent) : QLineEdit(parent)
{
    infoAutoComplete = nullptr;

    //Initialize QCompleter
    mCompleter = new QCompleter(QStringList(), this);
    mCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    mCompleter->setCompletionMode(QCompleter::PopupCompletion);
    mCompleter->setFilterMode(Qt::MatchContains);
    mCompleterModel = (QStringListModel*)mCompleter->model();
    this->setCompleter(mCompleter);

    //Setup signals & slots
    //connect(mCompleter, SIGNAL(activated(const QString &)), this, SLOT(clear()), Qt::QueuedConnection);
    connect(this, SIGNAL(textEdited(QString)), this, SLOT(autoCompleteUpdate(QString)));
    connect(Config(), SIGNAL(fontsUpdated()), this, SLOT(fontsUpdated()));

    fontsUpdated();
}

AutoCompleteEdit::~AutoCompleteEdit()
{
}

void AutoCompleteEdit::keyPressEvent(QKeyEvent* event)
{
    if(event->type() == QEvent::KeyPress && event->key() == Qt::Key_Tab)
    {
        // TAB autocompletes the command
        QStringList stringList = mCompleterModel->stringList();

        if(stringList.size())
        {
            QAbstractItemView* popup = mCompleter->popup();
            QModelIndex currentModelIndex = popup->currentIndex();

            // If not item selected, select first one in the list
            if(currentModelIndex.row() < 0)
                currentModelIndex = mCompleter->currentIndex();

            // If popup list is not visible, selected next suggested command
            if(!popup->isVisible())
            {
                for(int row = 0; row < popup->model()->rowCount(); row++)
                {
                    QModelIndex modelIndex = popup->model()->index(row, 0);

                    // If the lineedit contains a suggested command, get the next suggested one
                    if(popup->model()->data(modelIndex) == this->text())
                    {
                        int nextModelIndexRow = (currentModelIndex.row() + 1) % popup->model()->rowCount();
                        currentModelIndex = popup->model()->index(nextModelIndexRow, 0);
                        break;
                    }
                }
            }

            popup->setCurrentIndex(currentModelIndex);
            popup->hide();
        }
    }
    else
        QLineEdit::keyPressEvent(event);
}

// Disables moving to Prev/Next child when pressing tab
bool AutoCompleteEdit::focusNextPrevChild(bool next)
{
    Q_UNUSED(next);
    return false;
}

void AutoCompleteEdit::setAutoComplete(SCRIPTTYPEINFO* info)
{
    infoAutoComplete = info;
}

void AutoCompleteEdit::autoCompleteUpdate(const QString text)
{
    // No command, no completer
    if(text.length() <= 0)
    {
        mCompleterModel->setStringList(QStringList());
    }
    else
    {
        // Save current index
        QModelIndex modelIndex = mCompleter->popup()->currentIndex();

        // User supplied callback
        GUISCRIPTCOMPLETER complete = nullptr;
        if(infoAutoComplete)
            complete = infoAutoComplete->completeCommand;

        if(complete)
        {
            // This will hold an array of strings allocated by BridgeAlloc
            char* completionList[64];
            int completionCount = _countof(completionList);

            complete(text.toUtf8().constData(), completionList, &completionCount);
            if(completionCount > 0)
            {
                QStringList stringList;
                // Append to the QCompleter string list and free the data
                for(int i = 0; i < completionCount; i++)
                {
                    stringList.append(completionList[i]);
                    BridgeFree(completionList[i]);
                }
                mCompleterModel->setStringList(stringList);
            }
            else
            {
                // Otherwise set the completer to nothing
                mCompleterModel->setStringList(QStringList());
            }
        }
        else
        {
            // Native auto-completion
            mCompleterModel->setStringList(mDefaultCompletions);
        }

        // Restore index
        if(mCompleter->popup()->model()->rowCount() > modelIndex.row())
            mCompleter->popup()->setCurrentIndex(modelIndex);
    }
}

void AutoCompleteEdit::fontsUpdated()
{
    setFont(ConfigFont("Log"));
    mCompleter->popup()->setFont(ConfigFont("Log"));
}

void AutoCompleteEdit::setFocus()
{
    QLineEdit::setFocus();
}
