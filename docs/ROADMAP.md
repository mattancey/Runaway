docs/ROADMAP.md

# Runaway — Feuille de Route & Point de Reprise

> **Comment reprendre la prochaine fois :**
> 1. Remplace ton fichier `docs/ROADMAP.md` sur GitHub par celui-ci.
> 2. À ta prochaine connexion dans un nouveau chat, copie-colle ce fichier (puis envoie les fichiers `Source/` de ton GitHub si le chat te les demande).
> 3. **Rappel pour l'assistant :** L'utilisateur est musicien (non-codeur). Ne donner **que des fichiers complets par bloc avec leur adresse précise juste avant** (un fichier à la fois si plusieurs fichiers changent).

---

## 1. Fiche Technique du Projet
- **Nom** : **Runaway** (v0.5.0+ — `RunawayAudio`)
- **Stack** : C++17, JUCE `8.0.14` (FetchContent), CMake
- **Cible** : macOS 12+ Universal Binary (`arm64;x86_64`), formats **AU**, **VST3**, **Standalone** (`kAudioUnitType_MusicEffect`, entrée MIDI activée)
- **Structure actuelle du dépôt** :
  - `CMakeLists.txt` *(inchangé)*
  - `.github/workflows/build.yml`
  - `docs/ROADMAP.md` *(ce fichier)*
  - `README.md`
  - `Source/DSP/WaveshaperEngine.h`
  - `Source/PluginProcessor.h`
  - `Source/PluginProcessor.cpp`
  - `Source/PluginEditor.h`
  - `Source/PluginEditor.cpp`

---

## 2. Tout ce qui est déjà implémenté aujourd'hui
1. **4 Topologies de distorsion (oversampling 4× + ADAA)** :
   - `BUCHLA / SOMA` (259 parallel folder + Harvezi Spoil)
   - `BIFOLD MORPH` (Buchla parallel ↔ Serge serial)
   - `ERICA / XENO` (ADAA tanh + biais Germanium + Scream EQ)
   - `112dB / CAT` (Werner nonlinear TPT SVF)
2. **3 Moteurs de Boucle (`LOOP ENGINE`)** :
   - `STRING` (*Morphor Echon 6* BBD + dérive stéréo)
   - `CHORD` (*112dB Razornator* 4 voix accordées)
   - `DIFFUSE` (*Strymon NightSky* matrice FDN Hadamard 4×4 + shimmer octave)
   - Contrôles associés : `LOOP: ON/BYPASS`, `IN-LOOP LVL`, `RESO PITCH` (+ `MIDI TRACK`), `FEEDBACK`, `LOOP LPF`, `SPREAD`, `cosmos`, `POLARITY`, `ROUTING: PRE / IN-LOOP`.
3. **Envelope Follower (*Elektron Analog Heat*)** :
   - Assignable à `OFF`, `DRIVE`, `CHARACTER`, `LOOP LPF`, `RESO PITCH` + `ENV AMOUNT`, `ENV RELEASE`, vumètre `ENV ACTIVITY`.
4. **Vactrol LPG (*Buchla 292 / Optomix*)** :
   - Modes `OFF`, `BOTH (292)`, `VCA ONLY`, `FILTER ONLY` + `LPG DECAY`, `LPG RING / Q`, déclenchement audio & notes MIDI, vumètre `VACTROL LED`.
5. **Gestionnaire de Presets & Visuels** :
   - Comparateur **A/B** (`SLOT: A/B`, `COPY`).
   - Oscilloscope `DRY IN / RUNAWAY OUT` + vue `TRANSFER CURVE y = f(x)` cliquable.
   - **8 Factory Presets** dans `Source/PluginProcessor.cpp`.
   - **Sauvegarde directe intégrée (`SavePresetOverlay`)** dans `Source/PluginEditor.h/.cpp` : plus de fenêtre Finder/Windows, les patches `.runaway` sont enregistrés dans `~/Library/Audio/Presets/RunawayAudio/Runaway/` et apparaissent immédiatement dans la section **`USER PRESETS`** du menu déroulant.
6. **Fonction `cosmos` (tout juste ajoutée)** :
   - Potard nommé **`cosmos`** (en minuscules) dans Row 2 (`LOOP ENGINE`).
   - Paramètre APVTS `"cosmos"` (`0–100 %`), mémorisé par mode (`numPerModeParams = 20`, index `10`).
   - Méthode `WaveshaperEngine::cosmos (int ch, float amount)` : 3 LFO lents désynchronisés Gauche/Droite (`0.043–0.317 Hz`) + marche aléatoire lissée (`cosmosWanderA/B`), modulant le pitch de délai, le `Loop LPF` en asymétrie L/R et la diffusion `Chord`/`FDN`.

---

## 3. Point exact de reprise pour la prochaine session

### Priorité 1 — Rendre `cosmos` beaucoup plus audible et expressif
- **Constat actuel** : L'utilisateur a testé et **a du mal à entendre la différence** avec `cosmos`.
- **Pourquoi** : Les modulations actuelles sont très sages (`pitchDrift` max ±2.4 %, cycles très lents de 3 à 23 secondes, `filterDrift` ±0.85 octave), donc sur des boucles courtes ou un feedback modéré l'effet paraît presque imperceptible.
- **Pistes pour le prochain tour** :
  - Amplifier nettement la plage musicale de `cosmos` (surtout au-delà de 30–40 % : micro-dérive tape/chorus organique bien perceptible sur le pitch, balayage asymétrique L/R plus large du `Loop LPF`, léger mouvement de gain/saturation ou décalage rythmique/de phase entre L et R, et cycles un peu plus vivants sur le haut du potard).
  - Éventuellement ajouter un preset dédié ou tester sur `07 NIGHTSKY SHIMMER CLOUD` / `03 SOMA TOTAL CHAOS` pour entendre immédiatement le avant/après.

### Notes utiles conservées
- **Presets d'usine** : L'utilisateur sait modifier les valeurs dans `factoryPresets` (`Source/PluginProcessor.cpp`), notamment l'index `7` (`feedback`) pour calmer les presets les plus explosifs.
- **Shimmer (`FDN GLIMMER`)** : Étude annulée pour le moment.
- **Idées futures en réserve** : *Make Noise Strega* (sub-harmoniques / bruit de boucle BBD).
