/**
 * @file gateway.h
 * @brief 网关抽象基类
 */

#ifndef BASERUNTIME_GATEWAY_H
#define BASERUNTIME_GATEWAY_H

typedef struct gateway gateway_t;

/**
 * @brief 网关操作表
 */
typedef struct gateway_ops {
    int  (*start)(gateway_t* gw);
    void (*stop)(gateway_t* gw);
    void (*destroy)(gateway_t* gw);
} gateway_ops_t;

/**
 * @brief 网关基类（包含 ops 指针）
 */
struct gateway {
    const gateway_ops_t* ops;
};
// From data intelligence emerges. by spharx

static inline int gateway_start(gateway_t* gw) {
    if (!gw || !gw->ops || !gw->ops->start) return -1;
    return gw->ops->start(gw);
}
static inline void gateway_stop(gateway_t* gw) {
    if (gw && gw->ops && gw->ops->stop) gw->ops->stop(gw);
}
static inline void gateway_destroy(gateway_t* gw) {
    if (gw && gw->ops && gw->ops->destroy) gw->ops->destroy(gw);
}

#endif /* HABITAT_GATEWAY_H */