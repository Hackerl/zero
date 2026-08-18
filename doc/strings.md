# strings — String Utilities, Cmdline, Env

Headers:
- `#include <zero/strings.h>` — string utilities
- `#include <zero/cmdline.h>` — command-line parser
- `#include <zero/env.h>` — environment variables

Namespaces: `zero::strings`, `zero::env`; `zero::Cmdline` is in the root namespace.

---

## String Utilities (`strings.h`)

```cpp
#include <zero/strings.h>

// Trim whitespace — returns a new string
std::string s = "  hello  ";
auto trimmed = zero::strings::trim(s);   // "hello"
auto ltrimmed = zero::strings::ltrim(s); // trim left only
auto rtrimmed = zero::strings::rtrim(s); // trim right only

// Case conversion — returns a new string
auto lower = zero::strings::tolower(s);
auto upper = zero::strings::toupper(s);

// Split on whitespace
auto parts = zero::strings::split("a b  c"); // ["a","b","c"]

// Split on delimiter with optional limit
auto parts = zero::strings::split("a:b:c", ":", 2); // ["a","b:c"]

// Parse numbers
auto n = zero::strings::toNumber<int>("42");        // expected<int, error_code>
auto h = zero::strings::toNumber<int>("ff", 16);    // expected<int, error_code>

// Wide/narrow encoding (iconv-backed; Windows only for most conversions)
auto wide = zero::strings::decode(narrow_str, "UTF-8");   // expected<wstring, error_code>
auto narr = zero::strings::encode(wide_str, "UTF-8");     // expected<string, error_code>
```

---

## Command-Line Parser (`cmdline.h`)

```cpp
#include <zero/cmdline.h>

zero::Cmdline cmd;

// Positional arguments (required, in order)
cmd.add<std::string>("input",  "Input file path");
cmd.add<int>        ("count",  "Number of items");

// Optional flags with values
cmd.addOptional<int>   ("port",    'p', "Server port",  8080);
cmd.addOptional<std::string>("host", 'h', "Server host", "localhost");

// Boolean flag
cmd.addOptional("verbose", 'v', "Enable verbose output");

// Custom footer
cmd.footer("Example: myapp input.txt 10 --port 9090");

// Parse (throws on error, prints usage and exits)
cmd.parse(argc, argv);

// Access values
auto input   = cmd.get<std::string>("input");
auto count   = cmd.get<int>("count");
auto port    = cmd.getOptional<int>("port");     // returns the value (or default)
bool verbose = cmd.exist("verbose");
auto rest    = cmd.rest();                        // remaining unparsed args
```

For custom types, specialize `zero::parseValue<T>()` which in turn calls `zero::scan<T>()`.

---

## Environment Variables (`env.h`)

```cpp
#include <zero/env.h>

auto val = zero::env::get("HOME");                     // optional<string>
zero::env::set("MY_VAR", "value");                     // void
zero::env::unset("MY_VAR");                            // void
auto all = zero::env::list();                           // map<string, string>
```

---

## Notes

- `trim`, `tolower`, `toupper` take a `string_view` and return a new `std::string`; they do not modify the input.
- `split` with a delimiter ignores empty tokens between consecutive delimiters unless `limit` is specified.
- `toNumber` uses `std::from_chars` internally and is locale-independent.
- `encode`/`decode` require iconv; on non-Windows platforms iconv is available via system libraries.
