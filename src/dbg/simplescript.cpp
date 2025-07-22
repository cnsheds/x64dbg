/**
 @file simplescript.cpp

 @brief Implements the simplescript class.
 */

#include "simplescript.h"
#include "console.h"
#include "variable.h"
#include "debugger.h"
#include "filehelper.h"
#include "jobqueue.h"
#include "threading.h"
#include <atomic>

static JobQueue scriptQueue;

enum CMDRESULT
{
    STATUS_ERROR = false,
    STATUS_CONTINUE = true,
    STATUS_EXIT = 2,
    STATUS_PAUSE = 3,
    STATUS_CONTINUE_BRANCH = 4
};

struct SCRIPTBP
{
    int line;
    bool silent; //do not show in GUI
};

struct LINEMAPENTRY
{
    SCRIPTLINETYPE type;
    char raw[256];
    union
    {
        char command[256];
        SCRIPTBRANCH branch;
        char label[256];
        char comment[256];
    } u;
};

static std::vector<LINEMAPENTRY> linemap;
static std::vector<SCRIPTBP> scriptbplist;
static std::vector<int> scriptstack;
static int scriptIp = 0;
static int scriptIpOld = 0;
static std::atomic_bool bScriptAbort = false;
static std::atomic_bool bScriptRunning = false;
static std::atomic_bool bScriptLogEnabled = false;
static CMDRESULT scriptLastError = STATUS_ERROR;

static SCRIPTBRANCHTYPE scriptGetBranchType(const char* text)
{
    char newtext[MAX_SCRIPT_LINE_SIZE] = "";
    strcpy_s(newtext, StringUtils::Trim(text).c_str());
    if(!strstr(newtext, " "))
        strcat_s(newtext, " ");
    if(!strncmp(newtext, "jmp ", 4) || !strncmp(newtext, "goto ", 5))
        return scriptjmp;
    else if(!strncmp(newtext, "jbe ", 4) || !strncmp(newtext, "ifbe ", 5) || !strncmp(newtext, "ifbeq ", 6) || !strncmp(newtext, "jle ", 4) || !strncmp(newtext, "ifle ", 5) || !strncmp(newtext, "ifleq ", 6))
        return scriptjbejle;
    else if(!strncmp(newtext, "jae ", 4) || !strncmp(newtext, "ifae ", 5) || !strncmp(newtext, "ifaeq ", 6) || !strncmp(newtext, "jge ", 4) || !strncmp(newtext, "ifge ", 5) || !strncmp(newtext, "ifgeq ", 6))
        return scriptjaejge;
    else if(!strncmp(newtext, "jne ", 4) || !strncmp(newtext, "ifne ", 5) || !strncmp(newtext, "ifneq ", 6) || !strncmp(newtext, "jnz ", 4) || !strncmp(newtext, "ifnz ", 5))
        return scriptjnejnz;
    else if(!strncmp(newtext, "je ", 3)  || !strncmp(newtext, "ife ", 4) || !strncmp(newtext, "ifeq ", 5) || !strncmp(newtext, "jz ", 3) || !strncmp(newtext, "ifz ", 4))
        return scriptjejz;
    else if(!strncmp(newtext, "jb ", 3) || !strncmp(newtext, "ifb ", 4) || !strncmp(newtext, "jl ", 3) || !strncmp(newtext, "ifl ", 4))
        return scriptjbjl;
    else if(!strncmp(newtext, "ja ", 3) || !strncmp(newtext, "ifa ", 4) || !strncmp(newtext, "jg ", 3) || !strncmp(newtext, "ifg ", 4))
        return scriptjajg;
    else if(!strncmp(newtext, "call ", 5))
        return scriptcall;
    return scriptnobranch;
}

static int scriptLabelFind(const char* labelname)
{
    SHARED_ACQUIRE(LockScriptLineMap);
    int linecount = (int)linemap.size();
    for(int i = 0; i < linecount; i++)
        if(linemap.at(i).type == linelabel && !strcmp(linemap.at(i).u.label, labelname))
            return i + 1;
    return 0;
}

