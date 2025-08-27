# Additions to extend the Lisp interpreter

## String processing functions

### strlen

    (strlen <str>)

returns the length in bytes of string expression `<str>`.

```c
L f_strlen(L t, L *_) {
  L x = car(t);
  return (T(x) == STRG) ? *(I*)(A+ord(x)-Z)-1 : 0;
}
struct { ... } prim[] = { ...
  {"strlen",   f_strlen,  NORMAL},              /* (strlen x) => length of string x or 0 if not a string */
```

### strpos

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

### strat

    (strat <str> <pos>)

returns the code of the character at position `<pos>` in string expression `<str>`
- if `<pos>` is negative, then the code of the character from `<pos>` positions from the end of the string is returned

For example:
- `(strat "ABCDEF" 0)` returns 65 (ASCII A)
- `(strat "ABCDEF" -1)` returns 70 (ASCII F)

```c
L f_strat(L t, L *_) {
  I k, n; L x = car(t), y = car(t = cdr(t));
  if (T(x) != STRG)
    return err(5);
  n = *(I*)(A+ord(x)-Z)-1;
  k = y < 0 ? y < n ? n+y : 0 : y < n ? y : n-1;
  return (unsigned char)*(A+ord(x)+k);
}
struct { ... } prim[] = { ...
  {"strat",    f_strat,   NORMAL},              /* (strat x k) => character code at position k in string x */
```

### substr

    (substr <str> <pos> <len>)

returns the substring of string expression `<str>` from position `<pos>` spanning `<len>` bytes (when specified)
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
- `(let (i 0) (while #t (write "\r" (substr "Welcome to Lisp!    " (mod i 20) 20)) (setq i (+ i 1))))` rotates a banner

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

## UTF-8 Unicode string processing functions

Add the following three UTF-8 helper functions:

```c
/* return the utf8 character length of the utf8 string s */
size_t utf8_len(const char *s) {
  size_t n = 0;
  while (*s)
    n += ((unsigned char)*s++ & 0xc0) != 0x80;
  return n;
}

/* return utf8 character position at byte offset k in utf8 string s */
size_t utf8_pos(const char *s, size_t k) {
  size_t n = 0;
  while (k-- && *s)
    n += ((unsigned char)*s++ & 0xc0) != 0x80;
  return n;
}

/* return byte offset for utf8 character position k in utf8 string s */
size_t utf8_loc(const char *s, size_t k) {
  const char *t = s;
  while (*s && (((unsigned char)*s & 0xc0) == 0x80 || k--))
    ++s;
  return s-t;
}
```

### utf8.len

    (utf8.len <str>)

returns the Unicode character length of string expression `<str>` encoded in UTF-8.

```c
L f_utf8_len(L t, L *_) {
  L x = car(t);
  return (T(x) == STRG) ? utf8_len(A+ord(x)) : 0;
}
struct { ... } prim[] = { ...
  {"utf8.len", f_utf8_len, NORMAL},             /* (utf8.len x) => character length of string x or 0 if not a string */
```

### utf8.pos

    (utf8.pos <substr> <bigstr> <pos>)

returns the Unicode character position of string expression `<substr>` in `<bigstr>` starting from `<pos>` (when specified), or returns `()` when not found
- if `<pos>` is not specified, then `<bigstr>` is searched from the start
- if `<pos>` is non-negative, then `<bigstr>` is searched from position `<pos>` from the start
- if `<pos>` is negative, then `<bigstr>` is searched starting from `<pos>` positions from the end

For example:
- `(utf8.pos "DË" "ÀBCDËF")` returns 3
- `(utf8.pos "DË" "ÀBCDËF" 0)` returns 3
- `(utf8.pos "DË" "ÀBCDËF" 4)` returns `()`
- `(utf8.pos "DË" "ÀBCDËF" -3)` returns 3
- `(utf8.pos "DË" "ÀBCDËF" -2)` returns `()`

