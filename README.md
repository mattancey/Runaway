# Runaway — Processeur Audio Distortion & Résonateur (VST3 / AU)

Plug-in audio **macOS 12.0+ Universal (`arm64` Apple Silicon + `x86_64` Intel)** pour **Ableton Live** (formats **VST3** et **AU `.component`** + app Standalone).

---

## 1. Architecture exacte du dépôt GitHub

```text
Runaway/
├── .github/
│   └── workflows/
│       └── build.yml
├── docs/
│   └── ROADMAP.md
├── Source/
│   ├── DSP/
│   │   └── WaveshaperEngine.h
│   ├── PluginEditor.cpp
│   ├── PluginEditor.h
│   ├── PluginProcessor.cpp
│   └── PluginProcessor.h
├── CMakeLists.txt
└── README.md
