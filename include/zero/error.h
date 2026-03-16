#ifndef ZERO_ERROR_H
#define ZERO_ERROR_H

#include <array>
#include <utility>
#include <expected>
#include <system_error>

#if __has_include(<stacktrace>)
#include <stacktrace>
#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
#include <fmt/core.h>
#endif
#endif

/*
 * When std::error_category is defined as a static variable,
 * its destruction timing is uncertain and it may be invalid when the program exits.
 * The implementation of each standard library has been changed to avoid the destruction of std::error_category:
 * - https://github.com/llvm/llvm-project/commit/fbdf684fae5243e7a9ff50dd4abdc5b55e6aa895
 * - https://github.com/gcc-mirror/gcc/commit/dd396a321be5099536af36e64454c1fcf9d67e12
 *
 * When static variables are initialized, `atexit` may be used to register the destructor,
 * which may cause additional problems in Windows UCRT:
 * - https://developercommunity.visualstudio.com/t/atexit-deadlock-with-thread-safe-static-/1654756
 *
 * If you compile the following code using Clang + MSVC, the program will deadlock when exiting:
 * ```
 * #include <thread>
 * #include <system_error>
 *
 * struct OnTeardown {
 *     ~OnTeardown() {
 *         std::thread([] {
 *             std::ignore = std::system_category();
 *         }).join();
 *     }
 * };
 *
 * int main() {
 *     static OnTeardown s;
 *     return 0;
 * }
 * ```
 *
 * However, it can exit normally when compiled with MSVC alone,
 * because Microsoft's implementation uses `[[msvc::noop_dtor]]` to skip the destructor:
 * - https://github.com/microsoft/STL/pull/1016/files
 *
 * Perhaps in newer compilers we can define the std::error_category singleton directly using `static constexpr`,
 * but for now, I'll choose to use `placement new` to solve everything simply and directly.
 */

namespace zero::error {
    template<typename T>
    auto categoryImmortalize() {
        alignas(T) std::array<std::byte, sizeof(T)> storage{};
        new(storage.data()) T();
        return storage;
    }

    template<typename T>
    const T &categoryInstance() {
        static const auto storage = categoryImmortalize<T>();
        return *reinterpret_cast<const T *>(storage.data());
    }
}

#define Z_ERROR_EXPAND(x) x
#define Z_ERROR_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, NAME,...) NAME

#define Z_ERROR_PASTE(...) Z_ERROR_EXPAND(Z_ERROR_GET_MACRO(__VA_ARGS__, \
        Z_ERROR_PASTE64, \
        Z_ERROR_PASTE63, \
        Z_ERROR_PASTE62, \
        Z_ERROR_PASTE61, \
        Z_ERROR_PASTE60, \
        Z_ERROR_PASTE59, \
        Z_ERROR_PASTE58, \
        Z_ERROR_PASTE57, \
        Z_ERROR_PASTE56, \
        Z_ERROR_PASTE55, \
        Z_ERROR_PASTE54, \
        Z_ERROR_PASTE53, \
        Z_ERROR_PASTE52, \
        Z_ERROR_PASTE51, \
        Z_ERROR_PASTE50, \
        Z_ERROR_PASTE49, \
        Z_ERROR_PASTE48, \
        Z_ERROR_PASTE47, \
        Z_ERROR_PASTE46, \
        Z_ERROR_PASTE45, \
        Z_ERROR_PASTE44, \
        Z_ERROR_PASTE43, \
        Z_ERROR_PASTE42, \
        Z_ERROR_PASTE41, \
        Z_ERROR_PASTE40, \
        Z_ERROR_PASTE39, \
        Z_ERROR_PASTE38, \
        Z_ERROR_PASTE37, \
        Z_ERROR_PASTE36, \
        Z_ERROR_PASTE35, \
        Z_ERROR_PASTE34, \
        Z_ERROR_PASTE33, \
        Z_ERROR_PASTE32, \
        Z_ERROR_PASTE31, \
        Z_ERROR_PASTE30, \
        Z_ERROR_PASTE29, \
        Z_ERROR_PASTE28, \
        Z_ERROR_PASTE27, \
        Z_ERROR_PASTE26, \
        Z_ERROR_PASTE25, \
        Z_ERROR_PASTE24, \
        Z_ERROR_PASTE23, \
        Z_ERROR_PASTE22, \
        Z_ERROR_PASTE21, \
        Z_ERROR_PASTE20, \
        Z_ERROR_PASTE19, \
        Z_ERROR_PASTE18, \
        Z_ERROR_PASTE17, \
        Z_ERROR_PASTE16, \
        Z_ERROR_PASTE15, \
        Z_ERROR_PASTE14, \
        Z_ERROR_PASTE13, \
        Z_ERROR_PASTE12, \
        Z_ERROR_PASTE11, \
        Z_ERROR_PASTE10, \
        Z_ERROR_PASTE9, \
        Z_ERROR_PASTE8, \
        Z_ERROR_PASTE7, \
        Z_ERROR_PASTE6, \
        Z_ERROR_PASTE5, \
        Z_ERROR_PASTE4, \
        Z_ERROR_PASTE3, \
        Z_ERROR_PASTE2, \
        Z_ERROR_PASTE1)(__VA_ARGS__))

