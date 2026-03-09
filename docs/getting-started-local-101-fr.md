# Boarder 101 : Mode Hero

Vous etes le realisateur. Votre mission : passer de zero a un storyboard vivant en quelques minutes.

Ouvrez ce guide a tout moment via `Help -> Getting Started (Local 101)`.

## Ce Que Vous Allez Apprendre Dans Ce Parcours 101
- Creer et ouvrir un nouveau projet.
- Commencer a dessiner immediatement sur le plan de depart.
- Sauvegarder avec confiance et lire l'indicateur modifie/propre dans le titre.
- Ajouter un plan a la position du curseur dans la timeline.
- Inserer un nouveau panel dans un plan existant.
- Creer un nouveau layer et dessiner dessus.
- Creer une camera dans un plan et la verifier visuellement.
- Creer une nouvelle scene rattachee a votre flux narratif actuel.
- Sauvegarder, rouvrir et verifier la persistance des donnees.

## Acte 1 : Creer Votre Projet
1. Lancez Boarder.
2. Allez dans `File -> New`.
3. Renseignez le nom du projet, l'emplacement, la resolution et le preset canvas.
4. Validez.

Resultat attendu :
- Le projet s'ouvre automatiquement.
- Une scene/shot/panel de depart est creee.
- Vous pouvez dessiner immediatement.

Boarder juste apres le lancement.

![Boarder launch window](./images/101-01-app-launch.png)

Ouvrez le menu File et choisissez New.

![File menu with New selected](./images/101-02-file-new-menu.png)

Boite de dialogue New Project avant creation.

![New Project dialog](./images/101-03-new-project-dialog.png)

Projet ouvert avec la scene, le shot et le panel de depart.

![Default starter content after project creation](./images/101-04-project-opened-defaults.png)

## Acte 2 : Premier Trait, Sans Configuration
1. Verifiez que `Layer 1` est selectionne.
2. Tracez un premier trait dans la zone de dessin.

Premier trait sur le canvas.

![First stroke on canvas](./images/101-05-first-stroke.png)

## Acte 3 : Sauvegarde (Ultra Bref)
- Menu : `File -> Save`
- Raccourci : `Cmd+S` (macOS) ou `Ctrl+S` (Windows/Linux)

Repere visuel :
- `*` dans le titre = modifications non sauvegardees
- plus de `*` apres sauvegarde = etat propre

Entree Save dans le menu File.

![Save action in menu](./images/101-06-save-menu-and-shortcut.png)

Etat non sauvegarde avec `*` dans le titre de fenetre.

![Dirty title state with asterisk](./images/101-07-title-dirty-asterisk.png)

Titre propre apres sauvegarde.

![Clean title state after save](./images/101-08-title-clean-after-save.png)

## Acte 4 : Ajouter Un Shot
1. Placez le curseur timeline a l'endroit voulu.
2. Ouvrez `Storyboard -> Shot -> New Shot`.
3. Confirmez les parametres du shot et le mode d'insertion.

Resultat attendu :
- Un nouveau shot apparait dans la timeline et la liste des shots.

Commande New Shot depuis le menu Storyboard.

![New Shot menu action](./images/101-09-new-shot-menu.png)

Boite de dialogue New Shot avec les options d'insertion.

![New Shot dialog](./images/101-10-new-shot-dialog.png)

Timeline et arbre des shots apres ajout.

![Shot added to timeline](./images/101-11-shot-added-timeline.png)

## Acte 5 : Inserer Un Nouveau Panel Dans Un Shot
1. Placez le curseur timeline dans le shot cible.
2. Ouvrez `Storyboard -> Panel -> Insert Panel`.
3. Choisissez le mode d'insertion puis validez.

Resultat attendu :
- Un nouveau panel est insere dans le shot selectionne.

Commande Insert Panel depuis Storyboard.

![Insert Panel menu action](./images/101-12-insert-panel-menu.png)

Boite de dialogue Insert Panel avant validation.

![Insert Panel dialog](./images/101-13-insert-panel-dialog.png)

Shot apres insertion du nouveau panel.

![Panel inserted in shot](./images/101-14-panel-inserted-result.png)

## Acte 6 : Creer Un Layer
1. Selectionnez le panel cible.
2. Creez un layer depuis les controles/context menu des layers.
3. Selectionnez ce nouveau layer et dessinez.

Resultat attendu :
- Le nouveau layer apparait dans la liste.
- Votre trait est bien enregistre sur ce layer.

Action de creation de layer.

![New layer command](./images/101-15-new-layer-menu.png)

Liste des layers apres creation.

![Layer list with newly created layer](./images/101-16-layer-list-after-create.png)

Dessin sur le nouveau layer.

![Stroke on newly created layer](./images/101-17-draw-on-new-layer.png)

## Acte 7 : Creer Une Camera Dans Un Shot
1. Placez le curseur timeline sur un shot valide.
2. Ouvrez `Storyboard -> Camera -> New Camera`.
3. Verifiez l'apparition de la camera dans la vue de dessin et la timeline.

Astuce :
- Si necessaire, utilisez le mode camera dans la zone de dessin pour cadrer directement.

Curseur positionne sur un shot valide.

![Cursor on valid shot for camera creation](./images/101-18-cursor-on-shot-for-camera.png)

Commande New Camera depuis Storyboard.

![New Camera menu action](./images/101-19-new-camera-menu.png)

Cadre camera visible dans la vue de dessin.

![Camera frame visible](./images/101-20-camera-frame-visible.png)

Confirmation camera dans timeline/keyframes.

![Camera track or keyframe visible](./images/101-21-camera-track-or-keyframe.png)

## Acte 8 : Creer Une Nouvelle Scene Rattachee Au Flux Actuel
1. Placez le curseur sur ou pres du shot ou vous voulez brancher/ajouter.
2. Ouvrez `Storyboard -> Scene -> New Scene`.
3. Dans le mode d'insertion, choisissez ou rattacher la scene (par exemple en append).
4. Confirmez la creation.

Resultat attendu :
- La nouvelle scene apparait avec ses shots dans la timeline et l'arbre des shots.

Commande New Scene depuis Storyboard.

![New Scene menu action](./images/101-22-new-scene-menu.png)

Boite de dialogue New Scene avec mode append/insert selectionne.

![New Scene dialog in append mode](./images/101-23-new-scene-dialog-append.png)

Timeline apres creation de la nouvelle scene.

![Scene added to timeline](./images/101-24-scene-added-timeline.png)

## Acte 9 : Verifier La Persistance
1. Sauvegardez.
2. Fermez puis rouvrez le projet.
3. Verifiez que scene/shot/panel/layers/cameras sont toujours presents.

Projet rouvert avec contenu intact.

![Save and reopen validation](./images/101-25-save-reopen-validation.png)

## Recuperation Rapide
`L'action shot/camera indique qu'aucune selection valide n'est disponible`
- Deplacez le curseur dans un shot puis recommencez.

`Panel ou layer invisible`
- Selectionnez d'abord le shot, puis le panel, puis le layer.

`Changements absents apres reouverture`
- Sauvegardez avant fermeture et verifiez que le `*` a disparu du titre.
