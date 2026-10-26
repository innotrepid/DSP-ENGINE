/**
 * Minimal single-header test framework for DSP ENGINE.
 * No external dependencies.
 */
#ifndef DSP_TEST_FRAMEWORK_H
#define DSP_TEST_FRAMEWORK_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <functional>

namespace dsp_test {

struct TestResult {
    const char* name;
    bool passed;
    std::string message;
};

inline std::vector<TestResult>& results() {
    static std::vector<TestResult> r;
    return r;
}

inline int& total() {
    static int t = 0;
    return t;
}

inline int& failed() {
    static int f = 0;
    return f;
}

#define TEST(name) \
    static void test_##name(); \
    struct TestReg_##name { \
        TestReg_##name() { \
            dsp_test::total()++; \
            try { \
                test_##name(); \
                dsp_test::results().push_back({#name, true, ""}); \
            } catch (const std::string& msg) { \
                dsp_test::failed()++; \
                dsp_test::results().push_back({#name, false, msg}); \
            } catch (...) { \
                dsp_test::failed()++; \
                dsp_test::results().push_back({#name, false, "unknown exception"}); \
            } \
        } \
    }; \
    static TestReg_##name reg_##name; \
    static void test_##name()

#define EXPECT_TRUE(cond) \
    do { \
        if (!(cond)) throw std::string("EXPECT_TRUE failed: " #cond); \
    } while (0)

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

#define EXPECT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            char buf[256]; \
            snprintf(buf, sizeof(buf), "EXPECT_EQ failed: %s != %s", #a, #b); \
            throw std::string(buf); \
        } \
    } while (0)

#define EXPECT_NEAR(a, b, tol) \
    do { \
        double _a = (double)(a), _b = (double)(b); \
        if (std::fabs(_a - _b) > (tol)) { \
            char buf[256]; \
            snprintf(buf, sizeof(buf), "EXPECT_NEAR failed: %g vs %g (tol %g)", _a, _b, (double)(tol)); \
            throw std::string(buf); \
        } \
    } while (0)

#define EXPECT_GT(a, b) EXPECT_TRUE((a) > (b))
#define EXPECT_LT(a, b) EXPECT_TRUE((a) < (b))
#define EXPECT_GE(a, b) EXPECT_TRUE((a) >= (b))
#define EXPECT_LE(a, b) EXPECT_TRUE((a) <= (b))

inline int run_all() {
    printf("\n=== DSP ENGINE unit tests ===\n");
    for (const auto& r : results()) {
        if (r.passed)
            printf("[ PASS ] %s\n", r.name);
        else
            printf("[ FAIL ] %s — %s\n", r.name, r.message.c_str());
    }
    printf("------------------------------\n");
    printf("%d tests, %d failed\n\n", total(), failed());
    return failed() == 0 ? 0 : 1;
}

} // namespace dsp_test

#endif // DSP_TEST_FRAMEWORK_H