static bool isEmptyLine(SCRIPTLINETYPE type)
{
    return type == lineempty || type == linecomment || type == linelabel;
}

static int scriptInternalStep(int fromIp) //internal step routine
{
    SHARED_ACQUIRE(LockScriptLineMap);
    int maxIp = (int)linemap.size(); //maximum ip
    if(fromIp >= maxIp) //script end
        return fromIp;
    while(isEmptyLine(linemap.at(fromIp).type) && fromIp < maxIp) //skip empty lines
        fromIp++;
    fromIp++;
    return fromIp;
}

static bool scriptIsInternalCommand(const char* text, const char* cmd)
{
    int len = (int)strlen(text);
    int cmdlen = (int)strlen(cmd);
    if(cmdlen > len)
        return false;
    else if(cmdlen == len)
        return scmp(text, cmd);
    else if(text[cmdlen] == ' ')
        return (!_strnicmp(text, cmd, cmdlen));
    return false;
}

static bool scriptCreateLineMap(const char* filename)
{
    String filedata;
    if(!FileHelper::ReadAllText(filename, filedata))
    {
        String TranslatedString = GuiTranslateText(QT_TRANSLATE_NOOP("DBG", "FileHelper::ReadAllText failed..."));
        GuiScriptError(0, TranslatedString.c_str());
        return false;
    }
    auto len = filedata.length();
    char temp[256] = "";
    LINEMAPENTRY entry = {};
    linemap.clear();
    for(size_t i = 0, j = 0; i < len; i++) //make raw line map
    {
        if(filedata[i] == '\r' && filedata[i + 1] == '\n') //windows file
        {
            entry = {};
            int add = 0;
            while(isspace(temp[add]))
                add++;
            strcpy_s(entry.raw, temp + add);
            *temp = 0;
            j = 0;
            i++;
            linemap.push_back(entry);
        }
        else if(filedata[i] == '\n') //other file
        {
            entry = {};
            int add = 0;
            while(isspace(temp[add]))
                add++;
            strcpy_s(entry.raw, temp + add);
            *temp = 0;
            j = 0;
            linemap.push_back(entry);
        }
        else if(j >= 254)
        {
            entry = {};
            int add = 0;
            while(isspace(temp[add]))
                add++;
            strcpy_s(entry.raw, temp + add);
            *temp = 0;
            j = 0;
            linemap.push_back(entry);
        }
        else
            j += sprintf_s(temp + j, sizeof(temp) - j, "%c", filedata[i]);
    }
    if(*temp)
    {
        entry = {};
        strcpy_s(entry.raw, temp);
        linemap.push_back(entry);
    }
    int linemapsize = (int)linemap.size();
    while(linemapsize && !*linemap.at(linemapsize - 1).raw) //remove empty lines from the end
    {
        linemapsize--;
        linemap.pop_back();
    }
    for(int i = 0; i < linemapsize; i++)
    {
        LINEMAPENTRY cur = linemap.at(i);

        //temp. remove comments from the raw line
        char line_comment[256] = "";
        char* comment = nullptr;
        {
            auto len = strlen(cur.raw);
            auto inquote = false;
            auto inescape = false;
            for(size_t i = 0; i < len; i++)
            {
                auto ch = cur.raw[i];
                switch(ch) //simple state machine to determine if the "//" is in quotes
                {
                case '\"':
                    if(!inescape)
                        inquote = !inquote;
                    inescape = false;
                    break;
                case '\\':
                    inescape = !inescape;
                    break;
                default:
                    inescape = false;
                }
                if(!inquote && ch == '/' && i + 1 < len && cur.raw[i + 1] == '/')
                {
                    comment = cur.raw + i;
                    break;
                }
            }
        }

        if(comment && comment != cur.raw) //only when the line doesn't start with a comment
        {
            if(*(comment - 1) == ' ') //space before comment
            {
                strcpy_s(line_comment, comment);
                *(comment - 1) = '\0';
            }
            else //no space before comment
            {
                strcpy_s(line_comment, comment);
                *comment = 0;
            }
        }

        int rawlen = (int)strlen(cur.raw);
        while((rawlen > 0) && isspace(cur.raw[rawlen - 1]))  //Trim trailing whitespace
        {
            rawlen--;
        }
        cur.raw[rawlen] = '\0'; //// Truncate the string by setting a new null terminator
        if(!rawlen) //empty
        {
            cur.type = lineempty;
        }
        else if(!strncmp(cur.raw, "//", 2) || *cur.raw == ';') //comment
        {
            cur.type = linecomment;
            strcpy_s(cur.u.comment, cur.raw);
        }
        else if(cur.raw[rawlen - 1] == ':') //label
        {
            cur.type = linelabel;
            sprintf_s(cur.u.label, "l %.*s", rawlen - 1, cur.raw); //create a fake command for formatting
            strcpy_s(cur.u.label, StringUtils::Trim(cur.u.label).c_str());
            strcpy_s(temp, cur.u.label + 2);
            strcpy_s(cur.u.label, temp); //remove fake command
            if(!*cur.u.label || !strcmp(cur.u.label, "\"\"")) //no label text
            {
                char message[256] = "";
                sprintf_s(message, GuiTranslateText(QT_TRANSLATE_NOOP("DBG", "Empty label detected on line %d!")), i + 1);
                GuiScriptError(0, message);
                linemap.clear();
                return false;
            }
            int foundlabel = scriptLabelFind(cur.u.label);
            if(foundlabel) //label defined twice
            {
                char message[256] = "";
                sprintf_s(message, GuiTranslateText(QT_TRANSLATE_NOOP("DBG", "Duplicate label \"%s\" detected on lines %d and %d!")), cur.u.label, foundlabel, i + 1);
                GuiScriptError(0, message);
                linemap.clear();
                return false;
            }
        }
        else if(scriptGetBranchType(cur.raw) != scriptnobranch) //branch
        {
            cur.type = linebranch;
            cur.u.branch.type = scriptGetBranchType(cur.raw);
            char newraw[MAX_SCRIPT_LINE_SIZE] = "";
            strcpy_s(newraw, StringUtils::Trim(cur.raw).c_str());
            int rlen = (int)strlen(newraw);
            for(int j = 0; j < rlen; j++)
                if(newraw[j] == ' ')
                {
                    strcpy_s(cur.u.branch.branchlabel, newraw + j + 1);
                    break;
                }
        }
        else
        {
            cur.type = linecommand;
            strcpy_s(cur.u.command, cur.raw);
        }

        //append the comment to the raw line again
        if(*line_comment)
            sprintf_s(cur.raw + rawlen, sizeof(cur.raw) - rawlen, "\1%s", line_comment);
        linemap.at(i) = cur;
    }
    linemapsize = (int)linemap.size();
    for(int i = 0; i < linemapsize; i++)
    {
        auto & currentLine = linemap.at(i);
        if(currentLine.type == linebranch) //invalid branch label
        {
            int labelline = scriptLabelFind(currentLine.u.branch.branchlabel);
            if(!labelline) //invalid branch label
            {
                char message[256] = "";
                sprintf_s(message, GuiTranslateText(QT_TRANSLATE_NOOP("DBG", "Invalid branch label \"%s\" detected on line %d!")), currentLine.u.branch.branchlabel, i + 1);
                GuiScriptError(0, message);
                linemap.clear();
                return false;
            }
            else //set the branch destination line
                currentLine.u.branch.dest = scriptInternalStep(labelline);
        }
    }
    // append a ret instruction at the end of the script when appropriate
    if(!linemap.empty())
    {
        entry = {};
        entry.type = linecommand;
        strcpy_s(entry.raw, "ret");
        strcpy_s(entry.u.command, "ret");

        const auto & lastline = linemap.back();
        switch(lastline.type)
        {
        case linecommand:
            if(scriptIsInternalCommand(lastline.u.command, "ret")
                    || scriptIsInternalCommand(lastline.u.command, "invalid")
                    || scriptIsInternalCommand(lastline.u.command, "error"))
            {
                // there is already a terminating command at the end of the script
            }
            else
            {
                linemap.push_back(entry);
            }
            break;
        case linebranch:
            // an unconditional branch at the end of the script can only go back
            if(lastline.u.branch.type == scriptjmp)
                break;
        // fallthough to append the ret
        case linelabel:
        case linecomment:
        case lineempty:
            linemap.push_back(entry);
            break;
        }
    }
    return true;
}

