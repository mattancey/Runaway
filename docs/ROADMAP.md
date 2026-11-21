# Runaway — Feuille de Route & Laboratoire Sonore (VST3 / AU)

**Runaway** est un processeur hybride **Distortion West-Coast + Résonateur BBD + Synthèse Réverbérante**, conçu étape par étape pour macOS 12+ (Universal Binary `arm64 + x86_64`, VST3 & AU dans Ableton Live).

---

## État Actuel : ÉTAPE 3 — L'Espace Diffus, Accords & Contrôle Musical *(Implémentée)*

### Nouveautés de l'Étape 3 (v0.3.0) :
1. **Contrôle dédié de la boucle (`LOOP: ON/OFF` + `LOOP LEVEL`)** :
   - Bypass fluide (crossfade 25 ms sans clic) de la boucle de résonance.
   - **`LOOP LEVEL` (`-24 dB` à `+12 dB`)** actif uniquement lorsque la boucle est enclenchée, pour égaliser exactement le volume entre `LOOP OFF`, `PRE-SHAPER` et `IN-SHAPER`.
2. **3 Moteurs de Boucle (`LOOP ENGINE`)** :
   - **`STRING` (*Morphor Echon 6*)** : résonateur BBD pur à 1 ligne accordée + dérive stéréo (*Intellijel Sealegs*).
   - **`CHORD` (*Erica Synths × 112dB Razornator*)** : 4 lignes parallèles accordées (Fondamentale, Quinte, Tierce mineure, Octave) contrôlées par `CHORD BLOOM`.
   - **`DIFFUSE` (*Strymon NightSky*)** : matrice **FDN Hadamard 4×4** à préservation d'énergie, amortissement 12 dB/oct par branche et **Glimmer / Shimmer octave supérieure (`+12`)** dosé par `FDN GLIMMER`.
3. **MIDI Pitch Tracking (`MIDI TRACK`)** :
   - Joue les notes sur une piste MIDI routée vers **Runaway** dans Ableton Live pour accorder en direct le résonateur, l'accord ou le cœur diffus du FDN.
4. **Mémoire indépendante par mode de distorsion** :
   - Chacune des 4 distorsions (`BUCHLA/SOMA`, `BIFOLD`, `ERICA/XENO`, `112dB/CAT`) mémorise ses 13 paramètres (dont `LOOP ENGINE`, `LOOP ON/OFF` et `LOOP LEVEL`).

---

## Prochaine Étape (Étape 4 — Mouvement & Modulations Modernes)
- **Inspirations** : **Elektron Analog Heat** (Envelope Follower), **SOMA Cosmos** (dérive lente), **Make Noise Strega** (sub-harmoniques / bruit de boucle).
- **Pistes à explorer ensemble** :
  - **Envelope Follower** assignable (`Drive`, `Character`, `Loop LPF`, `Pitch`).
  - **Gestionnaire de Presets** complet + visualisation enrichie dans l'UI.
