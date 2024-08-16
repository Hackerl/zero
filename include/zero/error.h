#ifndef ZERO_ERROR_H
#define ZERO_ERROR_H

#include <array>
#include <utility>
#include <system_error>

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

template<typename T>
auto errorCategoryImmortalize() {
    std::array<std::byte, sizeof(T)> storage = {};
    new(storage.data()) T();
    return storage;
}

template<typename T>
const T &errorCategoryInstance() {
    static const auto storage = errorCategoryImmortalize<T>();
    return *reinterpret_cast<const T *>(storage.data());
}

#define ZERO_ERROR_EXPAND(x) x
#define ZERO_ERROR_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, NAME,...) NAME

#define ZERO_ERROR_PASTE(...) ZERO_ERROR_EXPAND(ZERO_ERROR_GET_MACRO(__VA_ARGS__, \
        ZERO_ERROR_PASTE64, \
        ZERO_ERROR_PASTE63, \
        ZERO_ERROR_PASTE62, \
        ZERO_ERROR_PASTE61, \
        ZERO_ERROR_PASTE60, \
        ZERO_ERROR_PASTE59, \
        ZERO_ERROR_PASTE58, \
        ZERO_ERROR_PASTE57, \
        ZERO_ERROR_PASTE56, \
        ZERO_ERROR_PASTE55, \
        ZERO_ERROR_PASTE54, \
        ZERO_ERROR_PASTE53, \
        ZERO_ERROR_PASTE52, \
        ZERO_ERROR_PASTE51, \
        ZERO_ERROR_PASTE50, \
        ZERO_ERROR_PASTE49, \
        ZERO_ERROR_PASTE48, \
        ZERO_ERROR_PASTE47, \
        ZERO_ERROR_PASTE46, \
        ZERO_ERROR_PASTE45, \
        ZERO_ERROR_PASTE44, \
        ZERO_ERROR_PASTE43, \
        ZERO_ERROR_PASTE42, \
        ZERO_ERROR_PASTE41, \
        ZERO_ERROR_PASTE40, \
        ZERO_ERROR_PASTE39, \
        ZERO_ERROR_PASTE38, \
        ZERO_ERROR_PASTE37, \
        ZERO_ERROR_PASTE36, \
        ZERO_ERROR_PASTE35, \
        ZERO_ERROR_PASTE34, \
        ZERO_ERROR_PASTE33, \
        ZERO_ERROR_PASTE32, \
        ZERO_ERROR_PASTE31, \
        ZERO_ERROR_PASTE30, \
        ZERO_ERROR_PASTE29, \
        ZERO_ERROR_PASTE28, \
        ZERO_ERROR_PASTE27, \
        ZERO_ERROR_PASTE26, \
        ZERO_ERROR_PASTE25, \
        ZERO_ERROR_PASTE24, \
        ZERO_ERROR_PASTE23, \
        ZERO_ERROR_PASTE22, \
        ZERO_ERROR_PASTE21, \
        ZERO_ERROR_PASTE20, \
        ZERO_ERROR_PASTE19, \
        ZERO_ERROR_PASTE18, \
        ZERO_ERROR_PASTE17, \
        ZERO_ERROR_PASTE16, \
        ZERO_ERROR_PASTE15, \
        ZERO_ERROR_PASTE14, \
        ZERO_ERROR_PASTE13, \
        ZERO_ERROR_PASTE12, \
        ZERO_ERROR_PASTE11, \
        ZERO_ERROR_PASTE10, \
        ZERO_ERROR_PASTE9, \
        ZERO_ERROR_PASTE8, \
        ZERO_ERROR_PASTE7, \
        ZERO_ERROR_PASTE6, \
        ZERO_ERROR_PASTE5, \
        ZERO_ERROR_PASTE4, \
        ZERO_ERROR_PASTE3, \
        ZERO_ERROR_PASTE2, \
        ZERO_ERROR_PASTE1)(__VA_ARGS__))

