# Red Blade FS — Aurora + OpenRGB + VIA

Firmware SonixQMK pour le **Designed by GG Red Blade Full Size ISO**, avec pavé numérique, USB **320F:5136**, contrôleur **SN32F248B**, **109 LED**.

**État : expérimental, premier flash vérifié sur un Red Blade Full Size.** Le firmware est destiné à ce modèle précis. L’écriture et le checksum du flash, VIA 13, VialRGB 1 et les 109 LED ont été vérifiés sur le clavier ; la saisie a été confirmée par son utilisateur. La latence matérielle et les essais prolongés restent à mesurer. Ce dépôt ne contient aucune sauvegarde personnelle.

```text
Aurora (effets et jeux) → serveur SDK OpenRGB → VialRGB → Red Blade FS
```

OpenRGB reste utilisable directement, ou avec d'autres logiciels compatibles avec son SDK. Les jeux utilisent les intégrations des logiciels hôtes ; ce firmware ne promet pas une compatibilité directe avec tous les jeux. VIA conserve le remappage des touches et les macros. L'extension implémente le sous-ensemble VialRGB nécessaire à OpenRGB, pas l'interface complète de configuration Vial.

## Choix de performance

- Polling USB demandé : **1 ms**. NKRO, matrice, broches et anti-rebond `sym_eager_pk` de 20 ms conservés. Cet anti-rebond est de type eager : il n'ajoute pas une attente initiale de 20 ms à chaque appui.
- Horloge matérielle Sonix conservée à la valeur calculée par le pilote amont pour cette carte : **720 896 Hz**. C'est l'horloge de la minuterie partagée, pas une mesure de la fréquence de balayage complète ni de la latence.
- Le patch découple cette horloge de la taille des blocs de rendu. Le rendu traite au plus **8 LED** par passage et attend au moins **33 ms** entre images.
- Une seule table HSV de **327 octets** garde les dernières couleurs. Aucune allocation dynamique, aucune file d'images et aucune écriture EEPROM dans le chemin RGB direct. Les deux buffers du pilote Sonix amont sont conservés.
- Un paquet direct valide copie au plus **9 couleurs**. La conversion HSV → RGB se fait dans la tâche de rendu, pas dans le gestionnaire USB.
- Les effets autonomes conservés sont simples : couleur fixe, respiration, cycle horizontal, dégradés et alphas/modificateurs. Les effets complexes et réactifs sont fournis par Aurora/OpenRGB ; leurs anciens buffers et leur suivi des frappes sont retirés du firmware.
- Configuration Aurora proposée : intervalle de **34 ms**, environ **29,4 FPS**. Pas de service Windows supplémentaire.

Ces choix bornent le travail ajouté. Ils ne constituent pas une mesure matérielle de latence : comparer le firmware d'origine et celui-ci avec un analyseur USB ou un dispositif de mesure reste nécessaire. Le transport VialRGB standard utilise 13 échanges pour une image complète de 109 LED ; ses paquets peuvent arriver pendant le rendu d'une image.

## Compilation