#define Z_ERROR_PASTE2(func, v1) func(v1)
#define Z_ERROR_PASTE3(func, v1, v2) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE2(func, v2)
#define Z_ERROR_PASTE4(func, v1, v2, v3) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE3(func, v2, v3)
#define Z_ERROR_PASTE5(func, v1, v2, v3, v4) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE4(func, v2, v3, v4)
#define Z_ERROR_PASTE6(func, v1, v2, v3, v4, v5) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE5(func, v2, v3, v4, v5)
#define Z_ERROR_PASTE7(func, v1, v2, v3, v4, v5, v6) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE6(func, v2, v3, v4, v5, v6)
#define Z_ERROR_PASTE8(func, v1, v2, v3, v4, v5, v6, v7) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE7(func, v2, v3, v4, v5, v6, v7)
#define Z_ERROR_PASTE9(func, v1, v2, v3, v4, v5, v6, v7, v8) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE8(func, v2, v3, v4, v5, v6, v7, v8)
#define Z_ERROR_PASTE10(func, v1, v2, v3, v4, v5, v6, v7, v8, v9) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE9(func, v2, v3, v4, v5, v6, v7, v8, v9)
#define Z_ERROR_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE10(func, v2, v3, v4, v5, v6, v7, v8, v9, v10)
#define Z_ERROR_PASTE12(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE11(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11)
#define Z_ERROR_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE12(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define Z_ERROR_PASTE14(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE13(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13)
#define Z_ERROR_PASTE15(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE14(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14)
#define Z_ERROR_PASTE16(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE15(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15)
#define Z_ERROR_PASTE17(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE16(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)
#define Z_ERROR_PASTE18(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE17(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17)
#define Z_ERROR_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE18(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define Z_ERROR_PASTE20(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE19(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19)
#define Z_ERROR_PASTE21(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE20(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20)
#define Z_ERROR_PASTE22(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE21(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21)
#define Z_ERROR_PASTE23(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE22(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22)
#define Z_ERROR_PASTE24(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE23(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23)
#define Z_ERROR_PASTE25(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE24(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24)
#define Z_ERROR_PASTE26(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE25(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25)
#define Z_ERROR_PASTE27(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE26(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26)
#define Z_ERROR_PASTE28(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE27(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27)
#define Z_ERROR_PASTE29(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE28(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28)
#define Z_ERROR_PASTE30(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE29(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29)
#define Z_ERROR_PASTE31(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE30(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30)
#define Z_ERROR_PASTE32(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE31(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31)
#define Z_ERROR_PASTE33(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE32(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32)
#define Z_ERROR_PASTE34(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE33(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33)
#define Z_ERROR_PASTE35(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE34(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34)
#define Z_ERROR_PASTE36(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE35(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35)
#define Z_ERROR_PASTE37(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE36(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36)
#define Z_ERROR_PASTE38(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE37(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37)
#define Z_ERROR_PASTE39(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE38(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38)
#define Z_ERROR_PASTE40(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE39(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39)
#define Z_ERROR_PASTE41(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE40(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40)
#define Z_ERROR_PASTE42(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE41(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41)
#define Z_ERROR_PASTE43(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE42(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42)
#define Z_ERROR_PASTE44(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE43(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43)
#define Z_ERROR_PASTE45(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE44(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44)
#define Z_ERROR_PASTE46(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE45(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45)
#define Z_ERROR_PASTE47(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE46(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46)
#define Z_ERROR_PASTE48(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE47(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47)
#define Z_ERROR_PASTE49(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE48(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48)
#define Z_ERROR_PASTE50(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE49(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49)
#define Z_ERROR_PASTE51(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE50(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50)
#define Z_ERROR_PASTE52(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE51(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51)
#define Z_ERROR_PASTE53(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE52(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52)
#define Z_ERROR_PASTE54(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE53(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53)
#define Z_ERROR_PASTE55(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE54(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54)
#define Z_ERROR_PASTE56(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE55(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55)
#define Z_ERROR_PASTE57(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE56(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56)
#define Z_ERROR_PASTE58(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE57(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57)
#define Z_ERROR_PASTE59(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE58(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58)
#define Z_ERROR_PASTE60(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE59(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59)
#define Z_ERROR_PASTE61(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE60(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60)
#define Z_ERROR_PASTE62(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE61(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61)
#define Z_ERROR_PASTE63(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE62(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62)
#define Z_ERROR_PASTE64(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63) Z_ERROR_PASTE2(func, v1) Z_ERROR_PASTE63(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63)