#define ZERO_ERROR_PASTE2(func, v1) func(v1)
#define ZERO_ERROR_PASTE3(func, v1, v2) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE2(func, v2)
#define ZERO_ERROR_PASTE4(func, v1, v2, v3) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE3(func, v2, v3)
#define ZERO_ERROR_PASTE5(func, v1, v2, v3, v4) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE4(func, v2, v3, v4)
#define ZERO_ERROR_PASTE6(func, v1, v2, v3, v4, v5) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE5(func, v2, v3, v4, v5)
#define ZERO_ERROR_PASTE7(func, v1, v2, v3, v4, v5, v6) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE6(func, v2, v3, v4, v5, v6)
#define ZERO_ERROR_PASTE8(func, v1, v2, v3, v4, v5, v6, v7) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE7(func, v2, v3, v4, v5, v6, v7)
#define ZERO_ERROR_PASTE9(func, v1, v2, v3, v4, v5, v6, v7, v8) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE8(func, v2, v3, v4, v5, v6, v7, v8)
#define ZERO_ERROR_PASTE10(func, v1, v2, v3, v4, v5, v6, v7, v8, v9) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE9(func, v2, v3, v4, v5, v6, v7, v8, v9)
#define ZERO_ERROR_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE10(func, v2, v3, v4, v5, v6, v7, v8, v9, v10)
#define ZERO_ERROR_PASTE12(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE11(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11)
#define ZERO_ERROR_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE12(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define ZERO_ERROR_PASTE14(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE13(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13)
#define ZERO_ERROR_PASTE15(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE14(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14)
#define ZERO_ERROR_PASTE16(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE15(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15)
#define ZERO_ERROR_PASTE17(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE16(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)
#define ZERO_ERROR_PASTE18(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE17(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17)
#define ZERO_ERROR_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE18(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define ZERO_ERROR_PASTE20(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE19(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19)
#define ZERO_ERROR_PASTE21(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE20(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20)
#define ZERO_ERROR_PASTE22(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE21(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21)
#define ZERO_ERROR_PASTE23(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE22(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22)
#define ZERO_ERROR_PASTE24(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE23(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23)
#define ZERO_ERROR_PASTE25(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE24(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24)
#define ZERO_ERROR_PASTE26(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE25(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25)
#define ZERO_ERROR_PASTE27(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE26(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26)
#define ZERO_ERROR_PASTE28(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE27(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27)
#define ZERO_ERROR_PASTE29(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE28(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28)
#define ZERO_ERROR_PASTE30(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE29(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29)
#define ZERO_ERROR_PASTE31(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE30(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30)
#define ZERO_ERROR_PASTE32(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE31(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31)
#define ZERO_ERROR_PASTE33(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE32(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32)
#define ZERO_ERROR_PASTE34(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE33(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33)
#define ZERO_ERROR_PASTE35(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE34(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34)
#define ZERO_ERROR_PASTE36(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE35(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35)
#define ZERO_ERROR_PASTE37(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE36(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36)
#define ZERO_ERROR_PASTE38(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE37(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37)
#define ZERO_ERROR_PASTE39(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE38(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38)
#define ZERO_ERROR_PASTE40(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE39(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39)
#define ZERO_ERROR_PASTE41(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE40(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40)
#define ZERO_ERROR_PASTE42(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE41(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41)
#define ZERO_ERROR_PASTE43(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE42(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42)
#define ZERO_ERROR_PASTE44(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE43(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43)
#define ZERO_ERROR_PASTE45(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE44(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44)
#define ZERO_ERROR_PASTE46(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE45(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45)
#define ZERO_ERROR_PASTE47(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE46(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46)
#define ZERO_ERROR_PASTE48(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE47(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47)
#define ZERO_ERROR_PASTE49(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE48(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48)
#define ZERO_ERROR_PASTE50(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE49(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49)
#define ZERO_ERROR_PASTE51(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE50(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50)
#define ZERO_ERROR_PASTE52(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE51(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51)
#define ZERO_ERROR_PASTE53(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE52(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52)
#define ZERO_ERROR_PASTE54(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE53(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53)
#define ZERO_ERROR_PASTE55(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE54(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54)
#define ZERO_ERROR_PASTE56(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE55(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55)
#define ZERO_ERROR_PASTE57(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE56(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56)
#define ZERO_ERROR_PASTE58(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE57(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57)
#define ZERO_ERROR_PASTE59(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE58(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58)
#define ZERO_ERROR_PASTE60(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE59(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59)
#define ZERO_ERROR_PASTE61(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE60(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60)
#define ZERO_ERROR_PASTE62(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE61(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61)
#define ZERO_ERROR_PASTE63(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE62(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62)
#define ZERO_ERROR_PASTE64(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63) ZERO_ERROR_PASTE2(func, v1) ZERO_ERROR_PASTE63(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63)

