# Additions to extend the Lisp interpreter

## String processing functions

    (strlen <str>)

returns the length of string expression `<str>`.

```c
L f_strlen(L t, L *_) {
  L x = car(t);
  return (T(x) == STRG) ? *(I*)(A+ord(x)-Z)-1 : 0;
}
struct { ... } prim[] = { ...
  {"strlen",   f_strlen,  NORMAL},              /* (strlen x) => length of string x or 0 if not a string */
 
```

    (strpos <substr> <bigstr> <pos>)

returns the position of string expression `<substr>` in `<bigstr>` starting from `<pos>` (when specified), or returns `()` when not found
- if `<pos>` is not specified, then `<bigstr>` is searched from the start
- if `<pos>` is non-negative, then `<bigstr>` is searched from position `<pos>` from the start
- if `<pos>` is negative, then `<bigstr>` is searched starting from `<pos>` positions from the end

For example:
- `(strpos "de" "abcdef")` returns 3
- `(strpos "de" "abcdef" 0)` returns 3
- `(strpos "de" "abcdef" 4)` returns `()`
- `(strpos "de" "abcdef" -3)` returns 3
- `(strpos "de" "abcdef" -2)` returns `()`

```c
L f_strpos(L t, L *_) {
  const char *p; I k = 0, n; L x = car(t), y = car(t = cdr(t));
  if (T(x) != STRG || T(y) != STRG)
    return err(5);
  n = *(I*)(A+ord(y)-Z)-1;
  if (!not(t = cdr(t))) {
    L z = car(t);
    k = z < 0 ? z < n ? n+z : 0 : z < n ? z : n;
  }
  p = strstr(A+ord(y)+k, A+ord(x));
  return p ? p-A-ord(y) : nil;
}
struct { ... } prim[] = { ...
  {"strpos",   f_strpos,  NORMAL},              /* (strpos x y [k]) find string x in y from position k onwards */
```

It should be straightforward to add a case-insensitive version of this function that uses `strcasestr` instead of `strstr`.

    (substr <str> <pos> <len>)

returns the substring of string expression `<str>` from position `<pos>` spanning `<len>` characters or bytes (when specified)
- if `<pos>` is negative, then characters are copied from `<pos>` positions from the end of the string
- if `<len>` is not specified, then the substring spans the string from `<pos>` to the end
- if `<len>` is larger than the substring length, then `<str>` wraps around in the resulting string (offers some neat tricks!)

For example:
- `(substr "abcdef" 1)` returns `"bcdef"`
- `(substr "abcdef" 3 2)` returns `"de"`
- `(substr "abcdef" -2)` returns `"ef"`
- `(substr "abcdef" 1 6)` returns `"bcdefa"`
- `(substr "abcdef" -2 6)` returns `"efabcd"`
- `(substr "<>" 0 8)` returns `"<><><><>"`
- `(substr "<>" 1 8)` returns `"><><><><"`

```c
L f_substr(L t, L *_) {
  I i, j, k, m, n; L x = car(t), y = car(t = cdr(t));
  if (T(x) != STRG)
    return err(5);
  n = *(I*)(A+ord(x)-Z)-1;
  k = y < 0 ? y < n ? n+y : 0 : y < n ? y : n;
  m = not(t = cdr(t)) ? n-k : car(t);
  i = j = alloc(m);
  if (m <= n-k)
    memcpy(A+j, A+ord(x)+k, m);
  else {
    memcpy(A+j, A+ord(x)+k, n-k);
    for (j += n-k, m -= n-k; m > n; j += n, m -= n)
      memcpy(A+j, A+ord(x), n);
    memcpy(A+j, A+ord(x), m);
  }
  return box(STRG, i);
}
struct { ... } prim[] = { ...
  {"substr",   f_substr,  NORMAL},              /* (substr x k [m]) => slice string x from position k length m */
```

## Math functions

These definitions require `#include <math.h>`

    (sqrt x)
    (log x)
    (log2 x)
    (log10 x)
    (exp x)
    (pow x y)
    (sin x)
    (cos x)
    (tan x)
    (asin x)
    (acos x)
    (atan x)
    (atan2 x y)

```c
L f_sqrt(L t, L *_) {
  return sqrt(car(t));
}
L f_log(L t, L *_) {
  return log(car(t));
}
L f_log2(L t, L *_) {
  return log2(car(t));
}
L f_log10(L t, L *_) {
  return log10(car(t));
}
L f_exp(L t, L *_) {
  return exp(car(t));
}
L f_pow(L t, L *_) {
  return pow(car(t), car(cdr(t)));
}
L f_sin(L t, L *_) {
  return sin(car(t));
}
L f_cos(L t, L *_) {
  return cos(car(t));
}
L f_tan(L t, L *_) {
  return tan(car(t));
}
L f_asin(L t, L *_) {
  return asin(car(t));
}
L f_acos(L t, L *_) {
  return acos(car(t));
}
L f_atan(L t, L *_) {
  return atan(car(t));
}
L f_atan2(L t, L *_) {
  return atan2(car(t), car(cdr(t)));
}
struct { ... } prim[] = { ...
  {"sqrt",  f_sqrt,    NORMAL},
  {"log",   f_log,     NORMAL},
  {"log2",  f_log2,    NORMAL},
  {"log10", f_log10,   NORMAL},
  {"exp",   f_exp,     NORMAL},
  {"pow",   f_pow,     NORMAL},
  {"sin",   f_sin,     NORMAL},
  {"cos",   f_cos,     NORMAL},
  {"tan",   f_tan,     NORMAL},
  {"asin",  f_asin,    NORMAL},
  {"acos",  f_acos,    NORMAL},
  {"atan",  f_atan,    NORMAL},
  {"atan2", f_atan2,   NORMAL},
```