static bool scriptInternalBpGet(int line) //internal bpget routine
{
    SHARED_ACQUIRE(LockScriptBreakpoints);
    int bpcount = (int)scriptbplist.size();
    for(int i = 0; i < bpcount; i++)
        if(scriptbplist.at(i).line == line)
            return true;
    return false;
}

static bool scriptInternalBpToggle(int line) //internal breakpoint
{
    SHARED_ACQUIRE(LockScriptLineMap);
    EXCLUSIVE_ACQUIRE(LockScriptBreakpoints);

    if(!line || line > (int)linemap.size()) //invalid line
        return false;
    line = scriptInternalStep(line - 1); //no breakpoints on non-executable locations

    if(scriptInternalBpGet(line)) //remove breakpoint
    {
        int bpcount = (int)scriptbplist.size();
        for(int i = 0; i < bpcount; i++)
            if(scriptbplist.at(i).line == line)
            {
                scriptbplist.erase(scriptbplist.begin() + i);
                break;
            }
    }
    else //add breakpoint
    {
        SCRIPTBP newbp = {};
        newbp.silent = true;
        newbp.line = line;
        scriptbplist.push_back(newbp);
    }
    return true;
}

static bool scriptInternalBranch(SCRIPTBRANCHTYPE type) //determine if we should jump
{
    duint ezflag = 0;
    duint bsflag = 0;
    varget("$_EZ_FLAG", &ezflag, nullptr, nullptr);
    varget("$_BS_FLAG", &bsflag, nullptr, nullptr);
    bool bJump = false;
    switch(type)
    {
    case scriptcall:
    case scriptjmp:
        bJump = true;
        break;
    case scriptjnejnz: //$_EZ_FLAG=0
        if(!ezflag)
            bJump = true;
        break;
    case scriptjejz: //$_EZ_FLAG=1
        if(ezflag)
            bJump = true;
        break;
    case scriptjbjl: //$_BS_FLAG=0 and $_EZ_FLAG=0 //below, not equal
        if(!bsflag && !ezflag)
            bJump = true;
        break;
    case scriptjajg: //$_BS_FLAG=1 and $_EZ_FLAG=0 //above, not equal
        if(bsflag && !ezflag)
            bJump = true;
        break;
    case scriptjbejle: //$_BS_FLAG=0 or $_EZ_FLAG=1
        if(!bsflag || ezflag)
            bJump = true;
        break;
    case scriptjaejge: //$_BS_FLAG=1 or $_EZ_FLAG=1
        if(bsflag || ezflag)
            bJump = true;
        break;
    default:
        bJump = false;
        break;
    }
    return bJump;
}