#define ZERO_ERROR_DOUBLE_PASTE(...) ZERO_ERROR_EXPAND(ZERO_ERROR_GET_MACRO(__VA_ARGS__, \
        ZERO_ERROR_DOUBLE_PASTE63, \
        ZERO_ERROR_DOUBLE_PASTE63, \
        ZERO_ERROR_DOUBLE_PASTE61, \
        ZERO_ERROR_DOUBLE_PASTE61, \
        ZERO_ERROR_DOUBLE_PASTE59, \
        ZERO_ERROR_DOUBLE_PASTE59, \
        ZERO_ERROR_DOUBLE_PASTE57, \
        ZERO_ERROR_DOUBLE_PASTE57, \
        ZERO_ERROR_DOUBLE_PASTE55, \
        ZERO_ERROR_DOUBLE_PASTE55, \
        ZERO_ERROR_DOUBLE_PASTE53, \
        ZERO_ERROR_DOUBLE_PASTE53, \
        ZERO_ERROR_DOUBLE_PASTE51, \
        ZERO_ERROR_DOUBLE_PASTE51, \
        ZERO_ERROR_DOUBLE_PASTE49, \
        ZERO_ERROR_DOUBLE_PASTE49, \
        ZERO_ERROR_DOUBLE_PASTE47, \
        ZERO_ERROR_DOUBLE_PASTE47, \
        ZERO_ERROR_DOUBLE_PASTE45, \
        ZERO_ERROR_DOUBLE_PASTE45, \
        ZERO_ERROR_DOUBLE_PASTE43, \
        ZERO_ERROR_DOUBLE_PASTE43, \
        ZERO_ERROR_DOUBLE_PASTE41, \
        ZERO_ERROR_DOUBLE_PASTE41, \
        ZERO_ERROR_DOUBLE_PASTE39, \
        ZERO_ERROR_DOUBLE_PASTE39, \
        ZERO_ERROR_DOUBLE_PASTE37, \
        ZERO_ERROR_DOUBLE_PASTE37, \
        ZERO_ERROR_DOUBLE_PASTE35, \
        ZERO_ERROR_DOUBLE_PASTE35, \
        ZERO_ERROR_DOUBLE_PASTE33, \
        ZERO_ERROR_DOUBLE_PASTE33, \
        ZERO_ERROR_DOUBLE_PASTE31, \
        ZERO_ERROR_DOUBLE_PASTE31, \
        ZERO_ERROR_DOUBLE_PASTE29, \
        ZERO_ERROR_DOUBLE_PASTE29, \
        ZERO_ERROR_DOUBLE_PASTE27, \
        ZERO_ERROR_DOUBLE_PASTE27, \
        ZERO_ERROR_DOUBLE_PASTE25, \
        ZERO_ERROR_DOUBLE_PASTE25, \
        ZERO_ERROR_DOUBLE_PASTE23, \
        ZERO_ERROR_DOUBLE_PASTE23, \
        ZERO_ERROR_DOUBLE_PASTE21, \
        ZERO_ERROR_DOUBLE_PASTE21, \
        ZERO_ERROR_DOUBLE_PASTE19, \
        ZERO_ERROR_DOUBLE_PASTE19, \
        ZERO_ERROR_DOUBLE_PASTE17, \
        ZERO_ERROR_DOUBLE_PASTE17, \
        ZERO_ERROR_DOUBLE_PASTE15, \
        ZERO_ERROR_DOUBLE_PASTE15, \
        ZERO_ERROR_DOUBLE_PASTE13, \
        ZERO_ERROR_DOUBLE_PASTE13, \
        ZERO_ERROR_DOUBLE_PASTE11, \
        ZERO_ERROR_DOUBLE_PASTE11, \
        ZERO_ERROR_DOUBLE_PASTE9, \
        ZERO_ERROR_DOUBLE_PASTE9, \
        ZERO_ERROR_DOUBLE_PASTE7, \
        ZERO_ERROR_DOUBLE_PASTE7, \
        ZERO_ERROR_DOUBLE_PASTE5, \
        ZERO_ERROR_DOUBLE_PASTE5, \
        ZERO_ERROR_DOUBLE_PASTE3, \
        ZERO_ERROR_DOUBLE_PASTE3, \
        ZERO_ERROR_DOUBLE_PASTE1, \
        ZERO_ERROR_DOUBLE_PASTE1)(__VA_ARGS__))

