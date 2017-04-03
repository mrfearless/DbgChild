#include "plugin.h"
#include "icons.h"
#include "com_common.h"

static duint processEntry;

enum
{
    MENU_HOOK,
    MENU_PATCH_NTDLL,
    MENU_UNPATCH_NTDLL,
    MENU_NEW_PROCESS_WATCHER,
    MENU_NEW_PROCESS_WATCHER_OLD,
    MENU_GO_TO_HOOK,
    MENU_GO_TO_NTDLL,
    MENU_HELP,
    MENU_INFO
};

PLUG_EXPORT void CBINITDEBUG(CBTYPE cbType, PLUG_CB_INITDEBUG* info)
{
}

PLUG_EXPORT void CBSTOPDEBUG(CBTYPE cbType, PLUG_CB_STOPDEBUG* info)
{
}

PLUG_EXPORT void CBEXCEPTION(CBTYPE cbType, PLUG_CB_EXCEPTION* info)
{
}

PLUG_EXPORT void CBDEBUGEVENT(CBTYPE cbType, PLUG_CB_DEBUGEVENT* info)
{
}

PLUG_EXPORT void CBCREATEPROCESS(CBTYPE cbType, PLUG_CB_CREATEPROCESS* info)
{
    processEntry = Script::Module::EntryFromAddr(duint(info->CreateProcessInfo->lpBaseOfImage));
}

