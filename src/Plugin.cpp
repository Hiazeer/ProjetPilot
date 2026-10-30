// Projet Pilot — plugin Notepad++ de conduite de projet.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_WINNT 0x0601
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <string>
#include <vector>
#include "NppPlugin.h"
#include "Projet.h"

using namespace pp;

static const wchar_t NOM_PLUGIN[] = L"Projet Pilot";
static const wchar_t NOM_MODULE[] = L"ProjetPilot.dll";

static HINSTANCE g_hInst;
static NppData g_npp;

// ---- Accès à Scintilla ------------------------------------------------------

static HWND sci() {
	int vue = -1;
	SendMessage(g_npp._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&vue);
	return vue == 1 ? g_npp._scintillaSecondHandle : g_npp._scintillaMainHandle;
}

static LRESULT S(UINT msg, WPARAM w = 0, LPARAM l = 0) { return SendMessage(sci(), msg, w, l); }

static UINT pageCode() { return S(SCI_GETCODEPAGE) == SC_CP_UTF8 ? CP_UTF8 : CP_ACP; }

static std::wstring versLarge(const std::string &s) {
	if (s.empty()) return L"";
	UINT cp = pageCode();
	int n = MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring r(n, L'\0');
	MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), &r[0], n);
	return r;
}

static std::string versDoc(const std::wstring &s) {
	if (s.empty()) return "";
	UINT cp = pageCode();
	int n = WideCharToMultiByte(cp, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
	std::string r(n, '\0');
	WideCharToMultiByte(cp, 0, s.data(), (int)s.size(), &r[0], n, nullptr, nullptr);
	return r;
}

static std::wstring texteDocument() {
	intptr_t len = S(SCI_GETLENGTH);
	std::string buf(len + 1, '\0');
	S(SCI_GETTEXT, len + 1, (LPARAM)&buf[0]);
	buf.resize(len);
	return versLarge(buf);
}

static std::wstring texteLigne(int ligne) {
	intptr_t len = S(SCI_LINELENGTH, ligne);
	std::string buf(len + 1, '\0');
	S(SCI_GETLINE, ligne, (LPARAM)&buf[0]);
	buf.resize(len);
	return versLarge(buf);
}

static int ligneCourante() { return (int)S(SCI_LINEFROMPOSITION, S(SCI_GETCURRENTPOS)); }

static std::wstring finDeLigne() {
	switch (S(SCI_GETEOLMODE)) {
	case SC_EOL_CRLF: return L"\r\n";
	case SC_EOL_CR: return L"\r";
	default: return L"\n";
	}
}

static void allerALigne(int ligne) {
	S(SCI_ENSUREVISIBLE, ligne);
	S(SCI_GOTOLINE, ligne);
	S(SCI_GRABFOCUS);
}

// Insère une ligne après `apres` et sélectionne `aSelectionner` (ou place le curseur en fin de ligne).
static void insererLigne(int apres, const std::wstring &contenu, const std::wstring &aSelectionner) {
	std::string eol = versDoc(finDeLigne());
	std::string txt = eol + versDoc(contenu);
	intptr_t pos = S(SCI_GETLINEENDPOSITION, apres);
	S(SCI_INSERTTEXT, pos, (LPARAM)txt.c_str());
	intptr_t debut = pos + (intptr_t)eol.size(), fin = pos + (intptr_t)txt.size();
	size_t i = aSelectionner.empty() ? std::wstring::npos : contenu.find(aSelectionner);
	if (i != std::wstring::npos) {
		debut += (intptr_t)versDoc(contenu.substr(0, i)).size();
		fin = debut + (intptr_t)versDoc(aSelectionner).size();
	} else {
		debut = fin;
	}
	S(SCI_SETSEL, debut, fin);
	S(SCI_SCROLLCARET);
	S(SCI_GRABFOCUS);
}

// ---- Panneau tableau de bord -------------------------------------------------

enum { ID_LISTE = 100, ID_RESUME, ID_BTN_ACTU, ID_BTN_GANTT, ID_BTN_TACHE, ID_BTN_STATUT };
static const UINT_PTR ID_MINUTERIE = 1;

static HWND g_panneau, g_resume, g_liste, g_boutons[4];
static HFONT g_police;
static bool g_panneauVisible = false, g_panneauEnregistre = false;
static tTbData g_tb;
struct RangPanneau { int ligne; COLORREF couleur; };
static std::vector<RangPanneau> g_rangs;

static int g_cmdPanneau = 0; // index du menu, renseigné à l'initialisation

static int ech(int v) {
	static int dpi = 0;
	if (!dpi) { HDC dc = GetDC(nullptr); dpi = GetDeviceCaps(dc, LOGPIXELSY); ReleaseDC(nullptr, dc); }
	return MulDiv(v, dpi, 96);
}

static void ajouterRang(const std::wstring &elem, const std::wstring &statut, const std::wstring &resp,
                        const std::wstring &echeance, const std::wstring &alerte, int ligne, COLORREF couleur) {
	LVITEMW it = {};
	it.mask = LVIF_TEXT;
	it.iItem = (int)g_rangs.size();
	it.pszText = const_cast<wchar_t *>(elem.c_str());
	int idx = (int)SendMessage(g_liste, LVM_INSERTITEMW, 0, (LPARAM)&it);
	const std::wstring *cols[] = {&statut, &resp, &echeance, &alerte};
	for (int c = 0; c < 4; ++c) {
		LVITEMW sub = {};
		sub.iSubItem = c + 1;
		sub.pszText = const_cast<wchar_t *>(cols[c]->c_str());
		SendMessage(g_liste, LVM_SETITEMTEXTW, idx, (LPARAM)&sub);
	}
	g_rangs.push_back({ligne, couleur});
}

static std::wstring ecart(int jours) {
	if (jours == 0) return L"aujourd'hui";
	if (jours > 0) return L"dans " + std::to_wstring(jours) + L" j";
	return L"il y a " + std::to_wstring(-jours) + L" j";
}

static void actualiserPanneau() {
	if (!g_panneau || !g_panneauVisible) return;
	Projet p = analyser(texteDocument());
	int auj = aujourdhui();
	Stats s = calculer(p, auj);

	// Résumé
	std::wstring r;
	r += p.nom.empty() ? L"(aucun « PROJET: » dans ce document)" : p.nom;
	if (!p.chef.empty()) r += L" — chef : " + p.chef;
	r += L"\r\n";
	if (p.debut != AUCUNE || p.fin != AUCUNE) {
		r += L"Période : " + dateFr(p.debut) + L" → " + dateFr(p.fin);
		if (p.fin != AUCUNE) r += L" (fin " + ecart(p.fin - auj) + L")";
		r += L"\r\n";
	}
	int plein = s.pourcent / 10;
	r += L"Avancement : " + std::to_wstring(s.pourcent) + L" %  " + std::wstring(plein, L'█') +
	     std::wstring(10 - plein, L'░') + L"\r\n";
	r += L"Tâches : " + std::to_wstring(s.total) + L"  (à faire " + std::to_wstring(s.aFaire) + L", en cours " +
	     std::to_wstring(s.enCours) + L", faites " + std::to_wstring(s.fait) + L", bloquées " +
	     std::to_wstring(s.bloque) + L")\r\n";
	r += L"En retard : " + std::to_wstring(s.enRetard.size()) + L"   ·   Échéance sous 7 j : " +
	     std::to_wstring(s.bientot.size()) + L"\r\n";
	if (s.finPrevue != AUCUNE && p.fin != AUCUNE && s.finPrevue > p.fin)
		r += L"⚠ Dernière tâche le " + dateFr(s.finPrevue) + L", après la fin du projet\r\n";
	if (s.prochainJalon)
		r += L"Prochain jalon : " + s.prochainJalon->titre + L" le " + dateFr(s.prochainJalon->date) + L" (" +
		     ecart(s.prochainJalon->date - auj) + L")\r\n";
	if (!s.jalonsEnRetard.empty())
		r += L"Jalons dépassés non atteints : " + std::to_wstring(s.jalonsEnRetard.size()) + L"\r\n";
	r += L"Risques ouverts : " + std::to_wstring(s.risquesOuverts) + L"   ·   Décisions : " +
	     std::to_wstring(p.decisions.size()) + L"   ·   Journal : " + std::to_wstring(p.journal.size());
	if (!p.journal.empty()) r += L" (dernier : " + dateFr(p.journal.back().date) + L")";
	r += L"\r\n";
	if (!s.charge.empty()) {
		r += L"Charge restante : ";
		for (size_t i = 0; i < s.charge.size(); ++i)
			r += (i ? L", " : L"") + s.charge[i].first + L" " + std::to_wstring(s.charge[i].second);
		r += L"\r\n";
	}
	SetWindowTextW(g_resume, r.c_str());

	// Liste
	int haut = (int)SendMessage(g_liste, LVM_GETTOPINDEX, 0, 0);
	SendMessage(g_liste, WM_SETREDRAW, FALSE, 0);
	SendMessage(g_liste, LVM_DELETEALLITEMS, 0, 0);
	g_rangs.clear();
	const COLORREF ROUGE = RGB(200, 30, 30), ORANGE = RGB(200, 110, 0), GRIS = RGB(140, 140, 140),
	               BLEU = RGB(20, 90, 200), VIOLET = RGB(110, 60, 190), NORMAL = CLR_DEFAULT;
	for (auto &t : p.taches) {
		std::wstring alerte;
		COLORREF c = NORMAL;
		if (t.statut == Statut::Fait) c = GRIS;
		else if (t.fin != AUCUNE && t.fin < auj) { alerte = L"Retard " + std::to_wstring(auj - t.fin) + L" j"; c = ROUGE; }
		else if (t.statut == Statut::Bloque) { alerte = L"Bloqué"; c = ORANGE; }
		else if (t.fin != AUCUNE && t.fin <= auj + 7) { alerte = L"J-" + std::to_wstring(t.fin - auj); c = BLEU; }
		std::wstring st = libelleStatut(t.statut);
		if (t.statut != Statut::Fait && t.avancement >= 0) st += L" " + std::to_wstring(t.avancement) + L"%";
		ajouterRang((t.id.empty() ? L"" : t.id + L" ") + t.titre, st, t.resp, dateFr(t.fin), alerte, t.ligne, c);
	}
	for (auto &j : p.jalons) {
		std::wstring alerte;
		COLORREF c = VIOLET;
		if (j.atteint) c = GRIS;
		else if (j.date != AUCUNE && j.date < auj) { alerte = L"Dépassé"; c = ROUGE; }
		else if (j.date != AUCUNE && j.date <= auj + 7) alerte = L"J-" + std::to_wstring(j.date - auj);
		ajouterRang(L"◆ " + (j.id.empty() ? L"" : j.id + L" ") + j.titre, j.atteint ? L"Atteint" : L"À venir", L"",
		            dateFr(j.date), alerte, j.ligne, c);
	}
	for (auto &rk : p.risques) {
		ajouterRang(L"⚠ " + (rk.id.empty() ? L"" : rk.id + L" ") + rk.texte, rk.clos ? L"Clos" : L"Ouvert", L"",
		            L"", rk.niveau, rk.ligne, rk.clos ? GRIS : ORANGE);
	}
	for (auto &d : p.decisions)
		ajouterRang(L"✔ " + d.texte, L"Décision", L"", dateFr(d.date), L"", d.ligne, NORMAL);
	SendMessage(g_liste, WM_SETREDRAW, TRUE, 0);
	if (haut > 0 && haut < (int)g_rangs.size()) {
		SendMessage(g_liste, LVM_ENSUREVISIBLE, g_rangs.size() - 1, FALSE);
		SendMessage(g_liste, LVM_ENSUREVISIBLE, haut, FALSE);
	}
}

static void placerControles(int w, int h) {
	int m = ech(4), bh = ech(24), bw = (w - m * 5) / 4;
	for (int i = 0; i < 4; ++i) MoveWindow(g_boutons[i], m + i * (bw + m), m, bw, bh, TRUE);
	int y = m * 2 + bh, rh = ech(150);
	MoveWindow(g_resume, m, y, w - 2 * m, rh, TRUE);
	y += rh + m;
	MoveWindow(g_liste, m, y, w - 2 * m, h - y - m, TRUE);
}

static void cmdGantt();
static void cmdStatut();
static void cmdTache();
static void cocherMenuPanneau();

static LRESULT CALLBACK procPanneau(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
	case WM_SIZE:
		placerControles(LOWORD(lp), HIWORD(lp));
		return 0;
	case WM_TIMER:
		if (wp == ID_MINUTERIE) { KillTimer(hwnd, ID_MINUTERIE); actualiserPanneau(); }
		return 0;
	case WM_COMMAND:
		switch (LOWORD(wp)) {
		case ID_BTN_ACTU: actualiserPanneau(); break;
		case ID_BTN_GANTT: cmdGantt(); break;
		case ID_BTN_TACHE: cmdTache(); break;
		case ID_BTN_STATUT: cmdStatut(); break;
		}
		return 0;
	case WM_NOTIFY: {
		NMHDR *nh = (NMHDR *)lp;
		if (nh->hwndFrom == g_liste) {
			if (nh->code == NM_CUSTOMDRAW) {
				NMLVCUSTOMDRAW *cd = (NMLVCUSTOMDRAW *)lp;
				if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
				if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
					size_t i = cd->nmcd.dwItemSpec;
					if (i < g_rangs.size() && g_rangs[i].couleur != CLR_DEFAULT) cd->clrText = g_rangs[i].couleur;
					return CDRF_NEWFONT;
				}
				return CDRF_DODEFAULT;
			}
			if (nh->code == LVN_ITEMACTIVATE) {
				int i = ((NMITEMACTIVATE *)lp)->iItem;
				if (i >= 0 && i < (int)g_rangs.size()) allerALigne(g_rangs[i].ligne);
			}
			return 0;
		}
		if (nh->hwndFrom == g_npp._nppHandle && LOWORD(nh->code) == DMN_CLOSE) {
			g_panneauVisible = false;
			cocherMenuPanneau();
		}
		break;
	}
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}