#define ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) func(v1, v2)
#define ZERO_ERROR_DOUBLE_PASTE5(func, v1, v2, v3, v4) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE3(func, v3, v4)
#define ZERO_ERROR_DOUBLE_PASTE7(func, v1, v2, v3, v4, v5, v6) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE5(func, v3, v4, v5, v6)
#define ZERO_ERROR_DOUBLE_PASTE9(func, v1, v2, v3, v4, v5, v6, v7, v8) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE7(func, v3, v4, v5, v6, v7, v8)
#define ZERO_ERROR_DOUBLE_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE9(func, v3, v4, v5, v6, v7, v8, v9, v10)
#define ZERO_ERROR_DOUBLE_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE11(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define ZERO_ERROR_DOUBLE_PASTE15(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE13(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14)
#define ZERO_ERROR_DOUBLE_PASTE17(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE15(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)
#define ZERO_ERROR_DOUBLE_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE17(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define ZERO_ERROR_DOUBLE_PASTE21(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE19(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20)
#define ZERO_ERROR_DOUBLE_PASTE23(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE21(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22)
#define ZERO_ERROR_DOUBLE_PASTE25(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE23(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24)
#define ZERO_ERROR_DOUBLE_PASTE27(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE25(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26)
#define ZERO_ERROR_DOUBLE_PASTE29(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE27(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28)
#define ZERO_ERROR_DOUBLE_PASTE31(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE29(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30)
#define ZERO_ERROR_DOUBLE_PASTE33(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE31(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32)
#define ZERO_ERROR_DOUBLE_PASTE35(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE33(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34)
#define ZERO_ERROR_DOUBLE_PASTE37(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE35(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36)
#define ZERO_ERROR_DOUBLE_PASTE39(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE37(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38)
#define ZERO_ERROR_DOUBLE_PASTE41(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE39(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40)
#define ZERO_ERROR_DOUBLE_PASTE43(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE41(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42)
#define ZERO_ERROR_DOUBLE_PASTE45(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE43(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44)
#define ZERO_ERROR_DOUBLE_PASTE47(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE45(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46)
#define ZERO_ERROR_DOUBLE_PASTE49(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE47(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48)
#define ZERO_ERROR_DOUBLE_PASTE51(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE49(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50)
#define ZERO_ERROR_DOUBLE_PASTE53(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE51(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52)
#define ZERO_ERROR_DOUBLE_PASTE55(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE53(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54)
#define ZERO_ERROR_DOUBLE_PASTE57(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE55(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56)
#define ZERO_ERROR_DOUBLE_PASTE59(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE57(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58)
#define ZERO_ERROR_DOUBLE_PASTE61(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE59(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60)
#define ZERO_ERROR_DOUBLE_PASTE63(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62) ZERO_ERROR_DOUBLE_PASTE3(func, v1, v2) ZERO_ERROR_DOUBLE_PASTE61(func, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62)

