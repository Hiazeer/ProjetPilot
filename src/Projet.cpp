#include "Projet.h"
#include <algorithm>
#include <climits>
#include <cstdint>
#include <ctime>
#include <cwctype>
#include <map>
#include <cwchar>

namespace pp {

// ---- Chaînes ----------------------------------------------------------------

std::wstring trim(const std::wstring &s) {
	size_t a = s.find_first_not_of(L" \t\r\n");
	if (a == std::wstring::npos) return L"";
	size_t b = s.find_last_not_of(L" \t\r\n");
	return s.substr(a, b - a + 1);
}

// Minuscules sans accents, pour comparer les mots-clés.
std::wstring normaliser(const std::wstring &s) {
	static const wchar_t *de = L"ÀÂÄàâäÇçÉÈÊËéèêëÎÏîïÔÖôöÙÛÜùûü";
	static const wchar_t *vers = L"aaaaaacceeeeeeeeiiiioooouuuuuu";
	std::wstring r;
	r.reserve(s.size());
	for (wchar_t c : s) {
		if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';
		else if (const wchar_t *p = wcschr(de, c)) c = vers[p - de];
		r += c;
	}
	return r;
}

static bool commencePar(const std::wstring &s, const wchar_t *prefixe) {
	return s.compare(0, wcslen(prefixe), prefixe) == 0;
}

static std::vector<std::wstring> decouper(const std::wstring &s, wchar_t sep) {
	std::vector<std::wstring> r;
	size_t pos = 0;
	for (;;) {
		size_t f = s.find(sep, pos);
		r.push_back(trim(s.substr(pos, f == std::wstring::npos ? std::wstring::npos : f - pos)));
		if (f == std::wstring::npos) break;
		pos = f + 1;
	}
	return r;
}

std::string versUtf8(const std::wstring &s) {
	std::string r;
	r.reserve(s.size() + s.size() / 4);
	for (size_t i = 0; i < s.size(); ++i) {
		uint32_t c = s[i];
		if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size()) {
			uint32_t c2 = s[i + 1];
			if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
				c = 0x10000 + ((c - 0xD800) << 10) + (c2 - 0xDC00);
				++i;
			}
		}
		if (c < 0x80) r += (char)c;
		else if (c < 0x800) { r += (char)(0xC0 | (c >> 6)); r += (char)(0x80 | (c & 0x3F)); }
		else if (c < 0x10000) { r += (char)(0xE0 | (c >> 12)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
		else { r += (char)(0xF0 | (c >> 18)); r += (char)(0x80 | ((c >> 12) & 0x3F)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
	}
	return r;
}

// ---- Dates ------------------------------------------------------------------

int dateVersJour(int y, int m, int d) {
	y -= m <= 2;
	const int era = (y >= 0 ? y : y - 399) / 400;
	const int yoe = y - era * 400;
	const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}

void jourVersDate(int z, int &y, int &m, int &d) {
	z += 719468;
	const int era = (z >= 0 ? z : z - 146096) / 146097;
	const int doe = z - era * 146097;
	const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const int mp = (5 * doy + 2) / 153;
	d = doy - (153 * mp + 2) / 5 + 1;
	m = mp < 10 ? mp + 3 : mp - 9;
	y = yoe + era * 400 + (m <= 2);
}

int jourSemaine(int n) { return ((n + 3) % 7 + 7) % 7; }

// Accepte AAAA-MM-JJ et JJ/MM/AAAA (séparateurs - / .).
bool lireDate(const std::wstring &s0, int &out) {
	std::wstring s = trim(s0);
	int v[3] = {0, 0, 0}, len[3] = {0, 0, 0}, k = 0;
	for (wchar_t c : s) {
		if (c >= L'0' && c <= L'9') {
			if (len[k] >= 4) return false;
			v[k] = v[k] * 10 + (c - L'0');
			++len[k];
		} else if ((c == L'-' || c == L'/' || c == L'.') && len[k] > 0 && k < 2) {
			++k;
		} else return false;
	}
	if (k != 2 || len[2] == 0) return false;
	int a, m, j;
	if (len[0] == 4) { a = v[0]; m = v[1]; j = v[2]; }
	else if (len[2] == 4) { j = v[0]; m = v[1]; a = v[2]; }
	else return false;
	if (m < 1 || m > 12 || j < 1 || j > 31) return false;
	int n = dateVersJour(a, m, j);
	int a2, m2, j2;
	jourVersDate(n, a2, m2, j2);
	if (m2 != m) return false; // 31/02 etc.
	out = n;
	return true;
}

std::wstring dateIso(int n) {
	int a, m, j;
	jourVersDate(n, a, m, j);
	wchar_t b[16];
	swprintf(b, 16, L"%04d-%02d-%02d", a, m, j);
	return b;
}

std::wstring dateFr(int n) {
	if (n == AUCUNE) return L"";
	int a, m, j;
	jourVersDate(n, a, m, j);
	wchar_t b[16];
	swprintf(b, 16, L"%02d/%02d/%04d", j, m, a);
	return b;
}

int aujourdhui() {
	std::time_t t = std::time(nullptr);
	std::tm *lt = std::localtime(&t);
	return dateVersJour(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
}

std::wstring heureCourante() {
	std::time_t t = std::time(nullptr);
	std::tm *lt = std::localtime(&t);
	wchar_t b[8];
	swprintf(b, 8, L"%02d:%02d", lt->tm_hour, lt->tm_min);
	return b;
}

// ---- Analyse ----------------------------------------------------------------

static bool estIdentifiant(const std::wstring &t) {
	size_t i = 0;
	while (i < t.size() && iswalpha(t[i]) && i < 2) ++i;
	if (i == 0 || i == t.size()) return false;
	size_t chiffres = 0;
	for (; i < t.size(); ++i) {
		if (t[i] >= L'0' && t[i] <= L'9') ++chiffres;
		else if (t[i] != L'.' && t[i] != L'-') return false;
	}
	return chiffres > 0;
}

static void lireTitre(const std::wstring &seg, std::wstring &id, std::wstring &titre) {
	size_t esp = seg.find_first_of(L" \t");
	std::wstring premier = seg.substr(0, esp);
	if (estIdentifiant(premier)) {
		id = premier;
		titre = esp == std::wstring::npos ? L"" : trim(seg.substr(esp));
	} else {
		titre = seg;
	}
}

// Sépare "clé: valeur". Renvoie false si le segment n'a pas de clé.
static bool cleValeur(const std::wstring &seg, std::wstring &cle, std::wstring &val) {
	size_t c = seg.find(L':');
	if (c == std::wstring::npos || c == 0) return false;
	cle = normaliser(trim(seg.substr(0, c)));
	if (cle.find(L' ') != std::wstring::npos) return false;
	val = trim(seg.substr(c + 1));
	return true;
}

static void ajouterNote(std::wstring &note, const std::wstring &s) {
	if (s.empty()) return;
	if (!note.empty()) note += L" ; ";
	note += s;
}

static void lireTache(Projet &p, const std::wstring &t, int ligne) {
	Tache tk;
	tk.ligne = ligne;
	wchar_t c = t[1];
	if (c == L'~' || c == L'-' || c == L'/') tk.statut = Statut::EnCours;
	else if (c == L'x' || c == L'X') tk.statut = Statut::Fait;
	else if (c == L'!') tk.statut = Statut::Bloque;
	auto segs = decouper(t.substr(3), L'|');
	lireTitre(segs[0], tk.id, tk.titre);
	std::vector<int> datesNues;
	for (size_t i = 1; i < segs.size(); ++i) {
		const std::wstring &seg = segs[i];
		std::wstring cle, val;
		int d;
		if (cleValeur(seg, cle, val)) {
			if (cle == L"resp" || cle == L"responsable" || cle == L"qui") tk.resp = val;
			else if (cle == L"debut" || cle == L"du" || cle == L"start") { if (lireDate(val, d)) tk.debut = d; }
			else if (cle == L"fin" || cle == L"au" || cle == L"echeance" || cle == L"due") { if (lireDate(val, d)) tk.fin = d; }
			else if (cle == L"prio" || cle == L"priorite") tk.prio = val;
			else if (cle == L"av" || cle == L"avancement" || cle == L"%") tk.avancement = std::max(0, std::min(100, _wtoi(val.c_str())));
			else if (cle == L"dep" || cle == L"apres" || cle == L"depend") tk.dep = val;
			else if (cle == L"note" || cle == L"com" || cle == L"commentaire") ajouterNote(tk.note, val);
			else ajouterNote(tk.note, seg);
		} else if (lireDate(seg, d)) {
			datesNues.push_back(d);
		} else if (!seg.empty() && seg.back() == L'%') {
			tk.avancement = std::max(0, std::min(100, _wtoi(seg.c_str())));
		} else {
			ajouterNote(tk.note, seg);
		}
	}
	if (datesNues.size() == 1 && tk.fin == AUCUNE) tk.fin = datesNues[0];
	else if (datesNues.size() >= 2) {
		if (tk.debut == AUCUNE) tk.debut = datesNues[0];
		if (tk.fin == AUCUNE) tk.fin = datesNues[1];
	}
	p.taches.push_back(tk);
}

static void lireJalon(Projet &p, const std::wstring &t, int ligne) {
	Jalon j;
	j.ligne = ligne;
	j.atteint = t[1] == L'x' || t[1] == L'X';
	auto segs = decouper(t.substr(3), L'|');
	lireTitre(segs[0], j.id, j.titre);
	for (size_t i = 1; i < segs.size(); ++i) {
		std::wstring cle, val;
		int d;
		if (cleValeur(segs[i], cle, val)) { if (lireDate(val, d)) j.date = d; }
		else if (lireDate(segs[i], d)) j.date = d;
	}
	p.jalons.push_back(j);
}

static void lireDecision(Projet &p, const std::wstring &t, int ligne) {
	Decision dc;
	dc.ligne = ligne;
	std::wstring reste = trim(t.substr(8));
	if (!reste.empty() && reste[0] == L':') reste = trim(reste.substr(1));
	auto segs = decouper(reste, L'|');
	size_t i = 0;
	if (lireDate(segs[0], dc.date)) i = 1;
	for (; i < segs.size(); ++i) {
		if (!dc.texte.empty()) dc.texte += L" | ";
		dc.texte += segs[i];
	}
	p.decisions.push_back(dc);
}

static void lireRisque(Projet &p, const std::wstring &t, int ligne) {
	Risque r;
	r.ligne = ligne;
	std::wstring reste = trim(t.substr(6));
	if (!reste.empty() && reste[0] == L':') reste = trim(reste.substr(1));
	auto segs = decouper(reste, L'|');
	size_t i = 0;
	if (segs.size() > 1) {
		std::wstring tete = segs[0];
		size_t esp = tete.find_first_of(L" \t");
		std::wstring premier = tete.substr(0, esp);
		if (estIdentifiant(premier)) {
			r.id = premier;
			tete = esp == std::wstring::npos ? L"" : trim(tete.substr(esp));
		}
		r.niveau = tete;
		i = 1;
	}
	for (; i < segs.size(); ++i) {
		std::wstring cle, val;
		if (cleValeur(segs[i], cle, val) && (cle == L"action" || cle == L"parade" || cle == L"plan")) r.action = val;
		else if (r.texte.empty()) r.texte = segs[i];
		else r.texte += L" | " + segs[i];
	}
	std::wstring n = normaliser(r.niveau);
	r.clos = n.find(L"clos") != std::wstring::npos || n.find(L"ferme") != std::wstring::npos ||
	         n.find(L"traite") != std::wstring::npos;
	p.risques.push_back(r);
}

static bool lireJournal(Projet &p, const std::wstring &t, int ligne) {
	size_t f = t.find_first_of(L" \t|");
	int d;
	if (!lireDate(t.substr(0, f), d)) return false;
	Entree e;
	e.ligne = ligne;
	e.date = d;
	std::wstring reste = f == std::wstring::npos ? L"" : trim(t.substr(f));
	if (reste.size() >= 4 && iswdigit(reste[0]) && reste.find(L':') <= 2) {
		size_t g = reste.find_first_of(L" \t|");
		e.heure = reste.substr(0, g);
		reste = g == std::wstring::npos ? L"" : trim(reste.substr(g));
	}
	if (!reste.empty() && reste[0] == L'|') reste = trim(reste.substr(1));
	e.texte = reste;
	p.journal.push_back(e);
	return true;
}

static void lireLigne(Projet &p, const std::wstring &brute, int ligne) {
	std::wstring t = trim(brute);
	if (t.empty() || commencePar(t, L"//")) return;
	if (commencePar(t, L"==") || t[0] == L'#') {
		std::wstring nom = t;
		nom.erase(std::remove_if(nom.begin(), nom.end(), [](wchar_t c) { return c == L'=' || c == L'#'; }), nom.end());
		p.sections.push_back({normaliser(trim(nom)), ligne});
		return;
	}
	if (t.size() >= 3 && t[0] == L'[' && t[2] == L']') { lireTache(p, t, ligne); return; }
	if (t.size() >= 3 && t[0] == L'{' && t[2] == L'}') { lireJalon(p, t, ligne); return; }
	std::wstring n = normaliser(t);
	if (commencePar(n, L"decision")) { lireDecision(p, t, ligne); return; }
	if (commencePar(n, L"risque")) { lireRisque(p, t, ligne); return; }
	if (lireJournal(p, t, ligne)) return;
	std::wstring cle, val;
	if (cleValeur(t, cle, val)) {
		int d;
		if (cle == L"projet" || cle == L"nom") p.nom = val;
		else if (cle == L"chef") p.chef = val;
		else if (cle == L"debut" && lireDate(val, d)) p.debut = d;
		else if (cle == L"fin" && lireDate(val, d)) p.fin = d;
	}
}

// Une tâche sans début qui dépend d'autres tâches commence le lendemain de la plus tardive.
static void resoudreDependances(Projet &p) {
	std::map<std::wstring, size_t> index;
	for (size_t i = 0; i < p.taches.size(); ++i)
		if (!p.taches[i].id.empty()) index[normaliser(p.taches[i].id)] = i;
	for (size_t passe = 0; passe < p.taches.size(); ++passe) {
		bool change = false;
		for (auto &t : p.taches) {
			if (t.dep.empty() || (t.debut != AUCUNE && !t.debutCalcule)) continue;
			int max = AUCUNE;
			for (auto &d : decouper(t.dep, L',')) {
				auto it = index.find(normaliser(d));
				if (it != index.end()) max = std::max(max, p.taches[it->second].fin);
			}
			if (max != AUCUNE && t.debut != max + 1) {
				t.debut = max + 1;
				t.debutCalcule = true;
				change = true;
			}
		}
		if (!change) break;
	}
}

Projet analyser(const std::wstring &texte) {
	Projet p;
	size_t pos = 0;
	int ligne = 0;
	for (;;) {
		size_t f = texte.find(L'\n', pos);
		lireLigne(p, texte.substr(pos, f == std::wstring::npos ? std::wstring::npos : f - pos), ligne);
		if (f == std::wstring::npos) break;
		pos = f + 1;
		++ligne;
	}
	resoudreDependances(p);
	return p;
}

// ---- Indicateurs ------------------------------------------------------------

int avancementEffectif(const Tache &t) {
	if (t.statut == Statut::Fait) return 100;
	if (t.avancement >= 0) return t.avancement;
	return t.statut == Statut::EnCours ? 50 : 0;
}

std::wstring libelleStatut(Statut s) {
	switch (s) {
	case Statut::EnCours: return L"En cours";
	case Statut::Fait: return L"Fait";
	case Statut::Bloque: return L"Bloqué";
	default: return L"À faire";
	}
}

Stats calculer(const Projet &p, int auj) {
	Stats s;
	int somme = 0;
	std::map<std::wstring, int> charge;
	for (auto &t : p.taches) {
		++s.total;
		switch (t.statut) {
		case Statut::AFaire: ++s.aFaire; break;
		case Statut::EnCours: ++s.enCours; break;
		case Statut::Fait: ++s.fait; break;
		case Statut::Bloque: ++s.bloque; break;
		}
		somme += avancementEffectif(t);
		if (t.fin != AUCUNE) s.finPrevue = std::max(s.finPrevue, t.fin);
		if (t.statut == Statut::Fait) continue;
		if (t.fin != AUCUNE && t.fin < auj) s.enRetard.push_back(&t);
		else if (t.fin != AUCUNE && t.fin <= auj + 7) s.bientot.push_back(&t);
		std::wstring r = t.resp.empty() || t.resp == L"?" ? L"(non affecté)" : t.resp;
		++charge[r];
	}
	s.pourcent = s.total ? somme / s.total : 0;
	for (auto &j : p.jalons) {
		if (j.atteint || j.date == AUCUNE) continue;
		if (j.date < auj) s.jalonsEnRetard.push_back(&j);
		else if (!s.prochainJalon || j.date < s.prochainJalon->date) s.prochainJalon = &j;
	}
	for (auto &r : p.risques) if (!r.clos) ++s.risquesOuverts;
	auto parEcheance = [](const Tache *a, const Tache *b) { return a->fin < b->fin; };
	std::sort(s.enRetard.begin(), s.enRetard.end(), parEcheance);
	std::sort(s.bientot.begin(), s.bientot.end(), parEcheance);
	s.charge.assign(charge.begin(), charge.end());
	std::sort(s.charge.begin(), s.charge.end(), [](auto &a, auto &b) { return a.second > b.second; });
	return s;
}

} // namespace pp
