# Projet Pilot

**Plugin Notepad++ de conduite de projet.** Pilotez un projet depuis un simple fichier texte : tâches, jalons, journal, décisions, risques, un tableau de bord et un diagramme de Gantt — le tout dans Notepad++ (64 bits).

## Fonctions

- **Tableau de bord** (panneau ancré à droite) : avancement en %, tâches en retard, échéances à 7 jours, prochain jalon, risques ouverts, charge par personne. Double-clic sur une ligne = saut à l'endroit du fichier. Mise à jour pendant la saisie.
- **Diagramme de Gantt** : page HTML (barres, avancement, jalons, repère du jour, week-ends) ouverte dans le navigateur.
- **Insertion assistée** : tâche, jalon, entrée de journal horodatée, décision, risque.
- **Statuts** : à faire → en cours → fait ; blocage/déblocage.
- **Modèle** : création d'un nouveau projet pré-rempli.

## Format du fichier projet

Du texte simple, une ligne par élément :

```
PROJET: Refonte du site web
CHEF: Camille
DEBUT: 2026-10-12
FIN: 2026-12-18

== TÂCHES ==
[x] T1 Cadrage | resp:Camille | debut:2026-10-12 | fin:2026-10-16 | prio:haute
[~] T2 Maquettes | resp:Alice | dep:T1 | fin:2026-10-27 | av:70
[!] T3 Contenus | resp:Client | fin:2026-11-06 | note:en attente des textes

== JALONS ==
{x} J1 Lancement | 2026-10-12
{ } J2 Mise en ligne | 2026-12-18

== JOURNAL ==
2026-10-12 09:30 | Réunion de lancement

== DÉCISIONS ==
DECISION 2026-10-14 | On part sur WordPress

== RISQUES ==
RISQUE R1 haute | Retard des contenus | action:relance hebdo
```

- Statuts des tâches : `[ ]` à faire, `[~]` en cours, `[x]` fait, `[!]` bloqué.
- Jalons : `{ }` à venir, `{x}` atteint.
- Champs : `resp:` `debut:` `fin:` `prio:` `av:` (avancement %) `dep:` (démarre après une autre tâche) `note:`.
- Dates : `AAAA-MM-JJ` ou `JJ/MM/AAAA`. Commentaires : lignes commençant par `//`.

Voir [exemple.projet](exemple.projet).

## Installation (binaire)

1. Téléchargez `ProjetPilot.zip` depuis les *Releases*.
2. Fermez Notepad++.
3. Décompressez-le dans `C:\Program Files\Notepad++\plugins\` pour obtenir
   `C:\Program Files\Notepad++\plugins\ProjetPilot\ProjetPilot.dll`.
4. Rouvrez Notepad++ : menu **Modules > Projet Pilot**.

Le dossier doit porter le même nom que la DLL.

## Raccourcis

`Ctrl+Alt+Maj` + `D` (tableau de bord), `G` (Gantt), `T` (tâche), `J` (jalon), `L` (journal), `S` (statut).

## Compilation depuis les sources

Avec [MinGW-w64](https://winlibs.com/) (GCC) :

```bat
build.bat
```

Cela produit `build\ProjetPilot.dll` et lance les tests sur l'exemple.

## Licence

MIT — voir [LICENSE](LICENSE).