#define Z_ERROR_DOUBLE_PASTE(...) Z_ERROR_EXPAND(Z_ERROR_GET_MACRO(__VA_ARGS__, \
        Z_ERROR_DOUBLE_PASTE63, \
        Z_ERROR_DOUBLE_PASTE63, \
        Z_ERROR_DOUBLE_PASTE61, \
        Z_ERROR_DOUBLE_PASTE61, \
        Z_ERROR_DOUBLE_PASTE59, \
        Z_ERROR_DOUBLE_PASTE59, \
        Z_ERROR_DOUBLE_PASTE57, \
        Z_ERROR_DOUBLE_PASTE57, \
        Z_ERROR_DOUBLE_PASTE55, \
        Z_ERROR_DOUBLE_PASTE55, \
        Z_ERROR_DOUBLE_PASTE53, \
        Z_ERROR_DOUBLE_PASTE53, \
        Z_ERROR_DOUBLE_PASTE51, \
        Z_ERROR_DOUBLE_PASTE51, \
        Z_ERROR_DOUBLE_PASTE49, \
        Z_ERROR_DOUBLE_PASTE49, \
        Z_ERROR_DOUBLE_PASTE47, \
        Z_ERROR_DOUBLE_PASTE47, \
        Z_ERROR_DOUBLE_PASTE45, \
        Z_ERROR_DOUBLE_PASTE45, \
        Z_ERROR_DOUBLE_PASTE43, \
        Z_ERROR_DOUBLE_PASTE43, \
        Z_ERROR_DOUBLE_PASTE41, \
        Z_ERROR_DOUBLE_PASTE41, \
        Z_ERROR_DOUBLE_PASTE39, \
        Z_ERROR_DOUBLE_PASTE39, \
        Z_ERROR_DOUBLE_PASTE37, \
        Z_ERROR_DOUBLE_PASTE37, \
        Z_ERROR_DOUBLE_PASTE35, \
        Z_ERROR_DOUBLE_PASTE35, \
        Z_ERROR_DOUBLE_PASTE33, \
        Z_ERROR_DOUBLE_PASTE33, \
        Z_ERROR_DOUBLE_PASTE31, \
        Z_ERROR_DOUBLE_PASTE31, \
        Z_ERROR_DOUBLE_PASTE29, \
        Z_ERROR_DOUBLE_PASTE29, \
        Z_ERROR_DOUBLE_PASTE27, \
        Z_ERROR_DOUBLE_PASTE27, \
        Z_ERROR_DOUBLE_PASTE25, \
        Z_ERROR_DOUBLE_PASTE25, \
        Z_ERROR_DOUBLE_PASTE23, \
        Z_ERROR_DOUBLE_PASTE23, \
        Z_ERROR_DOUBLE_PASTE21, \
        Z_ERROR_DOUBLE_PASTE21, \
        Z_ERROR_DOUBLE_PASTE19, \
        Z_ERROR_DOUBLE_PASTE19, \
        Z_ERROR_DOUBLE_PASTE17, \
        Z_ERROR_DOUBLE_PASTE17, \
        Z_ERROR_DOUBLE_PASTE15, \
        Z_ERROR_DOUBLE_PASTE15, \
        Z_ERROR_DOUBLE_PASTE13, \
        Z_ERROR_DOUBLE_PASTE13, \
        Z_ERROR_DOUBLE_PASTE11, \
        Z_ERROR_DOUBLE_PASTE11, \
        Z_ERROR_DOUBLE_PASTE9, \
        Z_ERROR_DOUBLE_PASTE9, \
        Z_ERROR_DOUBLE_PASTE7, \
        Z_ERROR_DOUBLE_PASTE7, \
        Z_ERROR_DOUBLE_PASTE5, \
        Z_ERROR_DOUBLE_PASTE5, \
        Z_ERROR_DOUBLE_PASTE3, \
        Z_ERROR_DOUBLE_PASTE3, \
        Z_ERROR_DOUBLE_PASTE1, \
        Z_ERROR_DOUBLE_PASTE1)(__VA_ARGS__))