static FuncItem *funcs();

static void cocherMenuPanneau() {
	SendMessage(g_npp._nppHandle, NPPM_SETMENUITEMCHECK, funcs()[g_cmdPanneau]._cmdID, g_panneauVisible);
}

static void creerPanneau() {
	INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
	InitCommonControlsEx(&icc);

	WNDCLASSW wc = {};
	wc.lpfnWndProc = procPanneau;
	wc.hInstance = g_hInst;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszClassName = L"ProjetPilotPanneau";
	RegisterClassW(&wc);

	g_panneau = CreateWindowExW(0, wc.lpszClassName, NOM_PLUGIN, WS_CHILD | WS_CLIPCHILDREN, 0, 0, ech(380),
	                            ech(600), g_npp._nppHandle, nullptr, g_hInst, nullptr);

	NONCLIENTMETRICSW ncm = {sizeof(ncm)};
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
	g_police = CreateFontIndirectW(&ncm.lfMessageFont);

	static const wchar_t *libelles[] = {L"Actualiser", L"Gantt", L"+ Tâche", L"Statut"};
	for (int i = 0; i < 4; ++i)
		g_boutons[i] = CreateWindowExW(0, L"BUTTON", libelles[i], WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10,
		                               g_panneau, (HMENU)(INT_PTR)(ID_BTN_ACTU + i), g_hInst, nullptr);
	g_resume = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
	                           WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0,
	                           10, 10, g_panneau, (HMENU)ID_RESUME, g_hInst, nullptr);
	g_liste = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
	                          WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, 0, 10, 10,
	                          g_panneau, (HMENU)ID_LISTE, g_hInst, nullptr);
	SendMessage(g_liste, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
	            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

	HWND tous[] = {g_boutons[0], g_boutons[1], g_boutons[2], g_boutons[3], g_resume, g_liste};
	for (HWND h : tous) SendMessage(h, WM_SETFONT, (WPARAM)g_police, TRUE);

	struct { const wchar_t *nom; int larg; } cols[] = {
	    {L"Élément", 170}, {L"Statut", 75}, {L"Resp.", 70}, {L"Échéance", 78}, {L"Alerte", 80}};
	for (int i = 0; i < 5; ++i) {
		LVCOLUMNW c = {};
		c.mask = LVCF_TEXT | LVCF_WIDTH;
		c.pszText = const_cast<wchar_t *>(cols[i].nom);
		c.cx = ech(cols[i].larg);
		SendMessage(g_liste, LVM_INSERTCOLUMNW, i, (LPARAM)&c);
	}
}

