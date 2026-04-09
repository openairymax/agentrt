Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# Agent OS

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Mirror](https://img.shields.io/badge/mirror-GitHub-lightgrey.svg)](https://github.com/SpharxTeam/AgentOS)

---

**Syst猫me d'Exploitation SuperIA**

*"De la donn茅e 茅merge l'intelligence."*

---

馃摉 **[绠€浣撲腑鏂嘳(../../README.md)** | [English](../en/README.md) | 馃嚝馃嚪 **Fran莽ais** | [Deutsch](../de/README.md)

</div>

---

## Introduction

- Con莽u pour l'ex茅cution de t芒ches avec une efficacit茅 maximale des tokens
<!-- From data intelligence emerges. by spharx -->
- Nouvelle architecture offrant 2 脿 3 fois plus d'efficacit茅 que les frameworks standards
- 3 脿 5 fois plus efficace qu'OpenClaw pour les t芒ches d'ing茅nierie, 茅conomisant ~60% de tokens

## 馃搵 Aper莽u

- **Agent OS (SuperAI OS)** est le noyau du syst猫me d'exploitation d'agents intelligents de SpharxWorks, fournissant un environnement d'ex茅cution complet, un syst猫me de m茅moire, un moteur cognitif et un framework d'ex茅cution.
- En tant qu'infrastructure de donn茅es pour le monde physique, AgentOS impl茅mente une boucle ferm茅e compl猫te du traitement des donn茅es 脿 la prise de d茅cision intelligente.

### Valeurs Cl茅s

- **Micro-noyau**: Conception minimaliste avec tous les services en espace utilisateur
- **Architecture Trois Couches**: Cognition, Ex茅cution et M茅moire pour la gestion du cycle de vie des agents
- **Syst猫me de M茅moire Roll**: Abstraction progressive L1-L4 avec stockage, r茅cup茅ration, 茅volution et oubli
- **Appels Syst猫me**: Interfaces stables et s茅curis茅es masquant les d茅tails du noyau
- **Strat茅gies Enfichables**: Chargement dynamique et remplacement 脿 l'ex茅cution des algorithmes
- **Journalisation Unifi茅e**: Interface multi-langages avec tra莽abilit茅 compl猫te et OpenTelemetry
- **SDK Multi-langages**: Support natif Go, Python, Rust et TypeScript avec interfaces FFI

### 脡tat de la Version

**Version Actuelle**: v1.0.0.6 (Pr锚t pour la Production)

- 鉁?Conception de l'architecture principale termin茅e
- 鉁?Syst猫me de M茅moire MemoryRovol
  - Architecture quatre couches L1-L4 enti猫rement impl茅ment茅e
  - 脡criture synchrone/asynchrone (10 000+ entr茅es/sec)
  - Recherche vectorielle FAISS (indexation IVF/HNSW)
  - M茅canisme de r茅cup茅ration par r茅seau attracteur
  - Courbe d'oubli d'Ebbinghaus
  - Cache LRU et persistance des vecteurs
- 鉁?Architecture CoreLoopThree
  - Couche Cognition: Compr茅hension intention, planification, coordination (90%)
  - Couche Ex茅cution: Moteur d'ex茅cution, transactions compensation, tra莽age (85%)
  - Couche M茅moire: Interface FFI MemoryRovol (80%)
- 鉁?Module de base Micro-noyau (core)
  - Communication IPC Binder
  - Gestion m茅moire (RAII, pointeurs intelligents)
  - Ordonnancement de t芒ches (round-robin pond茅r茅)
  - Service temporel haute pr茅cision
- 鉁?Couche System Call (syscall) - 100% termin茅e
  - 鉁?Appels t芒che: `sys_task_submit/query/wait/cancel`
  - 鉁?Appels m茅moire: `sys_memory_write/search/get/delete`
  - 鉁?Appels session: `sys_session_create/get/close/list`
  - 鉁?Appels observabilit茅: `sys_telemetry_metrics/traces`
  - 鉁?Point d'entr茅e unique: `agentos_syscall_invoke()`
- 馃敳 Tests d'int茅gration complets

---

## 馃彈锔?Architecture Syst猫me

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?                   Architecture Globale AgentOS              鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?                                                            鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?          Couche Application (openlab)                鈹? 鈹?鈹? 鈹? docgen | ecommerce | research | videoedit | ...      鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                          鈫?                                鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?       Couche Services Principaux (daemon)             鈹? 鈹?鈹? 鈹? llm_d | market_d | monit_d | perm_d | sched_d | ...  鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                          鈫?                                鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?         Couche Noyau (atoms)                         鈹? 鈹?鈹? 鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹? 鈹?鈹? 鈹? 鈹?  core       鈹? 鈹俢oreloopthree 鈹? 鈹俶emoryrovol  鈹? 鈹? 鈹?鈹? 鈹? 鈹?Micro-noyau  鈹? 鈹俁untime 3-Couches鈹?鈹侻茅moire 4-N  鈹? 鈹? 鈹?鈹? 鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹? 鈹?鈹? 鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                                    鈹? 鈹?鈹? 鈹? 鈹?  syscall    鈹?                                    鈹? 鈹?鈹? 鈹? 鈹?Appels Syst猫me鈹?                                   鈹? 鈹?鈹? 鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                                    鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                          鈫?                                鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?           Couche SDK (toolkit)                         鈹? 鈹?鈹? 鈹? Go | Python | Rust | TypeScript | ...                鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                                                            鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

---

## 馃 CoreLoopThree: Architecture Trois Couches

### Philosophie de Conception

CoreLoopThree divise le runtime des agents en trois couches orthogonales et synergiques:

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?         Couche Cognition                  
   鈥?Compr茅hension Intention 鈥?Planification
   鈥?Ordonnancement Agents 鈥?Coordination  
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫撯攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?         Couche Ex茅cution                 
    鈥?Ex茅cution T芒ches 鈥?Compensation     
    鈥?Tra莽age Cha卯ne 鈥?脡tats              
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫撯攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?          Couche M茅moire                  
    鈥?脡criture 鈥?R茅cup茅ration             
    鈥?Contexte 鈥?脡volution & Oubli        
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

### Composants Principaux

#### 1. Couche Cognition
- **Moteur de Compr茅hension**: Analyse entr茅e utilisateur, identification intention
- **Planificateur de T芒ches**: D茅composition automatique en graphe DAG
- **Ordonnanceur d'Agents**: Coordination multi-agents et allocation ressources
- **Coordinateur de Mod猫les**: S茅lection LLM et ing茅nierie de prompts
- **Interfaces Strat茅gies**: Algorithmes enfichables (planification, coordination, ordonnancement)

#### 2. Couche Ex茅cution
- **Moteur d'Ex茅cution**: Ex茅cution t芒ches et suivi 茅tats
  - Machine 茅tats (Pending/Running/Succeeded/Failed/Cancelled/Retrying)
  - Contr么le concurrence et timeouts
- **Transactions Compensation**: Rollback 茅chec et logique compensation
- **Tra莽age Cha卯ne Responsabilit茅**: Enregistrement complet cha卯ne ex茅cution
- **Registre Unit茅s Ex茅cution**: Enregistrement unit茅s atomiques
- **Gestion Exceptions**: Capture hi茅rarchique et r茅cup茅ration

#### 3. Couche M茅moire
- **Service M茅moire**: Encapsulation MemoryRovol
  - Moteur m茅moire (`agentos_memory_engine_t`)
  - Types enregistrements (RAW/FEATURE/STRUCTURE/PATTERN)
- **Interface 脡criture**: Support synchrone/asynchrone
- **Interface Requ锚te**: Recherche s茅mantique et vectorielle
- **Montage Contextuel**: Association automatique m茅moire
- **Interface FFI**: `rov_ffi.h` pour appels multi-langages

Voir: [Documentation CoreLoopThree](agentos/docs/architecture/coreloopthree.md)

---

## 馃捑 MemoryRovol: Syst猫me de M茅moire

### Positionnement

MemoryRovol est le syst猫me de m茅moire au niveau du noyau, g茅rant la m茅moire compl猫te de la donn茅e brute aux motifs avanc茅s.

### Architecture Quatre Couches

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?       L4 Couche Motifs                    
   鈥?Homologie Persistante 鈥?Motifs Stables
   鈥?Clustering HDBSCAN 鈥?R猫gles           
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫戔攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?脡volution Abstraite
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?      L3 Couche Structure                  
   鈥?Op茅rateurs Liaison 鈥?Encodage Relations
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫戔攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?Extraction Caract茅ristiques
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?      L2 Couche Caract茅ristiques           
   鈥?Mod猫les Embedding (OpenAI/DeepSeek)  
   鈥?Index Vectoriel FAISS 鈥?Recherche Hybride
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫戔攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?Compression Donn茅es
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?       L1 Couche Brute                     
   鈥?Stockage Fichier 鈥?Fragmentation      
   鈥?Index M茅tadonn茅es 鈥?Int茅grit茅         
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

### Fonctionnalit茅s Principales

#### 1. Stockage M茅moire
- **脡criture Synchrone**: Blocante assurant persistance
- **脡criture Asynchrone**: Par lots pour d茅bit 茅lev茅 (10 000+/sec)
- **Support Transaction**: S茅mantique ACID
- **Compression Archivage**: Automatique m茅moires浣庨

#### 2. R茅cup茅ration M茅moire
- **Recherche Vectorielle**: Similarit茅 cosinus via FAISS
  - Latence < 10ms (k=10)
- **Recherche S茅mantique**: Requ锚te langage naturel
- **Sensible Contexte**: Filtrage automatique (temps, source, TraceID)
- **Cache LRU**: Cache vecteurs chauds
- **Re-classement**: Cross-encoder pour pertinence

#### 3. 脡volution M茅moire
- **Abstraction Progressive**: L1鈫扡2鈫扡3鈫扡4
  - Extraction caract茅ristiques
  - Liaison structurelle
  - Fouille motifs
- **D茅couverte Motifs**: Identification motifs fr茅quents
- **Mises 脿 Jour Poids**: Dynamique selon fr茅quence acc猫s
- **脡valuation 脡volution**: Comit茅 avec couche cognition

#### 4. Oubli M茅moire
- **Courbe Ebbinghaus**: D茅coupage intelligent selon courbe oubli
- **D茅croissance Lin茅aire**: D茅gradation lin茅aire simple
- **Compteur Acc猫s**: Strat茅gie LRU/LFU
- **Oubli Actif**: D茅clench茅 par couche cognition

Voir: [Documentation MemoryRovol](agentos/docs/architecture/memoryrovol.md)

---

## 馃洜锔?Guide D茅veloppement

### Pr茅requis

- **SE**: Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2)
- **Compilateur**: GCC 11+ ou Clang 14+
- **Outils Build**: CMake 3.20+, Ninja ou Make
- **D茅pendances**:
  - OpenSSL >= 1.1.1
  - libevent
  - pthread
  - FAISS >= 1.7.0
  - SQLite3 >= 3.35
  - libcurl >= 7.68
  - cJSON >= 1.7.15
  - Ripser >= 2.3.1 (optionnel)
  - HDBSCAN >= 0.8.27 (optionnel)

### D茅marrage Rapide

#### 1. Cloner D茅p么t

```bash
git clone https://gitee.com/spharx/agentos.git
cd agentos
```

#### 2. Initialiser Configuration

```bash
cp .env.example .env
python scripts/init_config.py
```

#### 3. Compiler Projet

```bash
mkdir build && cd build
cmake ../atoms \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DENABLE_TRACING=ON

cmake --build . --parallel $(nproc)
ctest --output-on-failure
```

#### 4. Options Configuration

| Variable CMake | Description | D茅faut |
| :--- | :--- | :--- |
| `CMAKE_BUILD_TYPE` | Debug/Release/RelWithDebInfo | `Release` |
| `BUILD_TESTS` | Tests unitaires | `OFF` |
| `ENABLE_TRACING` | Tra莽age OpenTelemetry | `OFF` |
| `ENABLE_ASAN` | AddressSanitizer | `OFF` |

Voir: [BUILD.md](agentos/atoms/BUILD.md)

### Syst猫me Journalisation

```
agentos/heapstore/logs/
鈹溾攢鈹€ kernel/         鈫?agentos.log
鈹溾攢鈹€ services/       鈫?llm_d.log, tool_d.log, etc.
鈹斺攢鈹€ apps/           鈫?logs ind茅pendants
```

- Format lisible: `%(asctime)s.%(msecs)03d [%(levelname)s] [%(name)s] %(message)s`
- Format JSON: Pour int茅gration ELK/Splunk
- Corr茅lation multi-langages via `trace_id`
- Int茅gration OpenTelemetry

Voir: [Documentation Logging](agentos/docs/architecture/logging_system.md)

### Tests

```bash
ctest -R unit --output-on-failure
ctest -R integration --output-on-failure
python scripts/benchmark.py
```

---

## 馃搳 Performances

Environnement: Intel i7-12700K, 32GB RAM, NVMe SSD

### Capacit茅 Traitement

| Mesure | Valeur | Conditions |
| :--- | :--- | :--- |
| **D茅bit 脡criture M茅moire** | 10 000+ entr茅es/sec | L1, asynchrone |
| **Latence Recherche Vectorielle** | < 10ms | FAISS IVF1024,PQ64, k=10 |
| **Latence Recherche Hybride** | < 50ms | Vectoriel+BM25, top-100 |
| **Vitesse Abstraction M茅moire** | 100 entr茅es/sec | L2鈫扡3 |
| **Vitesse Fouille Motifs** | 100k entr茅es/min | L4 |
| **Connexions Simultan茅es** | 1024 | IPC Binder |
| **Latence Ordonnancement** | < 1ms | Round-robin pond茅r茅 |
| **Latence Analyse Intention** | < 50ms | Intention simple |
| **Vitesse Planification** | 100+ n艙uds/sec | DAG |
| **Latence Ordonnancement Agents** | < 5ms | Round-robin pond茅r茅 |
| **D茅bit Ex茅cution T芒ches** | 1000+ t芒ches/sec | Concurrent |

### Utilisation Ressources

| Sc茅nario | CPU | M茅moire | IO Disque |
| :--- | :--- | :--- | :--- |
| **Inactif** | < 5% | 200MB | < 1MB/s |
| **Charge Moyenne** | 30-50% | 1-2GB | 10-50MB/s |
| **Charge 脡lev茅e** | 80-100% | 4-8GB | 100-500MB/s |

### 脡volutivit茅

- **Horizontale**: D茅ploiement multi-n艙uds (pr茅vu)
- **Verticale**: Limites et allocation configurables
- **脡lastique**: Ajustement automatique selon charge (pr茅vu)

Note: D茅tails dans [scripts/benchmark.py](scripts/benchmark.py)

---

## 馃摎 Documentation

### Documentation Principale

- [馃摌 Architecture CoreLoopThree](agentos/docs/architecture/coreloopthree.md)
- [馃捑 Architecture MemoryRovol](agentos/docs/architecture/memoryrovol.md)
- [馃敡 M茅canisme IPC](agentos/docs/architecture/ipc.md)
- [鈿欙笍 Conception Micro-noyau](agentos/docs/architecture/microkernel.md)
- [馃摓 Appels Syst猫me](agentos/docs/architecture/syscall.md)
- [馃摑 Syst猫me Journalisation](agentos/docs/architecture/logging_system.md)

### Guides D茅veloppement

- [馃殌 D茅marrage Rapide](agentos/docs/guides/getting_started.md)
- [馃 Cr茅er Agent](agentos/docs/guides/create_agent.md)
- [馃洜锔?Cr茅er Comp茅tence](agentos/docs/guides/create_skill.md)
- [馃摝 D茅ploiement](agentos/docs/guides/deployment.md)
- [馃帥锔?Optimisation Noyau](agentos/docs/guides/kernel_tuning.md)
- [馃攳 D茅pannage](agentos/docs/guides/troubleshooting.md)

### Sp茅cifications Techniques

- [馃搵 Standards Code](agentos/docs/specifications/coding_standards.md)
- [馃И Standards Tests](agentos/docs/specifications/testing.md)
- [馃敀 Standards S茅curit茅](agentos/docs/specifications/security.md)
- [馃搳 Performances](agentos/docs/specifications/performance.md)

### Documentation Externe

- [馃彮 Workshop](../Workshop/README.md)
- [馃敩 Deepness](../Deepness/README.md)
- [馃搳 Benchmark](../Benchmark/metrics/README.md)

---

## 馃攧 Feuille de Route

### Version Actuelle (v1.0.0.6) - Pr锚t Production

**Avancement**: 85%

- 鉁?Architecture principale termin茅e
- 鉁?MemoryRovol impl茅ment茅 (L1-L4)
- 鉁?CoreLoopThree runtime trois couches
- 鉁?Micro-noyau (core)
- 鉁?System calls (syscall) 100%
- 鉁?Syst猫me journalisation unifi茅
- 馃敳 Tests int茅gration complets

### Court Terme (2026 Q2-Q3)

**v1.0.0.4 - Am茅lioration & Optimisation**
- Gestion exceptions CoreLoopThree
- Performance r茅seau attracteur
- Taux succ猫s cache LRU
- Algorithmes 茅volution m茅moire
- Unit茅s ex茅cution suppl茅mentaires

**v1.0.1.0 - Performance**
- Optimisation recherche vectorielle
- Algorithmes abstraction m茅moire
- R茅duction latence syst猫me

**v1.0.2.0 - Outils D茅veloppeurs**
- Am茅lioration SDK (Go/Python/Rust/TS)
- Outils d茅bogage
- Documentation et exemples

### Moyen Terme (2026 Q4-2027)

**v1.0.3.0 - Production**
- Tests end-to-end complets
- Benchmarks performance
- Audit s茅curit茅
- Validation d茅ploiement production

**v1.0.4.0 - Distribu茅**
- Cluster multi-n艙uds
- M茅moire distribu茅e
- Ordonnancement inter-n艙uds

**v1.0.5.0 - Intelligence**
- Gestion m茅moire adaptative
- Optimisation apprentissage renforcement
- 脡volution autonome

### Vision Long Terme (2027+)

- 馃寪 Standard de facto syst猫mes exploitation agents
- 馃 脡cosyst猫me communautaire mondial
- 馃弳 Leader prochaine g茅n茅ration AGI
- 馃搱 Billion capacit茅s m茅moire, r茅cup茅ration milliseconde

---

## 馃 Coop茅ration 脡cosyst猫me

### Partenaires Technologiques
- **Laboratoires IA**: Experts grands mod猫les, m茅moire, architectures cognitives
- **Fournisseurs Mat茅riel**: GPU, NPU, stockage
- **Entreprises Applications**: Robotique, assistants intelligents, automatisation

### Contributions Communaut茅
- **Code**: D茅veloppement et optimisation fonctionnalit茅s principales
- **Documentation**: Guides utilisation et documentation technique
- **Tests**: Validation fonctionnelle et 茅valuation performance
- **脡cosyst猫me**: Animation communaut茅 et partage connaissances

---

## 馃摓 Support Technique

### Support Communaut茅
- **Gitee Issues**: [Suivi Officiel](https://gitee.com/spharx/agentos/issues) (pr茅f茅r茅)
- **GitHub Issues**: [Suvoi Miroir](https://github.com/SpharxTeam/AgentOS/issues)
- **Discussions**: [GitHub Discussions](https://github.com/SpharxTeam/AgentOS/discussions)
- **Documentation**: [Documentation En Ligne](https://docs.spharx.cn/agentos)

### Support Commercial
- **Version Entreprise**: Licence commerciale et support technique
- **D茅veloppement Sur Mesure**: Modules personnalis茅s
- **Formation**: Formation utilisation et d茅veloppement

Contact licence:
- Email: lidecheng@spharx.cn, wangliren@spharx.cn
- Site web: https://spharx.cn

---

## 馃搫 Licence

AgentOS adopte une **architecture de licence open-source stratifi茅e, ouverte et compatible commerce**, coh茅rente avec les conceptions majeures des SE.

### Licence Principale
Code noyau par d茅faut sous **Apache License 2.0**. Texte complet dans [LICENSE](../../LICENSE).

### D茅tails Licences Stratifi茅es
| R茅pertoire Module | Licence Applicable | Description |
|----------|----------|----------|
| `agentos/atoms/` (Noyau) | Apache License 2.0 | CoreLoopThree, MemoryRovol, runtime, isolation s茅curit茅 |
| `agentos/cupolas/` (Extensions) | Apache License 2.0 | Extensions architecture principale |
| `openlab/` (脡cosyst猫me) | MIT License | Marketplace agents/comp茅tences, contributions communaut茅 |
| D茅pendances Tierces | Licences originales | Toutes d茅pendances sous licences permissives |

### Vous Pouvez Librement
- 鉁?**Usage Commercial**: Produits commerciaux ferm茅s, projets entreprise, services commerciaux
- 鉁?**Modifier**: Modifier, personnaliser, 艙uvres d茅riv茅es sans open-sourcing code m茅tier
- 鉁?**Distribuer**: Distribuer et copier code source ou binaires compil茅s
- 鉁?**Usage Brevets**: Licence brevet permanente pour code principal
- 鉁?**Usage Priv茅**: Projets personnels/priv茅s sans divulgation obligatoire

### Vos Seules Obligations
- Pr茅server notices copyright originales, texte licence et fichier NOTICE
- Inclure historique modifications pour fichiers noyau modifi茅s

### Services Commerciaux
- Aucune restriction usage commercial sous cette licence open-source
- Support technique entreprise, d茅veloppement personnalis茅, d茅ploiement priv茅 disponibles

---

## 馃檹 Remerciements

Merci 脿 tous les d茅veloppeurs contribuant 脿 la communaut茅 open-source et partenaires soutenant AgentOS.

Remerciements sp茅ciaux:
- 脡quipe FAISS (Facebook AI Research)
- 脡quipe Sentence Transformers
- Communaut茅s Rust et Go
- Tous contributeurs et utilisateurs

---

<div align="center">

<h4>"De la donn茅e 茅merge l'intelligence"</h4>

---

#### 馃摓 Nous Contacter

馃摟 Email: lidecheng@spharx.cn; wangliren@spharx.cn

<p>
  <a href="https://gitee.com/spharx/agentos">Gitee (D茅p么t Officiel)</a> 路
  <a href="https://github.com/SpharxTeam/AgentOS">GitHub (D茅p么t Miroir)</a> 路
  <a href="https://spharx.cn">Site Officiel</a> 路
  <a href="mailto:lidecheng@spharx.cn">Support Technique</a>
</p>

漏 2026 SPHARX Ltd. Tous Droits R茅serv茅s.

</div>