#define ZERO_ERROR_TRIPLE_PASTE(...) ZERO_ERROR_EXPAND(ZERO_ERROR_GET_MACRO(__VA_ARGS__, \
        ZERO_ERROR_TRIPLE_PASTE64, \
        ZERO_ERROR_TRIPLE_PASTE61, \
        ZERO_ERROR_TRIPLE_PASTE61, \
        ZERO_ERROR_TRIPLE_PASTE61, \
        ZERO_ERROR_TRIPLE_PASTE58, \
        ZERO_ERROR_TRIPLE_PASTE58, \
        ZERO_ERROR_TRIPLE_PASTE58, \
        ZERO_ERROR_TRIPLE_PASTE55, \
        ZERO_ERROR_TRIPLE_PASTE55, \
        ZERO_ERROR_TRIPLE_PASTE55, \
        ZERO_ERROR_TRIPLE_PASTE52, \
        ZERO_ERROR_TRIPLE_PASTE52, \
        ZERO_ERROR_TRIPLE_PASTE52, \
        ZERO_ERROR_TRIPLE_PASTE49, \
        ZERO_ERROR_TRIPLE_PASTE49, \
        ZERO_ERROR_TRIPLE_PASTE49, \
        ZERO_ERROR_TRIPLE_PASTE46, \
        ZERO_ERROR_TRIPLE_PASTE46, \
        ZERO_ERROR_TRIPLE_PASTE46, \
        ZERO_ERROR_TRIPLE_PASTE43, \
        ZERO_ERROR_TRIPLE_PASTE43, \
        ZERO_ERROR_TRIPLE_PASTE43, \
        ZERO_ERROR_TRIPLE_PASTE40, \
        ZERO_ERROR_TRIPLE_PASTE40, \
        ZERO_ERROR_TRIPLE_PASTE40, \
        ZERO_ERROR_TRIPLE_PASTE37, \
        ZERO_ERROR_TRIPLE_PASTE37, \
        ZERO_ERROR_TRIPLE_PASTE37, \
        ZERO_ERROR_TRIPLE_PASTE34, \
        ZERO_ERROR_TRIPLE_PASTE34, \
        ZERO_ERROR_TRIPLE_PASTE34, \
        ZERO_ERROR_TRIPLE_PASTE31, \
        ZERO_ERROR_TRIPLE_PASTE31, \
        ZERO_ERROR_TRIPLE_PASTE31, \
        ZERO_ERROR_TRIPLE_PASTE28, \
        ZERO_ERROR_TRIPLE_PASTE28, \
        ZERO_ERROR_TRIPLE_PASTE28, \
        ZERO_ERROR_TRIPLE_PASTE25, \
        ZERO_ERROR_TRIPLE_PASTE25, \
        ZERO_ERROR_TRIPLE_PASTE25, \
        ZERO_ERROR_TRIPLE_PASTE22, \
        ZERO_ERROR_TRIPLE_PASTE22, \
        ZERO_ERROR_TRIPLE_PASTE22, \
        ZERO_ERROR_TRIPLE_PASTE19, \
        ZERO_ERROR_TRIPLE_PASTE19, \
        ZERO_ERROR_TRIPLE_PASTE19, \
        ZERO_ERROR_TRIPLE_PASTE16, \
        ZERO_ERROR_TRIPLE_PASTE16, \
        ZERO_ERROR_TRIPLE_PASTE16, \
        ZERO_ERROR_TRIPLE_PASTE13, \
        ZERO_ERROR_TRIPLE_PASTE13, \
        ZERO_ERROR_TRIPLE_PASTE13, \
        ZERO_ERROR_TRIPLE_PASTE10, \
        ZERO_ERROR_TRIPLE_PASTE10, \
        ZERO_ERROR_TRIPLE_PASTE10, \
        ZERO_ERROR_TRIPLE_PASTE7, \
        ZERO_ERROR_TRIPLE_PASTE7, \
        ZERO_ERROR_TRIPLE_PASTE7, \
        ZERO_ERROR_TRIPLE_PASTE4, \
        ZERO_ERROR_TRIPLE_PASTE4, \
        ZERO_ERROR_TRIPLE_PASTE4, \
        ZERO_ERROR_TRIPLE_PASTE1, \
        ZERO_ERROR_TRIPLE_PASTE1, \
        ZERO_ERROR_TRIPLE_PASTE1)(__VA_ARGS__))

