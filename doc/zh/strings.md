# strings — 字符串工具、命令行解析、环境变量

头文件：
- `#include <zero/strings.h>` — 字符串工具
- `#include <zero/cmdline.h>` — 命令行解析器
- `#include <zero/env.h>` — 环境变量

命名空间：`zero::strings`、`zero::env`；`zero::Cmdline` 位于根命名空间。

---

## 字符串工具 (`strings.h`)

```cpp
#include <zero/strings.h>

// 去除空白字符，返回新字符串
std::string s = "  hello  ";
auto trimmed  = zero::strings::trim(s);   // "hello"
auto ltrimmed = zero::strings::ltrim(s);  // 仅去除左侧
auto rtrimmed = zero::strings::rtrim(s);  // 仅去除右侧

// 大小写转换，返回新字符串
auto lower = zero::strings::tolower(s);
auto upper = zero::strings::toupper(s);

// 按空白字符分割
auto parts = zero::strings::split("a b  c"); // ["a","b","c"]

// 按分隔符分割，可选限制分割次数
auto parts = zero::strings::split("a:b:c", ":", 2); // ["a","b:c"]

// 解析数字
auto n = zero::strings::toNumber<int>("42");        // expected<int, error_code>
auto h = zero::strings::toNumber<int>("ff", 16);    // expected<int, error_code>

// 宽字符/窄字符编码转换（基于 iconv；Windows 上大部分转换均可用）
auto wide = zero::strings::decode(narrow_str, "UTF-8");   // expected<wstring, error_code>
auto narr = zero::strings::encode(wide_str, "UTF-8");     // expected<string, error_code>
```

---

## 命令行解析器 (`cmdline.h`)

```cpp
#include <zero/cmdline.h>

zero::Cmdline cmd;

// 位置参数（必须，按顺序）
cmd.add<std::string>("input",  "输入文件路径");
cmd.add<int>        ("count",  "元素数量");

// 带值的可选标志
cmd.addOptional<int>   ("port",    'p', "服务器端口",  8080);
cmd.addOptional<std::string>("host", 'h', "服务器主机", "localhost");

// 布尔标志
cmd.addOptional("verbose", 'v', "启用详细输出");

// 自定义页脚
cmd.footer("示例：myapp input.txt 10 --port 9090");

// 解析（出错时打印使用说明并抛出异常）
cmd.parse(argc, argv);

// 访问值
auto input   = cmd.get<std::string>("input");
auto count   = cmd.get<int>("count");
auto port    = cmd.getOptional<int>("port");   // 返回值（或默认值）
bool verbose = cmd.exist("verbose");
auto rest    = cmd.rest();                      // 未解析的剩余参数
```

如需支持自定义类型，可特化 `zero::parseValue<T>()`（内部调用 `zero::scan<T>()`）。

---

## 环境变量 (`env.h`)

```cpp
#include <zero/env.h>

auto val = zero::env::get("HOME");                    // optional<string>
zero::env::set("MY_VAR", "value");                    // void
zero::env::unset("MY_VAR");                           // void
auto all = zero::env::list();                          // map<string, string>
```

---

## 注意事项

- `trim`、`tolower`、`toupper` 接受 `string_view`，返回新的 `std::string`，不修改原字符串。
- 带分隔符的 `split` 会忽略连续分隔符之间的空字段，除非指定了 `limit`。
- `toNumber` 内部使用 `std::from_chars`，与语言环境无关。
- `encode`/`decode` 依赖 iconv；在非 Windows 平台上，iconv 通过系统库提供。
