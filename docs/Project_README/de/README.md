Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# Agent OS

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Mirror](https://img.shields.io/badge/mirror-GitHub-lightgrey.svg)](https://github.com/SpharxTeam/AgentOS)

---

**SuperAI-Betriebssystem**

*"Aus Daten entsteht Intelligenz."*

---

馃摉 **[绠€浣撲腑鏂嘳(../../README.md)** | [English](../en/README.md) | [Fran莽ais](../fr/README.md) | 馃嚛馃嚜 **Deutsch**

</div>

---

## Einf眉hrung

- Entwickelt f眉r Aufgabenausf眉hrung mit maximaler Token-Effizienz
<!-- From data intelligence emerges. by spharx -->
- Neuartige Architektur mit 2-3脳 besserer Token-Nutzung als Branchenstandards
- 3-5脳 effizienter als OpenClaw bei Engineering-Aufgaben, ~60% Token-Einsparung

## 馃搵 脺bersicht

- **Agent OS (SuperAI OS)** ist der intelligente Agenten-Betriebssystemkern von SpharxWorks mit kompletter Runtime-Umgebung, Speichersystem, kognitiver Engine und Ausf眉hrungs-Framework.
- Als physische Welt-Dateninfrastruktur implementiert AgentOS einen geschlossenen Kreislauf von der Datenverarbeitung bis zur intelligenten Entscheidungsfindung.

### Kernwerte

- **Mikrokern**: Minimalistische Kernel-Konstruktion, alle Services im User-Space
- **Drei-Schichten-Architektur**: Kognition, Ausf眉hrung und Speicher f眉r Agent-Lifecycle-Management
- **Memory-Roll-System**: L1-L4 progressive Abstraktion mit Speicherung, Abruf, Evolution und Vergessen
- **Systemaufrufe**: Stabile und sichere Schnittstellen mit gekapselter Kernel-Implementierung
- **Steckbare Strategien**: Dynamisches Laden und Runtime-Austausch von Algorithmen
- **Einheitliches Logging**: Sprach眉bergreifende Schnittstelle mit vollst盲ndiger Verfolgung und OpenTelemetry
- **Multi-Sprache-SDK**: Native Unterst眉tzung f眉r Go, Python, Rust und TypeScript mit FFI-Schnittstellen

### Versionsstatus

**Aktuelle Version**: v1.0.0.6 (Produktionsreif)

- 鉁?Kernarchitektur-Design abgeschlossen
- 鉁?MemoryRovol-Speichersystem
  - L1-L4 Vier-Schichten-Architektur vollst盲ndig implementiert
  - Synchrone/Asynchrone Schreibunterst眉tzung (10.000+ Eintr盲ge/Sek.)
  - FAISS-Vektorsuche (IVF/HNSW-Indexierung)
  - Attraktorennetzwerk-Abrufmechanismus
  - Ebbinghaus-Vergessenskurve
  - LRU-Cache und Vektorpersistenz
- 鉁?CoreLoopThree Drei-Schichten-Architektur
  - Kognitionsschicht: Intent-Verst盲ndnis, Aufgabenplanung, Koordination (90%)
  - Ausf眉hrungsschicht: Ausf眉hrungsmaschine, Kompensationstransaktionen, Verfolgung (85%)
  - Speicherschicht: MemoryRovol FFI-Wrapper (80%)
- 鉁?Mikrokern-Basismodul (core)
  - IPC Binder-Kommunikation
  - Speicherverwaltung (RAII, Smart Pointer)
  - Aufgabenplanung (gewichteter Round-Robin)
  - Hochpr盲ziser Zeitdienst
- 鉁?Systemaufrufschicht (syscall) - 100% abgeschlossen
  - 鉁?Aufgaben-Syscalls: `sys_task_submit/query/wait/cancel`
  - 鉁?Speicher-Syscalls: `sys_memory_write/search/get/delete`
  - 鉁?Sitzungs-Syscalls: `sys_session_create/get/close/list`
  - 鉁?Observability-Syscalls: `sys_telemetry_metrics/traces`
  - 鉁?Einheitlicher Einstiegspunkt: `agentos_syscall_invoke()`
- 馃敳 Vollst盲ndige End-to-End-Integrationstests

---

## 馃彈锔?Systemarchitektur

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?                   AgentOS Gesamtarchitektur                 鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?                                                            鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?          Anwendungsschicht (openlab)                 鈹? 鈹?鈹? 鈹? docgen | ecommerce | research | videoedit | ...      鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                          鈫?                                鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?       Kern-Dienstschicht (daemon)                     鈹? 鈹?鈹? 鈹? llm_d | market_d | monit_d | perm_d | sched_d | ...  鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                          鈫?                                鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?         Kernelschicht (atoms)                        鈹? 鈹?鈹? 鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹? 鈹?鈹? 鈹? 鈹?  core       鈹? 鈹俢oreloopthree 鈹? 鈹俶emoryrovol  鈹? 鈹? 鈹?鈹? 鈹? 鈹?Mikrokern    鈹? 鈹?-Schichten-Rt 鈹?鈹?-Schicht-Sp. 鈹? 鈹? 鈹?鈹? 鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹? 鈹?鈹? 鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                                    鈹? 鈹?鈹? 鈹? 鈹?  syscall    鈹?                                    鈹? 鈹?鈹? 鈹? 鈹?System Calls 鈹?                                    鈹? 鈹?鈹? 鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                                    鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                          鈫?                                鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?           SDK-Schicht (toolkit)                        鈹? 鈹?鈹? 鈹? Go | Python | Rust | TypeScript | ...                鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                                                            鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

---

## 馃 CoreLoopThree: Drei-Schichten-Architektur

### Designphilosophie

CoreLoopThree teilt die Agent-Runtime in drei orthogonale und synergistische Schichten:

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?         Kognitionsschicht                 
   鈥?Intent-Verst盲ndnis 鈥?Aufgabenplanung 
   鈥?Agent-Planung 鈥?Modell-Koordination  
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫撯攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?         Ausf眉hrungsschicht                
    鈥?Aufgabenausf眉hrung 鈥?Kompensation   
    鈥?Kettenverfolgung 鈥?Zustandsmanagement
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫撯攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?          Speicherschicht                  
    鈥?Speicherschreiben 鈥?-abruf          
    鈥?Kontext-Montage 鈥?Evolution & Vergessen
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

### Kernkomponenten

#### 1. Kognitionsschicht
- **Intent-Verst盲ndnis-Engine**: Analyse Benutzereingabe, Identifikation echter Absichten
- **Aufgabenplaner**: Automatische Zerlegung in DAG-Aufgabengraph
- **Agent-Scheduler**: Multi-Agenten-Koordination und Ressourcenallokation
- **Modell-Koordinator**: LLM-Auswahl und Prompt-Engineering
- **Strategie-Schnittstellen**: Steckbare Algorithmen (Planung, Koordination, Scheduling)

#### 2. Ausf眉hrungsschicht
- **Ausf眉hrungsmaschine**: Aufgabenausf眉hrung und Zustandsverfolgung
  - Zustandsmaschine (Pending/Running/Succeeded/Failed/Cancelled/Retrying)
  - Nebenl盲ufigkeitskontrolle und Timeout-Management
- **Kompensationstransaktionen**: Rollback bei Fehlern und Kompensationslogik
- **Verantwortungsketten-Verfolgung**: Vollst盲ndige Aufzeichnung der Ausf眉hrungskette
- **Ausf眉hrungseinheiten-Register**: Registrierung atomarer Ausf眉hrungseinheiten
- **Ausnahmebehandlung**: Hierarchische Erfassung und Wiederherstellung

#### 3. Speicherschicht
- **Speicherdienst**: Kapselung von MemoryRovol
  - Speicher-Engine (`agentos_memory_engine_t`)
  - Rekordtypen (RAW/FEATURE/STRUCTURE/PATTERN)
- **Schreibschnittstelle**: Synchrone/asynchrone Unterst眉tzung
- **Abfrageschnittstelle**: Semantische Suche und Vektorabruf
- **Kontext-Montage**: Automatische Speicherzuordnung
- **FFI-Schnittstelle**: `rov_ffi.h` f眉r sprach眉bergreifende Aufrufe

Siehe: [CoreLoopThree Architekturdokumentation](agentos/docs/architecture/coreloopthree.md)

---

## 馃捑 MemoryRovol: Speichersystem

### Positionierung

MemoryRovol ist das Kernel-Level-Speichersystem von AgentOS f眉r umfassendes Speichermanagement von Rohdaten bis zu fortgeschrittenen Mustern.

### Vier-Schichten-Architektur

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?       L4 Musterschicht                    
   鈥?Persistente Homologie 鈥?Stabile Muster
   鈥?HDBSCAN-Clustering 鈥?Regelgenerierung 
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫戔攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?Abstrakte Evolution
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?      L3 Strukturschicht                   
   鈥?Bindeoperatoren 鈥?Relationskodierung  
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫戔攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?Merkmalsextraktion
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?      L2 Merkmals schicht                  
   鈥?Embedding-Modelle (OpenAI/DeepSeek)  
   鈥?FAISS-Vektorindex 鈥?Hybride Suche     
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫戔攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?Datenkompression
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?       L1 Rohschicht                       
   鈥?Dateisystem-Speicher 鈥?Fragmentierung
   鈥?Metadaten-Index 鈥?Integrit盲tspr眉fung  
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

### Hauptfunktionen

#### 1. Speicherschreibung
- **Synchrones Schreiben**: Blockierend f眉r Datenpersistenz
- **Asynchrones Schreiben**: Batch-Schreiben f眉r hohen Durchsatz (10.000+/Sek.)
- **Transaktionsunterst眉tzung**: ACID-Semantik
- **Komprimierungsarchivierung**: Automatische Komprimierung seltener Speicher

#### 2. Speicherabruf
- **Vektorsuche**: Kosinus-脛hnlichkeit 眉ber FAISS
  - Latenz < 10ms (k=10)
- **Semantische Suche**: Nat眉rlichsprachliche Abfragen
- **Kontextsensitiv**: Automatische Filterung (Zeit, Quelle, TraceID)
- **LRU-Cache**: Cache f眉r hei脽e Vektoren
- **Neubewertung**: Cross-Encoder f眉r Relevanz

#### 3. Speicherevolution
- **Progressive Abstraktion**: L1鈫扡2鈫扡3鈫扡4
  - Merkmalsextraktion
  - Strukturbindung
  - Mustersuche
- **Mustererkennung**: Identifikation h盲ufiger Muster
- **Gewichtsaktualisierungen**: Dynamisch nach Zugriffsh盲ufigkeit
- **Evaluierung**: Komitee mit Kognitionsschicht

#### 4. Speichervergessen
- **Ebbinghaus-Kurve**: Intelligentes Beschneiden nach Vergessenskurve
- **Linearer Zerfall**: Einfacher linearer Gewichtszerfall
- **Zugriffsz盲hler**: LRU/LFU-Strategie
- **Aktives Vergessen**: Durch Kognitionsschicht ausgel枚st

Siehe: [MemoryRovol Architekturdokumentation](agentos/docs/architecture/memoryrovol.md)

---

## 馃洜锔?Entwicklungsleitfaden

### Voraussetzungen

- **Betriebssystem**: Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2)
- **Compiler**: GCC 11+ oder Clang 14+
- **Build-Tools**: CMake 3.20+, Ninja oder Make
- **Abh盲ngigkeiten**:
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

Siehe: [BUILD.md](agentos/atoms/BUILD.md)

### Logging-System

```
agentos/heapstore/logs/
鈹溾攢鈹€ kernel/         鈫?agentos.log
鈹溾攢鈹€ services/       鈫?llm_d.log, tool_d.log, etc.
鈹斺攢鈹€ apps/           鈫?Eigene Logs pro App
```

- Lesbares Format: `%(asctime)s.%(msecs)03d [%(levelname)s] [%(name)s] %(message)s`
- JSON-Format: F眉r ELK/Splunk-Integration
- Sprach眉bergreifende Korrelation via `trace_id`
- OpenTelemetry-Integration

Siehe: [Logging Architekturdokumentation](agentos/docs/architecture/logging_system.md)

### Tests

```bash
ctest -R unit --output-on-failure
ctest -R integration --output-on-failure
python scripts/benchmark.py
```

---

## 馃搳 Leistungsmetriken

Testumgebung: Intel i7-12700K, 32GB RAM, NVMe SSD

### Verarbeitungskapazit盲t

| Metrik | Wert | Bedingungen |
| :--- | :--- | :--- |
| **Speicher-Schreibdurchsatz** | 10.000+ Eintr盲ge/Sek. | L1, asynchron |
| **Vektorsuche-Latenz** | < 10ms | FAISS IVF1024,PQ64, k=10 |
| **Hybride Suche-Latenz** | < 50ms | Vektor+BM25, Top-100 |
| **Speicherabstraktion** | 100 Eintr盲ge/Sek. | L2鈫扡3 |
| **Mustersuche** | 100k Eintr盲ge/Min. | L4 |
| **Gleichzeitige Verbindungen** | 1024 | IPC Binder |
| **Aufgabenplanung-Latenz** | < 1ms | Gewichteter Round-Robin |
| **Intent-Analyse-Latenz** | < 50ms | Einfacher Intent |
| **Aufgabenplanung** | 100+ Knoten/Sek. | DAG |
| **Agent-Scheduling-Latenz** | < 5ms | Gewichteter Round-Robin |
| **Aufgabenausf眉hrung** | 1000+ Aufgaben/Sek. | Parallel |

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

## 馃摎 Dokumentation

### Kerndokumentation

- [馃摌 CoreLoopThree Architektur](agentos/docs/architecture/coreloopthree.md)
- [馃捑 MemoryRovol Architektur](agentos/docs/architecture/memoryrovol.md)
- [馃敡 IPC Mechanismus](agentos/docs/architecture/ipc.md)
- [鈿欙笍 Mikrokern Design](agentos/docs/architecture/microkernel.md)
- [馃摓 Systemaufrufe](agentos/docs/architecture/syscall.md)
- [馃摑 Logging System](agentos/docs/architecture/logging_system.md)

### Entwicklungsleitf盲den

- [馃殌 Schnellstart](agentos/docs/guides/getting_started.md)
- [馃 Agent Erstellen](agentos/docs/guides/create_agent.md)
- [馃洜锔?F盲higkeit Erstellen](agentos/docs/guides/create_skill.md)
- [馃摝 Bereitstellung](agentos/docs/guides/deployment.md)
- [馃帥锔?Kernel Optimierung](agentos/docs/guides/kernel_tuning.md)
- [馃攳 Fehlerbehebung](agentos/docs/guides/troubleshooting.md)

### Technische Spezifikationen

- [馃搵 Kodierungsstandards](agentos/docs/specifications/coding_standards.md)
- [馃И Teststandards](agentos/docs/specifications/testing.md)
- [馃敀 Sicherheitsstandards](agentos/docs/specifications/security.md)
- [馃搳 Leistungsmetriken](agentos/docs/specifications/performance.md)

### Externe Dokumentation

- [馃彮 Workshop](../Workshop/README.md)
- [馃敩 Deepness](../Deepness/README.md)
- [馃搳 Benchmark](../Benchmark/metrics/README.md)

---

## 馃攧 Versions-Roadmap

### Aktuelle Version (v1.0.0.6) - Produktionsreif

**Fortschritt**: 85%

- 鉁?Kernarchitektur abgeschlossen
- 鉁?MemoryRovol implementiert (L1-L4)
- 鉁?CoreLoopThree Drei-Schichten-Runtime
- 鉁?Mikrokern (core)
- 鉁?Systemaufrufe (syscall) 100%
- 鉁?Einheitliches Logging-System
- 馃敳 Vollst盲ndige Integrationstests

### Kurzfristig (2026 Q2-Q3)

**v1.0.0.4 - Verbesserung & Optimierung**
- CoreLoopThree Ausnahmebehandlung
- Attraktorennetzwerk-Performance
- LRU-Cache-Trefferquote
- Speicherevolutionsalgorithmen
- Zus盲tzliche Ausf眉hrungseinheiten

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
- Vollst盲ndige End-to-End-Tests
- Performance-Benchmarks
- Sicherheitsaudit
- Produktionsbereitstellung validiert

**v1.0.4.0 - Verteilung**
- Multi-Node-Cluster
- Verteilter Speicher
- Knoten眉bergreifendes Scheduling

**v1.0.5.0 - Intelligenz**
- Adaptive Speicherverwaltung
- Verst盲rkungslernen
- Autonome Evolution

### Langfristige Vision (2027+)

- 馃寪 De-facto-Standard f眉r Agenten-Betriebssysteme
- 馃 Globales Open-Source-脰kosystem
- 馃弳 F眉hrung n盲chste Generation AGI
- 馃搱 Billionen Speicherkapazit盲t, Millisekunden-Abruf

---

## 馃 脰kosystem-Zusammenarbeit

### Technologiepartner
- **KI-Labore**: Experten f眉r gro脽e Modelle, Speicher, kognitive Architekturen
- **Hardware-Anbieter**: GPU, NPU, Speicheranbieter
- **Anwendungsunternehmen**: Robotik, intelligente Assistenten, Automatisierung

### Gemeinschaftsbeitr盲ge
- **Code**: Entwicklung und Optimierung von Kernfunktionen
- **Dokumentation**: Benutzerhandb眉cher und technische Dokumentation
- **Tests**: Funktionstests und Leistungsbewertung
- **脰kosystem**: Gemeinschaftsbetrieb und Wissensaustausch

---

## 馃摓 Technischer Support

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

## 馃搫 Lizenz

AgentOS verwendet eine **geschichtete Open-Source-Lizenzarchitektur**, kompatibel mit kommerzieller Nutzung und offenem 脰kosystem.

### Hauptlizenz
Kernel-Code standardm盲脽ig unter **Apache License 2.0**. Vollst盲ndiger Text in [LICENSE](../../LICENSE).

### Geschichtete Lizenzdetails
| Modulverzeichnis | Anwendbare Lizenz | Beschreibung |
|----------|----------|----------|
| `agentos/atoms/` (Kernel) | Apache License 2.0 | CoreLoopThree, MemoryRovol, Runtime, Sicherheitsisolierung |
| `agentos/cupolas/` (Erweiterungen) | Apache License 2.0 | Erweiterungen der Kernarchitektur |
| `openlab/` (脰kosystem) | MIT License | Agenten-/F盲higkeiten-Marktplatz, Gemeinschaftsbeitr盲ge |
| Drittabh盲ngigkeiten | Originallizenzen | Alle Drittabh盲ngigkeiten unter erlaubnisreichen Lizenzen |

### Sie K枚nnen Frei
- 鉁?**Kommerzielle Nutzung**: Geschlossene kommerzielle Produkte, Unternehmensprojekte, kommerzielle Dienste
- 鉁?**脛ndern**: 脛ndern, Anpassen, abgeleitete Werke ohne Open-Source-Pflicht f眉r Gesch盲ftscode
- 鉁?**Verteilen**: Verteilen und Kopieren von Quellcode oder kompilierten Bin盲rdateien
- 鉁?**Patentnutzung**: Permanente Patentlizenz f眉r Kerncode
- 鉁?**Private Nutzung**: Private Projekte ohne Offenlegungspflicht

### Ihre Einzigen Verpflichtungen
- Urspr眉ngliche Copyright-Vermerke, Lizenztext und NOTICE-Datei erhalten
- 脛nderungsprotokolle f眉r ge盲nderte Kernel-Dateien beif眉gen

### Kommerzielle Dienste
- Keine kommerziellen Nutzungsbeschr盲nkungen unter dieser Open-Source-Lizenz
- Enterprise-Support, individuelle Entwicklung, private Bereitstellung verf眉gbar

---

## 馃檹 Danksagung

Danke an alle Entwickler, die zur Open-Source-Gemeinschaft beitragen, und Partner, die AgentOS unterst眉tzen.

Besonderer Dank an:
- FAISS-Team (Facebook AI Research)
- Sentence Transformers-Team
- Rust- und Go-Sprachgemeinschaften
- Alle Mitwirkenden und Benutzer

---

<div align="center">

<h4>"Aus Daten entsteht Intelligenz"</h4>

---

#### 馃摓 Kontakt

馃摟 E-Mail: lidecheng@spharx.cn; wangliren@spharx.cn

<p>
  <a href="https://gitee.com/spharx/agentos">Gitee (Offizielles Repository)</a> 路
  <a href="https://github.com/SpharxTeam/AgentOS">GitHub (Spiegel-Repository)</a> 路
  <a href="https://spharx.cn">Offizielle Website</a> 路
  <a href="mailto:lidecheng@spharx.cn">Technischer Support</a>
</p>

漏 2026 SPHARX Ltd. Alle Rechte Vorbehalten.

</div>
