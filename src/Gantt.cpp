// Génération du diagramme de Gantt en HTML autonome.
#include "Projet.h"
#include <algorithm>
#include <climits>

namespace pp {

static std::wstring echapper(const std::wstring &s) {
	std::wstring r;
	for (wchar_t c : s) {
		switch (c) {
		case L'&': r += L"&amp;"; break;
		case L'<': r += L"&lt;"; break;
		case L'>': r += L"&gt;"; break;
		case L'"': r += L"&quot;"; break;
		default: r += c;
		}
	}
	return r;
}

static std::wstring px(int n) { return std::to_wstring(n) + L"px"; }

std::wstring genererGantt(const Projet &p, int auj) {
	struct Ligne {
		std::wstring lib, sous, cls, info;
		int d = AUCUNE, f = AUCUNE, pct = 0;
		bool jalon = false;
	};
	std::vector<Ligne> lignes;
	std::vector<std::wstring> sansDate;
	int mn = INT_MAX, mx = INT_MIN;
	auto inclure = [&](int d) {
		if (d == AUCUNE) return;
		mn = std::min(mn, d);
		mx = std::max(mx, d);
	};

	for (auto &t : p.taches) {
		std::wstring nom = (t.id.empty() ? L"" : t.id + L" ") + t.titre;
		int d = t.debut, f = t.fin;
		if (d == AUCUNE && f == AUCUNE) { sansDate.push_back(nom); continue; }
		if (d == AUCUNE) d = f;
		if (f == AUCUNE) f = d;
		if (f < d) std::swap(d, f);
		Ligne l;
		l.lib = nom;
		l.sous = t.resp;
		l.d = d;
		l.f = f;
		l.pct = avancementEffectif(t);
		static const wchar_t *classes[] = {L"todo", L"doing", L"done", L"block"};
		l.cls = classes[(int)t.statut];
		if (t.statut != Statut::Fait && t.fin != AUCUNE && t.fin < auj) l.cls += L" late";
		l.info = libelleStatut(t.statut) + L" · " + dateFr(d) + L" → " + dateFr(f) + L" · " +
		         std::to_wstring(l.pct) + L" %";
		if (!t.resp.empty()) l.info += L" · " + t.resp;
		if (!t.dep.empty()) l.info += L" · après " + t.dep;
		if (!t.note.empty()) l.info += L" · " + t.note;
		inclure(d);
		inclure(f);
		lignes.push_back(l);
	}
	for (auto &j : p.jalons) {
		std::wstring nom = (j.id.empty() ? L"" : j.id + L" ") + j.titre;
		if (j.date == AUCUNE) { sansDate.push_back(L"◆ " + nom); continue; }
		Ligne l;
		l.jalon = true;
		l.lib = L"◆ " + nom;
		l.d = l.f = j.date;
		l.cls = j.atteint ? L"ok" : (j.date < auj ? L"late" : L"");
		l.info = (j.atteint ? L"Atteint · " : L"À venir · ") + dateFr(j.date);
		inclure(j.date);
		lignes.push_back(l);
	}
	inclure(p.debut);
	inclure(p.fin);

	const std::wstring titre = p.nom.empty() ? L"Projet" : p.nom;
	std::wstring h;
	h += L"<!doctype html><html lang=\"fr\"><head><meta charset=\"utf-8\">"
	     L"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Gantt — ";
	h += echapper(titre) + L"</title><style>";

	if (mn == INT_MAX) {
		h += L"body{font:15px 'Segoe UI',sans-serif;padding:40px}</style></head><body><h1>" + echapper(titre) +
		     L"</h1><p>Aucune tâche ni aucun jalon daté. Ajoutez <code>debut:</code> et <code>fin:</code> "
		     L"à vos tâches pour obtenir un planning.</p></body></html>";
		return h;
	}
	if (auj >= mn - 30 && auj <= mx + 30) inclure(auj);
	mn -= 1;
	mn -= jourSemaine(mn); // commence un lundi
	mx += 2;
	const int R = mx - mn + 1;
	const int dw = std::max(6, std::min(32, 1100 / R));
	const int W = R * dw, LW = 300;

	h += L":root{--bg:#fff;--fg:#1f2937;--mut:#6b7280;--ln:#e5e7eb;--we:#f3f4f6;--hd:#f8fafc;"
	     L"--todo:#94a3b8;--doing:#3b82f6;--done:#22c55e;--block:#f59e0b;--late:#ef4444;--ms:#8b5cf6}"
	     L"@media (prefers-color-scheme:dark){:root{--bg:#0f172a;--fg:#e2e8f0;--mut:#94a3b8;--ln:#334155;"
	     L"--we:#1e293b;--hd:#111c33}}"
	     L"body{margin:0;padding:24px;font:14px/1.4 'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--fg)}"
	     L"h1{font-size:22px;margin:0 0 4px}.meta{color:var(--mut);margin:0 0 16px}"
	     L".wrap{overflow-x:auto;border:1px solid var(--ln);border-radius:8px}"
	     L".chart{position:relative;width:";
	h += px(LW + W);
	h += L"}.row{display:flex;height:30px;border-bottom:1px solid var(--ln)}"
	     L".row:last-child{border-bottom:0}"
	     L".lbl{position:sticky;left:0;z-index:3;box-sizing:border-box;width:";
	h += px(LW) + L";min-width:" + px(LW);
	h += L";padding:0 10px;background:var(--bg);border-right:1px solid var(--ln);display:flex;align-items:center;"
	     L"gap:8px;white-space:nowrap;overflow:hidden}"
	     L".lbl b{font-weight:500;overflow:hidden;text-overflow:ellipsis}.lbl small{color:var(--mut);margin-left:auto}"
	     L".trk{position:relative;flex:none;width:";
	h += px(W);
	h += L"}.head .row{height:24px;background:var(--hd)}.head .lbl{background:var(--hd);color:var(--mut);font-size:12px}"
	     L".cell{position:absolute;top:0;bottom:0;border-left:1px solid var(--ln);font-size:11px;padding:4px 0 0 3px;"
	     L"color:var(--mut);white-space:nowrap;overflow:hidden;box-sizing:border-box}"
	     L".bar{position:absolute;top:7px;height:16px;border-radius:4px;overflow:hidden;z-index:2}"
	     L".bar .pg{position:absolute;left:0;top:0;bottom:0;background:rgba(0,0,0,.28)}"
	     L".todo{background:var(--todo)}.doing{background:var(--doing)}.done{background:var(--done)}.block{background:var(--block)}"
	     L".bar.late{box-shadow:0 0 0 2px var(--late)}"
	     L".ms{position:absolute;top:8px;width:13px;height:13px;margin-left:-6px;transform:rotate(45deg);background:var(--ms);z-index:2}"
	     L".ms.ok{background:var(--done)}.ms.late{background:var(--late)}"
	     L".body{position:relative}.ov{position:absolute;top:0;bottom:0;z-index:1;pointer-events:none;left:";
	h += px(LW) + L";width:" + px(W);
	h += L"}.we{position:absolute;top:0;bottom:0;background:var(--we)}"
	     L".today{position:absolute;top:0;bottom:0;width:2px;background:var(--late)}"
	     L".msl{position:absolute;top:0;bottom:0;border-left:1px dashed var(--ms);opacity:.6}"
	     L".legend{display:flex;flex-wrap:wrap;gap:16px;margin:14px 0;color:var(--mut);font-size:13px}"
	     L".legend i{display:inline-block;width:12px;height:12px;border-radius:3px;margin-right:6px;vertical-align:-1px}"
	     L"ul{color:var(--mut)}footer{margin-top:20px;color:var(--mut);font-size:12px}"
	     L"</style></head><body>";

	h += L"<h1>" + echapper(titre) + L"</h1><p class=\"meta\">";
	if (!p.chef.empty()) h += L"Chef de projet : " + echapper(p.chef) + L" · ";
	h += L"Du " + dateFr(p.debut != AUCUNE ? p.debut : mn + 1) + L" au " + dateFr(p.fin != AUCUNE ? p.fin : mx - 2);
	Stats s = calculer(p, auj);
	h += L" · Avancement " + std::to_wstring(s.pourcent) + L" % · " + std::to_wstring(s.enRetard.size()) +
	     L" tâche(s) en retard</p>";

	h += L"<div class=\"wrap\"><div class=\"chart\"><div class=\"head\">";
	// Ligne des mois
	static const wchar_t *mois[] = {L"janv.", L"févr.", L"mars", L"avr.", L"mai", L"juin",
	                                L"juil.", L"août", L"sept.", L"oct.", L"nov.", L"déc."};
	h += L"<div class=\"row\"><div class=\"lbl\">Tâche</div><div class=\"trk\">";
	for (int d = mn; d <= mx;) {
		int a, m, j;
		jourVersDate(d, a, m, j);
		int finMois = std::min(mx, dateVersJour(m == 12 ? a + 1 : a, m == 12 ? 1 : m + 1, 1) - 1);
		h += L"<div class=\"cell\" style=\"left:" + px((d - mn) * dw) + L";width:" + px((finMois - d + 1) * dw) + L"\">" +
		     mois[m - 1] + L" " + std::to_wstring(a) + L"</div>";
		d = finMois + 1;
	}
	h += L"</div></div>";
	// Ligne des jours (ou des semaines si l'échelle est serrée)
	h += L"<div class=\"row\"><div class=\"lbl\"></div><div class=\"trk\">";
	for (int d = mn; d <= mx; ++d) {
		int a, m, j;
		jourVersDate(d, a, m, j);
		if (dw >= 18 || jourSemaine(d) == 0) {
			int larg = dw >= 18 ? dw : 7 * dw;
			h += L"<div class=\"cell\" style=\"left:" + px((d - mn) * dw) + L";width:" + px(larg) + L"\">" +
			     std::to_wstring(j) + L"</div>";
		}
	}
	h += L"</div></div></div><div class=\"body\"><div class=\"ov\">";
	for (int d = mn; d <= mx; ++d)
		if (jourSemaine(d) == 5)
			h += L"<div class=\"we\" style=\"left:" + px((d - mn) * dw) + L";width:" + px(2 * dw) + L"\"></div>";
	for (auto &l : lignes)
		if (l.jalon) h += L"<div class=\"msl\" style=\"left:" + px((l.d - mn) * dw + dw / 2) + L"\"></div>";
	if (auj >= mn && auj <= mx)
		h += L"<div class=\"today\" title=\"Aujourd'hui\" style=\"left:" + px((auj - mn) * dw + dw / 2) + L"\"></div>";
	h += L"</div>";

	for (auto &l : lignes) {
		h += L"<div class=\"row\" title=\"" + echapper(l.info) + L"\"><div class=\"lbl\"><b>" + echapper(l.lib) +
		     L"</b><small>" + echapper(l.sous) + L"</small></div><div class=\"trk\">";
		if (l.jalon) {
			h += L"<div class=\"ms " + l.cls + L"\" style=\"left:" + px((l.d - mn) * dw + dw / 2) + L"\"></div>";
		} else {
			int larg = std::max(4, (l.f - l.d + 1) * dw - 2);
			h += L"<div class=\"bar " + l.cls + L"\" style=\"left:" + px((l.d - mn) * dw + 1) + L";width:" + px(larg) +
			     L"\"><div class=\"pg\" style=\"width:" + std::to_wstring(l.pct) + L"%\"></div></div>";
		}
		h += L"</div></div>";
	}
	h += L"</div></div></div>";

	h += L"<div class=\"legend\"><span><i class=\"todo\"></i>À faire</span><span><i class=\"doing\"></i>En cours</span>"
	     L"<span><i class=\"done\"></i>Fait</span><span><i class=\"block\"></i>Bloqué</span>"
	     L"<span><i style=\"box-shadow:0 0 0 2px var(--late)\"></i>En retard</span>"
	     L"<span><i style=\"background:var(--ms);transform:rotate(45deg)\"></i>Jalon</span>"
	     L"<span><i style=\"width:2px;background:var(--late)\"></i>Aujourd'hui</span>"
	     L"<span>Zone foncée d'une barre = avancement</span></div>";
	if (!sansDate.empty()) {
		h += L"<p>Éléments sans date :</p><ul>";
		for (auto &n : sansDate) h += L"<li>" + echapper(n) + L"</li>";
		h += L"</ul>";
	}
	h += L"<footer>Généré par Projet Pilot le " + dateFr(auj) + L" à " + heureCourante() + L"</footer></body></html>";
	return h;
}

} // namespace pp
