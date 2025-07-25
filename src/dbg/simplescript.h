#pragma once

#include "_global.h"

bool ScriptLoadAwait(const char* filename, bool guiStatus);
void ScriptUnloadAwait();
void ScriptRunAsync(int destline, bool guiStatus);
bool ScriptRunAwait(int destline, bool guiStatus);
void ScriptStepAsync(bool guiStatus);
bool ScriptBpGetLocked(int line);
bool ScriptBpToggleLocked(int line);
bool ScriptCmdExecAwait(const char* command, bool guiStatus);
void ScriptSetIpAsync(int line);
void ScriptAbortAwait();
SCRIPTLINETYPE ScriptGetLineTypeLocked(int line);
bool ScriptGetBranchInfoLocked(int line, SCRIPTBRANCH* info);
void ScriptLogLocked(const char* msg);
bool ScriptExecAwait(const char* filename, bool guiStatus);
