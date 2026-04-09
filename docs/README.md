---
copyright: "Copyright (c) 2026 SPHARX. All Rights Reserved."
slogan: "From data intelligence emerges."
title: "AgentOS 鎶€鏈枃妗ｄ腑蹇?
version: "Doc V1.7"
last_updated: "2026-03-31"
author: "LirenWang"
status: "production_ready"
review_due: "2026-06-30"
theoretical_basis: "宸ョ▼涓よ銆佷簲缁存浜ょ郴缁熴€佸弻绯荤粺璁ょ煡鐞嗚"
target_audience: "鏋舵瀯甯?寮€鍙戣€?杩愮淮/鍒濆鑰?
prerequisites: "鏃?
estimated_reading_time: "30鍒嗛挓"
core_concepts: "鏂囨。浣撶郴, 瀛︿範璺緞, 鏋舵瀯瀵艰埅, 寮€鍙戞寚鍗?
---

# AgentOS 鎶€鏈枃妗ｄ腑蹇?
## 馃搵 鏂囨。淇℃伅鍗?- **鐩爣璇昏€?*: 鏋舵瀯甯?寮€鍙戣€?杩愮淮/鍒濆鑰?- **鍓嶇疆鐭ヨ瘑**: 鏃?- **棰勮闃呰鏃堕棿**: 30鍒嗛挓
- **鏍稿績姒傚康**: 鏂囨。浣撶郴, 瀛︿範璺緞, 鏋舵瀯瀵艰埅, 寮€鍙戞寚鍗?- **鏂囨。鐘舵€?*: 馃煝 鐢熶骇灏辩华
- **澶嶆潅搴︽爣璇?*: 猸?鍒濆鑰呭弸濂?
## 馃幆 蹇€熷鑸?
### 鏂版墜鍏ラ棬

```
鐜鎼缓 鈫?鏋舵瀯鐞嗚В 鈫?绗竴涓狝gent寮€鍙?鈫?鐢熶骇閮ㄧ讲
```

