// Modèle et analyse d'un fichier projet texte (indépendant de Notepad++).
#pragma once
#include <string>
#include <vector>
#include <utility>

namespace pp {

const int AUCUNE = -1000000; // date absente (les dates sont en jours depuis 1970-01-01)

enum class Statut { AFaire, EnCours, Fait, Bloque };

struct Tache {
	int ligne = 0; // 0-based, comme Scintilla
	std::wstring id, titre, resp, prio, dep, note;
	int debut = AUCUNE, fin = AUCUNE;
	bool debutCalcule = false; // début déduit de dep:
	int avancement = -1;       // 0-100, -1 = non renseigné
	Statut statut = Statut::AFaire;
};

struct Jalon {
	int ligne = 0;
	std::wstring id, titre;
	int date = AUCUNE;
	bool atteint = false;
};

struct Entree {
	int ligne = 0;
	int date = AUCUNE;
	std::wstring heure, texte;
};

struct Decision {
	int ligne = 0;
	int date = AUCUNE;
	std::wstring texte;
};

struct Risque {
	int ligne = 0;
	std::wstring id, niveau, texte, action;
	bool clos = false;
};

struct Projet {
	std::wstring nom, chef;
	int debut = AUCUNE, fin = AUCUNE;
	std::vector<Tache> taches;
	std::vector<Jalon> jalons;
	std::vector<Entree> journal;
	std::vector<Decision> decisions;
	std::vector<Risque> risques;
	std::vector<std::pair<std::wstring, int>> sections; // nom normalisé, ligne
};

struct Stats {
	int total = 0, aFaire = 0, enCours = 0, fait = 0, bloque = 0;
	int pourcent = 0;
	std::vector<const Tache *> enRetard, bientot;
	std::vector<const Jalon *> jalonsEnRetard;
	const Jalon *prochainJalon = nullptr;
	int risquesOuverts = 0;
	int finPrevue = AUCUNE;
	std::vector<std::pair<std::wstring, int>> charge; // tâches non terminées par responsable
};

Projet analyser(const std::wstring &texte);
Stats calculer(const Projet &p, int aujourdhui);
std::wstring genererGantt(const Projet &p, int aujourdhui);

// Outils
int dateVersJour(int annee, int mois, int jour);
void jourVersDate(int n, int &annee, int &mois, int &jour);
int jourSemaine(int n); // lundi = 0
bool lireDate(const std::wstring &s, int &jour);
std::wstring dateIso(int jour);
std::wstring dateFr(int jour);
int aujourdhui();
std::wstring heureCourante();
int avancementEffectif(const Tache &t);
std::wstring libelleStatut(Statut s);
std::wstring normaliser(const std::wstring &s);
std::wstring trim(const std::wstring &s);
std::string versUtf8(const std::wstring &s);

} // namespace pp