static CMDRESULT scriptInternalCmdExec(const char* cmd, bool silentRet)
{
    if(scriptIsInternalCommand(cmd, "ret")) //script finished
    {
        if(scriptstack.empty()) //nothing on the stack
        {
            if(!silentRet)
            {
                String TranslatedString = GuiTranslateText(QT_TRANSLATE_NOOP("DBG", "Script finished!"));
                GuiScriptMessage(TranslatedString.c_str());
            }
            return STATUS_EXIT;
        }
        scriptIp = scriptstack.back();
        scriptstack.pop_back(); //remove last stack entry
        return STATUS_CONTINUE_BRANCH;
    }
    if(scriptIsInternalCommand(cmd, "error")) //show an error and end the script
    {
        GuiScriptError(0, StringUtils::Trim(cmd + strlen("error"), " \"'").c_str());
        return STATUS_EXIT;
    }
    if(scriptIsInternalCommand(cmd, "invalid")) //invalid command for testing
        return STATUS_ERROR;
    if(bScriptRunning && (scriptIsInternalCommand(cmd, "scriptrun") || scriptIsInternalCommand(cmd, "scriptexec")))  // do not allow recursive script runs
        return STATUS_ERROR;
    if(scriptIsInternalCommand(cmd, "pause")) //pause the script
        return STATUS_PAUSE;
    if(scriptIsInternalCommand(cmd, "nop")) //do nothing
        return STATUS_CONTINUE;
    if(scriptIsInternalCommand(cmd, "log"))
        bScriptLogEnabled = true;
    //super disgusting hack(s) to support branches in the GUI
    {
        auto branchtype = scriptGetBranchType(cmd);
        if(branchtype != scriptnobranch)
        {
            String cmdStr = cmd;
            auto branchlabel = StringUtils::Trim(cmdStr.substr(cmdStr.find(' ')));
            auto labelIp = scriptLabelFind(branchlabel.c_str());
            if(!labelIp)
            {
                char message[256] = "";
                sprintf_s(message, GuiTranslateText(QT_TRANSLATE_NOOP("DBG", "Invalid branch label \"%s\" detected on line %d!")), branchlabel.c_str(), 0);
                GuiScriptError(0, message);
                return STATUS_ERROR;
            }
            if(scriptInternalBranch(branchtype))
            {
                if(branchtype == scriptcall) //calls have a special meaning
                    scriptstack.push_back(scriptIp);
                scriptIp = scriptInternalStep(labelIp); //go to the first command after the label
                GuiScriptSetIp(scriptIp);
            }
            return STATUS_CONTINUE_BRANCH;
        }
    }
    auto res = cmddirectexec(cmd);
    while(bIsDebugging && dbgisrunning() && !bScriptAbort)
    {
        Sleep(1);
    }
    bScriptLogEnabled = false;
    return res ? STATUS_CONTINUE : STATUS_ERROR;
}

