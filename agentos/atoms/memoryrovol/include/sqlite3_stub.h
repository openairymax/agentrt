/**
 * @file sqlite3_stub.h
 * @brief SQLite3 stub头文件（用于编译时无SQLite3的情况）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 当系统未安装SQLite3开发库时，使用此stub文件进行编译。
 * 所有SQLite3函数调用将返回错误码或空操作。
 */

#ifndef AGENTOS_SQLITE3_STUB_H
#define AGENTOS_SQLITE3_STUB_H

#ifdef __cplusplus
extern "C" {
#endif

/* SQLite3 类型定义 */
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

/* SQLite3 返回码 */
#define SQLITE_OK           0   /* 成功 */
#define SQLITE_ERROR        1   /* SQL错误或数据库丢失 */
#define SQLITE_INTERNAL     2   /* 内部逻辑错误 */
#define SQLITE_PERM         3   /* 访问权限拒绝 */
#define SQLITE_ABORT        4   /* 回调函数请求中止 */
#define SQLITE_BUSY         5   /* 数据库文件被锁定 */
#define SQLITE_LOCKED       6   /* 表中的记录被锁定 */
#define SQLITE_NOMEM        7   /* malloc()失败 */
#define SQLITE_READONLY      8   /* 尝试写入只读数据库 */
#define SQLITE_INTERRUPT     9   /* 操作被sqlite3_interrupt()中断 */
#define SQLITE_IOERR        10  /* 磁盘I/O发生错误 */
#define SQLITE_CORRUPT      11  /* 数据库磁盘映像不正确 */
#define SQLITE_NOTFOUND     12  /* sqlite3_file_control()中未知操作数 */
#define SQLITE_FULL         13  /* 数据库满时插入失败 */
#define SQLITE_CANTOPEN     14  /* Cannot open database file */
#define SQLITE_PROTOCOL     15  /* 数据库锁定协议错误 */
#define SQLITE_EMPTY        16  /* 数据库为空 */
#define SQLITE_SCHEMA       17  /* 数据库模式已更改 */
#define SQLITE_TOOBIG       18  /* 字符串或BLOB超出大小限制 */
#define SQLITE_CONSTRAINT   19  /* 由于约束违反而中止 */
#define SQLITE_MISMATCH     20  /* 数据类型不匹配 */
#define SQLITE_MISUSE       21  /* 库使用不正确 */
#define SQLITE_NOLFS        22  /* 使用OS功能不支持 */
#define SQLITE_AUTH         23  /* 授权被拒绝 */
#define SQLITE_FORMAT       24  /* 辅助数据库格式不正确 */
#define SQLITE_RANGE        25  /* sqlite3_bind参数的索引超出范围 */
#define SQLITE_NOTADB       26  /* 打开的文件不是数据库文件 */
#define SQLITE_NOTICE       27  /* 来自sqlite3_log()的通知 */
#define SQLITE_WARNING      28  /* 来自sqlite3_log()的警告 */

/* SQLite3 额外常量 */
#define SQLITE_STATIC      ((void*)0)   /* 指针值，表示不需要复制 */
#define SQLITE_TRANSIENT    ((void*)-1) /* 指针值，表示需要在sqlite3_exec前复制 */
#define SQLITE_ROW         100          /* sqlite3_step()有另一行就绪 */
#define SQLITE_DONE        101          /* sqlite3_step()完成执行 */

/* SQLite3 函数声明（stub实现） */
static inline int sqlite3_open(const char* filename, sqlite3** ppDb) {
    (void)filename;
    (void)ppDb;
    return SQLITE_NOLFS;  /* 返回"不支持"错误 */
}

static inline int sqlite3_close(sqlite3* db) {
    (void)db;
    return SQLITE_OK;
}

static inline int sqlite3_exec(
    sqlite3* db,
    const char* sql,
    int (*callback)(void*, int, char**, char**),
    void* arg,
    char** errmsg
) {
    (void)db;
    (void)sql;
    (void)callback;
    (void)arg;
    if (errmsg) *errmsg = NULL;
    return SQLITE_MISUSE;
}

static inline int sqlite3_prepare_v2(
    sqlite3* db,
    const char* sql,
    int nByte,
    sqlite3_stmt** ppStmt,
    const char** pzTail
) {
    (void)db;
    (void)sql;
    (void)nByte;
    (void)ppStmt;
    (void)pzTail;
    return SQLITE_MISUSE;
}

static inline int sqlite3_step(sqlite3_stmt* stmt) {
    (void)stmt;
    return SQLITE_MISUSE;
}

static inline int sqlite3_finalize(sqlite3_stmt* stmt) {
    (void)stmt;
    return SQLITE_OK;
}

static inline int sqlite3_reset(sqlite3_stmt* stmt) {
    (void)stmt;
    return SQLITE_OK;
}

static inline const char* sqlite3_errmsg(sqlite3* db) {
    (void)db;
    return "SQLite3 not available (stub implementation)";
}

static inline void sqlite3_free(void* ptr) {
    (void)ptr;
    /* 空操作 */
}

/* SQLite3 额外函数声明（stub实现） */
static inline int sqlite3_bind_text(
    sqlite3_stmt* stmt,
    int col,
    const char* val,
    int len,
    void(*destructor)(void*)
) {
    (void)stmt; (void)col; (void)val; (void)len; (void)destructor;
    return SQLITE_MISUSE;
}

static inline int sqlite3_bind_blob(
    sqlite3_stmt* stmt,
    int col,
    const void* val,
    int len,
    void(*destructor)(void*)
) {
    (void)stmt; (void)col; (void)val; (void)len; (void)destructor;
    return SQLITE_MISUSE;
}

static inline int sqlite3_bind_int(sqlite3_stmt* stmt, int col, int val) {
    (void)stmt; (void)col; (void)val;
    return SQLITE_MISUSE;
}

static inline const void* sqlite3_column_blob(sqlite3_stmt* stmt, int col) {
    (void)stmt; (void)col;
    return NULL;
}

static inline int sqlite3_column_bytes(sqlite3_stmt* stmt, int col) {
    (void)stmt; (void)col;
    return 0;
}

static inline int sqlite3_column_int(sqlite3_stmt* stmt, int col) {
    (void)stmt; (void)col;
    return 0;
}

static inline const unsigned char* sqlite3_column_text(sqlite3_stmt* stmt, int col) {
    (void)stmt; (void)col;
    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SQLITE3_STUB_H */