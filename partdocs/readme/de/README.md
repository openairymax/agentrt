# Agent OS

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.5-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Mirror](https://img.shields.io/badge/mirror-GitHub-lightgrey.svg)](https://github.com/SpharxTeam/AgentOS)

---

**SuperAI-Betriebssystem**

*"Aus Daten entsteht Intelligenz."*

---

📖 **[简体中文](../../README.md)** | [English](../en/README.md) | [Français](../fr/README.md) | 🇩🇪 **Deutsch**

</div>

---

## Einführung

- Entwickelt für Aufgabenausführung mit maximaler Token-Effizienz
- Neuartige Architektur mit 2-3× besserer Token-Nutzung als Branchenstandards
- 3-5× effizienter als OpenClaw bei Engineering-Aufgaben, ~60% Token-Einsparung

## 📋 Übersicht

- **Agent OS (SuperAI OS)** ist der intelligente Agenten-Betriebssystemkern von SpharxWorks mit kompletter Runtime-Umgebung, Speichersystem, kognitiver Engine und Ausführungs-Framework.
- Als physische Welt-Dateninfrastruktur implementiert AgentOS einen geschlossenen Kreislauf von der Datenverarbeitung bis zur intelligenten Entscheidungsfindung.

### Kernwerte

- **Mikrokern**: Minimalistische Kernel-Konstruktion, alle Services im User-Space
- **Drei-Schichten-Architektur**: Kognition, Ausführung und Speicher für Agent-Lifecycle-Management
- **Memory-Roll-System**: L1-L4 progressive Abstraktion mit Speicherung, Abruf, Evolution und Vergessen
- **Systemaufrufe**: Stabile und sichere Schnittstellen mit gekapselter Kernel-Implementierung
- **Steckbare Strategien**: Dynamisches Laden und Runtime-Austausch von Algorithmen
- **Einheitliches Logging**: Sprachübergreifende Schnittstelle mit vollständiger Verfolgung und OpenTelemetry
- **Multi-Sprache-SDK**: Native Unterstützung für Go, Python, Rust und TypeScript mit FFI-Schnittstellen

### Versionsstatus

**Aktuelle Version**: v1.0.0.5 (Produktionsreif)

- ✅ Kernarchitektur-Design abgeschlossen
- ✅ MemoryRovol-Speichersystem
  - L1-L4 Vier-Schichten-Architektur vollständig implementiert
  - Synchrone/Asynchrone Schreibunterstützung (10.000+ Einträge/Sek.)
  - FAISS-Vektorsuche (IVF/HNSW-Indexierung)
  - Attraktorennetzwerk-Abrufmechanismus
  - Ebbinghaus-Vergessenskurve
  - LRU-Cache und Vektorpersistenz
- ✅ CoreLoopThree Drei-Schichten-Architektur
  - Kognitionsschicht: Intent-Verständnis, Aufgabenplanung, Koordination (90%)
  - Ausführungsschicht: Ausführungsmaschine, Kompensationstransaktionen, Verfolgung (85%)
  - Speicherschicht: MemoryRovol FFI-Wrapper (80%)
- ✅ Mikrokern-Basismodul (core)
  - IPC Binder-Kommunikation
  - Speicherverwaltung (RAII, Smart Pointer)
  - Aufgabenplanung (gewichteter Round-Robin)
  - Hochpräziser Zeitdienst
- ✅ Systemaufrufschicht (syscall) - 100% abgeschlossen
  - ✅ Aufgaben-Syscalls: `sys_task_submit/query/wait/cancel`
  - ✅ Speicher-Syscalls: `sys_memory_write/search/get/delete`
  - ✅ Sitzungs-Syscalls: `sys_session_create/get/close/list`
  - ✅ Observability-Syscalls: `sys_telemetry_metrics/traces`
  - ✅ Einheitlicher Einstiegspunkt: `agentos_syscall_invoke()`
- 🔲 Vollständige End-to-End-Integrationstests

---

## 🏗️ Systemarchitektur

```
┌─────────────────────────────────────────────────────────────┐
│                    AgentOS Gesamtarchitektur                 │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           Anwendungsschicht (openhub)                 │  │
│  │  docgen | ecommerce | research | videoedit | ...      │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │        Kern-Dienstschicht (backs)                     │  │
│  │  llm_d | market_d | monit_d | perm_d | sched_d | ...  │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │          Kernelschicht (atoms)                        │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │  │
│  │  │   core       │  │coreloopthree │  │memoryrovol  │  │  │
│  │  │ Mikrokern    │  │3-Schichten-Rt │ │4-Schicht-Sp. │  │  │
│  │  └──────────────┘  └──────────────┘  └─────────────┘  │  │
│  │  ┌──────────────┐                                     │  │
│  │  │   syscall    │                                     │  │
│  │  │ System Calls │                                     │  │
│  │  └──────────────┘                                     │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │            SDK-Schicht (tools)                        │  │
│  │  Go | Python | Rust | TypeScript | ...                │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 🧠 CoreLoopThree: Drei-Schichten-Architektur

### Designphilosophie

CoreLoopThree teilt die Agent-Runtime in drei orthogonale und synergistische Schichten:

```
┌─────────────────────────────────────────┐
         Kognitionsschicht                 
   • Intent-Verständnis • Aufgabenplanung 
   • Agent-Planung • Modell-Koordination  
└───────────────↓─────────────────────────┘
                ↓
┌─────────────────────────────────────────┐
         Ausführungsschicht                
    • Aufgabenausführung • Kompensation   
    • Kettenverfolgung • Zustandsmanagement
└───────────────↓─────────────────────────┘
                ↓
┌─────────────────────────────────────────┐
          Speicherschicht                  
    • Speicherschreiben • -abruf          
    • Kontext-Montage • Evolution & Vergessen
└─────────────────────────────────────────┘
```

### Kernkomponenten

#### 1. Kognitionsschicht
- **Intent-Verständnis-Engine**: Analyse Benutzereingabe, Identifikation echter Absichten
- **Aufgabenplaner**: Automatische Zerlegung in DAG-Aufgabengraph
- **Agent-Scheduler**: Multi-Agenten-Koordination und Ressourcenallokation
- **Modell-Koordinator**: LLM-Auswahl und Prompt-Engineering
- **Strategie-Schnittstellen**: Steckbare Algorithmen (Planung, Koordination, Scheduling)

#### 2. Ausführungsschicht
- **Ausführungsmaschine**: Aufgabenausführung und Zustandsverfolgung
  - Zustandsmaschine (Pending/Running/Succeeded/Failed/Cancelled/Retrying)
  - Nebenläufigkeitskontrolle und Timeout-Management
- **Kompensationstransaktionen**: Rollback bei Fehlern und Kompensationslogik
- **Verantwortungsketten-Verfolgung**: Vollständige Aufzeichnung der Ausführungskette
- **Ausführungseinheiten-Register**: Registrierung atomarer Ausführungseinheiten
- **Ausnahmebehandlung**: Hierarchische Erfassung und Wiederherstellung

#### 3. Speicherschicht
- **Speicherdienst**: Kapselung von MemoryRovol
  - Speicher-Engine (`agentos_memory_engine_t`)
  - Rekordtypen (RAW/FEATURE/STRUCTURE/PATTERN)
- **Schreibschnittstelle**: Synchrone/asynchrone Unterstützung
- **Abfrageschnittstelle**: Semantische Suche und Vektorabruf
- **Kontext-Montage**: Automatische Speicherzuordnung
- **FFI-Schnittstelle**: `rov_ffi.h` für sprachübergreifende Aufrufe

Siehe: [CoreLoopThree Architekturdokumentation](partdocs/architecture/coreloopthree.md)

---

## 💾 MemoryRovol: Speichersystem

### Positionierung

MemoryRovol ist das Kernel-Level-Speichersystem von AgentOS für umfassendes Speichermanagement von Rohdaten bis zu fortgeschrittenen Mustern.

### Vier-Schichten-Architektur

```
┌─────────────────────────────────────────┐
       L4 Musterschicht                    
   • Persistente Homologie • Stabile Muster
   • HDBSCAN-Clustering • Regelgenerierung 
└───────────────↑─────────────────────────┘
                ↓ Abstrakte Evolution
┌─────────────────────────────────────────┐
      L3 Strukturschicht                   
   • Bindeoperatoren • Relationskodierung  
└───────────────↑─────────────────────────┘
                ↓ Merkmalsextraktion
┌─────────────────────────────────────────┐
      L2 Merkmals schicht                  
   • Embedding-Modelle (OpenAI/DeepSeek)  
   • FAISS-Vektorindex • Hybride Suche     
└───────────────↑─────────────────────────┘
                ↓ Datenkompression
┌─────────────────────────────────────────┐
       L1 Rohschicht                       
   • Dateisystem-Speicher • Fragmentierung
   • Metadaten-Index • Integritätsprüfung  
└─────────────────────────────────────────┘
```

### Hauptfunktionen

#### 1. Speicherschreibung
- **Synchrones Schreiben**: Blockierend für Datenpersistenz
- **Asynchrones Schreiben**: Batch-Schreiben für hohen Durchsatz (10.000+/Sek.)
- **Transaktionsunterstützung**: ACID-Semantik
- **Komprimierungsarchivierung**: Automatische Komprimierung seltener Speicher

#### 2. Speicherabruf
- **Vektorsuche**: Kosinus-Ähnlichkeit über FAISS
  - Latenz < 10ms (k=10)
- **Semantische Suche**: Natürlichsprachliche Abfragen
- **Kontextsensitiv**: Automatische Filterung (Zeit, Quelle, TraceID)
- **LRU-Cache**: Cache für heiße Vektoren
- **Neubewertung**: Cross-Encoder für Relevanz

#### 3. Speicherevolution
- **Progressive Abstraktion**: L1→L2→L3→L4
  - Merkmalsextraktion
  - Strukturbindung
  - Mustersuche
- **Mustererkennung**: Identifikation häufiger Muster
- **Gewichtsaktualisierungen**: Dynamisch nach Zugriffshäufigkeit
- **Evaluierung**: Komitee mit Kognitionsschicht

#### 4. Speichervergessen
- **Ebbinghaus-Kurve**: Intelligentes Beschneiden nach Vergessenskurve
- **Linearer Zerfall**: Einfacher linearer Gewichtszerfall
- **Zugriffszähler**: LRU/LFU-Strategie
- **Aktives Vergessen**: Durch Kognitionsschicht ausgelöst

Siehe: [MemoryRovol Architekturdokumentation](partdocs/architecture/memoryrovol.md)

---

## 🛠️ Entwicklungsleitfaden

### Voraussetzungen

- **Betriebssystem**: Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2)
- **Compiler**: GCC 11+ oder Clang 14+
- **Build-Tools**: CMake 3.20+, Ninja oder Make
- **Abhängigkeiten**:
  - OpenSSL >= 1.1.1
  - libevent
  - pthread
  - FAISS >= 1.7.0
  - SQLite3 >= 3.35
  - libcurl >= 7.68
  - cJSON >= 1.7.15
  - Ripser >= 2.3.1 (optional)
  - HDBSCAN >= 0.8.27 (optional)

### Schnellstart

#### 1. Repository Klonen

```bash
git clone https://gitee.com/spharx/agentos.git
cd agentos
```

#### 2. Konfiguration Initialisieren

```bash
cp .env.example .env
python scripts/init_config.py
```

#### 3. Projekt Kompilieren

```bash
mkdir build && cd build
cmake ../atoms \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DENABLE_TRACING=ON

cmake --build . --parallel $(nproc)
ctest --output-on-failure
```

#### 4. Konfigurationsoptionen

| CMake-Variable | Beschreibung | Standard |
| :--- | :--- | :--- |
| `CMAKE_BUILD_TYPE` | Debug/Release/RelWithDebInfo | `Release` |
| `BUILD_TESTS` | Unit-Tests | `OFF` |
| `ENABLE_TRACING` | OpenTelemetry-Tracing | `OFF` |
| `ENABLE_ASAN` | AddressSanitizer | `OFF` |

Siehe: [BUILD.md](atoms/BUILD.md)

### Logging-System

```
partdata/logs/
├── kernel/         → agentos.log
├── services/       → llm_d.log, tool_d.log, etc.
└── apps/           → Eigene Logs pro App
```

- Lesbares Format: `%(asctime)s.%(msecs)03d [%(levelname)s] [%(name)s] %(message)s`
- JSON-Format: Für ELK/Splunk-Integration
- Sprachübergreifende Korrelation via `trace_id`
- OpenTelemetry-Integration

Siehe: [Logging Architekturdokumentation](partdocs/architecture/logging_system.md)

### Tests

```bash
ctest -R unit --output-on-failure
ctest -R integration --output-on-failure
python scripts/benchmark.py
```

---

## 📊 Leistungsmetriken

Testumgebung: Intel i7-12700K, 32GB RAM, NVMe SSD

### Verarbeitungskapazität

| Metrik | Wert | Bedingungen |
| :--- | :--- | :--- |
| **Speicher-Schreibdurchsatz** | 10.000+ Einträge/Sek. | L1, asynchron |
| **Vektorsuche-Latenz** | < 10ms | FAISS IVF1024,PQ64, k=10 |
| **Hybride Suche-Latenz** | < 50ms | Vektor+BM25, Top-100 |
| **Speicherabstraktion** | 100 Einträge/Sek. | L2→L3 |
| **Mustersuche** | 100k Einträge/Min. | L4 |
| **Gleichzeitige Verbindungen** | 1024 | IPC Binder |
| **Aufgabenplanung-Latenz** | < 1ms | Gewichteter Round-Robin |
| **Intent-Analyse-Latenz** | < 50ms | Einfacher Intent |
| **Aufgabenplanung** | 100+ Knoten/Sek. | DAG |
| **Agent-Scheduling-Latenz** | < 5ms | Gewichteter Round-Robin |
| **Aufgabenausführung** | 1000+ Aufgaben/Sek. | Parallel |

### Ressourcennutzung

| Szenario | CPU | Speicher | Festplatten-IO |
| :--- | :--- | :--- | :--- |
| **Leerlauf** | < 5% | 200MB | < 1MB/Sek. |
| **Mittlere Last** | 30-50% | 1-2GB | 10-50MB/Sek. |
| **Hohe Last** | 80-100% | 4-8GB | 100-500MB/Sek. |

### Skalierbarkeit

- **Horizontal**: Multi-Node-Bereitstellung (geplant)
- **Vertikal**: Konfigurierbare Grenzen und Allokation
- **Elastisch**: Automatische Anpassung nach Last (geplant)

Hinweis: Details in [scripts/benchmark.py](scripts/benchmark.py)

---

## 📚 Dokumentation

### Kerndokumentation

- [📘 CoreLoopThree Architektur](partdocs/architecture/coreloopthree.md)
- [💾 MemoryRovol Architektur](partdocs/architecture/memoryrovol.md)
- [🔧 IPC Mechanismus](partdocs/architecture/ipc.md)
- [⚙️ Mikrokern Design](partdocs/architecture/microkernel.md)
- [📞 Systemaufrufe](partdocs/architecture/syscall.md)
- [📝 Logging System](partdocs/architecture/logging_system.md)

### Entwicklungsleitfäden

- [🚀 Schnellstart](partdocs/guides/getting_started.md)
- [🤖 Agent Erstellen](partdocs/guides/create_agent.md)
- [🛠️ Fähigkeit Erstellen](partdocs/guides/create_skill.md)
- [📦 Bereitstellung](partdocs/guides/deployment.md)
- [🎛️ Kernel Optimierung](partdocs/guides/kernel_tuning.md)
- [🔍 Fehlerbehebung](partdocs/guides/troubleshooting.md)

### Technische Spezifikationen

- [📋 Kodierungsstandards](partdocs/specifications/coding_standards.md)
- [🧪 Teststandards](partdocs/specifications/testing.md)
- [🔒 Sicherheitsstandards](partdocs/specifications/security.md)
- [📊 Leistungsmetriken](partdocs/specifications/performance.md)

### Externe Dokumentation

- [🏭 Workshop](../Workshop/README.md)
- [🔬 Deepness](../Deepness/README.md)
- [📊 Benchmark](../Benchmark/metrics/README.md)

---

## 🔄 Versions-Roadmap

### Aktuelle Version (v1.0.0.5) - Produktionsreif

**Fortschritt**: 85%

- ✅ Kernarchitektur abgeschlossen
- ✅ MemoryRovol implementiert (L1-L4)
- ✅ CoreLoopThree Drei-Schichten-Runtime
- ✅ Mikrokern (core)
- ✅ Systemaufrufe (syscall) 100%
- ✅ Einheitliches Logging-System
- 🔲 Vollständige Integrationstests

### Kurzfristig (2026 Q2-Q3)

**v1.0.0.4 - Verbesserung & Optimierung**
- CoreLoopThree Ausnahmebehandlung
- Attraktorennetzwerk-Performance
- LRU-Cache-Trefferquote
- Speicherevolutionsalgorithmen
- Zusätzliche Ausführungseinheiten

**v1.0.1.0 - Leistung**
- Vektorsuchoptimierung
- Speicherabstraktionsalgorithmen
- Systemlatenzreduktion

**v1.0.2.0 - Entwickler-Tools**
- SDK-Verbesserung (Go/Python/Rust/TS)
- Debugging-Tools
- Dokumentation und Beispiele

### Mittelfristig (2026 Q4-2027)

**v1.0.3.0 - Produktion**
- Vollständige End-to-End-Tests
- Performance-Benchmarks
- Sicherheitsaudit
- Produktionsbereitstellung validiert

**v1.0.4.0 - Verteilung**
- Multi-Node-Cluster
- Verteilter Speicher
- Knotenübergreifendes Scheduling

**v1.0.5.0 - Intelligenz**
- Adaptive Speicherverwaltung
- Verstärkungslernen
- Autonome Evolution

### Langfristige Vision (2027+)

- 🌐 De-facto-Standard für Agenten-Betriebssysteme
- 🤝 Globales Open-Source-Ökosystem
- 🏆 Führung nächste Generation AGI
- 📈 Billionen Speicherkapazität, Millisekunden-Abruf

---

## 🤝 Ökosystem-Zusammenarbeit

### Technologiepartner
- **KI-Labore**: Experten für große Modelle, Speicher, kognitive Architekturen
- **Hardware-Anbieter**: GPU, NPU, Speicheranbieter
- **Anwendungsunternehmen**: Robotik, intelligente Assistenten, Automatisierung

### Gemeinschaftsbeiträge
- **Code**: Entwicklung und Optimierung von Kernfunktionen
- **Dokumentation**: Benutzerhandbücher und technische Dokumentation
- **Tests**: Funktionstests und Leistungsbewertung
- **Ökosystem**: Gemeinschaftsbetrieb und Wissensaustausch

---

## 📞 Technischer Support

### Gemeinschaftssupport
- **Gitee Issues**: [Offizieller Issue-Tracker](https://gitee.com/spharx/agentos/issues) (bevorzugt)
- **GitHub Issues**: [Spiegel-Issue-Tracker](https://github.com/SpharxTeam/AgentOS/issues)
- **Discussions**: [GitHub Discussions](https://github.com/SpharxTeam/AgentOS/discussions)
- **Dokumentation**: [Online-Dokumentation](https://docs.spharx.cn/agentos)

### Kommerzieller Support
- **Enterprise Edition**: Kommerzielle Lizenz und technischer Support
- **Individuelle Entwicklung**: Angepasste Module
- **Schulungen**: Schulung zu Verwendung und Entwicklung

Lizenzanfragen:
- E-Mail: lidecheng@spharx.cn, wangliren@spharx.cn
- Website: https://spharx.cn

---

## 📄 Lizenz

AgentOS verwendet eine **geschichtete Open-Source-Lizenzarchitektur**, kompatibel mit kommerzieller Nutzung und offenem Ökosystem.

### Hauptlizenz
Kernel-Code standardmäßig unter **Apache License 2.0**. Vollständiger Text in [LICENSE](../../LICENSE).

### Geschichtete Lizenzdetails
| Modulverzeichnis | Anwendbare Lizenz | Beschreibung |
|----------|----------|----------|
| `atoms/` (Kernel) | Apache License 2.0 | CoreLoopThree, MemoryRovol, Runtime, Sicherheitsisolierung |
| `domes/` (Erweiterungen) | Apache License 2.0 | Erweiterungen der Kernarchitektur |
| `openhub/` (Ökosystem) | MIT License | Agenten-/Fähigkeiten-Marktplatz, Gemeinschaftsbeiträge |
| Drittabhängigkeiten | Originallizenzen | Alle Drittabhängigkeiten unter erlaubnisreichen Lizenzen |

### Sie Können Frei
- ✅ **Kommerzielle Nutzung**: Geschlossene kommerzielle Produkte, Unternehmensprojekte, kommerzielle Dienste
- ✅ **Ändern**: Ändern, Anpassen, abgeleitete Werke ohne Open-Source-Pflicht für Geschäftscode
- ✅ **Verteilen**: Verteilen und Kopieren von Quellcode oder kompilierten Binärdateien
- ✅ **Patentnutzung**: Permanente Patentlizenz für Kerncode
- ✅ **Private Nutzung**: Private Projekte ohne Offenlegungspflicht

### Ihre Einzigen Verpflichtungen
- Ursprüngliche Copyright-Vermerke, Lizenztext und NOTICE-Datei erhalten
- Änderungsprotokolle für geänderte Kernel-Dateien beifügen

### Kommerzielle Dienste
- Keine kommerziellen Nutzungsbeschränkungen unter dieser Open-Source-Lizenz
- Enterprise-Support, individuelle Entwicklung, private Bereitstellung verfügbar

---

## 🙏 Danksagung

Danke an alle Entwickler, die zur Open-Source-Gemeinschaft beitragen, und Partner, die AgentOS unterstützen.

Besonderer Dank an:
- FAISS-Team (Facebook AI Research)
- Sentence Transformers-Team
- Rust- und Go-Sprachgemeinschaften
- Alle Mitwirkenden und Benutzer

---

<div align="center">

<h4>"Aus Daten entsteht Intelligenz"</h4>

---

#### 📞 Kontakt

📧 E-Mail: lidecheng@spharx.cn; wangliren@spharx.cn

<p>
  <a href="https://gitee.com/spharx/agentos">Gitee (Offizielles Repository)</a> ·
  <a href="https://github.com/SpharxTeam/AgentOS">GitHub (Spiegel-Repository)</a> ·
  <a href="https://spharx.cn">Offizielle Website</a> ·
  <a href="mailto:lidecheng@spharx.cn">Technischer Support</a>
</p>

© 2026 SPHARX Ltd. Alle Rechte Vorbehalten.

</div>
