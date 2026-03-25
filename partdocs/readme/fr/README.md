Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# Agent OS

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.5-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Mirror](https://img.shields.io/badge/mirror-GitHub-lightgrey.svg)](https://github.com/SpharxTeam/AgentOS)

---

**Système d'Exploitation SuperIA**

*"De la donnée émerge l'intelligence."*

---

📖 **[简体中文](../../README.md)** | [English](../en/README.md) | 🇫🇷 **Français** | [Deutsch](../de/README.md)

</div>

---

## Introduction

- Conçu pour l'exécution de tâches avec une efficacité maximale des tokens
<!-- From data intelligence emerges. by spharx -->
- Nouvelle architecture offrant 2 à 3 fois plus d'efficacité que les frameworks standards
- 3 à 5 fois plus efficace qu'OpenClaw pour les tâches d'ingénierie, économisant ~60% de tokens

## 📋 Aperçu

- **Agent OS (SuperAI OS)** est le noyau du système d'exploitation d'agents intelligents de SpharxWorks, fournissant un environnement d'exécution complet, un système de mémoire, un moteur cognitif et un framework d'exécution.
- En tant qu'infrastructure de données pour le monde physique, AgentOS implémente une boucle fermée complète du traitement des données à la prise de décision intelligente.

### Valeurs Clés

- **Micro-noyau**: Conception minimaliste avec tous les services en espace utilisateur
- **Architecture Trois Couches**: Cognition, Exécution et Mémoire pour la gestion du cycle de vie des agents
- **Système de Mémoire Roll**: Abstraction progressive L1-L4 avec stockage, récupération, évolution et oubli
- **Appels Système**: Interfaces stables et sécurisées masquant les détails du noyau
- **Stratégies Enfichables**: Chargement dynamique et remplacement à l'exécution des algorithmes
- **Journalisation Unifiée**: Interface multi-langages avec traçabilité complète et OpenTelemetry
- **SDK Multi-langages**: Support natif Go, Python, Rust et TypeScript avec interfaces FFI

### État de la Version

**Version Actuelle**: v1.0.0.6 (Prêt pour la Production)

- ✅ Conception de l'architecture principale terminée
- ✅ Système de Mémoire MemoryRovol
  - Architecture quatre couches L1-L4 entièrement implémentée
  - Écriture synchrone/asynchrone (10 000+ entrées/sec)
  - Recherche vectorielle FAISS (indexation IVF/HNSW)
  - Mécanisme de récupération par réseau attracteur
  - Courbe d'oubli d'Ebbinghaus
  - Cache LRU et persistance des vecteurs
- ✅ Architecture CoreLoopThree
  - Couche Cognition: Compréhension intention, planification, coordination (90%)
  - Couche Exécution: Moteur d'exécution, transactions compensation, traçage (85%)
  - Couche Mémoire: Interface FFI MemoryRovol (80%)
- ✅ Module de base Micro-noyau (core)
  - Communication IPC Binder
  - Gestion mémoire (RAII, pointeurs intelligents)
  - Ordonnancement de tâches (round-robin pondéré)
  - Service temporel haute précision
- ✅ Couche System Call (syscall) - 100% terminée
  - ✅ Appels tâche: `sys_task_submit/query/wait/cancel`
  - ✅ Appels mémoire: `sys_memory_write/search/get/delete`
  - ✅ Appels session: `sys_session_create/get/close/list`
  - ✅ Appels observabilité: `sys_telemetry_metrics/traces`
  - ✅ Point d'entrée unique: `agentos_syscall_invoke()`
- 🔲 Tests d'intégration complets

---

## 🏗️ Architecture Système

```
┌─────────────────────────────────────────────────────────────┐
│                    Architecture Globale AgentOS              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           Couche Application (openhub)                │  │
│  │  docgen | ecommerce | research | videoedit | ...      │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │        Couche Services Principaux (backs)             │  │
│  │  llm_d | market_d | monit_d | perm_d | sched_d | ...  │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │          Couche Noyau (atoms)                         │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │  │
│  │  │   core       │  │coreloopthree │  │memoryrovol  │  │  │
│  │  │ Micro-noyau  │  │Runtime 3-Couches│ │Mémoire 4-N  │  │  │
│  │  └──────────────┘  └──────────────┘  └─────────────┘  │  │
│  │  ┌──────────────┐                                     │  │
│  │  │   syscall    │                                     │  │
│  │  │ Appels Système│                                    │  │
│  │  └──────────────┘                                     │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │            Couche SDK (tools)                         │  │
│  │  Go | Python | Rust | TypeScript | ...                │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 🧠 CoreLoopThree: Architecture Trois Couches

### Philosophie de Conception

CoreLoopThree divise le runtime des agents en trois couches orthogonales et synergiques:

```
┌─────────────────────────────────────────┐
         Couche Cognition                  
   • Compréhension Intention • Planification
   • Ordonnancement Agents • Coordination  
└───────────────↓─────────────────────────┘
                ↓
┌─────────────────────────────────────────┐
         Couche Exécution                 
    • Exécution Tâches • Compensation     
    • Traçage Chaîne • États              
└───────────────↓─────────────────────────┘
                ↓
┌─────────────────────────────────────────┐
          Couche Mémoire                  
    • Écriture • Récupération             
    • Contexte • Évolution & Oubli        
└─────────────────────────────────────────┘
```

### Composants Principaux

#### 1. Couche Cognition
- **Moteur de Compréhension**: Analyse entrée utilisateur, identification intention
- **Planificateur de Tâches**: Décomposition automatique en graphe DAG
- **Ordonnanceur d'Agents**: Coordination multi-agents et allocation ressources
- **Coordinateur de Modèles**: Sélection LLM et ingénierie de prompts
- **Interfaces Stratégies**: Algorithmes enfichables (planification, coordination, ordonnancement)

#### 2. Couche Exécution
- **Moteur d'Exécution**: Exécution tâches et suivi états
  - Machine états (Pending/Running/Succeeded/Failed/Cancelled/Retrying)
  - Contrôle concurrence et timeouts
- **Transactions Compensation**: Rollback échec et logique compensation
- **Traçage Chaîne Responsabilité**: Enregistrement complet chaîne exécution
- **Registre Unités Exécution**: Enregistrement unités atomiques
- **Gestion Exceptions**: Capture hiérarchique et récupération

#### 3. Couche Mémoire
- **Service Mémoire**: Encapsulation MemoryRovol
  - Moteur mémoire (`agentos_memory_engine_t`)
  - Types enregistrements (RAW/FEATURE/STRUCTURE/PATTERN)
- **Interface Écriture**: Support synchrone/asynchrone
- **Interface Requête**: Recherche sémantique et vectorielle
- **Montage Contextuel**: Association automatique mémoire
- **Interface FFI**: `rov_ffi.h` pour appels multi-langages

Voir: [Documentation CoreLoopThree](partdocs/architecture/coreloopthree.md)

---

## 💾 MemoryRovol: Système de Mémoire

### Positionnement

MemoryRovol est le système de mémoire au niveau du noyau, gérant la mémoire complète de la donnée brute aux motifs avancés.

### Architecture Quatre Couches

```
┌─────────────────────────────────────────┐
       L4 Couche Motifs                    
   • Homologie Persistante • Motifs Stables
   • Clustering HDBSCAN • Règles           
└───────────────↑─────────────────────────┘
                ↓ Évolution Abstraite
┌─────────────────────────────────────────┐
      L3 Couche Structure                  
   • Opérateurs Liaison • Encodage Relations
└───────────────↑─────────────────────────┘
                ↓ Extraction Caractéristiques
┌─────────────────────────────────────────┐
      L2 Couche Caractéristiques           
   • Modèles Embedding (OpenAI/DeepSeek)  
   • Index Vectoriel FAISS • Recherche Hybride
└───────────────↑─────────────────────────┘
                ↓ Compression Données
┌─────────────────────────────────────────┐
       L1 Couche Brute                     
   • Stockage Fichier • Fragmentation      
   • Index Métadonnées • Intégrité         
└─────────────────────────────────────────┘
```

### Fonctionnalités Principales

#### 1. Stockage Mémoire
- **Écriture Synchrone**: Blocante assurant persistance
- **Écriture Asynchrone**: Par lots pour débit élevé (10 000+/sec)
- **Support Transaction**: Sémantique ACID
- **Compression Archivage**: Automatique mémoires低频

#### 2. Récupération Mémoire
- **Recherche Vectorielle**: Similarité cosinus via FAISS
  - Latence < 10ms (k=10)
- **Recherche Sémantique**: Requête langage naturel
- **Sensible Contexte**: Filtrage automatique (temps, source, TraceID)
- **Cache LRU**: Cache vecteurs chauds
- **Re-classement**: Cross-encoder pour pertinence

#### 3. Évolution Mémoire
- **Abstraction Progressive**: L1→L2→L3→L4
  - Extraction caractéristiques
  - Liaison structurelle
  - Fouille motifs
- **Découverte Motifs**: Identification motifs fréquents
- **Mises à Jour Poids**: Dynamique selon fréquence accès
- **Évaluation Évolution**: Comité avec couche cognition

#### 4. Oubli Mémoire
- **Courbe Ebbinghaus**: Découpage intelligent selon courbe oubli
- **Décroissance Linéaire**: Dégradation linéaire simple
- **Compteur Accès**: Stratégie LRU/LFU
- **Oubli Actif**: Déclenché par couche cognition

Voir: [Documentation MemoryRovol](partdocs/architecture/memoryrovol.md)

---

## 🛠️ Guide Développement

### Prérequis

- **SE**: Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2)
- **Compilateur**: GCC 11+ ou Clang 14+
- **Outils Build**: CMake 3.20+, Ninja ou Make
- **Dépendances**:
  - OpenSSL >= 1.1.1
  - libevent
  - pthread
  - FAISS >= 1.7.0
  - SQLite3 >= 3.35
  - libcurl >= 7.68
  - cJSON >= 1.7.15
  - Ripser >= 2.3.1 (optionnel)
  - HDBSCAN >= 0.8.27 (optionnel)

### Démarrage Rapide

#### 1. Cloner Dépôt

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

| Variable CMake | Description | Défaut |
| :--- | :--- | :--- |
| `CMAKE_BUILD_TYPE` | Debug/Release/RelWithDebInfo | `Release` |
| `BUILD_TESTS` | Tests unitaires | `OFF` |
| `ENABLE_TRACING` | Traçage OpenTelemetry | `OFF` |
| `ENABLE_ASAN` | AddressSanitizer | `OFF` |

Voir: [BUILD.md](atoms/BUILD.md)

### Système Journalisation

```
partdata/logs/
├── kernel/         → agentos.log
├── services/       → llm_d.log, tool_d.log, etc.
└── apps/           → logs indépendants
```

- Format lisible: `%(asctime)s.%(msecs)03d [%(levelname)s] [%(name)s] %(message)s`
- Format JSON: Pour intégration ELK/Splunk
- Corrélation multi-langages via `trace_id`
- Intégration OpenTelemetry

Voir: [Documentation Logging](partdocs/architecture/logging_system.md)

### Tests

```bash
ctest -R unit --output-on-failure
ctest -R integration --output-on-failure
python scripts/benchmark.py
```

---

## 📊 Performances

Environnement: Intel i7-12700K, 32GB RAM, NVMe SSD

### Capacité Traitement

| Mesure | Valeur | Conditions |
| :--- | :--- | :--- |
| **Débit Écriture Mémoire** | 10 000+ entrées/sec | L1, asynchrone |
| **Latence Recherche Vectorielle** | < 10ms | FAISS IVF1024,PQ64, k=10 |
| **Latence Recherche Hybride** | < 50ms | Vectoriel+BM25, top-100 |
| **Vitesse Abstraction Mémoire** | 100 entrées/sec | L2→L3 |
| **Vitesse Fouille Motifs** | 100k entrées/min | L4 |
| **Connexions Simultanées** | 1024 | IPC Binder |
| **Latence Ordonnancement** | < 1ms | Round-robin pondéré |
| **Latence Analyse Intention** | < 50ms | Intention simple |
| **Vitesse Planification** | 100+ nœuds/sec | DAG |
| **Latence Ordonnancement Agents** | < 5ms | Round-robin pondéré |
| **Débit Exécution Tâches** | 1000+ tâches/sec | Concurrent |

### Utilisation Ressources

| Scénario | CPU | Mémoire | IO Disque |
| :--- | :--- | :--- | :--- |
| **Inactif** | < 5% | 200MB | < 1MB/s |
| **Charge Moyenne** | 30-50% | 1-2GB | 10-50MB/s |
| **Charge Élevée** | 80-100% | 4-8GB | 100-500MB/s |

### Évolutivité

- **Horizontale**: Déploiement multi-nœuds (prévu)
- **Verticale**: Limites et allocation configurables
- **Élastique**: Ajustement automatique selon charge (prévu)

Note: Détails dans [scripts/benchmark.py](scripts/benchmark.py)

---

## 📚 Documentation

### Documentation Principale

- [📘 Architecture CoreLoopThree](partdocs/architecture/coreloopthree.md)
- [💾 Architecture MemoryRovol](partdocs/architecture/memoryrovol.md)
- [🔧 Mécanisme IPC](partdocs/architecture/ipc.md)
- [⚙️ Conception Micro-noyau](partdocs/architecture/microkernel.md)
- [📞 Appels Système](partdocs/architecture/syscall.md)
- [📝 Système Journalisation](partdocs/architecture/logging_system.md)

### Guides Développement

- [🚀 Démarrage Rapide](partdocs/guides/getting_started.md)
- [🤖 Créer Agent](partdocs/guides/create_agent.md)
- [🛠️ Créer Compétence](partdocs/guides/create_skill.md)
- [📦 Déploiement](partdocs/guides/deployment.md)
- [🎛️ Optimisation Noyau](partdocs/guides/kernel_tuning.md)
- [🔍 Dépannage](partdocs/guides/troubleshooting.md)

### Spécifications Techniques

- [📋 Standards Code](partdocs/specifications/coding_standards.md)
- [🧪 Standards Tests](partdocs/specifications/testing.md)
- [🔒 Standards Sécurité](partdocs/specifications/security.md)
- [📊 Performances](partdocs/specifications/performance.md)

### Documentation Externe

- [🏭 Workshop](../Workshop/README.md)
- [🔬 Deepness](../Deepness/README.md)
- [📊 Benchmark](../Benchmark/metrics/README.md)

---

## 🔄 Feuille de Route

### Version Actuelle (v1.0.0.6) - Prêt Production

**Avancement**: 85%

- ✅ Architecture principale terminée
- ✅ MemoryRovol implémenté (L1-L4)
- ✅ CoreLoopThree runtime trois couches
- ✅ Micro-noyau (core)
- ✅ System calls (syscall) 100%
- ✅ Système journalisation unifié
- 🔲 Tests intégration complets

### Court Terme (2026 Q2-Q3)

**v1.0.0.4 - Amélioration & Optimisation**
- Gestion exceptions CoreLoopThree
- Performance réseau attracteur
- Taux succès cache LRU
- Algorithmes évolution mémoire
- Unités exécution supplémentaires

**v1.0.1.0 - Performance**
- Optimisation recherche vectorielle
- Algorithmes abstraction mémoire
- Réduction latence système

**v1.0.2.0 - Outils Développeurs**
- Amélioration SDK (Go/Python/Rust/TS)
- Outils débogage
- Documentation et exemples

### Moyen Terme (2026 Q4-2027)

**v1.0.3.0 - Production**
- Tests end-to-end complets
- Benchmarks performance
- Audit sécurité
- Validation déploiement production

**v1.0.4.0 - Distribué**
- Cluster multi-nœuds
- Mémoire distribuée
- Ordonnancement inter-nœuds

**v1.0.5.0 - Intelligence**
- Gestion mémoire adaptative
- Optimisation apprentissage renforcement
- Évolution autonome

### Vision Long Terme (2027+)

- 🌐 Standard de facto systèmes exploitation agents
- 🤝 Écosystème communautaire mondial
- 🏆 Leader prochaine génération AGI
- 📈 Billion capacités mémoire, récupération milliseconde

---

## 🤝 Coopération Écosystème

### Partenaires Technologiques
- **Laboratoires IA**: Experts grands modèles, mémoire, architectures cognitives
- **Fournisseurs Matériel**: GPU, NPU, stockage
- **Entreprises Applications**: Robotique, assistants intelligents, automatisation

### Contributions Communauté
- **Code**: Développement et optimisation fonctionnalités principales
- **Documentation**: Guides utilisation et documentation technique
- **Tests**: Validation fonctionnelle et évaluation performance
- **Écosystème**: Animation communauté et partage connaissances

---

## 📞 Support Technique

### Support Communauté
- **Gitee Issues**: [Suivi Officiel](https://gitee.com/spharx/agentos/issues) (préféré)
- **GitHub Issues**: [Suvoi Miroir](https://github.com/SpharxTeam/AgentOS/issues)
- **Discussions**: [GitHub Discussions](https://github.com/SpharxTeam/AgentOS/discussions)
- **Documentation**: [Documentation En Ligne](https://docs.spharx.cn/agentos)

### Support Commercial
- **Version Entreprise**: Licence commerciale et support technique
- **Développement Sur Mesure**: Modules personnalisés
- **Formation**: Formation utilisation et développement

Contact licence:
- Email: lidecheng@spharx.cn, wangliren@spharx.cn
- Site web: https://spharx.cn

---

## 📄 Licence

AgentOS adopte une **architecture de licence open-source stratifiée, ouverte et compatible commerce**, cohérente avec les conceptions majeures des SE.

### Licence Principale
Code noyau par défaut sous **Apache License 2.0**. Texte complet dans [LICENSE](../../LICENSE).

### Détails Licences Stratifiées
| Répertoire Module | Licence Applicable | Description |
|----------|----------|----------|
| `atoms/` (Noyau) | Apache License 2.0 | CoreLoopThree, MemoryRovol, runtime, isolation sécurité |
| `domes/` (Extensions) | Apache License 2.0 | Extensions architecture principale |
| `openhub/` (Écosystème) | MIT License | Marketplace agents/compétences, contributions communauté |
| Dépendances Tierces | Licences originales | Toutes dépendances sous licences permissives |

### Vous Pouvez Librement
- ✅ **Usage Commercial**: Produits commerciaux fermés, projets entreprise, services commerciaux
- ✅ **Modifier**: Modifier, personnaliser, œuvres dérivées sans open-sourcing code métier
- ✅ **Distribuer**: Distribuer et copier code source ou binaires compilés
- ✅ **Usage Brevets**: Licence brevet permanente pour code principal
- ✅ **Usage Privé**: Projets personnels/privés sans divulgation obligatoire

### Vos Seules Obligations
- Préserver notices copyright originales, texte licence et fichier NOTICE
- Inclure historique modifications pour fichiers noyau modifiés

### Services Commerciaux
- Aucune restriction usage commercial sous cette licence open-source
- Support technique entreprise, développement personnalisé, déploiement privé disponibles

---

## 🙏 Remerciements

Merci à tous les développeurs contribuant à la communauté open-source et partenaires soutenant AgentOS.

Remerciements spéciaux:
- Équipe FAISS (Facebook AI Research)
- Équipe Sentence Transformers
- Communautés Rust et Go
- Tous contributeurs et utilisateurs

---

<div align="center">

<h4>"De la donnée émerge l'intelligence"</h4>

---

#### 📞 Nous Contacter

📧 Email: lidecheng@spharx.cn; wangliren@spharx.cn

<p>
  <a href="https://gitee.com/spharx/agentos">Gitee (Dépôt Officiel)</a> ·
  <a href="https://github.com/SpharxTeam/AgentOS">GitHub (Dépôt Miroir)</a> ·
  <a href="https://spharx.cn">Site Officiel</a> ·
  <a href="mailto:lidecheng@spharx.cn">Support Technique</a>
</p>

© 2026 SPHARX Ltd. Tous Droits Réservés.

</div>