static bool scriptInternalCmd(bool silentRet)
{
    SHARED_ACQUIRE(LockScriptLineMap);
    bool bContinue = true;
    if(size_t(scriptIp - 1) >= linemap.size())
        return false;
    const LINEMAPENTRY & cur = linemap.at(scriptIp - 1);
    scriptIpOld = scriptIp;
    scriptIp = scriptInternalStep(scriptIp);
    if(cur.type == linecommand)
    {
        scriptLastError = scriptInternalCmdExec(cur.u.command, silentRet);
        switch(scriptLastError)
        {
        case STATUS_CONTINUE:
        case STATUS_CONTINUE_BRANCH:
            if(scriptIp == scriptIpOld)
            {
                bContinue = false;
                scriptIp = scriptInternalStep(0);
            }
            break;
        case STATUS_ERROR:
            bContinue = false;
            scriptIp = scriptIpOld;
            GuiScriptError(scriptIp, GuiTranslateText(QT_TRANSLATE_NOOP("DBG", "Error executing command!")));
            break;
        case STATUS_EXIT:
            bContinue = false;
            scriptIp = scriptInternalStep(0);
            GuiScriptSetIp(scriptIp);
            break;
        case STATUS_PAUSE:
            bContinue = false; //stop running the script
            GuiScriptSetIp(scriptIp);
            break;
        }
    }
    else if(cur.type == linebranch)
    {
        if(scriptInternalBranch(cur.u.branch.type))
        {
            if(cur.u.branch.type == scriptcall) //calls have a special meaning
                scriptstack.push_back(scriptIp);
            scriptIp = scriptLabelFind(cur.u.branch.branchlabel);
            scriptIp = scriptInternalStep(scriptIp); //go to the first command after the label
        }
    }
    return bContinue;
}

