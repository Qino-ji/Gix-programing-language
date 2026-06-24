#### Visibility Rules

Variables in **Vix** can be defined in three forms: `local`, `mutable`, and `immutable`.
Each form has different functionality and rules.

| Keyword | Type | Reassignable |
|---------|------|-------------|
| `let` | Immutable | No |
| `var` | Mutable | Yes |
| `local` | Global Immutable | No |
| `local var` | Global Mutable | Yes |

## Declaration
```vix
let a = 10        // immutable variable
var b = 50        // mutable variable
local c = 30      // global immutable variable
local var d = 30  // global mutable variable
```

## Rules
```vix
let a = 10
var b = 50
local c = 30

a += 1  // ERROR: cannot reassign immutable variable 'a'
b += 40 // OK
c += 5  // ERROR: cannot reassign global immutable variable 'c'
```