Sources amont épinglées : [SonixQMK `62973ee48a976c6f7cc6d4f7b198bd48a2fdfe60`](https://github.com/SonixQMK/qmk_firmware/tree/62973ee48a976c6f7cc6d4f7b198bd48a2fdfe60).

Dans un environnement QMK configuré (QMK MSYS sous Windows, ou les outils QMK sous Linux) :

```sh
git clone https://github.com/SonixQMK/qmk_firmware.git qmk_firmware
git -C qmk_firmware checkout 62973ee48a976c6f7cc6d4f7b198bd48a2fdfe60
bash tools/build.sh /chemin/vers/qmk_firmware
```

`prepare_firmware.py` refuse un autre commit, vérifie la carte des 109 LED, installe le keymap et applique un seul changement au pilote : ajout d'un paramètre d'horloge, avec comportement inchangé par défaut pour les autres cartes. Les sources QMK et leurs sous-modules gardent leurs licences respectives.

## Sauvegarde et flashage

Le flashage remplace le firmware et peut réinitialiser les touches/macros VIA. Sauvegarder avant de continuer, garder un autre moyen de saisie et le firmware de restauration du fabricant.

```powershell
python tools/redblade.py backup --output C:\chemin-prive\via-backup.json
```

La sauvegarde contient les couches et macros : **la garder privée**. Elle ne constitue pas un dump complet du firmware. Ne pas restaurer aveuglément des codes internes entre générations QMK différentes.

1. Vérifier le modèle Full Size, pas le Red Blade 75.
2. Utiliser **Fn + Échap** pour entrer dans le bootloader QMK.
3. Avec [SonixFlasherC](https://github.com/SonixQMK/SonixFlasherC), vérifier l'identité du bootloader **0C45:7040 / SN32F24xB** avant toute écriture.
4. Pour ce contrôleur, utiliser le binaire brut, à l'offset **0** ; ne pas utiliser un jumploader SN32F26x.

```powershell
sonixflasher.exe -v 0c45/7040 --info
# Seulement après vérification du contrôleur et décision explicite de flasher :
sonixflasher.exe -v 0c45/7040 -f dist/redblade-fs-aurora-via.bin -o 0
```

La référence actuelle est [SonixFlasherC](https://github.com/SonixQMK/SonixFlasherC). L'ancien dépôt `sonix-flasher` est archivé et déconseillé par ses mainteneurs.

Pour revenir au firmware constructeur, suivre la [procédure Designed by GG](https://designedby.gg/qmk-installateur-simplifie/) et utiliser l'archive officielle **Red Blade Full Size**. Le fabricant demande de repasser par le firmware d'origine avant d'utiliser ses installateurs de mise à jour QMK.

## Configuration Windows

OpenRGB doit contenir le contrôleur **QMK VialRGB Devices** (version vérifiée sur le PC de préparation : OpenRGB 1.1). Ajouter dans ses réglages VialRGB : nom `Designed by GG Red Blade FS (VialRGB)`, VID `0x320F`, PID `0x5136`, puis redémarrer complètement OpenRGB.

Le script PowerShell 7 fait cette configuration, sauvegarde les fichiers existants, règle le SDK local et Aurora, et prépare le démarrage des applications :

```powershell
pwsh -File tools/configure-windows.ps1        # lecture / description uniquement
pwsh -File tools/configure-windows.ps1 -Apply # applique puis relance les applications RGB
```

La liaison utilise **127.0.0.1:6742**. Dans Aurora, activer **OpenRGB (RGB.NET)**. Le clavier doit apparaître dans OpenRGB avec un mode **Direct** avant de valider les effets dans Aurora. Le script ne flashe rien et n'active pas le mode dangereux des périphériques sans mode direct.

Après flashage, relancer `configure-windows.ps1 -Apply`. Il arrête temporairement les clients RGB, sélectionne explicitement **Direct** dans le clavier et enregistre ce réglage une fois, avant de relancer OpenRGB/Aurora. Cela corrige le cas où des couleurs sont envoyées par le SDK alors que le clavier reste sur son effet autonome (par exemple rouge uniforme). L’écriture de configuration ne se produit pas pendant le streaming RGB.

Pour ne changer que ce réglage, avec OpenRGB, Aurora et VIA arrêtés :

```powershell
python tools/set-direct-mode.py --save
```

Fermer VIA pendant l'utilisation d'OpenRGB : les deux outils partagent la même interface Raw HID et peuvent recevoir la réponse destinée à l'autre.

## Vérification

```powershell
python tools/redblade.py verify
```

Cette vérification est en lecture seule et contrôle la signature RGB, la version, les 109 LED et leurs positions uniques. Vérifier ensuite dans OpenRGB les couleurs rouge/vert/bleu, le pavé numérique, les touches ISO, puis le mode Direct avec Aurora, la saisie NKRO, la sortie de veille et le rebranchement USB.

Arrêter OpenRGB, Aurora et VIA avant les commandes Raw HID `redblade.py backup`, `redblade.py verify` ou `set-direct-mode.py`. Une interface partagée avec un client actif peut recevoir une réponse destinée à l'autre outil. Le script de mappage ci-dessous utilise le SDK OpenRGB et peut être exécuté lorsque son serveur est actif.

Après apparition du clavier dans OpenRGB, établir le mappage physique explicite dans Aurora :

```powershell
pwsh -File tools/configure-aurora-mapping.ps1        # vérifie les 109 indices réels
pwsh -File tools/configure-aurora-mapping.ps1 -Apply # sauvegarde et écrit le mappage
```

Relancer ensuite Aurora. Le script utilise le fournisseur RGB.NET livré avec Aurora, donc le mappage ne dépend pas de la reconnaissance des noms de touches QMK dans OpenRGB. Le pavé numérique, les touches ISO et les quatre touches supplémentaires sont inclus.

Si OpenRGB est installé comme service Windows, `configure-windows.ps1` réutilise ce service et sa configuration dans `service_config`. L'application de ces réglages demande alors une console élevée ; aucun deuxième serveur n'est ajouté.

Les tests C compilent le véritable gestionnaire du firmware contre une petite simulation de QMK : passage des commandes VIA, découverte, limites des buffers, toutes les positions de LED, couleurs et modes. Le workflow GitHub les exécute avec AddressSanitizer et UndefinedBehaviorSanitizer. Il ne remplace pas un test sur clavier réel.

## Références

- [Fabricant : Red Blade Full Size](https://designedby.gg/product/red-blade-01/)
- [Définition de carte SonixQMK](https://github.com/SonixQMK/qmk_firmware/tree/62973ee48a976c6f7cc6d4f7b198bd48a2fdfe60/keyboards/designedbygg/redblade/rbfs)
- [Protocole VialRGB](https://github.com/vial-kb/vial-qmk/blob/vial/quantum/vialrgb.c)
- [Contrôleur VialRGB OpenRGB](https://github.com/CalcProgrammer1/OpenRGB/tree/master/Controllers/QMKController/QMKVialRGBController)
- [Aurora](https://github.com/Aurora-RGB/Aurora)

Le keymap est dérivé de celui de Dimitris Mantzouranis dans SonixQMK. Les sources ajoutées sont distribuées sous GPL-2.0-or-later ; voir `LICENSE` et les mentions des composants amont.