#define Z_ERROR_DOUBLE_PASTE3(func, v1, v2) func(v1, v2)
#define Z_ERROR_DOUBLE_PASTE5(func, v1, v2, v3, v4) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE3(func, v3, v4)
#define Z_ERROR_DOUBLE_PASTE7(func, v1, v2, v3, v4, v5, v6) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE5(func, v3, v4, v5, v6)
#define Z_ERROR_DOUBLE_PASTE9(func, v1, v2, v3, v4, v5, v6, v7, v8) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE7(func, v3, v4, v5, v6, v7, v8)
#define Z_ERROR_DOUBLE_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE9(func, v3, v4, v5, v6, v7, v8, v9, v10)
#define Z_ERROR_DOUBLE_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE11(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define Z_ERROR_DOUBLE_PASTE15(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE13(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14)
#define Z_ERROR_DOUBLE_PASTE17(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE15(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)
#define Z_ERROR_DOUBLE_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE17(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define Z_ERROR_DOUBLE_PASTE21(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE19(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20)
#define Z_ERROR_DOUBLE_PASTE23(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE21(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22)
#define Z_ERROR_DOUBLE_PASTE25(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE23(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24)
#define Z_ERROR_DOUBLE_PASTE27(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE25(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26)
#define Z_ERROR_DOUBLE_PASTE29(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE27(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28)
#define Z_ERROR_DOUBLE_PASTE31(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE29(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30)
#define Z_ERROR_DOUBLE_PASTE33(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE31(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32)
#define Z_ERROR_DOUBLE_PASTE35(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE33(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34)
#define Z_ERROR_DOUBLE_PASTE37(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE35(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36)
#define Z_ERROR_DOUBLE_PASTE39(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE37(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38)
#define Z_ERROR_DOUBLE_PASTE41(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE39(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40)
#define Z_ERROR_DOUBLE_PASTE43(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE41(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42)
#define Z_ERROR_DOUBLE_PASTE45(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE43(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44)
#define Z_ERROR_DOUBLE_PASTE47(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE45(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46)
#define Z_ERROR_DOUBLE_PASTE49(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE47(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48)
#define Z_ERROR_DOUBLE_PASTE51(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE49(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50)
#define Z_ERROR_DOUBLE_PASTE53(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE51(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52)
#define Z_ERROR_DOUBLE_PASTE55(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE53(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54)
#define Z_ERROR_DOUBLE_PASTE57(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE55(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56)
#define Z_ERROR_DOUBLE_PASTE59(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE57(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58)
#define Z_ERROR_DOUBLE_PASTE61(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE59(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60)
#define Z_ERROR_DOUBLE_PASTE63(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62) Z_ERROR_DOUBLE_PASTE3(func, v1, v2) Z_ERROR_DOUBLE_PASTE61(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62)