1. **[蹇€熷紑濮媇(guides/getting_started.md)** 鈥?鐜鎼缓涓嶩ello World绀轰緥锛?鍒嗛挓锛?2. **[鏋舵瀯璁捐鍘熷垯](ARCHITECTURAL_PRINCIPLES.md)** 鈥?鐞嗚В浜旂淮姝ｄ氦璁捐浣撶郴
3. **[鍒涘缓 Agent](guides/create_agent.md)** 鈥?寮€鍙戠涓€涓櫤鑳戒綋
4. **[閮ㄧ讲鎸囧崡](guides/deployment.md)** 鈥?閮ㄧ讲鍒扮敓浜х幆澧?
### 寮€鍙戣€呰繘闃?
```
缂栫爜瑙勮寖 鈫?寰唴鏍告灦鏋?鈫?涓夊眰璁ょ煡杩愯鏃?鈫?鍥涘眰璁板繂绯荤粺 鈫?绯荤粺璋冪敤璁捐 鈫?鍐呮牳璋冧紭
```

1. **[C 缂栫爜瑙勮寖](specifications/coding_standard/C_coding_style_guide.md)** 鈥?浠ｇ爜椋庢牸涓庢渶浣冲疄璺?2. **[寰唴鏍歌璁(architecture/microkernel.md)** 鈥?CoreKern鍘熷瓙鏈哄埗
3. **[涓夊眰璁ょ煡杩愯鏃禲(architecture/coreloopthree.md)** 鈥?CoreLoopThree鏋舵瀯
4. **[鍥涘眰璁板繂绯荤粺](architecture/memoryrovol.md)** 鈥?MemoryRovol鏋舵瀯
5. **[绯荤粺璋冪敤璁捐](architecture/syscall.md)** 鈥?Syscall鎺ュ彛濂戠害
6. **[鍐呮牳璋冧紭](guides/kernel_tuning.md)** 鈥?鎬ц兘浼樺寲瀹炴垬

### 鏋舵瀯甯堣瑙?
```
璁捐鍝插 鈫?鏋舵瀯鍘熷垯 鈫?璁ょ煡鐞嗚 鈫?璁板繂鐞嗚 鈫?濂戠害瑙勮寖
```

1. **[璁捐鍝插鎬昏](philosophy/README.md)** 鈥?鐞嗚鏍瑰熀
2. **[鏋舵瀯璁捐鍘熷垯 V1.7](ARCHITECTURAL_PRINCIPLES.md)** 鈥?浜旂淮姝ｄ氦鍘熷垯浣撶郴
3. **[璁ょ煡鐞嗚](philosophy/Cognition_Theory.md)** 鈥?鍙岀郴缁熻鐭ョ悊璁?4. **[璁板繂鐞嗚](philosophy/Memory_Theory.md)** 鈥?璁板繂鍒嗗眰鏈哄埗
5. **[濂戠害瑙勮寖闆哴(specifications/agentos_contract/README.md)** 鈥?鎺ュ彛濂戠害瀹氫箟

---

## 馃摎 鏂囨。浣撶郴缁撴瀯

```
agentos/docs/
鈹溾攢鈹€ 馃搻 architecture/          # 鏋舵瀯璁捐鏂囨。锛?绡囨牳蹇冭鏂囷級
鈹?  鈹溾攢鈹€ ARCHITECTURAL_PRINCIPLES.md           猸?鏋舵瀯璁捐鍘熷垯 V1.7
鈹?  鈹溾攢鈹€ folder/                               # 鏍稿績鏋舵瀯鏂囨。
鈹?  鈹?  鈹溾攢鈹€ coreloopthree.md                    猸?涓夊眰璁ょ煡杩愯鏃?鈹?  鈹?  鈹溾攢鈹€ memoryrovol.md                      猸?鍥涘眰璁板繂绯荤粺
鈹?  鈹?  鈹溾攢鈹€ microkernel.md                      猸?寰唴鏍歌璁?鈹?  鈹?  鈹溾攢鈹€ ipc.md                              猸?IPC Binder閫氫俊
鈹?  鈹?  鈹溾攢鈹€ syscall.md                          猸?绯荤粺璋冪敤璁捐
鈹?  鈹?  鈹斺攢鈹€ logging_system.md                   猸?缁熶竴鏃ュ織绯荤粺
鈹?  鈹斺攢鈹€ diagrams/                               # 鏋舵瀯鍥捐〃
鈹?鈹溾攢鈹€ 馃Л guides/                # 寮€鍙戞寚鍗楋紙7绡囧疄鎴樻暀绋嬶級
鈹?  鈹溾攢鈹€ getting_started.md          猸?蹇€熷紑濮?鈹?  鈹溾攢鈹€ create_agent.md             猸?Agent寮€鍙戞暀绋?鈹?  鈹溾攢鈹€ create_skill.md             猸?Skill寮€鍙戞暀绋?鈹?  鈹溾攢鈹€ deployment.md               猸?閮ㄧ讲鎸囧崡
鈹?  鈹溾攢鈹€ kernel_tuning.md            猸?鍐呮牳璋冧紭
鈹?  鈹溾攢鈹€ troubleshooting.md          猸?鏁呴殰鎺掓煡
鈹?  鈹斺攢鈹€ migration_guide.md          猸?鐗堟湰杩佺Щ
鈹?鈹溾攢鈹€ 馃挕 philosophy/            # 璁捐鍝插锛?绡囩悊璁哄熀纭€锛?鈹?  鈹溾攢鈹€ Cognition_Theory.md         猸?璁ょ煡鐞嗚锛堝弻绯荤粺鐞嗚锛?鈹?  鈹溾攢鈹€ Design_Principles.md        猸?璁捐鍘熷垯
鈹?  鈹斺攢鈹€ Memory_Theory.md            猸?璁板繂鐞嗚锛堣蹇嗗垎灞傦級
鈹?鈹溾攢鈹€ 馃搵 specifications/        # 鎶€鏈鑼冿紙15+绡囨爣鍑嗘枃妗ｏ級
鈹?  鈹溾攢鈹€ TERMINOLOGY.md              猸?缁熶竴鏈琛?V1.7
鈹?  鈹溾攢鈹€ agentos_contract/           # 濂戠害瑙勮寖闆?鈹?  鈹?  鈹溾攢鈹€ agent/                  # Agent濂戠害
鈹?  鈹?  鈹溾攢鈹€ skill/                  # Skill濂戠害
鈹?  鈹?  鈹溾攢鈹€ protocol/               # 閫氫俊鍗忚锛圝SON-RPC 2.0锛?鈹?  鈹?  鈹溾攢鈹€ syscall/                # 绯荤粺璋冪敤濂戠害
鈹?  鈹?  鈹斺攢鈹€ log/                    # 鏃ュ織鏍煎紡瑙勮寖
鈹?  鈹溾攢鈹€ coding_standard/            # 缂栫爜瑙勮寖
鈹?  鈹?  鈹溾攢鈹€ C_coding_style_guide.md
鈹?  鈹?  鈹溾攢鈹€ Cpp_coding_style_guide.md
鈹?  鈹?  鈹溾攢鈹€ Python_coding_style_guide.md
鈹?  鈹?  鈹溾攢鈹€ JavaScript_coding_style_guide.md
鈹?  鈹?  鈹溾攢鈹€ Java_secure_coding_guide.md
鈹?  鈹?  鈹溾攢鈹€ C_Cpp_secure_coding_guide.md
鈹?  鈹?  鈹溾攢鈹€ Security_design_guide.md
鈹?  鈹?  鈹溾攢鈹€ Log_guide.md
鈹?  鈹?  鈹斺攢鈹€ Code_comment_template.md
鈹?  鈹斺攢鈹€ project_erp/                # 椤圭洰绠＄悊
鈹?      鈹溾攢鈹€ error_code_reference.md
鈹?      鈹溾攢鈹€ resource_management_table.md
鈹?      鈹溾攢鈹€ manuals_module_requirements.md
鈹?      鈹斺攢鈹€ SBOM.md
鈹?鈹溾攢鈹€ 馃攲 api/                   # API鍙傝€冿紙8+绡囨帴鍙ｆ枃妗ｏ級
鈹?  鈹溾攢鈹€ syscalls/                 # 绯荤粺璋冪敤API
鈹?  鈹?  鈹溾攢鈹€ task.md               # 浠诲姟绠＄悊API
鈹?  鈹?  鈹溾攢鈹€ memory.md             # 璁板繂绠＄悊API
鈹?  鈹?  鈹溾攢鈹€ session.md            # 浼氳瘽绠＄悊API
鈹?  鈹?  鈹斺攢鈹€ telemetry.md          # 鍙娴嬫€PI
鈹?  鈹斺攢鈹€ agentos/toolkit/                  # 澶氳瑷€SDK
鈹?      鈹溾攢鈹€ python/               # Python SDK
鈹?      鈹溾攢鈹€ rust/                 # Rust SDK
鈹?      鈹溾攢鈹€ go/                   # Go SDK
鈹?      鈹斺攢鈹€ typescript/           # TypeScript SDK
鈹?鈹溾攢鈹€ 馃摉 white_paper/           # 鐧界毊涔︼紙涓嫳鏂囩増鏈級
鈹?  鈹溾攢鈹€ zh/AgentOS_鎶€鏈櫧鐨功_V1.0.md
鈹?  鈹斺攢鈹€ en/AgentOS_Technical_White_Paper_V1.0.md
鈹?鈹溾攢鈹€ 馃實 readme/                # 澶氳瑷€README
鈹?  鈹溾攢鈹€ README.md                 # 涓枃
鈹?  鈹溾攢鈹€ en/README.md              # English
鈹?  鈹溾攢鈹€ de/README.md              # Deutsch
鈹?  鈹斺攢鈹€ fr/README.md              # Fran莽ais
鈹?鈹溾攢鈹€ DOCSINDEX.md              # 馃搼 瀹屾暣鏂囨。绱㈠紩
鈹斺攢鈹€ MANUALS_SUMMARY.md        # 馃搳 鏍稿績瑕佺偣鎬荤粨
```

---

## 馃彈锔?鏍稿績鎶€鏈灦鏋?
### 馃З 浜旂淮姝ｄ氦鍘熷垯浣撶郴

AgentOS 鍩轰簬浜旂淮姝ｄ氦绯荤粺璁捐锛屽寘鍚簲涓浉浜掔嫭绔嬬殑璁捐缁村害锛屽叡鍚屾瀯鎴愬畬鏁寸殑璁捐鍝插浣撶郴锛?
| 缁村害 | 鍘熷垯鏁伴噺 | 鏍稿績鐞嗗康 | 鍏抽敭鏂囨。 |
|------|----------|----------|----------|
| **绯荤粺瑙?(S)** | S-1 ~ S-4 | 鍙嶉闂幆銆佸眰娆″垎瑙ｃ€佹€讳綋璁捐閮ㄣ€佹秾鐜版€х鐞?| [鏋舵瀯璁捐鍘熷垯](architecture/ARCHITECTURAL_PRINCIPLES.md) |
| **鍐呮牳瑙?(K)** | K-1 ~ K-4 | 鍐呮牳鏋佺畝銆佹帴鍙ｅ绾﹀寲銆佹湇鍔￠殧绂汇€佸彲鎻掓嫈绛栫暐 | [寰唴鏍歌璁(architecture/folder/microkernel.md) |
| **璁ょ煡瑙?(C)** | C-1 ~ C-4 | 鍙岀郴缁熷崗鍚屻€佸閲忔紨鍖栥€佽蹇嗗嵎杞姐€侀仐蹇樻満鍒?| [璁ょ煡鐞嗚](philosophy/folder/Cognition_Theory.md) |
| **宸ョ▼瑙?(E)** | E-1 ~ E-8 | 瀹夊叏鍐呯敓銆佸彲瑙傛祴鎬с€佽祫婧愮‘瀹氭€с€佹枃妗ｅ嵆浠ｇ爜 | [缂栫爜瑙勮寖](specifications/coding_standard/C_coding_style_guide.md) |
| **璁捐缇庡 (A)** | A-1 ~ A-4 | 绠€绾﹁嚦涓娿€佹瀬鑷寸粏鑺傘€佷汉鏂囧叧鎬€銆佸畬缇庝富涔?| [璁捐鍘熷垯](philosophy/folder/Design_Principles.md) |

### 馃彌锔?绯荤粺灞傛鏋舵瀯

AgentOS 閲囩敤灞傛鍖栨灦鏋勮璁★紝鍚勫眰鑱岃矗鍒嗘槑锛岄€氳繃鏍囧噯鍖栨帴鍙ｄ氦浜掞細

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?             agentos/daemon/ 鐢ㄦ埛鎬佹湇鍔?                         鈹?鈹?    llm_d 路 market_d 路 monit_d 路 tool_d 路 sched_d       鈹?鈹?    鈺扳攢鈹€鈹€ 鍚庡彴鏈嶅姟杩涚▼锛屾彁渚涗笟鍔″姛鑳?鈹€鈹€鈹€鈺?               鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?            agentos/gateway/ 閫氫俊缃戝叧                            鈹?鈹?         HTTP/1.1 路 WebSocket 路 Stdio                   鈹?鈹?    鈺扳攢鈹€鈹€ 澶栭儴閫氫俊鎺ュ彛锛屾敮鎸佸绉嶅崗璁?鈹€鈹€鈹€鈺?               鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?         syscall/ 绯荤粺璋冪敤鎺ュ彛                           鈹?鈹?   task 路 memory 路 session 路 telemetry 路 agent          鈹?鈹?    鈺扳攢鈹€鈹€ 鐢ㄦ埛鎬佷笌鍐呮牳鎬佺殑鏍囧噯鎺ュ彛 鈹€鈹€鈹€鈺?                 鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?           agentos/cupolas/ 瀹夊叏闃叉姢灞?猸?                       鈹?鈹?    workbench 路 permission 路 sanitizer 路 audit          鈹?鈹?    鈺扳攢鈹€鈹€ 鍥涢噸瀹夊叏闃叉姢鏈哄埗 鈹€鈹€鈹€鈺?                         鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?corekern/    鈹? coreloopthree/                          鈹?鈹?寰唴鏍告牳蹇?   鈹? 涓夊眰璁ょ煡寰幆 猸?                        鈹?鈹?IPC路Mem路Task 鈹? 璁ょ煡鈫掕鍒掆啋璋冨害鈫掓墽琛?                    鈹?鈹?Time         鈹? 鍩虹鍐呮牳鏈哄埗                            鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹粹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?        memoryrovol/ 鍥涘眰璁板繂绯荤粺 猸?                    鈹?鈹? L1 鍘熷鍗?鈫?L2 鐗瑰緛灞?鈫?L3 缁撴瀯灞?鈫?L4 妯″紡灞?          鈹?鈹?    鈺扳攢鈹€鈹€ 鍒嗗眰璁板繂瀛樺偍涓庢绱?鈹€鈹€鈹€鈺?                       鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?            agentos/commons/ 鍏叡鍩虹搴?                         鈹?鈹? error 路 logger 路 metrics 路 trace 路 cost                鈹?鈹?    鈺扳攢鈹€鈹€ 閫氱敤宸ュ叿鍜屽熀纭€璁炬柦 鈹€鈹€鈹€鈺?                       鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

### 涓夊ぇ鏍稿績鍒涙柊

#### 1. CoreLoopThree锛堜笁灞傝鐭ヨ繍琛屾椂锛?
**瀹炵幇璁ょ煡銆佽鍔ㄥ拰璁板繂鐨勬湁鏈虹粺涓€**

```
鐢ㄦ埛杈撳叆 鈫?璁ょ煡灞傦紙鎰忓浘鐞嗚В锛?鈫?鍙屾ā鍨嬪崗鍚屾帹鐞?         鈫?澧為噺瑙勫垝鍣紙DAG 鐢熸垚锛?鈫?璋冨害瀹橈紙Agent 閫夋嫨锛?         鈫?鎵ц灞傦紙浠诲姟鎵ц锛?鈫?琛ュ伩浜嬪姟锛堝紓甯稿鐞嗭級
         鈫?璁板繂灞傦紙缁撴灉瀛樺偍锛?鈫?鍙嶉缁欒鐭ュ眰锛堢瓥鐣ヨ皟鏁达級
```

| 灞傛 | 鑱岃矗 | System 1锛堝揩锛?| System 2锛堟參锛?|
|------|------|---------------|---------------|
| **璁ょ煡灞?* | 鎰忓浘鐞嗚В銆佷换鍔¤鍒?| 杈呮ā鍨嬪揩閫熷垎绫?| 涓绘ā鍨嬫繁搴﹁鍒?|
| **琛屽姩灞?* | 浠诲姟鎵ц銆佽ˉ鍋夸簨鍔?| 棰勮鎵ц鍗曞厓 | 鍔ㄦ€佷唬鐮佺敓鎴?|
| **璁板繂灞?* | 璁板繂鍐欏叆銆佹煡璇㈡绱?| 缂撳瓨蹇€熻鍙?| 娣卞害妫€绱㈠垎鏋?|

**璇︾粏鏂囨。**: [CoreLoopThree 鏋舵瀯](architecture/folder/coreloopthree.md)

#### 2. MemoryRovol锛堝洓灞傝蹇嗙郴缁燂級

**浠庡師濮嬫暟鎹埌楂樼骇妯″紡鐨勫畬鏁磋蹇嗙鐞?*

| 灞傜骇 | 鍚嶇О | 鍔熻兘 | 鎶€鏈疄鐜?| 鎬ц兘鎸囨爣 |
|------|------|------|----------|----------|
| **L1** | 鍘熷鍗?| 鍘熷浜嬩欢瀛樺偍 | 鏂囦欢绯荤粺 + SQLite 绱㈠紩 | 10,000+ 鏉?绉?|
| **L2** | 鐗瑰緛灞?| 鍚戦噺宓屽叆妫€绱?| FAISS + Embedding 妯″瀷 | < 10ms (k=10) |
| **L3** | 缁撴瀯灞?| 鍏崇郴缁戝畾缂栫爜 | 缁戝畾绠楀瓙 + 鍥剧缁忕綉缁?| 100 鏉?绉?|
| **L4** | 妯″紡灞?| 妯″紡鎸栨帢鎶借薄 | 鎸佷箙鍚岃皟 + HDBSCAN | 10 涓囨潯/鍒嗛挓 |

**瀛樼敤鍒嗙**: L1 姘镐箙淇濆瓨鍘熷鏁版嵁锛堜粎杩藉姞锛夛紝L2-L4 浠呭瓨鍌ㄧ储寮曞拰鐗瑰緛

**璇︾粏鏂囨。**: [MemoryRovol 鏋舵瀯](architecture/folder/memoryrovol.md)

#### 3. cupolas锛堝畨鍏ㄧ┕椤讹級

**鍥涘眰绾垫繁闃插尽鐨勫畨鍏ㄤ綋绯?*

| 闃叉姢灞?| 缁勪欢 | 鏈哄埗 | 瀹夊叏绛夌骇 |
|--------|------|------|----------|
| **铏氭嫙宸ヤ綅** | workbench/ | 杩涚▼/瀹瑰櫒/WASM娌欑闅旂 | 杩涚▼绾ч殧绂?|
| **鏉冮檺瑁佸喅** | permission/ | YAML 瑙勫垯寮曟搸 + RBAC | 缁嗙矑搴﹁闂帶鍒?|
| **杈撳叆鍑€鍖?* | sanitizer/ | 姝ｅ垯杩囨护 + 绫诲瀷妫€鏌?| 娉ㄥ叆鏀诲嚮闃叉姢 |
| **瀹¤杩借釜** | audit/ | 鍏ㄩ摼璺拷韪?+ 涓嶅彲绡℃敼鏃ュ織 | 鍚堣瀹¤ |

**璇︾粏鏂囨。**: [瀹夊叏绌归《璁捐](architecture/ARCHITECTURAL_PRINCIPLES.md#瀹夊叏绌归《-cupolas)

---

## 馃摉 瀛︿範璺緞

### 璺緞 1: 鏋舵瀯甯堜箣璺?
**鐩爣**: 鎺屾彙 AgentOS 鐨勬暣浣撴灦鏋勮璁″拰鐞嗚鍩虹

```mermaid
graph LR
    A[鏋舵瀯璁捐鍘熷垯 V1.6] --> B[璁ょ煡鐞嗚]
    A --> C[璁板繂鐞嗚]
    A --> D[璁捐鍘熷垯]
    B --> E[涓夊眰璁ょ煡杩愯鏃禲
    C --> F[鍥涘眰璁板繂绯荤粺]
    D --> G[濂戠害瑙勮寖闆哴
```

**鎺ㄨ崘闃呰椤哄簭**:
1. [鏋舵瀯璁捐鍘熷垯 V1.7](architecture/ARCHITECTURAL_PRINCIPLES.md) 鈥?浜旂淮姝ｄ氦鍘熷垯浣撶郴
2. [璁ょ煡鐞嗚](philosophy/folder/Cognition_Theory.md) 鈥?鍙岀郴缁熻鐭ョ悊璁哄熀纭€
3. [璁板繂鐞嗚](philosophy/folder/Memory_Theory.md) 鈥?璁板繂鍒嗗眰鐨勭缁忕瀛﹀熀纭€
4. [璁捐鍘熷垯](philosophy/folder/Design_Principles.md) 鈥?璁捐鍝插涓庣編瀛?5. [涓夊眰璁ょ煡杩愯鏃禲(architecture/folder/coreloopthree.md) 鈥?CoreLoopThree 鎶€鏈疄鐜?6. [鍥涘眰璁板繂绯荤粺](architecture/folder/memoryrovol.md) 鈥?MemoryRovol 鎶€鏈疄鐜?7. [濂戠害瑙勮寖闆哴(specifications/agentos_contract/README.md) 鈥?鎺ュ彛濂戠害瀹氫箟

**棰勮鏃堕棿**: 15-20 灏忔椂

### 璺緞 2: 鏍稿績寮€鍙戣€呬箣璺?
**鐩爣**: 娣卞叆鐞嗚В鍐呮牳瀹炵幇锛岃兘澶熶慨鏀瑰拰鎵╁睍鏍稿績鍔熻兘

```mermaid
graph LR
    A[蹇€熷紑濮媇 --> B[C 缂栫爜瑙勮寖]
    B --> C[寰唴鏍歌璁
    C --> D[绯荤粺璋冪敤璁捐]
    C --> E[IPC 閫氫俊]
    D --> F[鍒涘缓 Agent]
    D --> G[鍒涘缓 Skill]
    G --> H[鍐呮牳璋冧紭]
```

**鎺ㄨ崘闃呰椤哄簭**:
1. [蹇€熷紑濮媇(guides/getting_started.md) 鈥?鐜鎼缓涓?Hello World
2. [C 缂栫爜瑙勮寖](specifications/coding_standard/C_coding_style_guide.md) 鈥?浠ｇ爜椋庢牸涓庤鑼?3. [寰唴鏍歌璁(architecture/folder/microkernel.md) 鈥?CoreKern 鍘熷瓙鏈哄埗
4. [绯荤粺璋冪敤璁捐](architecture/folder/syscall.md) 鈥?Syscall 鎺ュ彛濂戠害
5. [IPC 閫氫俊](architecture/folder/ipc.md) 鈥?Binder IPC 鏈哄埗
6. [鍒涘缓 Agent](guides/create_agent.md) 鈥?Agent 寮€鍙戝疄鎴?7. [鍒涘缓 Skill](guides/create_skill.md) 鈥?Skill 寮€鍙戝疄鎴?8. [鍐呮牳璋冧紭](guides/kernel_tuning.md) 鈥?鎬ц兘浼樺寲瀹炴垬

**棰勮鏃堕棿**: 20-30 灏忔椂

### 璺緞 3: 搴旂敤寮€鍙戣€呬箣璺?
**鐩爣**: 蹇€熷紑鍙戝熀浜?AgentOS 鐨勬櫤鑳戒綋搴旂敤

```mermaid
graph LR
    A[蹇€熷紑濮媇 --> B[鍒涘缓 Agent]
    B --> C[鍒涘缓 Skill]
    C --> D[閮ㄧ讲鎸囧崡]
    D --> E[鏁呴殰鎺掓煡]
```

**鎺ㄨ崘闃呰椤哄簭**:
1. [蹇€熷紑濮媇(guides/getting_started.md) 鈥?5 鍒嗛挓蹇€熷紑濮?2. [鍒涘缓 Agent](guides/create_agent.md) 鈥?寮€鍙戠涓€涓櫤鑳戒綋
3. [鍒涘缓 Skill](guides/create_skill.md) 鈥?寮€鍙戣嚜瀹氫箟鎶€鑳?4. [閮ㄧ讲鎸囧崡](guides/deployment.md) 鈥?閮ㄧ讲鍒扮敓浜х幆澧?5. [鏁呴殰鎺掓煡](guides/troubleshooting.md) 鈥?甯歌闂璇婃柇

**棰勮鏃堕棿**: 5-8 灏忔椂

### 璺緞 4: 杩愮淮宸ョ▼甯堜箣璺?
**鐩爣**: 鎺屾彙 AgentOS 鐨勯儴缃层€佺洃鎺у拰璋冧紭

```mermaid
graph LR
    A[閮ㄧ讲鎸囧崡] --> B[鏁呴殰鎺掓煡]
    B --> C[鍐呮牳璋冧紭]
    C --> D[杩佺Щ鎸囧崡]
    D --> E[鏃ュ織绯荤粺]
```

**鎺ㄨ崘闃呰椤哄簭**:
1. [閮ㄧ讲鎸囧崡](guides/deployment.md) 鈥?澶氱幆澧冮儴缃叉柟妗?2. [鏁呴殰鎺掓煡](guides/troubleshooting.md) 鈥?鍒嗗眰璇婃柇鏂规硶璁?3. [鍐呮牳璋冧紭](guides/kernel_tuning.md) 鈥?鍙嶉闂幆璋冧紭娉?4. [杩佺Щ鎸囧崡](guides/migration_guide.md) 鈥?鐗堟湰鍗囩骇绛栫暐
5. [鏃ュ織绯荤粺](architecture/folder/logging_system.md) 鈥?缁熶竴鏃ュ織瑙勮寖

**棰勮鏃堕棿**: 10-15 灏忔椂

---

## 馃搳 鏂囨。鐘舵€佹€昏

| 鏂囨。绫诲埆 | 鏂囨。鏁伴噺 | 鐢熶骇灏辩华 | 鐗堟湰鑼冨洿 | 璐ㄩ噺璇勫垎 |
|---------|---------|---------|---------|---------|
| **鏋舵瀯鏂囨。** | 7 绡?| 鉁?鍏ㄩ儴 | v1.0.0.5 ~ V1.7 | 猸愨瓙猸愨瓙猸?A |
| **寮€鍙戞寚鍗?* | 7 绡?| 鉁?鍏ㄩ儴 | v1.0.0.5 | 猸愨瓙猸愨瓙猸?A |
| **璁捐鍝插** | 3 绡?| 鉁?鍏ㄩ儴 | v1.0 | 猸愨瓙猸愨瓙猸?A |
| **鎶€鏈鑼?* | 15+ 绡?| 鉁?鍏ㄩ儴 | V1.7 | 猸愨瓙猸愨瓙猸?A |
| **API 鏂囨。** | 8+ 绡?| 鉁?鍏ㄩ儴 | - | 猸愨瓙猸愨瓙猸?A |
| **鐧界毊涔?* | 2 绡?| 鉁?鍏ㄩ儴 | V1.0 | 猸愨瓙猸愨瓙猸?A |

**鏂囨。瑕嗙洊鐜?*:
- 鉁?鏍稿績鏋舵瀯鏂囨。锛?00%
- 鉁?寮€鍙戞寚鍗楋細100%
- 鉁?API 鏂囨。锛?00%
- 鉁?缂栫爜瑙勮寖锛?00%
- 鉁?娴嬭瘯鏂囨。锛?00%

---

## 馃攳 涓婚绱㈠紩

### 鏋舵瀯璁捐

| 涓婚 | 鏍稿績鏂囨。 | 鐩稿叧鏂囨。 |
|------|---------|---------|
| **寰唴鏍告灦鏋?* | [寰唴鏍歌璁(architecture/folder/microkernel.md) | [鏋舵瀯璁捐鍘熷垯](architecture/ARCHITECTURAL_PRINCIPLES.md) |
| **涓夊眰璁ょ煡杩愯鏃?* | [CoreLoopThree](architecture/folder/coreloopthree.md) | [璁ょ煡鐞嗚](philosophy/folder/Cognition_Theory.md) |
| **鍥涘眰璁板繂绯荤粺** | [MemoryRovol](architecture/folder/memoryrovol.md) | [璁板繂鐞嗚](philosophy/folder/Memory_Theory.md) |
| **IPC 閫氫俊** | [IPC Binder](architecture/folder/ipc.md) | [绯荤粺璋冪敤璁捐](architecture/folder/syscall.md) |
| **瀹夊叏绌归《** | [鏋舵瀯璁捐鍘熷垯 - cupolas 绔犺妭](architecture/ARCHITECTURAL_PRINCIPLES.md) | [瀹夊叏璁捐鎸囧崡](specifications/coding_standard/Security_design_guide.md) |

### 寮€鍙戝疄鎴?
| 涓婚 | 鏍稿績鏂囨。 | 鐩稿叧鏂囨。 |
|------|---------|---------|
| **Agent 寮€鍙?* | [鍒涘缓 Agent](guides/create_agent.md) | [Agent 濂戠害](specifications/agentos_contract/agent/agent_contract.md) |
| **Skill 寮€鍙?* | [鍒涘缓 Skill](guides/create_skill.md) | [Skill 濂戠害](specifications/agentos_contract/skill/skill_contract.md) |
| **绯荤粺璋冪敤 API** | [绯荤粺璋冪敤璁捐](architecture/folder/syscall.md) | [API 鍙傝€僝(api/syscalls/) |
| **缂栫爜瑙勮寖** | [C 缂栫爜瑙勮寖](specifications/coding_standard/C_coding_style_guide.md) | [浠ｇ爜娉ㄩ噴妯℃澘](specifications/coding_standard/Code_comment_template.md) |
| **瀹夊叏缂栫▼** | [C/C++ 瀹夊叏缂栫▼](specifications/coding_standard/C_Cpp_secure_coding_guide.md) | [Java 瀹夊叏缂栫爜](specifications/coding_standard/Java_secure_coding_guide.md) |

### 杩愮淮閮ㄧ讲

| 涓婚 | 鏍稿績鏂囨。 | 鐩稿叧鏂囨。 |
|------|---------|---------|
| **閮ㄧ讲** | [閮ㄧ讲鎸囧崡](guides/deployment.md) | [Docker 閮ㄧ讲](scripts/deploy/docker/README.md) |
| **鎬ц兘璋冧紭** | [鍐呮牳璋冧紭](guides/kernel_tuning.md) | [鏁呴殰鎺掓煡](guides/troubleshooting.md) |
| **鏃ュ織涓庤拷韪?* | [鏃ュ織绯荤粺](architecture/folder/logging_system.md) | [鏃ュ織鏍煎紡](specifications/agentos_contract/log/logging_format.md) |
| **鐗堟湰杩佺Щ** | [杩佺Щ鎸囧崡](guides/migration_guide.md) | [鍙樻洿鏃ュ織](../CHANGELOG.md) |

### 鐞嗚鍩虹

| 涓婚 | 鏍稿績鏂囨。 | 鐩稿叧鏂囨。 |
|------|---------|---------|
| **宸ョ▼涓よ** | [鏋舵瀯璁捐鍘熷垯 - 鐞嗚鍩虹](architecture/ARCHITECTURAL_PRINCIPLES.md) | [璁捐鍘熷垯](philosophy/folder/Design_Principles.md) |
| **鍙岀郴缁熻鐭?* | [璁ょ煡鐞嗚](philosophy/folder/Cognition_Theory.md) | [CoreLoopThree](architecture/folder/coreloopthree.md) |
| **寰唴鏍稿摬瀛?* | [寰唴鏍歌璁(architecture/folder/microkernel.md) | [鏋舵瀯璁捐鍘熷垯 - 鍐呮牳瑙俔(architecture/ARCHITECTURAL_PRINCIPLES.md) |
| **鏈瑙勮寖** | [缁熶竴鏈琛?V1.7](specifications/TERMINOLOGY.md) | [璇嶆眹绱㈠紩](specifications/agentos_contract/glossary_index.md) |

---

## 馃挕 鐞嗚鏍瑰熀

### 宸ョ▼涓よ

**銆婂伐绋嬫帶鍒惰銆?*
- **鍙嶉闂幆鐞嗚**: 姣忓眰璁捐瀹屾暣鐨?鎰熺煡 - 鍐崇瓥 - 鎵ц - 鍙嶉"闂幆
- **鍓嶉棰勬祴鎬ц璁?*: 鍩轰簬鍘嗗彶妯″紡鐨勮秼鍔块娴嬶紝鎻愬墠璋冩暣璧勬簮鍒嗛厤
- **鑷€傚簲璋冭妭鏈哄埗**: 鏍规嵁璐熻浇鍔ㄦ€佽皟鏁寸嚎绋嬫睜銆佺紦瀛樼瓥鐣ャ€侀噸璇曟鏁?
**銆婅绯荤粺宸ョ▼銆?*
- **灞傛鍒嗚В鏂规硶**: 姣忓眰鍙緷璧栧叾鐩存帴涓嬪眰鎺ュ彛锛屼粠涓嶈秺绾ц闂?- **鎬讳綋璁捐閮?*: 鍙仛鍗忚皟銆佷笉鍋氭墽琛岀殑鍏ㄥ眬鍐崇瓥灞?- **娲绘ā鍧楃悊璁?*: 妯″潡鍏锋湁鑷劅鐭ャ€佽嚜璋冭妭鑳藉姏

### 璁ょ煡绉戝

**鍙岀郴缁熻鐭ョ悊璁?* 鈥?涓瑰凹灏斅峰崱灏兼浖銆婃€濊€冿紝蹇笌鎱€?- **System 1锛堝揩鎬濊€冿級**: 蹇€熴€佺洿瑙夈€佽嚜鍔ㄥ寲锛岀敤浜庣畝鍗曚换鍔?- **System 2锛堟參鎬濊€冿級**: 缂撴參銆佺悊鎬с€佹繁搴﹀垎鏋愶紝鐢ㄤ簬澶嶆潅浠诲姟
- **鍒囨崲闃堝€兼ā鍨?*: `鍒囨崲闃堝€?= f(缃俊搴︼紝鏃堕棿棰勭畻锛岃祫婧愮害鏉燂紝椋庨櫓绛夌骇)`

**ACT-R 璁ょ煡鏋舵瀯**
- 妯″潡鍒掑垎锛氳瑙夈€佸惉瑙夈€佹墜鍔ㄣ€佸０鏄庢€ц蹇嗙瓑妯″潡
- 浜х敓寮忕郴缁燂細IF-THEN 瑙勫垯椹卞姩璁ょ煡琛屼负

**SOAR 璁ょ煡鏋舵瀯**
- 闂绌洪棿鍋囪锛氭墍鏈夋湁鐩殑鐨勮涓洪兘鍙槧灏勪负闂绌洪棿鎼滅储

### 璁＄畻鏈虹瀛?
**Liedtke 寰唴鏍告瀯閫犲畾鐞?*
- 鍐呮牳鏈€灏忚亴璐?= IPC + 鍦板潃绌洪棿绠＄悊 + 绾跨▼璋冨害
- 鏈哄埗涓庣瓥鐣ュ垎绂伙細鍐呮牳鎻愪緵鏈哄埗锛岀敤鎴锋€佸疄鐜扮瓥鐣?
**seL4 褰㈠紡鍖栭獙璇?*
- 鍔熻兘姝ｇ‘鎬э細鍐呮牳瀹炵幇瀹屽叏绗﹀悎褰㈠紡鍖栬鑼?- 瀹夊叏鎬ц川锛氫俊鎭祦瀹夊叏銆佹潈闄愰殧绂?
---

## 馃帗 鏍稿績姒傚康閫熸煡

### 鏈琛?
| 鏈 | 鑻辨枃 | 瀹氫箟 | 鏂囨。浣嶇疆 |
|------|------|------|---------|
| **鍘熷瓙鍐呮牳** | CoreKern | AgentOS 鐨勫井鍐呮牳瀹炵幇锛屾彁渚?IPC銆佸唴瀛樸€佷换鍔°€佹椂闂村洓澶ф満鍒?| [寰唴鏍歌璁(architecture/folder/microkernel.md) |
| **涓夊眰璁ょ煡杩愯鏃?* | CoreLoopThree | 璁ょ煡灞傘€佽鍔ㄥ眰銆佽蹇嗗眰缁勬垚鐨勯棴鐜郴缁?| [CoreLoopThree](architecture/folder/coreloopthree.md) |
| **鍥涘眰璁板繂绯荤粺** | MemoryRovol | L1 鍘熷鍗封啋L2 鐗瑰緛灞傗啋L3 缁撴瀯灞傗啋L4 妯″紡灞?| [MemoryRovol](architecture/folder/memoryrovol.md) |
| **瀹夊叏绌归《** | cupolas | 铏氭嫙宸ヤ綅銆佹潈闄愯鍐炽€佽緭鍏ュ噣鍖栥€佸璁¤拷韪洓閲嶉槻鎶?| [鏋舵瀯璁捐鍘熷垯](architecture/ARCHITECTURAL_PRINCIPLES.md) |
| **绯荤粺璋冪敤** | Syscall | 鐢ㄦ埛鎬佷笌鍐呮牳閫氫俊鐨勫敮涓€鏍囧噯閫氶亾 | [绯荤粺璋冪敤璁捐](architecture/folder/syscall.md) |
| **鍙岀郴缁熷崗鍚?* | Dual-System Synergy | System 1 蹇€熻矾寰勪笌 System 2 鎱㈤€熻矾寰勭殑鍗忓悓 | [璁ょ煡鐞嗚](philosophy/folder/Cognition_Theory.md) |

### 鏍稿績鎸囨爣

| 鎸囨爣 | 鏁板€?| 娴嬭瘯鏉′欢 | 璇存槑 |
|------|------|---------|------|
| **IPC Binder 寤惰繜** | < 1渭s | 鏈湴璋冪敤 | 闆舵嫹璐濅紭鍖?|
| **浠诲姟璋冨害寤惰繜** | < 1ms | 鍔犳潈杞 | 100 骞跺彂浠诲姟 |
| **璁板繂妫€绱㈠欢杩?* | < 10ms | FAISS IVF,PQ k=10 | 鐧句竾绾у悜閲忓簱 |
| **绯荤粺璋冪敤寮€閿€** | < 5% | 鐩告瘮鐩存帴璋冪敤 | 鍐呮牳鎬佸垏鎹㈠紑閿€ |
| **鍚姩鏃堕棿** | < 500ms | Cold start | 瀹屾暣鍒濆鍖?|
| **鍐呭瓨鍗犵敤** | ~2MB | 鍏稿瀷鍦烘櫙 | 鍩虹杩愯鏃?|

---

## 馃洜锔?宸ュ叿涓庤祫婧?
### 寮€鍙戝伐鍏?
- **CMake**: 3.20+ 鏋勫缓绯荤粺
- **GCC/Clang**: GCC 11 / Clang 14+
- **Python**: 3.10+ SDK
- **Go**: 1.16+ SDK
- **Rust**: 1.56+ SDK
- **TypeScript**: 4.0+ SDK

### 鏂囨。宸ュ叿

- **Doxygen**: API 鏂囨。鐢熸垚
- **clang-format**: 浠ｇ爜鏍煎紡鍖?- **pre-commit**: Git 閽╁瓙绠＄悊
- **lizard**: 鍦堝鏉傚害鍒嗘瀽
- **jscpd**: 浠ｇ爜閲嶅妫€娴?
### 娴嬭瘯宸ュ叿

- **CTest**: CMake 娴嬭瘯椹卞姩
- **pytest**: Python 娴嬭瘯妗嗘灦
- **Go test**: Go 娴嬭瘯宸ュ叿
- **Cargo test**: Rust 娴嬭瘯宸ュ叿
- **Jest**: TypeScript 娴嬭瘯妗嗘灦

---

## 馃 璐＄尞鎸囧崡

### 鏂囨。缁撴瀯鏍囧噯

姣忎唤鏂囨。搴斿寘鍚細
1. **鐗堟潈澹版槑**: `Copyright (c) 2026 SPHARX. All Rights Reserved.`
2. **鐗堟湰淇℃伅**: 鐗堟湰鍙枫€佹渶鍚庢洿鏂版棩鏈熴€佺姸鎬?3. **缁撴瀯鍖栫珷鑺?*: 姒傝堪 鈫?鏍稿績鍐呭 鈫?绀轰緥 鈫?鐩稿叧鏂囨。
4. **浜ゅ弶寮曠敤**: 閾炬帴鍒扮浉鍏虫枃妗ｇ殑绮剧‘璺緞
5. **鍘熷垯鏄犲皠**: 鏍囨敞涓庝簲缁存浜ゅ師鍒欑殑瀵瑰簲鍏崇郴

### 鎻愪氦娴佺▼

1. **Fork 椤圭洰**
2. **鍒涘缓鍒嗘敮**: `git checkout -b docs/topic-name`
3. **缂栧啓鏂囨。**: 閬靛惊涓婅堪缁撴瀯鏍囧噯
4. **楠岃瘉閾炬帴**: 纭繚鎵€鏈夊唴閮ㄩ摼鎺ユ湁鏁?5. **鎻愪氦 PR**: 鎻忚堪淇敼鍐呭鍜屽師鍥?
### 鏂囨。鐗堟湰绠＄悊

- **涓荤増鏈?(X.0)**: 鏋舵瀯閲嶅ぇ鍙樻洿锛岄渶鏋舵瀯濮斿憳浼氬鎵?- **娆＄増鏈?(x.Y)**: 鍐呭鏇存柊銆侀敊璇慨姝ｏ紝鐢辩淮鎶よ€呭鎵?- **鏂囨。鍗充唬鐮?*: 鏂囨。涓庝唬鐮佸悓姝ョ増鏈帶鍒讹紝CI 鑷姩妫€鏌?
---

## 馃搼 瀹屾暣绱㈠紩

- **[鏂囨。浣撶郴绱㈠紩](DOCSINDEX.md)** 鈥?瀹屾暣鐨勬枃妗ｅ湴鍥惧拰瀵艰埅
- **[鏍稿績瑕佺偣鎬荤粨](MANUALS_SUMMARY.md)** 鈥?鎵€鏈夋枃妗ｇ殑鏍稿績瑕佺偣鎻愮偧
- **[鏈琛╙(specifications/TERMINOLOGY.md)** 鈥?缁熶竴鏈瀹氫箟
- **[璇嶆眹绱㈠紩](specifications/agentos_contract/glossary_index.md)** 鈥?涓撲笟璇嶆眹琛?
---

## 馃敆 鐩稿叧璧勬簮

- **[涓婚」鐩?README](../README.md)** 鈥?AgentOS 椤圭洰鎬昏
- **[鍙樻洿鏃ュ織](../CHANGELOG.md)** 鈥?鐗堟湰鏇存柊璁板綍
- **[璐＄尞鎸囧崡](../CONTRIBUTING.md)** 鈥?浠ｇ爜璐＄尞瑙勮寖
- **[琛屼负鍑嗗垯](../CODE_OF_CONDUCT.md)** 鈥?绀惧尯琛屼负鍑嗗垯
- **[瀹夊叏鏀跨瓥](../SECURITY.md)** 鈥?瀹夊叏婕忔礊鎶ュ憡

---

## 馃摓 鑱旂郴鏂瑰紡

- **缁存姢鑰?*: AgentOS 鏋舵瀯濮斿憳浼?- **鎶€鏈敮鎸?*: lidecheng@spharx.cn
- **瀹夊叏闂**: wangliren@spharx.cn
- **闂鍙嶉**: https://github.com/SpharxTeam/AgentOS/issues
- **椤圭洰涓婚〉**: https://gitee.com/spharx/agentos

---

## 馃摑 鐗堟湰鍘嗗彶

| 鐗堟湰 | 鏃ユ湡 | 浣滆€?| 鍙樻洿璇存槑 |
|------|------|------|----------|
| Doc V1.7 | 2026-03-31 | LirenWang | 鍒濆鐗堟湰锛屽缓绔嬪畬鏁存枃妗ｄ綋绯?|

---

漏 2026 SPHARX Ltd. All Rights Reserved.

*"From data intelligence emerges 濮嬩簬鏁版嵁锛岀粓浜庢櫤鑳姐€?*

<div align="center">

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/SpharxTeam/AgentOS/actions)
[![Documentation Status](https://img.shields.io/badge/docs-V1.7-blue)](README.md)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue)](../LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0.6-orange)](../CHANGELOG.md)

</div>