#define ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) func(v1, v2, v3)
#define ZERO_ERROR_TRIPLE_PASTE7(func, v1, v2, v3, v4, v5, v6) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE4(func, v4, v5, v6)
#define ZERO_ERROR_TRIPLE_PASTE10(func, v1, v2, v3, v4, v5, v6, v7, v8, v9) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE7(func, v4, v5, v6, v7, v8, v9)
#define ZERO_ERROR_TRIPLE_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE10(func, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define ZERO_ERROR_TRIPLE_PASTE16(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE13(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15)
#define ZERO_ERROR_TRIPLE_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE16(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define ZERO_ERROR_TRIPLE_PASTE22(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE19(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21)
#define ZERO_ERROR_TRIPLE_PASTE25(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE22(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24)
#define ZERO_ERROR_TRIPLE_PASTE28(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE25(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27)
#define ZERO_ERROR_TRIPLE_PASTE31(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE28(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30)
#define ZERO_ERROR_TRIPLE_PASTE34(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE31(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32)
#define ZERO_ERROR_TRIPLE_PASTE37(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE34(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36)
#define ZERO_ERROR_TRIPLE_PASTE40(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE37(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39)
#define ZERO_ERROR_TRIPLE_PASTE43(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE40(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42)
#define ZERO_ERROR_TRIPLE_PASTE46(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE43(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45)
#define ZERO_ERROR_TRIPLE_PASTE49(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE46(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48)
#define ZERO_ERROR_TRIPLE_PASTE52(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE49(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51)
#define ZERO_ERROR_TRIPLE_PASTE55(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE52(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54)
#define ZERO_ERROR_TRIPLE_PASTE58(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE55(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57)
#define ZERO_ERROR_TRIPLE_PASTE61(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60) ZERO_ERROR_TRIPLE_PASTE4(func, v1, v2, v3) ZERO_ERROR_TRIPLE_PASTE58(func, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60)

#define ERROR_ENUM_ITEM(v1, v2) v1,
#define MESSAGE_SWITCH_BRANCH(v1, v2)                                                                           \
    case ErrorType::v1: /* NOLINT(*-branch-clone) */                                                            \
        _msg = v2;                                                                                              \
        break;                                                                                                  \

#define DEFINE_MAKE_ERROR_CODE(Type)                                                                            \
    inline std::error_code make_error_code(const Type _e) {                                                     \
        return {std::to_underlying(_e), Type##Category::instance()};                                            \
    }

#define DEFINE_MAKE_ERROR_CODE_INNER(Type)                                                                      \
    friend std::error_code make_error_code(const Type _e) {                                                     \
        return {std::to_underlying(_e), Type##Category::instance()};                                            \
    }

#define DEFINE_ERROR_CODE_TYPES(Type, category, ...)                                                            \
    enum class Type {                                                                                           \
        OK,                                                                                                     \
        ZERO_ERROR_EXPAND(ZERO_ERROR_DOUBLE_PASTE(ERROR_ENUM_ITEM, __VA_ARGS__))                                \
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
            std::string _msg;                                                                                   \
                                                                                                                \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            ZERO_ERROR_EXPAND(ZERO_ERROR_DOUBLE_PASTE(MESSAGE_SWITCH_BRANCH, __VA_ARGS__))                      \
                                                                                                                \
            default:                                                                                            \
                _msg = "unknown";                                                                               \
                break;                                                                                          \
            }                                                                                                   \
                                                                                                                \
            return _msg;                                                                                        \
        }                                                                                                       \
    };

#define DEFINE_ERROR_CODE(Type, category, ...)                                                                  \
    DEFINE_ERROR_CODE_TYPES(Type, category, __VA_ARGS__)                                                        \
    DEFINE_MAKE_ERROR_CODE(Type)

#define DEFINE_ERROR_CODE_INNER(Type, category, ...)                                                            \
    DEFINE_ERROR_CODE_TYPES(Type, category, __VA_ARGS__)                                                        \
    DEFINE_MAKE_ERROR_CODE_INNER(Type)

#define ERROR_ENUM_ITEM_EX(v1, v2, v3) v1,
#define MESSAGE_SWITCH_BRANCH_EX(v1, v2, v3)                                                                    \
    case ErrorType::v1: /* NOLINT(*-branch-clone) */                                                            \
        _msg = v2;                                                                                              \
        break;

#define DEFAULT_ERROR_CONDITION_SWITCH_BRANCH_EX(v1, v2, v3)                                                    \
    case ErrorType::v1: /* NOLINT(*-branch-clone) */                                                            \
        _condition = v3;                                                                                        \
        break;

#define DEFAULT_ERROR_CONDITION error_category::default_error_condition(_value)

