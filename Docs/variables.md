# Variables

## What are variables?
Variables are parts that define a memory location to store data. They are used to store data that can be changed during program execution. Variables in **vix** are immutable by default using the `let` keyword. This means that once a variable is defined it cannot be changed. To make a variable mutable you need to use the `var` keyword. Variables are allowed to be defined using any type in **vix**. This includes classes, functions, and other variables. Example:

```vix
let a = 10
var b = 20

a = 30 // Error: cannot change immutable variable
b = 40 // OK: b is mutable

print(a) // will print 10
print(b) // will print 40
```

Variables have many types in **vix**. Defining a type in a variable requires the `:` operator followed by the type. `var a: int32 = 0` this is how you define a type in a variable. If no type is defined the compiler will try to infer the type from the value. If the compiler cannot infer the type it will throw an error. Example:

```vix
let a = 10 // The compiler will infer the type as int32
let b: int32 = 20 // The type is explicitly defined as int32

print(a) // will print 10
print(b) // will print 20
```

> **Note**: **Vix** is a memory safe language and to prevent overflow the default type of numbers is `int32/float32` if no type is defined. Learn more about types in **vix** in [Types](https://vixlanguage.github.io/docs/types)

Defining a value in a variable requires the `=` operator followed by the value. `let a = 10` this is how you define a value in a variable. Values can be anything like `class/function calls`, `Enums/Structs`, `Variables`, etc. If no value is defined the compiler will throw an error. Example:

```vix
func example()
    return 10
end

let a = 10
let b = // Error! missing value
let c = example() // OK: example() returns a value.

print(a) // will print 10
print(c) // will print 10 too
```

> **Note**: Defining a value using `class/function calls` is allowed in **vix**. But it depends on the return type. If the return type is not the same as the variable type the compiler will throw an error. Example:

```vix
func example(): int32
    return 10
end

let a: str = example() // Error! example() returns int32 not str
```

> **Warning**: Defining a value using `class/function calls` when there is no return. The compiler will set the variable value to the default value of the type. For example, if a variable type is `int32` and it calls a function `example()` that is supposed to return `int32` but the function doesn't return anything, it will be treated as a warning and the variable value will be `0`. This can lead to undefined behavior (UB). Example:

```vix
func example()
end

let a: int32 = example() // Warning! example() returns nothing but a is int32.

print(a) // will print 0
```
This is especially dangerous when the function is supposed to return a pointer address. If a function is meant to return an address to a pointer in memory but has no `return`, it will automatically return `0` which is `NULL` in memory. Dereferencing `NULL` is undefined behavior and can crash your program or corrupt your memory! This will trigger an error in **vix**. Example:
```vix
func example(): *int32
end

let a: *int32 = example() // Error! example() returns nothing but a is *int32. Will be automatically set to 0 which is NULL in memory.

printf("%d", a) // Tries to access the memory address that is NULL! Will crash your program!
```

---
### Defining variables

Variables can be defined in many ways in **vix**. You can define them with the `let/const/var/local` keywords. Each keyword has its own purpose and usage. Some keywords cannot be used in every scope — for example, the `local` keyword can only be used in the whole program scope. Other keywords run at compile time like the `const` keyword. All keywords can be stacked together, for example to make a mutable local variable: `local var a = 10` or `local let a = 10`. Here are the differences between all types:

- `let` — Defines an immutable variable. Cannot be changed after definition. Can be used in any scope. Cannot be stacked with other keywords. Example: `let a = 10`
- `const` — Defines an immutable variable that runs at compile time. Cannot be changed after definition. Can be used in any scope. Cannot be stacked with other keywords. Example: `const a = 10`
- `var` — Defines a mutable variable. Can be changed after definition. Can be used in any scope. Cannot be stacked with other keywords. Example: `var a = 10`
- `local` — Defines a variable that is only visible in the whole file scope, or the whole src if `public local`. Can be used in any scope. Can be stacked with other keywords. Example: `local a = 10` or `local var a = 10`

> **Note**: `local` variables are only accessible within the defined file. To make them accessible across the whole src use `public local`. You can also define a variable with just the type: `var a = int32` — this will define a variable with the type `int32`.

> **Warning**: When using `local` in a variable definition, the variable will only be visible in the whole file scope, or the whole src if `public local`. If there is a variable with the same name in a function scope the compiler will throw an error right away.

Using `local` in a variable definition and treating it as a mutable global variable can lead to a [Data Race](https://en.wikipedia.org/wiki/Race_condition#Data_race) in multi-threaded programs. This can result in undefined behavior (UB) or corrupted data. **Vix** will not automatically add a mutex to `local` variables. You need to manually add a mutex to `local` variables if you are using them in a multi-threaded program, as recommended, by defining it using `import mutex from std`. Example:

```vix
import mutex from std

local var a = 0

public func main()
    thread.spawn(do
        a += 1
    end)

    thread.spawn(do
        a += 1
    end)
end
```

> **Warning**: You cannot define more than one variable with the same name in a single scope. Otherwise it will be counted as an error. Example:

```vix
let a = 10
let a = 20 // Error! a is already defined
```
You cannot define a variable with a name that is already defined as a built-in keyword or type. Otherwise it will be counted as an error. This can be fixed using `r#Name`. Example:

```vix
let r#int32 = 10 // OK: r#int32 is not a built-in keyword or type

print(r#int32)
```

Variables can hold values like `function/class calls`, `Enum variants/Enums` such as `var example_enum = Example.Yes(10)`, `Struct fields` such as `var example_field = Example2.a`, and much more. They can also be used to hold another variable. Example:

```vix
enum Example
    Yes(int32),
    No(int32)
end

struct Example2
    a: int32,
    b: int32
end

var example_enum = Example.Yes(10)
var example_field = Example2.a

let a = 10
let b = a // OK: b is now 10

print(b) // will print 10
print(a) // Error! a is moved by 'b'
```

> **Warning**: When declaring a variable using another variable, the original one will no longer exist and will throw an error. This is because **vix** is a memory safe programming language where every variable owns its value. Example:

```vix
let a = 10
let b = a // OK: b is now 10

print(b) // will print 10
print(a) // Error! a is moved by 'b' (owned!)
```

---
#### Variable Types

- Mutable variables are variables that can be changed after definition. They are defined using the `var` keyword. They can only be used in their defined scope. Unlike other variables, they are changeable after being defined. Example:

```vix
func example()
    var a = 10
    a = 20 // OK: a is mutable

    print(a) // will print 20
end
```

> **Note**: Mutable variables cannot be used in global scope. They can only be used in a defined scope like a `class/function/loop/if statement` etc.

- Immutable variables are variables that cannot be changed after definition. They are defined using the `let` keyword. They can only be used in their defined scope. Unlike mutable variables, they cannot be changed after being defined. Example:

```vix
func example()
    let a = 10
    a = 20 // Error: cannot change immutable variable

    print(a) // will print 10
end
```

- Constant variables are variables that cannot be changed after definition. They are defined using the `const` keyword. They can only be used in their defined scope. Unlike other variables, they are not changeable after being defined. They are also compile-time variables, meaning they are evaluated at compile time. Example:

```vix
const MAX = 100

func example()
    const b = 10 + 30 // Compile-time operation
    MAX = 20 // Error: cannot change constant variable

    print(b) // 40
    print(MAX) // will print 100
end
```

> **Note**: Constant variables can be used in global scope. They can also work with a function to make it run at compile time using `const func example()`. Learn more about constant functions in [Functions](https://vixlanguage.github.io/docs/functions#const-functions).

- Local variables are variables that are only visible in the whole file scope, or the whole src if `public local`. They are defined using the `local` keyword. They can be used in any scope. They are also not changeable after being defined by default. Example:

```vix
local a = 10

func example()
    print(a) // will print 10
end

func example2()
    print(a) // will print 10 too
end
```

> **Note**: Local variables are immutable by default. They can be defined as mutable using `local var` or as constant using `local const`. Example: `local var a = 10` or `local const a = 10`


#### Variables Usage
Changing a variable's value is allowed in **vix**, but it depends on the variable type. If the variable is immutable the compiler will throw an error. To change the variable value you need to use an [operation keyword](https://vixlanguage.github.io/docs/keywords#operation) like `+=/=/-=/*=/...` and so on. However, unlike some other languages, some operation keywords are not allowed with `+=/-=/*=/...` and so on. Only numbers are allowed to be used with `+=/-=/*=/...` because they do not take ownership of the value. Example:

```vix
var a = 10
a += 10 // OK: a is mutable and a number

let b = 10
b += 10 // Error: b is immutable

var c = "Hello"
c += "World" // Error: c is not a number
```

You can change the value using [operation keywords](https://vixlanguage.github.io/docs/keywords#operation) like `+=/-=/etc..` only with numeric values like `int32/float32` and so on. Other types are not allowed to be used with `+=/-=/etc..` because they do not take ownership of the value. To change the whole variable value you need to use the `=` operator, which works with all types including `str` and other types. But it only works with `mutable variables`. Example:

```vix
var a = 10
a = 20 // OK: a is mutable and a number

let b = 10
b = 20 // Error: b is immutable

var c = "Hello"
c = "World" // OK: c is mutable and a string
```

> **Note**: To change a string variable's value you need to take ownership of the value and reallocate the memory when inserting a new value. This can be done using libraries like [std.String](https://vixlanguage.github.io/docs/libraries#string), a built-in string library in **std**. Example:

```vix
import string from std

var a = string.new("Hello")
a.add("World") // OK: a is mutable and a string

print(a) // "Hello, World!"
```
> You can make your own string library or create your own string type that supports operations and owns the value. For more information check [String creation](https://vixlanguage.github.io/tutorial#string-creation).

---
Multiple assignment is allowed in **vix**. You can define multiple variables in a single line by placing variable names after the variable keyword: `let a, b, c = (10, 50, 30)`. This works with all variable keywords `let/var/const/local` and also works with arrays, tuples, and so on. This can also be used when a function returns multiple variables or a tuple: `var a, b, c = example()`. Example:

```vix
func example(): (int32, int32, int32)
    return 10, 50, 30
end

var a, b, c = example()
let (y, x, z) = (10, 50, 30)
let (d, e, f) = [10, 50, 30]

print(a) // will print 10
print(b) // will print 50
print(c) // will print 30
```

> **Note**: Variable values are swappable after defining them. Example: `var a, b = (10, 50)` can be swapped as `a, b = b, a`. A variable also cannot reference itself in its own definition like `var a = a + 10` — this will throw an error to prevent UB. Example:

```vix
var a, b = 10, 50
let c = c + 10 // Error!
a, b = b, a

print(a) // will print 50
print(b) // will print 10
```

When a function returns a tuple or multiple variables that don't match the number of defined variables, the compiler will throw an error. This can be fixed using `_` to ignore extra variables: `let a, b, _ = example()`. Ignoring multiple variables using `let a, _, _ = example()` is also allowed. Example:

```vix
func example(): (int32, int32, int32)
    return 10, 50, 30
end

var a, b = example() // Error! example() returns 3 variables but only 2 are defined.
// fix:
var a, b, _ = example()
var y, _, _ = example() // no problems!

print(a) // will print 10
print(b) // will print 50
print(y) // will print 10
```

When a function returns a tuple or multiple variables that don't match the number of defined variables, the compiler will throw an error. This can be fixed by removing the extra variables. Example:

```vix
func example(): (int32, int32, int32)
    return 10, 50, 30
end

var a, b, c, d = example() // Error! example() returns 3 variables but 4 are defined.
                 //   - d is not allowed and needs to be removed!
// fix:
var a, b, c = example()
```

> **Warning**: Using `_` to ignore all returned variables is not recommended. This will produce a warning because the variables are not being used.

Vix allows defining variables as array/tuple types, for example `var a, b, c = (int, int, int)` or `var a, b, c = [int, int, int]`. This can be used to define multiple variables in a single line and also supports nested tuples: `var (a, (b, c)) = (int, (int, int))`. Example:

```vix
var a, b, c = (int, int, int) // defines a, b, c as int
let (d, (e, f)) = (int, (int, int)) // defines d, e, f as int

a = 10
b = 50
c = 30

d = 10
e = 50
f = 30

print(a) // 10
print(b) // 50
print(c) // 30

print(d) // 10
print(e) // 50
print(f) // 30
```

Variables can be defined inside `if/else/match/for` etc. statements. For example `if let a = example() then` will define a variable `a` with the value returned by `example()`. This can be used to control mutability by using `var` for mutable and `let` for immutable. Variable types like `local/const` do NOT work in this case. For some operations like `for/match` loops it works differently — variables are defined as immutable automatically and CANNOT be made mutable. You can use `var` to make the variable mutable in other cases.

- `if/else` statement variables are defined using `let/var` keywords. Example: `if let a = example() then` or `if var a = example() then`. They can only be used inside the `if/else` statement scope. Example:

```vix
func example(): int
    return 10
end

if let a = example() then
    print(a) // 10
end

print(a) // Error! a is not defined here
```

- `for` loop variables are defined as immutable automatically and CANNOT be made mutable. They can only be used inside the `for` loop scope. Example:

```vix
for i in 0..10 do
    print(i) // 0 to 10
end

print(i) // Error! i is not defined here
```

- `match` statement variables are defined as immutable automatically and CANNOT be made mutable. They can only be used inside the `match` statement scope. The same variable name can be defined in multiple cases. Example:

```vix
enum Example
    Yes(int32),
    No(int32)
end

func example(): Example
    return Yes(10)
end

match example()
    case Yes(a) do
        print(a)
    case No(a) do
        print(a)
end

print(a) // Error! a is not defined here
```

- `while` loop variables are defined using `let/var` keywords. Example: `while let a = example() do` or `while var a = example() do`. They can only be used inside the `while` loop scope. Example:

```vix
func example(): int
    return 10
end

while let a = example() do
    print(a) // 10
end

print(a) // Error! a is not defined here
```

---
Converting a variable's type is allowed in **vix**. This can be done using the `as` keyword. Example: `var a = 10 as float32` will convert the value `10` to the `float32` type. Example:

```vix
var a = 10
var b = a as float32

print(b) // will print 10.0
```

Lossy conversions will produce a warning. For example `var a = 3.3 as int32` will produce a warning because `3.3` is not a whole number and will automatically be converted to the closest whole number, which is `3` in this case. Example:

```vix
var a = 3.3

print(a) // will print 3.3
a as int32 // Warning! 3.3 is not a whole number and will be converted to 3
print(a) // will print 3
```

> **Warning**: Casting a variable to an incompatible type, for example `int32 to str`, is not allowed and will throw an error at compile time, or panic at runtime. If this happens at runtime in `release` mode, the value will automatically wrap. This can lead to UB!

If the variable contains a value larger than what the target type can hold, the compiler will throw an error. For example `var a = 256 as uint8` will throw an error because `256` is larger than what `uint8` can store. If the value is unknown at compile time and the overflow happens at runtime, the behavior depends on the build mode. In [debug mode ( --debug )](https://vixlanguage.github.io/docs/flags/debug) the program will panic, stop, and display an error message. In [release mode ( --release )](https://vixlanguage.github.io/docs/flags/release) it will not panic and will wrap the value instead. This can lead to UB and will produce a `warning` at compile time. Example:

- Example in `vix run --debug`:

```vix
var a: int32 = input("Give me a number:")
a as uint8

// vix run --debug
// Give me a number: 256 → will panic! returning an error:
```

```json
[Error]: Runtime Error: Integer overflow.
|
1 | var a: int32 = input("Give me a number:") // input() returns int32
|          ----- returned value of 256
3 | a as uint8
|  ----------- converting int32 to uint8 but the value is 256, this leads to overflow
|
-> note:
- Changing from int32 to uint8 can overflow since uint8's max value is 255 and the input returned 256.
-> help:
    |> Use a larger type like uint16
    |> Don't convert to uint8
```

- Example in `vix run --release`:

```vix
var a: int32 = input("Give me a number:")
a as uint8

// Will wrap the value to 0 (or the wrapped equivalent)

print(a) // will print the wrapped value. This can lead to UB!
```

The recommended fix is to use `as?` which is backed by `Result/Option`. This ensures that if the value overflows it will return `Err()` or `None` instead of wrapping or panicking. The result can then be handled using `if/match` statements. Example:

```vix
var a: Option[int32] = input()
var b: Result[(), int32] = input()

a as? uint8 // will return None if the value overflows

match a:
    case Some(v) do print(v)
    case None do print("Overflow detected")
end

// this can also be done directly
match b as? uint8:
    case Ok(v) do print(v)
    case Err(e) do print("Overflow detected")
end
```

Casting between pointer types, for example `*float32 to *int32`, is allowed in **vix** but is considered an unsafe operation and cannot be used outside of an `unsafe` block or function. The reason is that `int32` and `float32` are stored **completely differently in memory**. Example:

```vix
var a: *int32 = 10 as *int32
// You need an unsafe block to dereference
unsafe do
    print(*a)
end
```