#define Z_ERROR_TRIPLE_PASTE(...) Z_ERROR_EXPAND(Z_ERROR_GET_MACRO(__VA_ARGS__, \
        Z_ERROR_TRIPLE_PASTE64, \
        Z_ERROR_TRIPLE_PASTE61, \
        Z_ERROR_TRIPLE_PASTE61, \
        Z_ERROR_TRIPLE_PASTE61, \
        Z_ERROR_TRIPLE_PASTE58, \
        Z_ERROR_TRIPLE_PASTE58, \
        Z_ERROR_TRIPLE_PASTE58, \
        Z_ERROR_TRIPLE_PASTE55, \
        Z_ERROR_TRIPLE_PASTE55, \
        Z_ERROR_TRIPLE_PASTE55, \
        Z_ERROR_TRIPLE_PASTE52, \
        Z_ERROR_TRIPLE_PASTE52, \
        Z_ERROR_TRIPLE_PASTE52, \
        Z_ERROR_TRIPLE_PASTE49, \
        Z_ERROR_TRIPLE_PASTE49, \
        Z_ERROR_TRIPLE_PASTE49, \
        Z_ERROR_TRIPLE_PASTE46, \
        Z_ERROR_TRIPLE_PASTE46, \
        Z_ERROR_TRIPLE_PASTE46, \
        Z_ERROR_TRIPLE_PASTE43, \
        Z_ERROR_TRIPLE_PASTE43, \
        Z_ERROR_TRIPLE_PASTE43, \
        Z_ERROR_TRIPLE_PASTE40, \
        Z_ERROR_TRIPLE_PASTE40, \
        Z_ERROR_TRIPLE_PASTE40, \
        Z_ERROR_TRIPLE_PASTE37, \
        Z_ERROR_TRIPLE_PASTE37, \
        Z_ERROR_TRIPLE_PASTE37, \
        Z_ERROR_TRIPLE_PASTE34, \
        Z_ERROR_TRIPLE_PASTE34, \
        Z_ERROR_TRIPLE_PASTE34, \
        Z_ERROR_TRIPLE_PASTE31, \
        Z_ERROR_TRIPLE_PASTE31, \
        Z_ERROR_TRIPLE_PASTE31, \
        Z_ERROR_TRIPLE_PASTE28, \
        Z_ERROR_TRIPLE_PASTE28, \
        Z_ERROR_TRIPLE_PASTE28, \
        Z_ERROR_TRIPLE_PASTE25, \
        Z_ERROR_TRIPLE_PASTE25, \
        Z_ERROR_TRIPLE_PASTE25, \
        Z_ERROR_TRIPLE_PASTE22, \
        Z_ERROR_TRIPLE_PASTE22, \
        Z_ERROR_TRIPLE_PASTE22, \
        Z_ERROR_TRIPLE_PASTE19, \
        Z_ERROR_TRIPLE_PASTE19, \
        Z_ERROR_TRIPLE_PASTE19, \
        Z_ERROR_TRIPLE_PASTE16, \
        Z_ERROR_TRIPLE_PASTE16, \
        Z_ERROR_TRIPLE_PASTE16, \
        Z_ERROR_TRIPLE_PASTE13, \
        Z_ERROR_TRIPLE_PASTE13, \
        Z_ERROR_TRIPLE_PASTE13, \
        Z_ERROR_TRIPLE_PASTE10, \
        Z_ERROR_TRIPLE_PASTE10, \
        Z_ERROR_TRIPLE_PASTE10, \
        Z_ERROR_TRIPLE_PASTE7, \
        Z_ERROR_TRIPLE_PASTE7, \
        Z_ERROR_TRIPLE_PASTE7, \
        Z_ERROR_TRIPLE_PASTE4, \
        Z_ERROR_TRIPLE_PASTE4, \
        Z_ERROR_TRIPLE_PASTE4, \
        Z_ERROR_TRIPLE_PASTE1, \
        Z_ERROR_TRIPLE_PASTE1, \
        Z_ERROR_TRIPLE_PASTE1)(__VA_ARGS__))

#define Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) func(v1, v2, v3)
#define Z_ERROR_TRIPLE_PASTE7(func, v1, v2, v3, v4, v5, v6) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE4(func, v4, v5, v6)
#define Z_ERROR_TRIPLE_PASTE10(func, v1, v2, v3, v4, v5, v6, v7, v8, v9) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE7(func, v4, v5, v6, v7, v8, v9)
#define Z_ERROR_TRIPLE_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE10(func, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define Z_ERROR_TRIPLE_PASTE16(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE13(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15)
#define Z_ERROR_TRIPLE_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE16(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define Z_ERROR_TRIPLE_PASTE22(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE19(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21)
#define Z_ERROR_TRIPLE_PASTE25(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE22(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24)
#define Z_ERROR_TRIPLE_PASTE28(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE25(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27)
#define Z_ERROR_TRIPLE_PASTE31(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE28(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30)
#define Z_ERROR_TRIPLE_PASTE34(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE31(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32)
#define Z_ERROR_TRIPLE_PASTE37(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE34(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36)
#define Z_ERROR_TRIPLE_PASTE40(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE37(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39)
#define Z_ERROR_TRIPLE_PASTE43(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE40(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42)
#define Z_ERROR_TRIPLE_PASTE46(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE43(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45)
#define Z_ERROR_TRIPLE_PASTE49(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE46(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48)
#define Z_ERROR_TRIPLE_PASTE52(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE49(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51)
#define Z_ERROR_TRIPLE_PASTE55(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE52(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54)
#define Z_ERROR_TRIPLE_PASTE58(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE55(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57)
#define Z_ERROR_TRIPLE_PASTE61(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60) Z_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) Z_ERROR_TRIPLE_PASTE58(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60)

#define Z_ERROR_ENUM_ITEM(v1, v2) v1,
#define Z_MESSAGE_SWITCH_BRANCH(v1, v2)                                                                         \
    case ErrorType::v1: /* NOLINT(*-branch-clone) */                                                            \
        return v2;