#define DEFINE_ERROR_CODE_TYPES_EX(Type, category, ...)                                                         \
    enum class Type {                                                                                           \
        OK,                                                                                                     \
        ZERO_ERROR_EXPAND(ZERO_ERROR_TRIPLE_PASTE(ERROR_ENUM_ITEM_EX, __VA_ARGS__))                             \
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
            std::string _msg;                                                                                   \
                                                                                                                \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            ZERO_ERROR_EXPAND(ZERO_ERROR_TRIPLE_PASTE(MESSAGE_SWITCH_BRANCH_EX, __VA_ARGS__))                   \
                                                                                                                \
            default:                                                                                            \
                _msg = "unknown";                                                                               \
                break;                                                                                          \
            }                                                                                                   \
                                                                                                                \
            return _msg;                                                                                        \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] std::error_condition default_error_condition(const int _value) const noexcept override {  \
            std::error_condition _condition;                                                                    \
                                                                                                                \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            ZERO_ERROR_EXPAND(ZERO_ERROR_TRIPLE_PASTE(DEFAULT_ERROR_CONDITION_SWITCH_BRANCH_EX, __VA_ARGS__))   \
                                                                                                                \
            default:                                                                                            \
                _condition = error_category::default_error_condition(_value);                                   \
                break;                                                                                          \
            }                                                                                                   \
                                                                                                                \
            return _condition;                                                                                  \
        }                                                                                                       \
    };

#define DEFINE_ERROR_CODE_EX(Type, category, ...)                                                               \
    DEFINE_ERROR_CODE_TYPES_EX(Type, category, __VA_ARGS__)                                                     \
    DEFINE_MAKE_ERROR_CODE(Type)

#define DEFINE_ERROR_CODE_INNER_EX(Type, category, ...)                                                         \
    DEFINE_ERROR_CODE_TYPES_EX(Type, category, __VA_ARGS__)                                                     \
    DEFINE_MAKE_ERROR_CODE_INNER(Type)

#define DECLARE_ERROR_CODE(Type)                                                                                \
    template<>                                                                                                  \
    struct std::is_error_code_enum<Type> : std::true_type {                                                     \
    };

#define DECLARE_ERROR_CODES(...) ZERO_ERROR_EXPAND(ZERO_ERROR_PASTE(DECLARE_ERROR_CODE, __VA_ARGS__))

#define DEFINE_MAKE_ERROR_CONDITION(Type)                                                                       \
    inline std::error_condition make_error_condition(const Type _e) {                                           \
        return {std::to_underlying(_e), Type##Category::instance()};                                            \
    }

#define DEFINE_MAKE_ERROR_CONDITION_INNER(Type)                                                                 \
    friend std::error_condition make_error_condition(const Type _e) {                                           \
        return {std::to_underlying(_e), Type##Category::instance()};                                            \
    }

#define DEFINE_ERROR_CONDITION_TYPES(Type, category, ...)                                                       \
    enum class Type {                                                                                           \
        OK,                                                                                                     \
        ZERO_ERROR_EXPAND(ZERO_ERROR_DOUBLE_PASTE(ERROR_ENUM_ITEM, __VA_ARGS__))                                \
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
            std::string _msg;                                                                                   \
                                                                                                                \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            ZERO_ERROR_EXPAND(ZERO_ERROR_DOUBLE_PASTE(MESSAGE_SWITCH_BRANCH, __VA_ARGS__))                      \
                                                                                                                \
            default:                                                                                            \
                _msg = "unknown";                                                                               \
                break;                                                                                          \
            }                                                                                                   \
                                                                                                                \
            return _msg;                                                                                        \
        }                                                                                                       \
    };

#define DEFINE_ERROR_CONDITION(Type, category, ...)                                                             \
    DEFINE_ERROR_CONDITION_TYPES(Type, category, __VA_ARGS__)                                                   \
    DEFINE_MAKE_ERROR_CONDITION(Type)

#define DEFINE_ERROR_CONDITION_INNER(Type, category, ...)                                                       \
    DEFINE_ERROR_CONDITION_TYPES(Type, category, __VA_ARGS__)                                                   \
    DEFINE_MAKE_ERROR_CONDITION_INNER(Type)

#define EQUIVALENT_SWITCH_BRANCH_EX(v1, v2, v3)                                                                 \
    case ErrorType::v1: /* NOLINT(*-branch-clone) */                                                            \
        return v3(_code);

