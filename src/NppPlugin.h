// Déclarations minimales de l'interface plugin Notepad++ et de Scintilla.
// Valeurs reprises de PluginInterface.h, Notepad_plus_msgs.h, Docking.h,
// menuCmdID.h et Scintilla.h (Notepad++ 8.x).
#pragma once
#include <windows.h>
#include <cstdint>

// ---- Interface plugin -------------------------------------------------------
struct NppData {
	HWND _nppHandle;
	HWND _scintillaMainHandle;
	HWND _scintillaSecondHandle;
};

const int menuItemSize = 64;

struct ShortcutKey {
	bool _isCtrl;
	bool _isAlt;
	bool _isShift;
	UCHAR _key;
};

typedef void(__cdecl *PFUNCPLUGINCMD)();

struct FuncItem {
	wchar_t _itemName[menuItemSize];
	PFUNCPLUGINCMD _pFunc;
	int _cmdID;
	bool _init2Check;
	ShortcutKey *_pShKey;
};

// En-tête commun aux notifications Scintilla et Notepad++ (seuls les champs lus ici).
struct SCNotification {
	NMHDR nmhdr;
	intptr_t position;
	int ch;
	int modifiers;
	int modificationType;
};

// ---- Messages Notepad++ -----------------------------------------------------
#define NPPMSG                   (WM_USER + 1000)
#define NPPM_GETCURRENTSCINTILLA (NPPMSG + 4)
#define NPPM_DMMSHOW             (NPPMSG + 30)
#define NPPM_DMMHIDE             (NPPMSG + 31)
#define NPPM_DMMREGASDCKDLG      (NPPMSG + 33)
#define NPPM_SETMENUITEMCHECK    (NPPMSG + 40)
#define NPPM_MENUCOMMAND         (NPPMSG + 48)

#define RUNCOMMAND_USER          (WM_USER + 3000)
#define NPPM_GETNAMEPART         (RUNCOMMAND_USER + 4)

#define IDM_FILE_NEW             41001

#define NPPN_FIRST               1000
#define NPPN_READY               (NPPN_FIRST + 1)
#define NPPN_FILESAVED           (NPPN_FIRST + 8)
#define NPPN_SHUTDOWN            (NPPN_FIRST + 9)
#define NPPN_BUFFERACTIVATED     (NPPN_FIRST + 10)

// ---- Panneaux ancrables -----------------------------------------------------
#define CONT_RIGHT               1
#define DWS_DF_CONT_RIGHT        (CONT_RIGHT << 28)
#define DMN_FIRST                1050
#define DMN_CLOSE                (DMN_FIRST + 1)

struct tTbData {
	HWND hClient;
	const wchar_t *pszName;
	int dlgID;
	UINT uMask;
	HICON hIconTab;
	const wchar_t *pszAddInfo;
	RECT rcFloat;
	int iPrevCont;
	const wchar_t *pszModuleName;
};

// ---- Scintilla --------------------------------------------------------------
#define SCI_INSERTTEXT           2003
#define SCI_GETLENGTH            2006
#define SCI_GETCURRENTPOS        2008
#define SCI_GOTOLINE             2024
#define SCI_GETEOLMODE           2030
#define SCI_BEGINUNDOACTION      2078
#define SCI_ENDUNDOACTION        2079
#define SCI_GETLINEENDPOSITION   2136
#define SCI_GETCODEPAGE          2137
#define SCI_GETLINE              2153
#define SCI_SETSEL               2160
#define SCI_LINEFROMPOSITION     2166
#define SCI_POSITIONFROMLINE     2167
#define SCI_SCROLLCARET          2169
#define SCI_GETTEXT              2182
#define SCI_SETTARGETSTART       2190
#define SCI_SETTARGETEND         2192
#define SCI_REPLACETARGET        2194
#define SCI_ENSUREVISIBLE        2232
#define SCI_LINELENGTH           2350
#define SCI_GRABFOCUS            2400

#define SC_CP_UTF8               65001
#define SC_EOL_CRLF              0
#define SC_EOL_CR                1
#define SC_MOD_INSERTTEXT        0x1
#define SC_MOD_DELETETEXT        0x2
#define SCN_MODIFIED             2008