void ExecuteNewProcessLauncher(BOOL old_process, wchar_t* path)
{
    int result = IDCANCEL;
    WCHAR * params = NULL;
    HANDLE hMutex;

    if (old_process)
    {
        result = IDYES;
        params = L"o";
    }
    else
    {
        hMutex = OpenMutexW(MUTEX_ALL_ACCESS, 0, L"NewProcessWatcherDreg");
        if (!hMutex)
        {
            ReleaseMutex(hMutex);
            result = MessageBoxA(NULL, "NewProcessWatcher is not running, do you want launch it?", PLUGIN_NAME, MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
        }
        else
        {
            _plugin_logprintf("[" PLUGIN_NAME "] NewProcessWatcher already Open");
        }
    }

    if (result == IDYES)
    {
        WCHAR watcher_path[MAX_PATH] = { 0 };

        wcscpy_s(watcher_path, path);
        ZeroMemory(&(watcher_path[wcslen(watcher_path) - 4]), 2);
        wcscat_s(watcher_path, L"NewProcessWatcher.exe");

        ShellExecuteW(NULL, L"runas", watcher_path, params, NULL, SW_SHOWNORMAL);
    }
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
    if (info->hEntry != MENU_INFO && 
        info->hEntry != MENU_HELP &&
        info->hEntry != MENU_NEW_PROCESS_WATCHER &&
        info->hEntry != MENU_NEW_PROCESS_WATCHER_OLD
        )
    {
        if (!DbgIsDebugging())
        {
            MessageBoxA(hwndDlg, "DONT DEBUGEE OPEN", PLUGIN_NAME, MB_ICONERROR);
            return;
        }
    }

    wchar_t path[MAX_PATH] = { 0 };
    wchar_t exe[MAX_PATH] = { 0 };
    wchar_t args[MAX_PATH] = { 0 };
    wchar_t actual_pid[ARRAYSIZE(L"4294967295")] = { 0 };
    wchar_t* op_type = L"runas";

    char* dis_cmd = NULL;
    duint breakEntry = 0;

    _itow_s(DbgGetProcessId(), actual_pid, 10);

    GetCurrentPath(path);

    switch(info->hEntry)
    {
        case MENU_HOOK:
            DbgCmdExecDirect("bc ZwCreateUserProcess");

            ExecuteNewProcessLauncher(FALSE, path);

            wcscpy_s(exe, L"CreateProcessPatch.exe");
            wcscpy_s(args, actual_pid);
            dis_cmd = "dis ZwCreateUserProcess";
            break;

        case MENU_PATCH_NTDLL:
            DbgCmdExecDirect("bc LdrInitializeThunk");

            wcscpy_s(exe, L"NTDLLEntryPatch.exe");
            wcscpy_s(args, actual_pid);
            wcscat_s(args, L" p");
            dis_cmd = "dis LdrInitializeThunk";
            break;

        case MENU_UNPATCH_NTDLL:
            DbgCmdExecDirect("bc LdrInitializeThunk");

            wcscpy_s(exe, L"NTDLLEntryPatch.exe");
            wcscpy_s(args, actual_pid);
            wcscat_s(args, L" u");
            dis_cmd = "dis LdrInitializeThunk";

            if(BridgeSettingGetUint("Events", "EntryBreakpoint", &breakEntry) && breakEntry)
            {
                char cmd[32] = "";
                sprintf_s(cmd, "bp %p, ss", processEntry);
                DbgCmdExecDirect(cmd);
            }
            break;

        case MENU_NEW_PROCESS_WATCHER:
            ExecuteNewProcessLauncher(FALSE, path);
            break;

        case MENU_NEW_PROCESS_WATCHER_OLD:
            ExecuteNewProcessLauncher(TRUE, path);
            break;

        case MENU_GO_TO_HOOK:
            dis_cmd = "dis ZwCreateUserProcess";
            break;

        case MENU_GO_TO_NTDLL:
            dis_cmd = "dis LdrInitializeThunk";
            break;

        case MENU_HELP:
            op_type = L"open";
            wcscpy_s(exe, path);
            ZeroMemory(&(exe[wcslen(exe) - 4]), 2);
            wcscat_s(exe, L"readme_dbgchild.txt");
            break;

        case MENU_INFO:
            MessageBoxA(hwndDlg, PLUGIN_NAME " by David Reguera Garcia aka Dreg\n\ndreg@fr33project.org\n\nhttps://github.com/David-Reguera-Garcia-Dreg/DbgChild\nhttp://www.fr33project.org", PLUGIN_NAME, MB_ICONINFORMATION);
            break;
    }

    if (info->hEntry == MENU_HOOK ||
        info->hEntry == MENU_PATCH_NTDLL ||
        info->hEntry == MENU_UNPATCH_NTDLL ||
        info->hEntry == MENU_HELP
        )
    {
        ShellExecuteW(NULL, op_type, exe, args, path, SW_SHOWNORMAL);
    }
    
    if (dis_cmd != NULL)
    {
        DbgCmdExec(dis_cmd);
    }
}

//Initialize your plugin data here.
bool pluginInit(PLUG_INITSTRUCT* initStruct)
{
    return true; //Return false to cancel loading the plugin.
}

//Deinitialize your plugin data here (clearing menus optional).
bool pluginStop()
{
    _plugin_unregistercommand(pluginHandle, PLUGIN_NAME);
    _plugin_menuclear(hMenu);
    return true;
}

//Do GUI/Menu related things here.
void pluginSetup()
{
	
	// Icons
	ICONDATA dbgchild_menu_icon;
	ICONDATA hookprocess_menu_icon;
	ICONDATA patchntdll_menu_icon;
	ICONDATA unpatchntdll_menu_icon;
	ICONDATA newprocesswatcher_menu_icon;
	ICONDATA gotohook_menu_icon;
	ICONDATA gotontdll_menu_icon;
	ICONDATA helpicon_menu_icon;
	
	dbgchild_menu_icon.data = DbgChildIcon;
	dbgchild_menu_icon.size = sizeof(DbgChildIcon);
	hookprocess_menu_icon.data = HookProcessIcon;
	hookprocess_menu_icon.size = sizeof(HookProcessIcon);
	patchntdll_menu_icon.data = patchNTDLLIcon;
	patchntdll_menu_icon.size = sizeof(patchNTDLLIcon);
	unpatchntdll_menu_icon.data = unpatchNTDLLIcon;
	unpatchntdll_menu_icon.size = sizeof(unpatchNTDLLIcon);
	newprocesswatcher_menu_icon.data = NewProcessWatcherIcon;
	newprocesswatcher_menu_icon.size = sizeof(NewProcessWatcherIcon);
	gotohook_menu_icon.data = GotoHookIcon;
	gotohook_menu_icon.size = sizeof(GotoHookIcon);	
	gotontdll_menu_icon.data = GotoNTDLLIcon;
	gotontdll_menu_icon.size = sizeof(GotoNTDLLIcon);	
	helpicon_menu_icon.data = HelpIcon;
	helpicon_menu_icon.size = sizeof(HelpIcon);


	// Add menu item entries
    _plugin_menuaddentry(hMenu, MENU_HOOK, "&Hook process creation");
    _plugin_menuaddentry(hMenu, MENU_UNPATCH_NTDLL, "&Unpatch NTDLL entry");
    _plugin_menuaddseparator(hMenu);

    _plugin_menuaddentry(hMenu, MENU_PATCH_NTDLL, "&Patch NTDLL entry");
    _plugin_menuaddseparator(hMenu);

    _plugin_menuaddentry(hMenu, MENU_NEW_PROCESS_WATCHER, "&Launch NewProcessWatcher");
    _plugin_menuaddentry(hMenu, MENU_NEW_PROCESS_WATCHER_OLD, "&Launch NewProcessWatcher with old processes");
    _plugin_menuaddseparator(hMenu);

    _plugin_menuaddentry(hMenu, MENU_GO_TO_HOOK, "&Go to Hook process creation");
    _plugin_menuaddentry(hMenu, MENU_GO_TO_NTDLL, "&Go to NTDLL patch");
    _plugin_menuaddseparator(hMenu);

    _plugin_menuaddentry(hMenu, MENU_HELP, "&Help");
    _plugin_menuaddseparator(hMenu);
    
    _plugin_menuaddentry(hMenu, MENU_INFO, "&Plugin info by Dreg");

	// Add icons to menu item entries
	_plugin_menuseticon(hMenu, &dbgchild_menu_icon);
	_plugin_menuentryseticon(pluginHandle, MENU_HOOK, &hookprocess_menu_icon);
	_plugin_menuentryseticon(pluginHandle, MENU_UNPATCH_NTDLL, &unpatchntdll_menu_icon);
	_plugin_menuentryseticon(pluginHandle, MENU_PATCH_NTDLL, &patchntdll_menu_icon);
	_plugin_menuentryseticon(pluginHandle, MENU_NEW_PROCESS_WATCHER, &newprocesswatcher_menu_icon);
	_plugin_menuentryseticon(pluginHandle, MENU_NEW_PROCESS_WATCHER_OLD, &newprocesswatcher_menu_icon);
	_plugin_menuentryseticon(pluginHandle, MENU_GO_TO_HOOK, &gotohook_menu_icon);
	_plugin_menuentryseticon(pluginHandle, MENU_GO_TO_NTDLL, &gotontdll_menu_icon);
	_plugin_menuentryseticon(pluginHandle, MENU_HELP, &helpicon_menu_icon);
	_plugin_menuentryseticon(pluginHandle, MENU_INFO, &dbgchild_menu_icon);
	
}