static bool scriptRun(int destline, bool silentRet)
{
    if(bIsDebugging && dbgisrunning())
    {
        if(!silentRet)
            GuiScriptError(0, GuiTranslateText(QT_TRANSLATE_NOOP("DBG", "Debugger must be paused to run a script!")));
        else
            dputs(QT_TRANSLATE_NOOP("DBG", "Debugger must be paused to run a script!"));
        return false;
    }

    // Use atomic compare_exchange to atomically check and set running state
    bool expected = false;
    if(!bScriptRunning.compare_exchange_strong(expected, true))
        return false; // Script was already running

    // disable GUI updates
    auto guiUpdateWasDisabled = GuiIsUpdateDisabled();
    if(!guiUpdateWasDisabled)
        GuiUpdateDisable();
    SHARED_ACQUIRE(LockScriptLineMap);
    if(destline == 0 || destline > (int)linemap.size()) //invalid line
        destline = 0;
    if(destline)
    {
        destline = scriptInternalStep(destline - 1); //no breakpoints on non-executable locations
        if(!scriptInternalBpGet(destline)) //no breakpoint set
            scriptInternalBpToggle(destline);
    }
    bScriptAbort = false;
    if(scriptIp)
        scriptIp--;
    scriptIp = scriptInternalStep(scriptIp);
    bool bContinue = true;
    bool bIgnoreTimeout = settingboolget("Engine", "NoScriptTimeout");
    auto start = GetTickCount();
    while(bContinue && !bScriptAbort) //run loop
    {
        bContinue = scriptInternalCmd(silentRet);
        if(scriptInternalBpGet(scriptIp)) //breakpoint=stop run loop
            bContinue = false;
        if(bContinue && !bIgnoreTimeout)
        {
            if(GetTickCount() - start >= 30000) // time out in 30 seconds (TODO: remove this?)
            {
                if(GuiScriptMsgyn(GuiTranslateText(QT_TRANSLATE_NOOP("DBG", "The script is too busy. Would you like to terminate it now?"))) != 0)
                {
                    dputs(QT_TRANSLATE_NOOP("DBG", "Script is terminated by user."));
                    break;
                }
                else
                    bIgnoreTimeout = true;
            }
        }
    }
    bScriptRunning = false; //not running anymore
    // re-enable GUI updates when appropriate
    if(!guiUpdateWasDisabled)
        GuiUpdateEnable(true);
    GuiScriptSetIp(scriptIp);
    // the script fully executed (which means scriptIp is reset to the first line), without any errors
    return scriptIp == scriptInternalStep(0) && (scriptLastError == STATUS_EXIT || scriptLastError == STATUS_CONTINUE || scriptLastError == STATUS_CONTINUE_BRANCH);
}

static bool scriptLoad(const char* filename)
{
    ExclusiveSectionLocker<LockScriptLineMap> lockLineMap;
    ExclusiveSectionLocker<LockScriptBreakpoints> lockBreakpoints;
    GuiScriptClear();
    GuiScriptEnableHighlighting(true); //enable default script syntax highlighting
    scriptIp = 0;
    scriptIpOld = 0;
    scriptbplist.clear();
    scriptstack.clear();
    bScriptAbort = false;
    if(!scriptCreateLineMap(filename))
        return false; // Script load failed
    int lines = (int)linemap.size();
    auto script = (const char**)BridgeAlloc(lines * sizeof(const char*));
    for(int i = 0; i < lines; i++) //add script lines
        script[i] = linemap.at(i).raw;
    GuiScriptAdd(lines, script);
    scriptIp = scriptInternalStep(0);
    GuiScriptSetIp(scriptIp);
    return true;
}

bool ScriptLoadAwait(const char* filename)
{
    return scriptQueue.await([filename]()
    {
        return scriptLoad(filename);
    });
}

void ScriptUnloadAwait()
{
    scriptQueue.await([]()
    {
        ExclusiveSectionLocker<LockScriptLineMap> lockLineMap;
        ExclusiveSectionLocker<LockScriptBreakpoints> lockBreakpoints;
        GuiScriptClear();
        linemap.clear();
        scriptbplist.clear();
        scriptstack.clear();
        scriptIp = 0;
        scriptIpOld = 0;
        bScriptAbort = false;
    });
}

void ScriptRunAsync(int destline, bool silentRet)
{
    scriptQueue.async([destline, silentRet]()
    {
        scriptRun(destline, silentRet);
    });
}