#define Z_DEFINE_MAKE_ERROR_CODE(Type)                                                                          \
    inline std::error_code make_error_code(const Type _e) {                                                     \
        return {std::to_underlying(_e), Type##Category::instance()};                                            \
    }

#define Z_DEFINE_MAKE_ERROR_CODE_INNER(Type)                                                                    \
    friend std::error_code make_error_code(const Type _e) {                                                     \
        return {std::to_underlying(_e), Type##Category::instance()};                                            \
    }

#define Z_DEFINE_ERROR_CODE_TYPES(Type, category, ...)                                                          \
    enum class Type {                                                                                           \
        OK,                                                                                                     \
        Z_ERROR_EXPAND(Z_ERROR_DOUBLE_PASTE(Z_ERROR_ENUM_ITEM, __VA_ARGS__))                                    \
    };                                                                                                          \
                                                                                                                \
    class Type##Category final : public std::error_category {                                                   \
        using ErrorType = Type;                                                                                 \
    public:                                                                                                     \
        static const std::error_category &instance();                                                           \
                                                                                                                \
        [[nodiscard]] const char *name() const noexcept override {                                              \
            return category;                                                                                    \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] std::string message(const int _value) const override {                                    \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            Z_ERROR_EXPAND(Z_ERROR_DOUBLE_PASTE(Z_MESSAGE_SWITCH_BRANCH, __VA_ARGS__))                          \
                                                                                                                \
            default:                                                                                            \
                return "unknown";                                                                               \
            }                                                                                                   \
        }                                                                                                       \
    };

#define Z_DEFINE_ERROR_CODE(Type, category, ...)                                                                \
    Z_DEFINE_ERROR_CODE_TYPES(Type, category, __VA_ARGS__)                                                      \
    Z_DEFINE_MAKE_ERROR_CODE(Type)

#define Z_DEFINE_ERROR_CODE_INNER(Type, category, ...)                                                          \
    Z_DEFINE_ERROR_CODE_TYPES(Type, category, __VA_ARGS__)                                                      \
    Z_DEFINE_MAKE_ERROR_CODE_INNER(Type)

#define Z_ERROR_ENUM_ITEM_EX(v1, v2, v3) v1,
#define Z_MESSAGE_SWITCH_BRANCH_EX(v1, v2, v3)                                                                  \
    case ErrorType::v1: /* NOLINT(*-branch-clone) */                                                            \
        return v2;

#define Z_DEFAULT_ERROR_CONDITION_SWITCH_BRANCH_EX(v1, v2, v3)                                                  \
    case ErrorType::v1: /* NOLINT(*-branch-clone) */                                                            \
        return v3;

#define Z_DEFAULT_ERROR_CONDITION error_category::default_error_condition(_value)

#define Z_DEFINE_ERROR_CODE_TYPES_EX(Type, category, ...)                                                       \
    enum class Type {                                                                                           \
        OK,                                                                                                     \
        Z_ERROR_EXPAND(Z_ERROR_TRIPLE_PASTE(Z_ERROR_ENUM_ITEM_EX, __VA_ARGS__))                                 \
    };                                                                                                          \
                                                                                                                \
    class Type##Category final : public std::error_category {                                                   \
        using ErrorType = Type;                                                                                 \
    public:                                                                                                     \
        static const std::error_category &instance();                                                           \
                                                                                                                \
        [[nodiscard]] const char *name() const noexcept override {                                              \
            return category;                                                                                    \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] std::string message(const int _value) const override {                                    \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            Z_ERROR_EXPAND(Z_ERROR_TRIPLE_PASTE(Z_MESSAGE_SWITCH_BRANCH_EX, __VA_ARGS__))                       \
                                                                                                                \
            default:                                                                                            \
                return "unknown";                                                                               \
            }                                                                                                   \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] std::error_condition default_error_condition(const int _value) const noexcept override {  \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            Z_ERROR_EXPAND(Z_ERROR_TRIPLE_PASTE(Z_DEFAULT_ERROR_CONDITION_SWITCH_BRANCH_EX, __VA_ARGS__)) \
                                                                                                                \
            default:                                                                                            \
                return error_category::default_error_condition(_value);                                         \
            }                                                                                                   \
        }                                                                                                       \
    };

#define Z_DEFINE_ERROR_CODE_EX(Type, category, ...)                                                             \
    Z_DEFINE_ERROR_CODE_TYPES_EX(Type, category, __VA_ARGS__)                                                   \
    Z_DEFINE_MAKE_ERROR_CODE(Type)

#define Z_DEFINE_ERROR_CODE_INNER_EX(Type, category, ...)                                                       \
    Z_DEFINE_ERROR_CODE_TYPES_EX(Type, category, __VA_ARGS__)                                                   \
    Z_DEFINE_MAKE_ERROR_CODE_INNER(Type)

#define Z_DECLARE_ERROR_CODE(Type)                                                                              \
    template<>                                                                                                  \
    struct std::is_error_code_enum<Type> : std::true_type {                                                     \
    };

