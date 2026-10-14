// Minimal self-contained test framework (no external dependencies).
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace rt90test {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry();
void report_failure(const char* file, int line, const std::string& msg);

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

}  // namespace rt90test

#define RT_CONCAT2(a, b) a##b
#define RT_CONCAT(a, b) RT_CONCAT2(a, b)

#define TEST(name)                                                                 \
    static void RT_CONCAT(test_fn_, name)();                                       \
    static ::rt90test::Registrar RT_CONCAT(test_reg_, name)(#name, RT_CONCAT(test_fn_, name)); \
    static void RT_CONCAT(test_fn_, name)()

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) ::rt90test::report_failure(__FILE__, __LINE__, "CHECK(" #cond ")"); \
    } while (0)

#define CHECK_EQ(a, b)                                                             \
    do {                                                                           \
        const auto& rt_a_ = (a);                                                   \
        const auto& rt_b_ = (b);                                                   \
        if (!(rt_a_ == rt_b_))                                                     \
            ::rt90test::report_failure(__FILE__, __LINE__,                         \
                "CHECK_EQ(" #a ", " #b "): " + ::rt90test::to_str(rt_a_) + " != " + ::rt90test::to_str(rt_b_)); \
    } while (0)

namespace rt90test {
template <typename T>
std::string to_str(const T& v) {
    if constexpr (std::is_convertible_v<T, std::string>) return "\"" + std::string(v) + "\"";
    else if constexpr (std::is_enum_v<T>) return std::to_string(static_cast<long long>(v));
    else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
    else if constexpr (std::is_arithmetic_v<T>) return std::to_string(v);
    else return "<?>";
}
}  // namespace rt90test
