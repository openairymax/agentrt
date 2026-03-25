"""
Partdocs 文档索引器单元测试

Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."
"""

import pytest
import math
from collections import defaultdict


class TestDocumentIndexer:
    """
    文档索引器单元测试类
    """
    
    def test_inverted_index_basic(self):
        """
        测试基本倒排索引功能
        """
        class MockInvertedIndex:
            def __init__(self):
                self.index = defaultdict(dict)  # {term: {doc_id: [positions]}}
                self.doc_count = 0
            
            def add_document(self, doc_id, tokens):
                """
                添加文档到索引
                
                参数:
                    doc_id: 文档ID
                    tokens: 词条列表
                """
                for position, token in enumerate(tokens):
                    if doc_id not in self.index[token]:
                        self.index[token][doc_id] = []
                    self.index[token][doc_id].append(position)
                
                self.doc_count += 1
            
            def search(self, query_tokens, operator="AND"):
                """
                搜索文档
                
                参数:
                    query_tokens: 查询词条列表
                    operator: 查询操作符
                    
                返回:
                    list: 匹配的文档ID列表
                """
                if not query_tokens:
                    return []
                
                # 获取每个查询词的文档集合
                doc_sets = []
                for token in query_tokens:
                    if token in self.index:
                        doc_sets.append(set(self.index[token].keys()))
                    else:
                        doc_sets.append(set())
                
                # 根据操作符合并
                if operator == "AND":
                    result = set.intersection(*doc_sets) if doc_sets else set()
                elif operator == "OR":
                    result = set.union(*doc_sets) if doc_sets else set()
                else:
                    raise ValueError(f"不支持的操作符: {operator}")
                
                return list(result)
            
            def get_term_frequency(self, token, doc_id):
                """
                获取词条在文档中的频率
                
                参数:
                    token: 词条
                    doc_id: 文档ID
                    
                返回:
                    int: 词频
                """
                if token in self.index and doc_id in self.index[token]:
                    return len(self.index[token][doc_id])
                return 0
            
            def get_document_frequency(self, token):
                """
                获取词条的文档频率
                
                参数:
                    token: 词条
                    
                返回:
                    int: 文档频率
                """
                if token in self.index:
                    return len(self.index[token])
                return 0
        
        index = MockInvertedIndex()
        
        # 添加测试文档
        doc1_tokens = ["python", "编程", "语言", "简单", "易学"]
        doc2_tokens = ["java", "编程", "语言", "企业级", "应用"]
        doc3_tokens = ["javascript", "前端", "开发", "网页", "交互"]
        
        index.add_document("doc1", doc1_tokens)
        index.add_document("doc2", doc2_tokens)
        index.add_document("doc3", doc3_tokens)
        
        # 测试搜索
        # AND 搜索
        results_and = index.search(["编程", "语言"], operator="AND")
        assert set(results_and) == {"doc1", "doc2"}
        
        # OR 搜索
        results_or = index.search(["python", "java"], operator="OR")
        assert set(results_or) == {"doc1", "doc2"}
        
        # 单个词搜索
        results_single = index.search(["前端"], operator="AND")
        assert results_single == ["doc3"]
        
        # 无结果搜索
        results_none = index.search(["不存在"], operator="AND")
        assert results_none == []
        
        # 测试词频
        assert index.get_term_frequency("编程", "doc1") == 1
        assert index.get_term_frequency("编程", "doc2") == 1
        assert index.get_term_frequency("编程", "doc3") == 0
        
        # 测试文档频率
        assert index.get_document_frequency("编程") == 2
        assert index.get_document_frequency("python") == 1
        assert index.get_document_frequency("不存在") == 0
    
    def test_tf_idf_scoring(self):
        """
        测试TF-IDF评分算法
        """
        class MockTFIDFScorer:
            def __init__(self, index):
                self.index = index
            
            def calculate_score(self, doc_id, query_tokens):
                """
                计算文档相关性分数（TF-IDF）
                
                参数:
                    doc_id: 文档ID
                    query_tokens: 查询词条列表
                    
                返回:
                    float: 相关性分数
                """
                total_score = 0.0
                
                for token in query_tokens:
                    # 计算TF（词频）
                    tf = self.index.get_term_frequency(token, doc_id)
                    if tf == 0:
                        continue
                    
                    # 计算IDF（逆文档频率）
                    df = self.index.get_document_frequency(token)
                    if df == 0:
                        continue
                    
                    idf = math.log((self.index.doc_count + 1) / (df + 1))
                    
                    # TF-IDF分数
                    total_score += tf * idf
                
                return total_score
            
            def rank_documents(self, doc_ids, query_tokens):
                """
                对文档进行排序
                
                参数:
                    doc_ids: 文档ID列表
                    query_tokens: 查询词条列表
                    
                返回:
                    list: 排序后的(文档ID, 分数)列表
                """
                scored_docs = []
                
                for doc_id in doc_ids:
                    score = self.calculate_score(doc_id, query_tokens)
                    scored_docs.append((doc_id, score))
                
                # 按分数降序排序
                scored_docs.sort(key=lambda x: x[1], reverse=True)
                return scored_docs
        
        # 创建索引
        class SimpleIndex:
            def __init__(self):
                self.doc_count = 3
                self.term_freq = {
                    "doc1": {"python": 2, "编程": 3, "语言": 1},
                    "doc2": {"java": 2, "编程": 2, "语言": 1},
                    "doc3": {"python": 1, "教程": 2}
                }
                self.doc_freq = {
                    "python": 2,  # 出现在doc1和doc3
                    "编程": 2,     # 出现在doc1和doc2
                    "语言": 2,     # 出现在doc1和doc2
                    "java": 1,    # 只出现在doc2
                    "教程": 1      # 只出现在doc3
                }
            
            def get_term_frequency(self, token, doc_id):
                if doc_id in self.term_freq and token in self.term_freq[doc_id]:
                    return self.term_freq[doc_id][token]
                return 0
            
            def get_document_frequency(self, token):
                return self.doc_freq.get(token, 0)
        
        index = SimpleIndex()
        scorer = MockTFIDFScorer(index)
        
        # 测试TF-IDF计算
        query = ["python", "编程"]
        
        score1 = scorer.calculate_score("doc1", query)
        score2 = scorer.calculate_score("doc2", query)
        score3 = scorer.calculate_score("doc3", query)
        
        # doc1应该分数最高（包含两个查询词且词频高）
        assert score1 > score3  # doc1比doc3分数高
        assert score2 < score1  # doc2只包含"编程"，分数比doc1低
        
        # 测试排序
        doc_ids = ["doc1", "doc2", "doc3"]
        ranked = scorer.rank_documents(doc_ids, query)
        
        # 验证排序顺序
        assert ranked[0][0] == "doc1"  # 分数最高
        assert ranked[1][0] == "doc3"  # 其次
        assert ranked[2][0] == "doc2"  # 分数最低
        
        # 验证分数降序
        assert ranked[0][1] >= ranked[1][1] >= ranked[2][1]
    
    def test_bm25_scoring(self):
        """
        测试BM25评分算法
        """
        class MockBM25Scorer:
            def __init__(self, index, k1=1.5, b=0.75):
                self.index = index
                self.k1 = k1
                self.b = b
                self.avg_doc_length = self._calculate_avg_doc_length()
            
            def _calculate_avg_doc_length(self):
                """
                计算平均文档长度
                
                返回:
                    float: 平均文档长度
                """
                # 模拟计算
                return 100.0  # 假设平均文档长度100个词
            
            def calculate_score(self, doc_id, query_tokens, doc_length):
                """
                计算BM25分数
                
                参数:
                    doc_id: 文档ID
                    query_tokens: 查询词条列表
                    doc_length: 文档长度（词条数）
                    
                返回:
                    float: BM25分数
                """
                total_score = 0.0
                
                for token in query_tokens:
                    # 获取词频
                    tf = self.index.get_term_frequency(token, doc_id)
                    if tf == 0:
                        continue
                    
                    # 获取文档频率
                    df = self.index.get_document_frequency(token)
                    if df == 0:
                        continue
                    
                    # 计算IDF
                    idf = math.log((self.index.doc_count - df + 0.5) / (df + 0.5) + 1.0)
                    
                    # 计算TF因子
                    tf_numerator = tf * (self.k1 + 1)
                    tf_denominator = tf + self.k1 * (1 - self.b + self.b * doc_length / self.avg_doc_length)
                    tf_factor = tf_numerator / tf_denominator
                    
                    # 累加分数
                    total_score += idf * tf_factor
                
                return total_score
        
        # 创建索引
        class SimpleIndex:
            def __init__(self):
                self.doc_count = 1000  # 假设有1000个文档
                self.term_freq = {
                    "doc1": {"python": 5, "编程": 3},
                    "doc2": {"python": 2, "教程": 4},
                    "doc3": {"java": 3, "编程": 2}
                }
                self.doc_freq = {
                    "python": 200,  # 出现在200个文档中
                    "编程": 150,     # 出现在150个文档中
                    "教程": 50,      # 出现在50个文档中
                    "java": 100     # 出现在100个文档中
                }
            
            def get_term_frequency(self, token, doc_id):
                if doc_id in self.term_freq and token in self.term_freq[doc_id]:
                    return self.term_freq[doc_id][token]
                return 0
            
            def get_document_frequency(self, token):
                return self.doc_freq.get(token, 0)
        
        index = SimpleIndex()
        scorer = MockBM25Scorer(index, k1=1.5, b=0.75)
        
        # 测试BM25计算
        query = ["python", "编程"]
        
        # 不同文档长度测试
        score1 = scorer.calculate_score("doc1", query, doc_length=80)   # 较短文档
        score2 = scorer.calculate_score("doc2", query, doc_length=120)  # 中等长度
        score3 = scorer.calculate_score("doc3", query, doc_length=200)  # 较长文档
        
        # doc1应该分数最高（包含两个查询词且文档较短）
        assert score1 > score2  # doc1比doc2分数高
        assert score1 > score3  # doc1比doc3分数高
        
        # 测试参数影响
        scorer_k1_high = MockBM25Scorer(index, k1=2.5, b=0.75)
        scorer_k1_low = MockBM25Scorer(index, k1=0.5, b=0.75)
        
        score_high_k1 = scorer_k1_high.calculate_score("doc1", query, doc_length=80)
        score_low_k1 = scorer_k1_low.calculate_score("doc1", query, doc_length=80)
        
        # k1参数影响词频饱和度
        # 高k1对高词频更敏感
        assert score_high_k1 != score_low_k1
    
    def test_index_compression(self):
        """
        测试索引压缩功能
        """
        class MockCompressedIndex:
            def __init__(self, compression_ratio=0.5):
                self.compression_ratio = compression_ratio
                self.original_size = 0
                self.compressed_size = 0
                self.index = {}
            
            def add_document(self, doc_id, tokens):
                """
                添加文档并记录大小
                
                参数:
                    doc_id: 文档ID
                    tokens: 词条列表
                """
                # 记录原始大小
                self.original_size += len(''.join(tokens))
                
                # 模拟压缩
                compressed_tokens = []
                for token in tokens:
                    # 简单压缩：取前3个字符
                    if len(token) > 3:
                        compressed_token = token[:3]
                    else:
                        compressed_token = token
                    compressed_tokens.append(compressed_token)
                
                # 记录压缩后大小
                self.compressed_size += len(''.join(compressed_tokens))
                
                # 添加到索引
                for token in compressed_tokens:
                    if token not in self.index:
                        self.index[token] = set()
                    self.index[token].add(doc_id)
            
            def search(self, query_token):
                """
                搜索文档
                
                参数:
                    query_token: 查询词条
                    
                返回:
                    list: 匹配的文档ID列表
                """
                # 压缩查询词
                if len(query_token) > 3:
                    compressed_query = query_token[:3]
                else:
                    compressed_query = query_token
                
                if compressed_query in self.index:
                    return list(self.index[compressed_query])
                return []
            
            def get_compression_stats(self):
                """
                获取压缩统计信息
                
                返回:
                    dict: 压缩统计
                """
                if self.original_size == 0:
                    return {"ratio": 0.0, "savings": 0.0}
                
                ratio = self.compressed_size / self.original_size
                savings = 1.0 - ratio
                
                return {
                    "original_size": self.original_size,
                    "compressed_size": self.compressed_size,
                    "compression_ratio": ratio,
                    "space_savings": savings
                }
        
        index = MockCompressedIndex()
        
        # 添加测试文档
        doc1_tokens = ["python programming", "language features", "simple syntax"]
        doc2_tokens = ["java development", "enterprise applications", "object oriented"]
        doc3_tokens = ["javascript framework", "frontend development", "web applications"]
        
        # 分割词条（模拟分词）
        doc1_split = ["python", "programming", "language", "features", "simple", "syntax"]
        doc2_split = ["java", "development", "enterprise", "applications", "object", "oriented"]
        doc3_split = ["javascript", "framework", "frontend", "development", "web", "applications"]
        
        index.add_document("doc1", doc1_split)
        index.add_document("doc2", doc2_split)
        index.add_document("doc3", doc3_split)
        
        # 测试搜索
        results1 = index.search("python")
        assert "doc1" in results1
        
        results2 = index.search("development")
        assert "doc2" in results2
        assert "doc3" in results2
        
        results3 = index.search("applications")
        assert "doc2" in results3
        assert "doc3" in results3
        
        # 测试压缩统计
        stats = index.get_compression_stats()
        assert stats["original_size"] > 0
        assert stats["compressed_size"] > 0
        assert 0 < stats["compression_ratio"] <= 1.0
        assert stats["space_savings"] >= 0.0
        
        # 验证压缩效果（长词被截断）
        # "programming" -> "pro"
        # "development" -> "dev"
        # "javascript" -> "jav"
        assert stats["compressed_size"] < stats["original_size"]
    
    def test_incremental_indexing(self):
        """
        测试增量索引功能
        """
        class MockIncrementalIndex:
            def __init__(self):
                self.index = defaultdict(dict)
                self.pending_updates = []
                self.is_rebuilding = False
            
            def add_document(self, doc_id, tokens):
                """
                添加文档（延迟更新）
                
                参数:
                    doc_id: 文档ID
                    tokens: 词条列表
                """
                self.pending_updates.append(("add", doc_id, tokens))
            
            def remove_document(self, doc_id):
                """
                移除文档（延迟更新）
                
                参数:
                    doc_id: 文档ID
                """
                self.pending_updates.append(("remove", doc_id, None))
            
            def apply_updates(self):
                """
                应用所有待处理的更新
                """
                for operation, doc_id, tokens in self.pending_updates:
                    if operation == "add":
                        for position, token in enumerate(tokens):
                            if doc_id not in self.index[token]:
                                self.index[token][doc_id] = []
                            self.index[token][doc_id].append(position)
                    elif operation == "remove":
                        # 从索引中移除文档
                        for token in list(self.index.keys()):
                            if doc_id in self.index[token]:
                                del self.index[token][doc_id]
                                # 如果词条没有其他文档引用，删除词条
                                if not self.index[token]:
                                    del self.index[token]
                
                # 清空待处理队列
                self.pending_updates = []
            
            def search(self, query_token):
                """
                搜索文档（只搜索已应用的索引）
                
                参数:
                    query_token: 查询词条
                    
                返回:
                    list: 匹配的文档ID列表
                """
                if query_token in self.index:
                    return list(self.index[query_token].keys())
                return []
            
            def get_pending_count(self):
                """
                获取待处理更新数量
                
                返回:
                    int: 待处理更新数量
                """
                return len(self.pending_updates)
        
        index = MockIncrementalIndex()
        
        # 添加文档（但未应用）
        index.add_document("doc1", ["python", "编程"])
        index.add_document("doc2", ["java", "编程"])
        
        # 验证待处理更新
        assert index.get_pending_count() == 2
        
        # 搜索应该没有结果（未应用更新）
        results_before = index.search("编程")
        assert len(results_before) == 0
        
        # 应用更新
        index.apply_updates()
        assert index.get_pending_count() == 0
        
        # 搜索应该有结果
        results_after = index.search("编程")
        assert set(results_after) == {"doc1", "doc2"}
        
        # 测试删除文档
        index.remove_document("doc1")
        assert index.get_pending_count() == 1
        
        # 应用删除
        index.apply_updates()
        results_after_remove = index.search("编程")
        assert results_after_remove == ["doc2"]  # 只剩doc2
    
    def test_index_sharding(self):
        """
        测试索引分片功能
        """
        class MockShardedIndex:
            def __init__(self, num_shards=3):
                self.num_shards = num_shards
                self.shards = [defaultdict(dict) for _ in range(num_shards)]
            
            def _get_shard(self, token):
                """
                根据词条获取分片
                
                参数:
                    token: 词条
                    
                返回:
                    int: 分片索引
                """
                # 简单哈希分片
                return hash(token) % self.num_shards
            
            def add_document(self, doc_id, tokens):
                """
                添加文档到对应分片
                
                参数:
                    doc_id: 文档ID
                    tokens: 词条列表
                """
                for position, token in enumerate(tokens):
                    shard_idx = self._get_shard(token)
                    shard = self.shards[shard_idx]
                    
                    if doc_id not in shard[token]:
                        shard[token][doc_id] = []
                    shard[token][doc_id].append(position)
            
            def search(self, query_tokens):
                """
                跨分片搜索
                
                参数:
                    query_tokens: 查询词条列表
                    
                返回:
                    dict: 每个词条的匹配结果
                """
                results = {}
                
                for token in query_tokens:
                    shard_idx = self._get_shard(token)
                    shard = self.shards[shard_idx]
                    
                    if token in shard:
                        results[token] = list(shard[token].keys())
                    else:
                        results[token] = []
                
                return results
            
            def get_shard_stats(self):
                """
                获取分片统计信息
                
                返回:
                    list: 每个分片的统计信息
                """
                stats = []
                
                for i, shard in enumerate(self.shards):
                    # 计算分片中的词条数
                    term_count = len(shard)
                    
                    # 计算分片中的文档引用总数
                    doc_ref_count = 0
                    for term_docs in shard.values():
                        doc_ref_count += len(term_docs)
                    
                    stats.append({
                        "shard_index": i,
                        "term_count": term_count,
                        "doc_ref_count": doc_ref_count,
                        "load_factor": doc_ref_count / max(term_count, 1)
                    })
                
                return stats
        
        index = MockShardedIndex(num_shards=3)
        
        # 添加测试文档
        documents = {
            "doc1": ["python", "编程", "语言", "简单"],
            "doc2": ["java", "编程", "语言", "企业级"],
            "doc3": ["javascript", "前端", "开发", "网页"],
            "doc4": ["python", "数据分析", "机器学习", "库"],
            "doc5": ["java", "后端", "开发", "框架"]
        }
        
        for doc_id, tokens in documents.items():
            index.add_document(doc_id, tokens)
        
        # 测试搜索
        query = ["python", "java", "编程", "前端"]
        results = index.search(query)
        
        # 验证每个查询词都有结果
        assert "python" in results
        assert "java" in results
        assert "编程" in results
        assert "前端" in results
        
        # 验证具体结果
        assert set(results["python"]) == {"doc1", "doc4"}
        assert set(results["java"]) == {"doc2", "doc5"}
        assert set(results["编程"]) == {"doc1", "doc2"}
        assert set(results["前端"]) == {"doc3"}
        
        # 测试分片统计
        stats = index.get_shard_stats()
        assert len(stats) == 3
        
        # 验证每个分片都有数据
        total_terms = sum(s["term_count"] for s in stats)
        total_refs = sum(s["doc_ref_count"] for s in stats)
        
        assert total_terms > 0
        assert total_refs > 0
        
        # 验证负载相对均衡（允许一定的不均衡）
        load_factors = [s["load_factor"] for s in stats]
        max_load = max(load_factors)
        min_load = min(load_factors)
        
        # 最大负载不应超过最小负载的3倍（简单均衡检查）
        assert max_load <= min_load * 3 or min_load == 0