#define DEFINE_ERROR_CONDITION_TYPES_EX(Type, category, ...)                                                    \
    enum class Type {                                                                                           \
        OK,                                                                                                     \
        ZERO_ERROR_EXPAND(ZERO_ERROR_TRIPLE_PASTE(ERROR_ENUM_ITEM_EX, __VA_ARGS__))                             \
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
            std::string _msg;                                                                                   \
                                                                                                                \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            ZERO_ERROR_EXPAND(ZERO_ERROR_TRIPLE_PASTE(MESSAGE_SWITCH_BRANCH_EX, __VA_ARGS__))                   \
                                                                                                                \
            default:                                                                                            \
                _msg = "unknown";                                                                               \
                break;                                                                                          \
            }                                                                                                   \
                                                                                                                \
            return _msg;                                                                                        \
        }                                                                                                       \
                                                                                                                \
        [[nodiscard]] bool equivalent(const std::error_code &_code, const int _value) const noexcept override { \
            switch (static_cast<ErrorType>(_value)) { /* NOLINT(*-multiway-paths-covered) */                    \
            ZERO_ERROR_EXPAND(ZERO_ERROR_TRIPLE_PASTE(EQUIVALENT_SWITCH_BRANCH_EX, __VA_ARGS__))                \
                                                                                                                \
            default:                                                                                            \
                return false;                                                                                   \
            }                                                                                                   \
        }                                                                                                       \
    };

#define DEFINE_ERROR_CONDITION_EX(Type, category, ...)                                                          \
    DEFINE_ERROR_CONDITION_TYPES_EX(Type, category, __VA_ARGS__)                                                \
    DEFINE_MAKE_ERROR_CONDITION(Type)

#define DEFINE_ERROR_CONDITION_INNER_EX(Type, category, ...)                                                    \
    DEFINE_ERROR_CONDITION_TYPES_EX(Type, category, __VA_ARGS__)                                                \
    DEFINE_MAKE_ERROR_CONDITION_INNER(Type)

#define DECLARE_ERROR_CONDITION(Type)                                                                           \
    template<>                                                                                                  \
    struct std::is_error_condition_enum<Type> : std::true_type {                                                \
    };

#define DECLARE_ERROR_CONDITIONS(...) ZERO_ERROR_EXPAND(ZERO_ERROR_PASTE(DECLARE_ERROR_CONDITION, __VA_ARGS__))

#define DEFINE_ERROR_TRANSFORMER_TYPES(Type, category, stringify)                                               \
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

#define DEFINE_ERROR_TRANSFORMER(Type, category, stringify)                                                     \
    DEFINE_ERROR_TRANSFORMER_TYPES(Type, category, stringify)                                                   \
    DEFINE_MAKE_ERROR_CODE(Type)

#define DEFINE_ERROR_TRANSFORMER_INNER(Type, category, stringify)                                               \
    DEFINE_ERROR_TRANSFORMER_TYPES(Type, category, stringify)                                                   \
    DEFINE_MAKE_ERROR_CODE_INNER(Type)

#define DEFINE_ERROR_TRANSFORMER_TYPES_EX(Type, category, stringify, classify)                                  \
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

#define DEFINE_ERROR_TRANSFORMER_EX(Type, category, stringify, classify)                                        \
    DEFINE_ERROR_TRANSFORMER_TYPES_EX(Type, category, stringify, classify)                                      \
    DEFINE_MAKE_ERROR_CODE(Type)

#define DEFINE_ERROR_TRANSFORMER_INNER_EX(Type, category, stringify, classify)                                  \
    DEFINE_ERROR_TRANSFORMER_TYPES_EX(Type, category, stringify, classify)                                      \
    DEFINE_MAKE_ERROR_CODE_INNER(Type)

#define DEFINE_ERROR_CATEGORY_INSTANCE(Type)                                                                    \
    const std::error_category &Type##Category::instance() {                                                     \
        return errorCategoryInstance<Type##Category>();                                                         \
    }

#define DEFINE_ERROR_CATEGORY_INSTANCES(...)                                                                    \
    ZERO_ERROR_EXPAND(ZERO_ERROR_PASTE(DEFINE_ERROR_CATEGORY_INSTANCE, __VA_ARGS__))

#endif //ZERO_ERROR_H