bool ScriptRunAwait(int destline, bool silentRet)
{
    return scriptQueue.await([destline, silentRet]()
    {
        return scriptRun(destline, silentRet);
    });
}

void ScriptStepAsync()
{
    scriptQueue.async([]()
    {
        if(!bScriptRunning) //only step when not running
        {
            scriptInternalCmd(false);
            GuiScriptSetIp(scriptIp);
        }
    });
}

bool ScriptBpGetLocked(int line)
{
    SHARED_ACQUIRE(LockScriptBreakpoints);
    int bpcount = (int)scriptbplist.size();
    for(int i = 0; i < bpcount; i++)
        if(scriptbplist.at(i).line == line && !scriptbplist.at(i).silent)
            return true;
    return false;
}

bool ScriptBpToggleLocked(int line)
{
    SHARED_ACQUIRE(LockScriptLineMap);
    EXCLUSIVE_ACQUIRE(LockScriptBreakpoints);
    if(!line || line > (int)linemap.size()) //invalid line
        return false;
    auto bpline = scriptInternalStep(line - 1); //no breakpoints on non-executable locations
    if(ScriptBpGetLocked(bpline)) //remove breakpoint
    {
        int bpcount = (int)scriptbplist.size();
        for(int i = 0; i < bpcount; i++)
            if(scriptbplist.at(i).line == bpline && !scriptbplist.at(i).silent)
            {
                scriptbplist.erase(scriptbplist.begin() + i);
                break;
            }
    }
    else //add breakpoint
    {
        SCRIPTBP newbp = {};
        newbp.silent = false;
        newbp.line = bpline;
        scriptbplist.push_back(newbp);
    }
    return true;
}

bool ScriptCmdExecAwait(const char* command)
{
    return scriptQueue.await([command]()
    {
        scriptIpOld = scriptIp;
        scriptLastError = scriptInternalCmdExec(command, false);
        switch(scriptLastError)
        {
        case STATUS_ERROR:
            return false;
        case STATUS_EXIT:
            scriptIp = scriptInternalStep(0);
            GuiScriptSetIp(scriptIp);
            break;
        case STATUS_PAUSE:
        case STATUS_CONTINUE:
            break;
        case STATUS_CONTINUE_BRANCH:
            if(!bScriptRunning)
            {
                // TODO: is this actually correct?
                ScriptRunAsync(scriptIp, true);
            }
            break;
        }
        return true;
    });
}

void ScriptSetIpAsync(int line)
{
    scriptQueue.async([line]()
    {
        if(line)
            scriptIp = scriptInternalStep(line - 1);
        else
            scriptIp = scriptInternalStep(0);
        GuiScriptSetIp(scriptIp);
    });
}

void ScriptAbortAwait()
{
    if(bScriptRunning)
    {
        bScriptAbort = true;
        scriptQueue.await([]()
        {
            // Empty lambda just to wait for queue completion
        });
    }
    else //reset the script
        ScriptSetIpAsync(0);
}

SCRIPTLINETYPE ScriptGetLineTypeLocked(int line)
{
    SHARED_ACQUIRE(LockScriptLineMap);
    if(line > (int)linemap.size())
        return lineempty;
    return linemap.at(line - 1).type;
}

bool ScriptGetBranchInfoLocked(int line, SCRIPTBRANCH* info)
{
    SHARED_ACQUIRE(LockScriptLineMap);
    if(!info || !line || line > (int)linemap.size()) //invalid line
        return false;
    if(linemap.at(line - 1).type != linebranch) //no branch
        return false;
    memcpy(info, &linemap.at(line - 1).u.branch, sizeof(SCRIPTBRANCH));
    return true;
}

void ScriptLogLocked(const char* msg)
{
    if(!bScriptLogEnabled)
        return;
    GuiScriptSetInfoLine(scriptIpOld, msg);
}

bool ScriptExecAwait(const char* filename)
{
    return scriptQueue.await([filename]()
    {
        if(!scriptLoad(filename))
            return false;
        if(!scriptRun(0, true))
            return false;
        ScriptUnloadAwait();
        return true;
    });
}