class TestIndexerIntegration:
    """
    索引器集成测试
    """
    
    def test_end_to_end_indexing(self):
        """
        测试端到端索引流程
        """
        class MockDocument:
            def __init__(self, doc_id, content):
                self.id = doc_id
                self.content = content
                self.tokens = []
            
            def tokenize(self):
                """
                分词
                
                返回:
                    list: 词条列表
                """
                # 简单分词：按空格分割，过滤停用词
                stop_words = {"的", "了", "在", "是", "我", "有", "和", "就"}
                words = self.content.split()
                self.tokens = [w for w in words if w not in stop_words]
                return self.tokens
        
        class MockIndexer:
            def __init__(self):
                self.index = defaultdict(dict)
                self.documents = {}
            
            def index_document(self, document):
                """
                索引文档
                
                参数:
                    document: 文档对象
                    
                返回:
                    bool: 是否成功
                """
                # 分词
                tokens = document.tokenize()
                
                # 添加到索引
                for position, token in enumerate(tokens):
                    if document.id not in self.index[token]:
                        self.index[token][document.id] = []
                    self.index[token][document.id].append(position)
                
                # 保存文档
                self.documents[document.id] = document
                return True
            
            def search(self, query):
                """
                搜索文档
                
                参数:
                    query: 查询字符串
                    
                返回:
                    list: 匹配的文档对象列表
                """
                # 查询分词
                query_tokens = query.split()
                
                # 收集匹配的文档
                matched_docs = set()
                for token in query_tokens:
                    if token in self.index:
                        matched_docs.update(self.index[token].keys())
                
                # 返回文档对象
                return [self.documents[doc_id] for doc_id in matched_docs if doc_id in self.documents]
        
        # 创建索引器
        indexer = MockIndexer()
        
        # 创建测试文档
        doc1 = MockDocument("doc1", "Python 是一种简单易学的编程语言")
        doc2 = MockDocument("doc2", "Java 是企业级应用开发的主要语言")
        doc3 = MockDocument("doc3", "JavaScript 用于网页前端开发")
        
        # 索引文档
        assert indexer.index_document(doc1) == True
        assert indexer.index_document(doc2) == True
        assert indexer.index_document(doc3) == True
        
        # 测试搜索
        results1 = indexer.search("Python 编程")
        assert len(results1) == 1
        assert results1[0].id == "doc1"
        
        results2 = indexer.search("Java 开发")
        assert len(results2) == 1
        assert results2[0].id == "doc2"
        
        results3 = indexer.search("网页 前端")
        assert len(results3) == 1
        assert results3[0].id == "doc3"
        
        # 测试多文档匹配
        results4 = indexer.search("语言")
        assert len(results4) == 2
        doc_ids = {doc.id for doc in results4}
        assert doc_ids == {"doc1", "doc2"}
    
    def test_index_persistence(self, tmp_path):
        """
        测试索引持久化
        """
        import json
        
        class MockPersistentIndex:
            def __init__(self, storage_path):
                self.storage_path = storage_path
                self.index = defaultdict(dict)
                self.loaded = False
            
            def add_document(self, doc_id, tokens):
                """
                添加文档
                
                参数:
                    doc_id: 文档ID
                    tokens: 词条列表
                """
                for position, token in enumerate(tokens):
                    if doc_id not in self.index[token]:
                        self.index[token][doc_id] = []
                    self.index[token][doc_id].append(position)
            
            def save(self):
                """
                保存索引到文件
                
                返回:
                    bool: 是否成功
                """
                try:
                    # 转换为可序列化的格式
                    serializable_index = {}
                    for token, docs in self.index.items():
                        serializable_index[token] = dict(docs)
                    
                    with open(self.storage_path, 'w', encoding='utf-8') as f:
                        json.dump(serializable_index, f, ensure_ascii=False)
                    
                    return True
                except Exception as e:
                    print(f"保存失败: {e}")
                    return False
            
            def load(self):
                """
                从文件加载索引
                
                返回:
                    bool: 是否成功
                """
                try:
                    if not self.storage_path.exists():
                        return False
                    
                    with open(self.storage_path, 'r', encoding='utf-8') as f:
                        serializable_index = json.load(f)
                    
                    # 转换回原始格式
                    self.index.clear()
                    for token, docs in serializable_index.items():
                        self.index[token] = docs
                    
                    self.loaded = True
                    return True
                except Exception as e:
                    print(f"加载失败: {e}")
                    return False
            
            def search(self, query_token):
                """
                搜索文档
                
                参数:
                    query_token: 查询词条
                    
                返回:
                    list: 匹配的文档ID列表
                """
                if query_token in self.index:
                    return list(self.index[query_token].keys())
                return []
        
        # 创建存储文件
        storage_file = tmp_path / "test_index.json"
        
        # 创建索引并添加数据
        index1 = MockPersistentIndex(storage_file)
        index1.add_document("doc1", ["python", "编程"])
        index1.add_document("doc2", ["java", "编程"])
        
        # 保存索引
        assert index1.save() == True
        assert storage_file.exists()
        
        # 创建新索引并加载
        index2 = MockPersistentIndex(storage_file)
        assert index2.load() == True
        assert index2.loaded == True
        
        # 验证加载的数据
        results = index2.search("编程")
        assert set(results) == {"doc1", "doc2"}
        
        results_python = index2.search("python")
        assert results_python == ["doc1"]
        
        results_java = index2.search("java")
        assert results_java == ["doc2"]
        
        # 测试文件损坏的情况
        storage_file.write_text("invalid json content")
        index3 = MockPersistentIndex(storage_file)
        assert index3.load() == False
        assert index3.loaded == False


if __name__ == "__main__":
    """
    直接运行测试
    """
    import sys
    pytest.main(sys.argv)