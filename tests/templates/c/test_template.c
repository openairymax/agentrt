/**
 * AgentOS 测试模板 - C 单元测试
 *
 * 使用方法:
 * 1. 复制此文件到目标目录
 * 2. 重命名为 test_<module_name>.c
 * 3. 替换 <module> 为实际模块名
 * 4. 实现测试用例
 *
 * Version: 2.0.0
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>
#include "<module>.h"

/* 测试辅助宏 */
#define ASSERT_PTR_NOT_NULL(ptr) \
    assert_true((ptr) != NULL)

#define ASSERT_INT_EQ(expected, actual) \
    assert_int_equal((expected), (actual))

#define ASSERT_STR_EQ(expected, actual) \
    assert_string_equal((expected), (actual))

/* 测试夹具 */
static void setup(void **state) {
    /* 测试前初始化 */
    *state = malloc(sizeof(int));
    assert_non_null(*state);
}

static void teardown(void **state) {
    /* 测试后清理 */
    free(*state);
}

/* 基本功能测试 */
static void test_basic_functionality(void **state) {
    (void) state;
    
    int result = module_function();
    assert_int_equal(result, EXPECTED_VALUE);
}

/* 边界情况测试 */
static void test_edge_cases(void **state) {
    (void) state;
    
    /* 测试空输入 */
    int result = module_function_with_input(NULL);
    assert_int_equal(result, ERROR_CODE);
    
    /* 测试最大值 */
    result = module_function_with_input(MAX_VALUE);
    assert_int_equal(result, SUCCESS);
}

/* 错误处理测试 */
static void test_error_handling(void **state) {
    (void) state;
    
    /* 测试无效参数 */
    int result = module_function_with_input(-1);
    assert_int_equal(result, INVALID_PARAM);
}

/* Mock 函数示例 */
static void test_with_mock(void **state) {
    (void) state;
    
    /* 设置 mock 函数返回值 */
    expect_value(dependency_function, param, 42);
    will_return(dependency_function, SUCCESS);
    
    int result = module_function_using_dependency();
    assert_int_equal(result, SUCCESS);
}

/* 性能测试 */
static void test_performance(void **state) {
    (void) state;
    
    clock_t start = clock();
    
    for (int i = 0; i < 10000; i++) {
        module_function();
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    assert_true(elapsed < 1.0);  /* 应该在 1 秒内完成 */
}

/* 测试套件 */
int main(int argc, char *argv[]) {
    const UnitTest tests[] = {
        unit_test_setup_teardown(test_basic_functionality, setup, teardown),
        unit_test_setup_teardown(test_edge_cases, setup, teardown),
        unit_test_setup_teardown(test_error_handling, setup, teardown),
        unit_test(test_with_mock),
        unit_test(test_performance),
    };
    
    return run_tests(tests);
}