// ---- Commandes --------------------------------------------------------------

static void cmdPanneau() {
	if (!g_panneau) creerPanneau();
	if (!g_panneauEnregistre) {
		g_tb = {};
		g_tb.hClient = g_panneau;
		g_tb.pszName = L"Projet Pilot — tableau de bord";
		g_tb.dlgID = g_cmdPanneau;
		g_tb.uMask = DWS_DF_CONT_RIGHT;
		g_tb.pszModuleName = NOM_MODULE;
		SendMessage(g_npp._nppHandle, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&g_tb);
		g_panneauEnregistre = true;
		g_panneauVisible = false;
	}
	g_panneauVisible = !g_panneauVisible;
	SendMessage(g_npp._nppHandle, g_panneauVisible ? NPPM_DMMSHOW : NPPM_DMMHIDE, 0, (LPARAM)g_panneau);
	cocherMenuPanneau();
	actualiserPanneau();
}

static void cmdGantt() {
	Projet p = analyser(texteDocument());
	std::string html = versUtf8(genererGantt(p, aujourdhui()));

	wchar_t nom[MAX_PATH] = L"projet";
	SendMessage(g_npp._nppHandle, NPPM_GETNAMEPART, MAX_PATH, (LPARAM)nom);
	for (wchar_t *c = nom; *c; ++c)
		if (wcschr(L"\\/:*?\"<>| ", *c)) *c = L'_';
	wchar_t dossier[MAX_PATH];
	GetTempPathW(MAX_PATH, dossier);
	std::wstring chemin = std::wstring(dossier) + L"ProjetPilot";
	CreateDirectoryW(chemin.c_str(), nullptr);
	chemin += L"\\" + std::wstring(nom) + L"_gantt.html";

	HANDLE f = CreateFileW(chemin.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (f == INVALID_HANDLE_VALUE) {
		MessageBoxW(g_npp._nppHandle, (L"Impossible d'écrire " + chemin).c_str(), NOM_PLUGIN, MB_ICONERROR);
		return;
	}
	DWORD ecrit;
	WriteFile(f, html.data(), (DWORD)html.size(), &ecrit, nullptr);
	CloseHandle(f);
	ShellExecuteW(g_npp._nppHandle, L"open", chemin.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static int prochainNumero(const std::vector<std::wstring> &ids) {
	int max = 0;
	for (auto &id : ids) {
		size_t i = id.find_first_of(L"0123456789");
		if (i != std::wstring::npos) max = std::max(max, _wtoi(id.c_str() + i));
	}
	return max + 1;
}

static int ligneSection(const Projet &p, const wchar_t *mot) {
	for (auto &s : p.sections)
		if (s.first.find(mot) != std::wstring::npos) return s.second;
	return -1;
}

// Ligne après laquelle insérer : dernier élément du même type, sinon titre de section, sinon ligne courante.
template <class T> static int ancrage(const std::vector<T> &elems, const Projet &p, const wchar_t *section) {
	if (!elems.empty()) return elems.back().ligne;
	int l = ligneSection(p, section);
	return l >= 0 ? l : ligneCourante();
}

static void cmdTache() {
	Projet p = analyser(texteDocument());
	std::vector<std::wstring> ids;
	for (auto &t : p.taches) ids.push_back(t.id);
	int auj = aujourdhui();
	std::wstring titre = L"Nouvelle tâche";
	std::wstring l = L"[ ] T" + std::to_wstring(prochainNumero(ids)) + L" " + titre + L" | resp:? | debut:" +
	                 dateIso(auj) + L" | fin:" + dateIso(auj + 7) + L" | prio:normale";
	insererLigne(ancrage(p.taches, p, L"tache"), l, titre);
}

static void cmdJalon() {
	Projet p = analyser(texteDocument());
	std::vector<std::wstring> ids;
	for (auto &j : p.jalons) ids.push_back(j.id);
	std::wstring titre = L"Nouveau jalon";
	std::wstring l = L"{ } J" + std::to_wstring(prochainNumero(ids)) + L" " + titre + L" | " + dateIso(aujourdhui() + 14);
	insererLigne(ancrage(p.jalons, p, L"jalon"), l, titre);
}

static void cmdJournal() {
	Projet p = analyser(texteDocument());
	std::wstring l = dateIso(aujourdhui()) + L" " + heureCourante() + L" | ";
	insererLigne(ancrage(p.journal, p, L"journal"), l, L"");
}

static void cmdDecision() {
	Projet p = analyser(texteDocument());
	std::wstring texte = L"Décision prise";
	std::wstring l = L"DECISION " + dateIso(aujourdhui()) + L" | " + texte;
	int a = !p.decisions.empty() || ligneSection(p, L"decision") >= 0 ? ancrage(p.decisions, p, L"decision")
	                                                                    : ancrage(p.journal, p, L"journal");
	insererLigne(a, l, texte);
}

static void cmdRisque() {
	Projet p = analyser(texteDocument());
	std::vector<std::wstring> ids;
	for (auto &r : p.risques) ids.push_back(r.id);
	std::wstring texte = L"Description du risque";
	std::wstring l = L"RISQUE R" + std::to_wstring(prochainNumero(ids)) + L" moyen | " + texte + L" | action:?";
	insererLigne(ancrage(p.risques, p, L"risque"), l, texte);
}

// Remplace le caractère de statut ([ ], {x}…) de la ligne courante.
static bool changerMarque(wchar_t (*suivant)(wchar_t ouvrant, wchar_t actuel)) {
	int ligne = ligneCourante();
	std::wstring t = texteLigne(ligne);
	size_t i = t.find_first_not_of(L" \t");
	if (i == std::wstring::npos || i + 2 >= t.size()) return false;
	wchar_t o = t[i];
	if (!((o == L'[' && t[i + 2] == L']') || (o == L'{' && t[i + 2] == L'}'))) return false;
	wchar_t n = suivant(o, t[i + 1]);
	if (!n) return false;
	intptr_t pos = S(SCI_POSITIONFROMLINE, ligne) + (intptr_t)i + 1; // l'indentation est en ASCII
	char rep[2] = {(char)n, 0};
	S(SCI_SETTARGETSTART, pos);
	S(SCI_SETTARGETEND, pos + 1);
	S(SCI_REPLACETARGET, 1, (LPARAM)rep);
	return true;
}

static void cmdStatut() {
	bool ok = changerMarque([](wchar_t o, wchar_t c) -> wchar_t {
		if (o == L'{') return (c == L'x' || c == L'X') ? L' ' : L'x';
		switch (c) {
		case L' ': return L'~';
		case L'~': case L'-': case L'/': return L'x';
		default: return L' ';
		}
	});
	if (!ok) MessageBeep(MB_ICONWARNING);
	S(SCI_GRABFOCUS);
}

static void cmdBloquer() {
	bool ok = changerMarque([](wchar_t o, wchar_t c) -> wchar_t {
		if (o != L'[') return 0;
		return c == L'!' ? L'~' : L'!';
	});
	if (!ok) MessageBeep(MB_ICONWARNING);
	S(SCI_GRABFOCUS);
}

static void cmdNouveauProjet() {
	SendMessage(g_npp._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
	int a = aujourdhui();
	std::wstring n = finDeLigne();
	std::wstring t =
	    L"PROJET: Mon projet" + n + L"CHEF: ?" + n + L"DEBUT: " + dateIso(a) + n + L"FIN: " + dateIso(a + 60) + n + n +
	    L"// Tâches : [ ] à faire   [~] en cours   [x] fait   [!] bloqué" + n +
	    L"// Champs : resp:  debut:  fin:  prio:  av:(%)  dep:(ID d'une autre tâche)  note:" + n +
	    L"// Jalons : { } à venir   {x} atteint" + n + n +
	    L"== TÂCHES ==" + n +
	    L"[ ] T1 Cadrage | resp:? | debut:" + dateIso(a) + L" | fin:" + dateIso(a + 7) + L" | prio:haute" + n +
	    L"[ ] T2 Réalisation | resp:? | dep:T1 | fin:" + dateIso(a + 40) + n +
	    L"[ ] T3 Recette | resp:? | dep:T2 | fin:" + dateIso(a + 55) + n + n +
	    L"== JALONS ==" + n +
	    L"{ } J1 Lancement | " + dateIso(a) + n +
	    L"{ } J2 Livraison | " + dateIso(a + 60) + n + n +
	    L"== JOURNAL ==" + n +
	    dateIso(a) + L" " + heureCourante() + L" | Création du projet" + n + n +
	    L"== DÉCISIONS ==" + n + n +
	    L"== RISQUES ==" + n +
	    L"RISQUE R1 moyen | Description du risque | action:?" + n;
	std::string doc = versDoc(t);
	S(SCI_INSERTTEXT, 0, (LPARAM)doc.c_str());
	actualiserPanneau();
}

static void cmdAide() {
	MessageBoxW(g_npp._nppHandle,
	            L"Format d'un fichier projet (texte libre, une ligne = un élément) :\n\n"
	            L"PROJET: Nom      CHEF: Nom      DEBUT: 2026-11-02      FIN: 2026-12-31\n\n"
	            L"Tâches\n"
	            L"  [ ] T1 Titre | resp:Alice | debut:2026-11-02 | fin:2026-11-10 | prio:haute\n"
	            L"  [ ] à faire   [~] en cours   [x] fait   [!] bloqué\n"
	            L"  av:40 (avancement %)   dep:T1 (commence après T1)   note:texte\n\n"
	            L"Jalons\n  { } J1 Livraison | 2026-12-01      {x} = atteint\n\n"
	            L"Journal\n  2026-10-29 14:30 | Réunion de lancement\n\n"
	            L"Décisions\n  DECISION 2026-10-29 | On part sur WordPress\n\n"
	            L"Risques\n  RISQUE R1 haute | Retard des contenus | action:relance hebdo\n"
	            L"  (niveau « clos » = risque fermé)\n\n"
	            L"Titres de section : == TÂCHES ==, == JALONS ==, == JOURNAL ==… (facultatifs)\n"
	            L"Commentaires : lignes commençant par //\n"
	            L"Dates : AAAA-MM-JJ ou JJ/MM/AAAA",
	            L"Projet Pilot — aide", MB_ICONINFORMATION);
}

static void cmdAPropos() {
	MessageBoxW(g_npp._nppHandle,
	            L"Projet Pilot 1.0\nConduite de projet dans Notepad++ : tâches, jalons, journal, décisions, "
	            L"risques, tableau de bord et diagramme de Gantt.",
	            NOM_PLUGIN, MB_ICONINFORMATION);
}

// ---- Menu -------------------------------------------------------------------

static ShortcutKey skPanneau = {true, true, true, 'D'};
static ShortcutKey skGantt = {true, true, true, 'G'};
static ShortcutKey skTache = {true, true, true, 'T'};
static ShortcutKey skJalon = {true, true, true, 'J'};
static ShortcutKey skJournal = {true, true, true, 'L'};
static ShortcutKey skStatut = {true, true, true, 'S'};

struct Commande { const wchar_t *nom; PFUNCPLUGINCMD f; ShortcutKey *sk; };
static const Commande COMMANDES[] = {
    {L"Tableau de bord", cmdPanneau, &skPanneau},
    {L"Diagramme de Gantt", cmdGantt, &skGantt},
    {L"", nullptr, nullptr},
    {L"Nouveau projet (modèle)", cmdNouveauProjet, nullptr},
    {L"Insérer une tâche", cmdTache, &skTache},
    {L"Insérer un jalon", cmdJalon, &skJalon},
    {L"Entrée de journal horodatée", cmdJournal, &skJournal},
    {L"Insérer une décision", cmdDecision, nullptr},
    {L"Insérer un risque", cmdRisque, nullptr},
    {L"", nullptr, nullptr},
    {L"Changer le statut (à faire → en cours → fait)", cmdStatut, &skStatut},
    {L"Bloquer / débloquer la tâche", cmdBloquer, nullptr},
    {L"", nullptr, nullptr},
    {L"Aide sur le format", cmdAide, nullptr},
    {L"À propos", cmdAPropos, nullptr},
};
static const int NB_COMMANDES = sizeof(COMMANDES) / sizeof(COMMANDES[0]);
static FuncItem g_funcs[NB_COMMANDES];

static FuncItem *funcs() { return g_funcs; }

// ---- Exports ----------------------------------------------------------------

BOOL APIENTRY DllMain(HINSTANCE h, DWORD raison, LPVOID) {
	if (raison == DLL_PROCESS_ATTACH) g_hInst = h;
	return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData d) {
	g_npp = d;
	for (int i = 0; i < NB_COMMANDES; ++i) {
		lstrcpynW(g_funcs[i]._itemName, COMMANDES[i].nom, menuItemSize);
		g_funcs[i]._pFunc = COMMANDES[i].f;
		g_funcs[i]._init2Check = false;
		g_funcs[i]._pShKey = COMMANDES[i].sk;
	}
	g_cmdPanneau = 0;
}

extern "C" __declspec(dllexport) const wchar_t *getName() { return NOM_PLUGIN; }

extern "C" __declspec(dllexport) FuncItem *getFuncsArray(int *n) {
	*n = NB_COMMANDES;
	return g_funcs;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *n) {
	HWND de = (HWND)n->nmhdr.hwndFrom;
	switch (n->nmhdr.code) {
	case NPPN_BUFFERACTIVATED:
	case NPPN_FILESAVED:
		if (de == g_npp._nppHandle) actualiserPanneau();
		break;
	case SCN_MODIFIED:
		if (g_panneauVisible && de == sci() && (n->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)))
			SetTimer(g_panneau, ID_MINUTERIE, 700, nullptr);
		break;
	case NPPN_SHUTDOWN:
		if (de == g_npp._nppHandle) {
			if (g_panneau) DestroyWindow(g_panneau);
			if (g_police) DeleteObject(g_police);
			g_panneau = nullptr;
			g_police = nullptr;
		}
		break;
	}
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }

extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
