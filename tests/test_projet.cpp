// Test du parseur et du Gantt hors Notepad++ : test_projet.exe fichier.projet sortie.html
#include "../src/Projet.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <windows.h>

using namespace pp;

static int g_echecs = 0;
#define VERIFIE(c) do { if (!(c)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #c); ++g_echecs; } } while (0)

static std::wstring lireUtf8(const char *chemin) {
	std::ifstream f(chemin, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	std::string s = ss.str();
	int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
	return w;
}

int main(int argc, char **argv) {
	if (argc < 3) { std::printf("usage : test_projet fichier.projet sortie.html\n"); return 2; }

	int d;
	VERIFIE(lireDate(L"2026-10-29", d) && dateIso(d) == L"2026-10-29");
	VERIFIE(lireDate(L"29/10/2026", d) && dateIso(d) == L"2026-10-29");
	VERIFIE(!lireDate(L"31/02/2026", d));
	VERIFIE(!lireDate(L"T1", d));
	VERIFIE(jourSemaine(dateVersJour(2026, 10, 26)) == 0); // lundi

	Projet p = analyser(lireUtf8(argv[1]));
	VERIFIE(p.nom == L"Refonte du site web");
	VERIFIE(p.chef == L"Camille");
	VERIFIE(p.taches.size() == 8);
	VERIFIE(p.jalons.size() == 4);
	VERIFIE(p.journal.size() == 3);
	VERIFIE(p.decisions.size() == 2);
	VERIFIE(p.risques.size() == 3);
	if (p.taches.size() == 8) {
		VERIFIE(p.taches[0].statut == Statut::Fait);
		VERIFIE(p.taches[2].avancement == 70 && p.taches[2].resp == L"Alice");
		VERIFIE(p.taches[3].debutCalcule && dateIso(p.taches[3].debut) == L"2026-10-28");
		VERIFIE(p.taches[4].statut == Statut::Bloque && p.taches[4].note == L"en attente des textes");
		VERIFIE(p.taches[6].debutCalcule && dateIso(p.taches[6].debut) == L"2026-12-05");
		VERIFIE(dateIso(p.taches[7].debut) == L"2026-12-15" && dateIso(p.taches[7].fin) == L"2026-12-16");
		VERIFIE(p.taches[0].ligne == 10);
	}
	if (p.risques.size() == 3) {
		VERIFIE(p.risques[0].id == L"R1" && p.risques[0].niveau == L"haute");
		VERIFIE(p.risques[0].action == L"relance hebdo + textes provisoires");
		VERIFIE(p.risques[2].clos);
	}
	if (p.journal.size() == 3) VERIFIE(p.journal[1].heure == L"14:00" && p.journal[1].texte.find(L"Point hebdo") == 0);

	int auj = dateVersJour(2026, 10, 29);
	Stats s = calculer(p, auj);
	VERIFIE(s.total == 8 && s.fait == 2 && s.enCours == 2 && s.bloque == 1 && s.aFaire == 3);
	VERIFIE(s.enRetard.size() == 1); // T3, fin 27/10
	VERIFIE(s.jalonsEnRetard.size() == 1); // J2, 28/10
	VERIFIE(s.prochainJalon && s.prochainJalon->id == L"J3");
	VERIFIE(s.risquesOuverts == 2);
	std::printf("Avancement %d %%, %zu en retard, %zu sous 7 j\n", s.pourcent, s.enRetard.size(), s.bientot.size());

	std::string html = versUtf8(genererGantt(p, auj));
	std::ofstream(argv[2], std::ios::binary) << html;
	VERIFIE(html.find("Refonte du site web") != std::string::npos);

	std::printf(g_echecs ? "%d echec(s)\n" : "Tous les tests passent\n", g_echecs);
	return g_echecs ? 1 : 0;
}