#define Z_DECLARE_ERROR_CODES(...) Z_ERROR_EXPAND(Z_ERROR_PASTE(Z_DECLARE_ERROR_CODE, __VA_ARGS__))

#define Z_DEFINE_MAKE_ERROR_CONDITION(Type)                                                                     \
    inline std::error_condition make_error_condition(const Type _e) {                                           \
        return {std::to_underlying(_e), Type##Category::instance()};                                            \
    }

#define Z_DEFINE_MAKE_ERROR_CONDITION_INNER(Type)                                                               \
    friend std::error_condition make_error_condition(const Type _e) {                                           \
        return {std::to_underlying(_e), Type##Category::instance()};                                            \
    }

#define Z_DEFINE_ERROR_CONDITION_TYPES(Type, category, ...)                                                     \
    enum class Type {                                                                                           \
        OK,                                                                                                     \
        Z_ERROR_EXPAND(Z_ERROR_DOUBLE_PASTE(Z_ERROR_ENUM_ITEM, __VA_ARGS__))                                    \
    };                                                                                                          \
                                                                                                                \
    class Type##Category final : public std::error_category {                                                   \
        using ErrorType = Type;                                                                                 \
    public:                                                                                                     \
        static const std::error_category &instance();                                                           \
                                                                                                                \
        [[nodiscard]] const char *name() const noexcept override {                                              \
            return category;                                                                                    \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] std::string message(const int _value) const override {                                    \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            Z_ERROR_EXPAND(Z_ERROR_DOUBLE_PASTE(Z_MESSAGE_SWITCH_BRANCH, __VA_ARGS__))                          \
                                                                                                                \
            default:                                                                                            \
                return "unknown";                                                                               \
            }                                                                                                   \
        }                                                                                                       \
    };

#define Z_DEFINE_ERROR_CONDITION(Type, category, ...)                                                           \
    Z_DEFINE_ERROR_CONDITION_TYPES(Type, category, __VA_ARGS__)                                                 \
    Z_DEFINE_MAKE_ERROR_CONDITION(Type)

#define Z_DEFINE_ERROR_CONDITION_INNER(Type, category, ...)                                                     \
    Z_DEFINE_ERROR_CONDITION_TYPES(Type, category, __VA_ARGS__)                                                 \
    Z_DEFINE_MAKE_ERROR_CONDITION_INNER(Type)

#define Z_EQUIVALENT_SWITCH_BRANCH_EX(v1, v2, v3)                                                               \
    case ErrorType::v1: /* NOLINT(*-branch-clone) */                                                            \
        return v3(_code);

#define Z_DEFINE_ERROR_CONDITION_TYPES_EX(Type, category, ...)                                                  \
    enum class Type {                                                                                           \
        OK,                                                                                                     \
        Z_ERROR_EXPAND(Z_ERROR_TRIPLE_PASTE(Z_ERROR_ENUM_ITEM_EX, __VA_ARGS__))                                 \
    };                                                                                                          \
                                                                                                                \
    class Type##Category final : public std::error_category {                                                   \
        using ErrorType = Type;                                                                                 \
    public:                                                                                                     \
        static const std::error_category &instance();                                                           \
                                                                                                                \
        [[nodiscard]] const char *name() const noexcept override {                                              \
            return category;                                                                                    \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] std::string message(const int _value) const override {                                    \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            Z_ERROR_EXPAND(Z_ERROR_TRIPLE_PASTE(Z_MESSAGE_SWITCH_BRANCH_EX, __VA_ARGS__))                       \
                                                                                                                \
            default:                                                                                            \
                return "unknown";                                                                               \
            }                                                                                                   \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] bool equivalent(const std::error_code &_code, const int _value) const noexcept override { \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            Z_ERROR_EXPAND(Z_ERROR_TRIPLE_PASTE(Z_EQUIVALENT_SWITCH_BRANCH_EX, __VA_ARGS__))                    \
                                                                                                                \
            default:                                                                                            \
                return false;                                                                                   \
            }                                                                                                   \
        }                                                                                                       \
    };

#define Z_DEFINE_ERROR_CONDITION_EX(Type, category, ...)                                                        \
    Z_DEFINE_ERROR_CONDITION_TYPES_EX(Type, category, __VA_ARGS__)                                              \
    Z_DEFINE_MAKE_ERROR_CONDITION(Type)

#define Z_DEFINE_ERROR_CONDITION_INNER_EX(Type, category, ...)                                                  \
    Z_DEFINE_ERROR_CONDITION_TYPES_EX(Type, category, __VA_ARGS__)                                              \
    Z_DEFINE_MAKE_ERROR_CONDITION_INNER(Type)