```c
L f_utf8_pos(L t, L *_) {
  const char *p; I k = 0, n; L x = car(t), y = car(t = cdr(t));
  if (T(x) != STRG || T(y) != STRG)
    return err(5);
  n = *(I*)(A+ord(y)-Z)-1;
  if (!not(t = cdr(t))) {
    L z = car(t);
    k = z < 0 ? z < n ? n+z : 0 : z < n ? z : n;
  }
  p = strstr(A+ord(y)+utf8_loc(A+ord(y), k), A+ord(x));
  return p ? utf8_pos(A+ord(y), p-A-ord(y)) : nil;
}
struct { ... } prim[] = { ...
  {"utf8.pos", f_utf8_pos, NORMAL},             /* (utf8.pos x y [k]) find string x in y from char position k onwards */
```

### utf8.sub

    (utf8.sub <str> <pos> <len>)

returns the substring of string expression `<str>` from position `<pos>` spanning `<len>` Unicode characters (when specified)
- if `<pos>` is negative, then characters are copied from `<pos>` positions from the end of the string
- if `<len>` is not specified, then the substring spans the string from `<pos>` to the end
- if `<len>` is larger than the substring length, then `<str>` wraps around in the resulting string (offers some neat tricks!)

For example:
- `(utf8.str "ÀBCDËF" 1)` returns `"BCDËF"`
- `(utf8.str "ÀBCDËF" 3 2)` returns `"DË"`
- `(utf8.str "ÀBCDËF" -2)` returns `"ËF"`
- `(utf8.str "ÀBCDËF" 1 6)` returns `"BCDËFÀ"`
- `(utf8.str "ÀBCDËF" -2 6)` returns `"ËFÀBCD"`
- `(utf8.str "▄▀" 0 8)` returns `"▄▀▄▀▄▀▄▀"`
- `(utf8.str "▄▀" 1 8)` returns `"▀▄▀▄▀▄▀▄"`

```c
L f_utf8_sub(L t, L *_) {
  I i, j, k, l, m, n, r; L x = car(t), y = car(t = cdr(t));
  if (T(x) != STRG)
    return err(5);
  n = utf8_len(A+ord(x));
  k = y < 0 ? y < n ? n+y : 0 : y < n ? y : n;
  l = utf8_loc(A+ord(x), k); /* leading number of bytes from begin to k */
  m = not(t = cdr(t)) ? utf8_len(A+ord(x)+l) : car(t);
  if (m <= n-k) {
    r = utf8_loc(A+ord(x)+l, m); /* remaining number of bytes from k to m */
    i = alloc(r);
    memcpy(A+i, A+ord(x)+l, r);
  }
  else {
    r = *(I*)(A+ord(x)-Z)-1-l; /* remaining number of bytes from k to end */
    i = j = alloc(r+(m-n+k)/n*(l+r)+utf8_loc(A+ord(x), (m-n+k)%n));
    memcpy(A+j, A+ord(x)+l, r);
    for (j += r, m -= n-k; m > n; j += l+r, m -= n)
      memcpy(A+j, A+ord(x), l+r);
    memcpy(A+j, A+ord(x), utf8_loc(A+ord(x), m));
  }
  return box(STRG, i);
}
struct { ... } prim[] = { ...
  {"utf8.sub", f_utf8_sub, NORMAL},             /* (utf8.sub x k [m]) => slice string x from char position k length m */
```

## System functions

### system

    (system <str>)

executes the command given by the string `<str>` and returns its status code

For example:
- `(define dir (lambda args (system (string "ls " . args))))` use `(dir)` to list files and with args `(dir "-l *.c")`

```c
L f_system(L t, L *_) {
  L x = car(t);
  if (T(x) != STRG)
    return err(5);
  return system(A+ord(x));
}
struct { ... } prim[] = { ...
  {"system",   f_system,  NORMAL},              /* (system x) executes the command x given as a string */
```

### usleep

    (usleep <expr>)

pauses execution for `<expr>` micro seconds

```c
#include <unistd.h>
L f_usleep(L t, L *_) {
  L x = car(t);
  usleep(x);
  return nil;
}
struct { ... } prim[] = { ...
  {"usleep",   f_usleep,  NORMAL},              /* (usleep k) pause k micro seconds */
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
