#pragma once

#include "_global.h"

bool ScriptLoadAwait(const char* filename);
void ScriptUnloadAwait();
void ScriptRunAsync(int destline, bool silentRet = false);
bool ScriptRunAwait(int destline, bool silentRet = false);
void ScriptStepAsync();
bool ScriptBpGetLocked(int line);
bool ScriptBpToggleLocked(int line);
bool ScriptCmdExecAwait(const char* command);
void ScriptSetIpAsync(int line);
void ScriptAbortAwait();
SCRIPTLINETYPE ScriptGetLineTypeLocked(int line);
bool ScriptGetBranchInfoLocked(int line, SCRIPTBRANCH* info);
void ScriptLogLocked(const char* msg);
bool ScriptExecAwait(const char* filename);