#define Z_DECLARE_ERROR_CONDITION(Type)                                                                         \
    template<>                                                                                                  \
    struct std::is_error_condition_enum<Type> : std::true_type {                                                \
    };

#define Z_DECLARE_ERROR_CONDITIONS(...) Z_ERROR_EXPAND(Z_ERROR_PASTE(Z_DECLARE_ERROR_CONDITION, __VA_ARGS__))

#define Z_DEFINE_ERROR_TRANSFORMER_TYPES(Type, category, stringify)                                             \
    enum class Type {                                                                                           \
    };                                                                                                          \
                                                                                                                \
    class Type##Category final : public std::error_category {                                                   \
        using ErrorType = Type;                                                                                 \
    public:                                                                                                     \
        static const std::error_category &instance();                                                           \
                                                                                                                \
        [[nodiscard]] const char *name() const noexcept override {                                              \
            return category;                                                                                    \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] std::string message(const int _value) const override {                                    \
            return stringify(_value);                                                                           \
        }                                                                                                       \
    };

#define Z_DEFINE_ERROR_TRANSFORMER(Type, category, stringify)                                                   \
    Z_DEFINE_ERROR_TRANSFORMER_TYPES(Type, category, stringify)                                                 \
    Z_DEFINE_MAKE_ERROR_CODE(Type)

#define Z_DEFINE_ERROR_TRANSFORMER_INNER(Type, category, stringify)                                             \
    Z_DEFINE_ERROR_TRANSFORMER_TYPES(Type, category, stringify)                                                 \
    Z_DEFINE_MAKE_ERROR_CODE_INNER(Type)

#define Z_DEFINE_ERROR_TRANSFORMER_TYPES_EX(Type, category, stringify, classify)                                \
    enum class Type {                                                                                           \
    };                                                                                                          \
                                                                                                                \
    class Type##Category final : public std::error_category {                                                   \
    public:                                                                                                     \
        static const std::error_category &instance();                                                           \
                                                                                                                \
        [[nodiscard]] const char *name() const noexcept override {                                              \
            return category;                                                                                    \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] std::string message(const int _value) const override {                                    \
            return stringify(_value);                                                                           \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] std::error_condition default_error_condition(const int _value) const noexcept override {  \
            return classify(_value).value_or(error_category::default_error_condition(_value));                  \
        }                                                                                                       \
    };

#define Z_DEFINE_ERROR_TRANSFORMER_EX(Type, category, stringify, classify)                                      \
    Z_DEFINE_ERROR_TRANSFORMER_TYPES_EX(Type, category, stringify, classify)                                    \
    Z_DEFINE_MAKE_ERROR_CODE(Type)

#define Z_DEFINE_ERROR_TRANSFORMER_INNER_EX(Type, category, stringify, classify)                                \
    Z_DEFINE_ERROR_TRANSFORMER_TYPES_EX(Type, category, stringify, classify)                                    \
    Z_DEFINE_MAKE_ERROR_CODE_INNER(Type)

#define Z_DEFINE_ERROR_CATEGORY_INSTANCE(Type)                                                                  \
    const std::error_category &Type##Category::instance() {                                                     \
        return zero::error::categoryInstance<Type##Category>();                                                 \
    }

#define Z_DEFINE_ERROR_CATEGORY_INSTANCES(...)                                                                  \
    Z_ERROR_EXPAND(Z_ERROR_PASTE(Z_DEFINE_ERROR_CATEGORY_INSTANCE, __VA_ARGS__))

namespace zero::error {
#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
    template<typename T>
        requires std::derived_from<T, std::exception>
    class StacktraceError final : public T {
    public:
        template<typename... Args>
        explicit StacktraceError(Args &&... args)
            : T{std::forward<Args>(args)...},
              mMessage{fmt::format("{} {}", T::what(), std::to_string(std::stacktrace::current(1)))} {
        }

        [[nodiscard]] const char *what() const noexcept override {
            return mMessage.c_str();
        }

    private:
        std::string mMessage;
    };
#else
    template<typename T>
        requires std::derived_from<T, std::exception>
    using StacktraceError = T;
#endif

    template<typename T, typename E>
        requires std::is_convertible_v<E, std::error_code>
    T guard(std::expected<T, E> &&expected) {
        if (!expected)
            throw StacktraceError<std::system_error>{expected.error()};

        if constexpr (std::is_void_v<T>)
            return;
        else
            return *std::move(expected);
    }
}

#endif //ZERO_ERROR_H
