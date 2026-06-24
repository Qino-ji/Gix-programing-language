# What is Functions

Functions are reusable blocks of code defined with the `func` keyword in Vix. They are used to perform a specific task and can be called from anywhere in the program depending on their visibility. Functions can take parameters, return values, and be passed around like any other value.

```vix
func function_name(param_1: type, param_2: type): type
    return something
end
```

---

## Declaring a Function

To declare a function you need the `func` keyword followed by the function name and `()`. Parameters go inside the parentheses and the return type goes after the closing parenthesis.
```vix
func add(a: int32, b: int32): int32
    return a + b
end
```

To call the function:
```vix
let i = add(10, 50)
print(i) // 60
```

---

## Parameters

Parameters are values passed into a function. Each parameter can have a specific type declared with `: type` after the parameter name. Parameters are optional   a function can have no parameters at all.
```vix
func greet()
    print("Hello!")
end

func add(a: int32, b: int32): int32
    return a + b
end
```

When you call a function and pass values to it, those values are bound to the parameter names inside the function. The original variable names outside the function do not matter inside it.
```vix
func add(a, b)
    print(a + b)
end

let x = 1
let y = 2

add(x, y) // x becomes a, y becomes b inside the function
```

---

## Return Type

The return type declares what type of value the function is allowed to return. It is placed after the closing `)` of the parameter list. If a function returns a value that does not match the declared return type, the typechecker will produce an error.
```vix
func add(a, b): str
    return a + b
    // ERROR: a + b produces int, but return type is str
end

func subtract(a, b): int
    return a - b
    // OK: a - b produces int, return type is int
end
```

Return type is optional. A function with no declared return type automatically returns `void`   it performs an action without producing a value.
```vix
func print_sum(a: int32, b: int32)
    print(a + b)
    // return type is void automatically
end
```

Trying to assign the result of a void function to a variable will produce an error:
```vix
func greet()
    print("Hello!")
end

var result = greet() // ERROR: greet() returns void, cannot assign to variable
```

You can also declare the return type as `void` explicitly, which behaves the same way:
```vix
func greet(): void
    print("Hello!")
end
```

---

## Returning a Value

To return a value from a function use the `return` keyword. The returned value will be passed back to wherever the function was called from.
```vix
func example(): int
    return 10
end

let a = example()
print(a) // 10
```

---

## Early Return

A function can return before reaching the end of its body. This is useful for guard clauses, validation, and avoiding deeply nested conditions.
```vix
func divide(a: int32, b: int32): int32
    if b == 0 then
        return 0
    end
    return a / b
end
```

Multiple early returns are allowed:
```vix
func classify(n: int32): str
    if n < 0 then return "negative" end
    if n == 0 then return "zero" end
    return "positive"
end
```

Early returns make code easier to read by handling edge cases at the top of the function and keeping the main logic at the bottom without extra nesting.

---

## Calling a Function

To call a function write its name followed by `()` with any arguments inside.
```vix
func add(a: int32, b: int32): int32
    return a + b
end

print(add(10, 50)) // 60
```

---

## Named Parameters

Vix allows you to call a function with parameters in any order by naming them explicitly. Vix will automatically match each named argument to the correct parameter.
```vix
func example(a, b, c)
    if a > b then
        return c
    end
end

let a = example(c = 30, a = 10, b = 50) // ok, order does not matter
print(a) // 30
```

---

## Optional Parameters

A parameter can be made optional by providing a default value using `=` after the parameter name. If the caller does not pass a value for that parameter, the default value is used instead.
```vix
func example(a, b = 10)
    return a + b
end

let a = example(10)     // b defaults to 10, returns 20
let b = example(10, 20) // b is 20, returns 30

print(a) // 20
print(b) // 30
```

---

## Missing or Extra Parameters

Calling a function with too few or too many arguments will trigger an error. The number of arguments passed must match the number of parameters the function declares unless a default value is provided.
```vix
func example(a, b, c)
    if a > b then
        return c
    end
end

let a = assert(example(10, 50, 30))     // ok, returns 30
let b = assert(example(10, 50))         // ERROR: missing parameter c
let c = assert(example(10, 50, 30, 40)) // ERROR: too many parameters

print(a) // 30
print(b) // false, error
print(c) // false, error
```

---
## Recursive Functions

A recursive function is a function that calls itself. Vix supports recursion   a function can reference its own name inside its body.
```vix
func factorial(n: int32): int32
    if n <= 1 then
        return 1
    end
    return n * factorial(n - 1)
end

print(factorial(5)) // 120
```

Recursion requires a base case   a condition that stops the function from calling itself forever. Without a base case the program will crash due to a stack overflow.
```vix
func infinite(n: int32): int32
    return infinite(n - 1) // ERROR: no base case, infinite recursion
end
```

Recursive functions are commonly used for tree traversal, mathematical sequences, and divide-and-conquer algorithms:
```vix
func fibonacci(n: int32): int32
    if n <= 1 then
        return n
    end
    return fibonacci(n - 1) + fibonacci(n - 2)
end

print(fibonacci(10)) // 55
```

---

## Function as a Type

Since functions and lambdas can be stored in variables and passed around, Vix treats functions as first-class values. The type of a function variable is declared using `func(param_types): return_type`.
```vix
var multiply: func(int32, int32): int32 = lambda a, b: a * b

multiply(3, 4) // 12
```

This also applies to parameters that expect a function value:
```vix
func apply(value: int32, operation: func(int32): int32): int32
    return operation(value)
end

apply(10, lambda x: x * 2) // 20
```

A function with no parameters and no return type:
```vix
var greet: func(): void = lambda: print("Hello!")

greet() // Hello!
```

Function types can also be stored in data structures:
```vix
var operations: func(int32): int32[] = [
    lambda x: x + 1,
    lambda x: x * 2,
    lambda x: x - 3
]

operations[0](10) // 11
operations[1](10) // 20
operations[2](10) // 7
```

---

## Higher-Order Functions

A higher-order function is a function that takes one or more functions as parameters or returns a function as its result. This is the formal pattern behind how lambdas are passed to functions like `map`, `filter`, and the error handling functions in `core/Option` and `core/Result`.

Taking a function as a parameter:
```vix
func apply(value: int32, operation: func(int32): int32): int32
    return operation(value)
end

apply(5, lambda x: x * 3)  // 15
apply(5, lambda x: x + 10) // 15
```

Returning a function:
```vix
func multiplier(factor: int32): func(int32): int32
    return lambda x: x * factor
end

var double = multiplier(2)
var triple = multiplier(3)

double(5) // 10
triple(5) // 15
```

Combining both   taking and returning functions:
```vix
func transform(arr: int32[], fn: func(int32): int32): int32[]
    return arr.map(fn)
end

var arr = [1, 2, 3, 4, 5]

transform(arr, lambda x: x * 2)  // [2, 4, 6, 8, 10]
transform(arr, lambda x: x + 10) // [11, 12, 13, 14, 15]
```

---

## Function Overloading

Vix supports function overloading   multiple functions can share the same name as long as their parameter types are different. The typechecker resolves which version to call based on the arguments passed.
```vix
func add(a: int32, b: int32): int32
    return a + b
end

func add(a: float, b: float): float
    return a + b
end

func add(a: str, b: str): str
    return a + b
end

add(1, 2)          // calls int32 version → 3
add(1.0, 2.0)      // calls float version → 3.0
add("hi", " vix")  // calls str version → "hi vix"
```

Calling with mismatched types will produce an error:
```vix
add(1, "hello") // ERROR: no overload matches (int32, str)
```

Overloading also works with different parameter counts:
```vix
func greet(): str
    return "Hello!"
end

func greet(name: str): str
    return "Hello, " + name + "!"
end

greet()      // "Hello!"
greet("Vix") // "Hello, Vix!"
```

> For functions that work across many types with the same logic, consider using [Generic Parameters](#generic-parameters) instead of writing multiple overloads.

---

## Type Inference

Vix has a smart and fast typechecker. It runs before code generation to check and infer types where possible. Parameter types and return types can often be inferred automatically from the values passed to the function.

```vix
func add(a, b)
    return a + b
end

var i = add(1, 2)
// Passing int, int to the function.
// Typechecker automatically infers a, b and return type as int.
```

---

## Function Visibility

Vix has 4 levels of function visibility that control where a function can be called from. Choosing the right visibility is important for encapsulation and API design.

| Keyword | Same File | Same Library | Other Files | Outside Library |
|---|---|---|---|---|
| `public` | ✓ | ✓ | ✓ | ✓ |
| none (private) | ✓ | ✗ | ✗ | ✗ |
| `local` | ✓ | ✓ | ✗ | ✗ |
| `extern` | ✓ | ✓ | ✓ | ✓ (dynamic lib only) |


For more details on each visibility type see [local](https://vixlanguage.github.io/docs/local), [public](https://vixlanguage.github.io/docs/public), [private](https://vixlanguage.github.io/docs/private), [extern](https://vixlanguage.github.io/docs/extern).

### Public

`public` is the most permissive visibility. A public function can be called from anywhere   the same file, other files in the same project, and even outside the library if the function is declared inside one.

Use `public` when you want to expose a function as part of your API or when other files need to call it.

**File structure:**
```
root/
    src/
        main.vix
        other.vix
```

**main.vix:**
```vix
public func add(a: int32, b: int32): int32
    return a + b
end
```

**other.vix:**
```vix
import add from main

var result = add(10, 20)
printf("%d\n", result) // 30
```

You can also call a public function from within the same file it was declared in:
```vix
public func add(a: int32, b: int32): int32
    return a + b
end

public func add_three(a: int32, b: int32, c: int32): int32
    return add(a, b) + c // ok, calling public from same file
end
```

When building a library, public functions form the public API that users of the library can call:
```
mylib/
    math.vix
    utils.vix
```
```vix
// math.vix
public func multiply(a: int32, b: int32): int32
    return a * b
end
```
```vix
// main.vix outside the library
import multiply from mylib // ok, public is accessible outside library
```

### Private (no keyword)

Private is the default visibility when no keyword is used. A private function can only be called from within the same file it was declared in. It is completely invisible to other files even if they try to import it.

Use private functions for internal helpers that are implementation details and should not be exposed to other files or users of your library.
```vix
// math.vix

func clamp_internal(val: int32, min: int32, max: int32): int32
    if val < min then return min end
    if val > max then return max end
    return val
end

public func safe_divide(a: int32, b: int32): int32
    var clamped = clamp_internal(b, 1, 1000) // ok, same file
    return a / clamped
end
```

Trying to access a private function from another file will always fail:
```vix
// other.vix
import clamp_internal from math // ERROR: clamp_internal is private
import safe_divide from math    // ok, safe_divide is public
```

### Local

`local` sits between `private` and `public`. A local function can be called from anywhere inside the same library but cannot be accessed from outside the library.

Use `local` when you have helper functions that need to be shared across multiple files within your library but should not be part of the public API.

**File structure:**
```
root/
    mylib/
        math.vix
        utils.vix
    src/
        main.vix
```

**math.vix:**
```vix
local func internal_multiply(a: int32, b: int32): int32
    return a * b
end

public func square(a: int32): int32
    return internal_multiply(a, a) // ok, same library
end
```

**utils.vix inside same library:**
```vix
import internal_multiply from math // ok, same library

public func cube(a: int32): int32
    return internal_multiply(internal_multiply(a, a), a)
end
```

**main.vix outside library:**
```vix
import internal_multiply from mylib // ERROR: local, not accessible outside library
import square from mylib            // ok, public
import cube from mylib              // ok, public
```

### Extern

`extern` is a special visibility designed for dynamic libraries. An extern function can be called both from inside and outside the file, but it only works when the file is compiled as a dynamic library (`.dll` on Windows, `.so` on Linux).

Use `extern` when you are building a dynamic library and want to expose functions to external programs or other languages like C, Python, etc.
```vix
// mylib.vix
extern func compute(a: int32, b: int32): int32
    return a + b
end
```
```vix
// main.vix
import compute from mylib

var result = compute(5, 10)
printf("%d\n", result) // 15
```

Extern functions can also be called from other languages that load the dynamic library. For example from C:
```c
#include <dlfcn.h>

int main() {
    void* lib = dlopen("mylib.so", RTLD_LAZY);
    int (*compute)(int, int) = dlsym(lib, "compute");
    printf("%d\n", compute(5, 10)); // 15
}
```

### When to use each

| Visibility | Use when |
|---|---|
| `public` | Building an API or sharing functions across files |
| none (private) | Internal implementation details that only one file needs |
| `local` | Shared helpers within a library that should not be public API |
| `extern` | Building a dynamic library exposing functions to external programs |

---

### Lambda Functions

Lambda functions are anonymous functions that can be defined inline and passed around like variables. They are useful for short operations like transforming or filtering data without needing a named function.

#### Syntax Variants

Vix supports two ways to define a lambda:

Shorthand   for simple inline expressions:
```vix
lambda params: body
```

Full syntax   for typed or multi-line lambdas:
```vix
func(params): return_type
    body
end
```

#### Parameters

Lambdas can take zero, one, or multiple parameters:
```vix
lambda: do_something()           // no params
lambda x: x * 2                 // single param
lambda a, b: a + b              // multiple params
```

#### Return Values

Lambdas can return values either implicitly or explicitly with the `return` keyword:
```vix
lambda a, b: a + b              // implicit return
lambda a, b: return a + b       // explicit return (inside functions only)
```

#### Closures

Lambdas can capture variables from their outer scope:
```vix
var c = 10
lambda a, b: a + b - c         // c is captured from the outer scope
```

#### Multi-line Lambdas

Lambdas are not limited to a single line:
```vix
lambda x:
    var result = x * 2
    return result
```

#### Full Syntax in Depth

The full syntax allows you to specify a return type and write multi-line bodies. It can be used as a function argument or stored in a variable:
```vix
func add(func(a, b): int32
    return a + b
end)

var multiply = func(a, b): int32
    return a * b
end

multiply(3, 4) // 12
```

#### When to Use Each Syntax

| | Shorthand `lambda` | Full `func()` |
|---|---|---|
| Return type annotation | ✗ | ✓ |
| Multi-line body | ✓ | ✓ |
| Inline/concise | ✓ | ✗ |
| As function argument | ✓ | ✓ |
| Stored in variable | ✓ | ✓ |

Use shorthand for simple operations. Use full syntax when you need type safety or a more complex body.

#### Usage Examples
```vix
var arr = [1, 2, 3, 4, 5]

arr.map(lambda x: x * 2)              // transform
arr.filter(lambda x: x > 2)          // filter
arr.split(lambda x: print(x))        // side effect

var multiply = lambda a, b: a * b    // stored in a variable
multiply(3, 4)                        // 12
```

#### Error Handling

Lambdas are central to error handling in Vix. The [core/Option](https://vixlanguage.github.io/docs/core/option) and [core/Result](https://vixlanguage.github.io/docs/core/result) libraries rely on lambdas as their primary operation mechanism. Functions like `map`, `map_err`, `and_then`, `or_else`, `expect`, `unwrap_or_else`, and more all take lambdas to handle success and failure cases.
```vix
var result = get_user(id)
    .map(lambda u: u.name)
    .unwrap_or_else(lambda e: "Unknown")
```

See [core/Option](https://vixlanguage.github.io/docs/core/option) and [core/Result](https://vixlanguage.github.io/docs/core/result) for the full list of available functions.

#### Error handling

Function can cause other errors. Errors be from E90 to E100 like: 

| Error Code | Error | Description |
|------------|-------|-------------|
| E90 | Wrong return type. | Returning a value with a different type than what the function declares as return type |
| E91 | Calling a function with wrong param | Calling a function with a different param than what the function declares as param type |
| E92 | Calling a function with wrong param count | Calling a function with a different param count than what the function declares as param count |
| E93 | Using a function that is not implemented | Using a function that is not implemented |
| E94 | Calling a function with missing param | Calling a function with a missing param than what the function declares as param |
| E95 | Calling a function with extra param | Calling a function with extra param than what the function declares as param |
| E96 | Calling a function inside a class without self | Calling a function inside a class without using `self` keyword |
| E97 | Using a function that is not allowed to be used in a class | Using a function that is not allowed to be used in a class |
| E98 | Calling private function | Calling a private function from outside the class it is defined in |
| E99 | Infinity loop function | Calling a function that calls itself without a base case example |
| E100 | Using a local function outside of a library | Using a local function outside of the library it is defined in |
---

#### Error handling
Error can be caused by lifetimes are presented in E80 to E89. Every error will show the exact location in the code, the lifetime that caused the error, and a suggestion on how to fix it.

| Error Code | Error | Description |
|------------|-------|-------------|
| E80 | Lifetime not declared | Using `@name` on a parameter but `name` was never defined in `life[...]` |
| E81 | Duplicate lifetime declaration | Declaring the same lifetime name twice in `life[a, a]` |
| E82 | Unused declared lifetime | Declaring a lifetime in `life a, life b` but never using it with `@name` anywhere in the function |
| E83 | Return lifetime mismatch | Returning a value with a different lifetime than what the function declares as return type |
| E84 | Missing lifetime on return type | Function has lifetime parameters and input uses `@name` but return type has no lifetime, possible lifetime information lost |
| E85 | Using undeclared lifetime in body | Using `@name` on a local variable inside the function but `name` was never declared |
| E86 | Returning local variable | Returning a local variable that has no lifetime attached but return type expects a lifetime |
| E87 | Mixing lifetimes in operation | Combining two values of different lifetimes in an expression and assigning to a specific lifetime |
| E88 | Calling function with wrong lifetime | Passing a value with lifetime `@a` to a function that expects a different lifetime |
| E89 | Lifetime outliving its scope | Using a variable outside the block it was declared in, its lifetime already ended |

> **Note:** E82 is a warning and will not stop compilation. All other errors E80-E89 are hard errors and will stop compilation.

# Class Functions
## Function Visibility
Class Functions. Functions that can be implemented inside vix's [hybred class](https://vixlanguage.github.io/docs/stmt/hybrid-classes). Aside of normal functions outside classes. Functions in the class can be called using different syntax, By accesing the function from the class it self. `ClassName.function_needed()` or by accesing from the variable it self `variable.function_needed()`. Functions are allowed to be implemented in a variable using `let var_name = ClassName.function_needed()` you can do var_name(10, 50) and automaticlly going to call that function. Example:
```vix
class Math()
    // Implement a function.
    func add(a, b)
        return a + b
    end

    func mini(a, b)
        return a - b
    end

    func multi(a, b)
        return a * b
    end
end

let math_class = Math() // Accese a function using a variable.
let multi_function = Math.multi()

print(Math.add(1, 2)) // 3
print(math_class.mini(1, 2)) // -1
print(multi_function(2, 2)) // 4
```

To call a function from inside the class that exists inside it. You need to use `self.function_name()` Example:
```vix
func print(a)
    printf("from outside the class: %s", a)
end

class Example()
    func print(a)
        printf("from the class: %s", a)
    end

    self.print("Hello, world!") // Calling function inside the class!
    print("Hello, world!") // Calling a function outside of the class!
```

**Info**: Private functions cannot be called inside another class implemention they need to be public example:
```vix
class Example()
    func print(a)
        printf("%s", a)
    end
end

class Example() // Using same function
    self.print() // Error: Function is not public
end
```

Functions in the class can be only in 3 different types: `unsafe`/`public`/`private`. 
- `unsafe`   Function is unsafe to call and can only be called from unsafe blocks or other unsafe functions.
- `public`   Function is safe to call and can be called from anywhere.
- `private`   Function is only visible inside the class and cannot be called from outside the class.

#### Unsafe functions
Unsafe functions that can be used for unsafe operations are not avalable in safe code. To implement unsafe function you'll need to use `unsafe` keyword before the function definition. Example:
```vix
class Example()
    unsafe func example()
        // Unsafe code here
    end

    unsafe func calling_example() // Use `unsafe` block to call an unsafe function!
        self.example() // calling unsafe function
    end
end
```

- Unsafe functions can only be called from other unsafe functions or blocks.
- Unsafe functions are allowed to do unsafe operations like dereferencing raw pointers, accessing memory outside of bounds, and more...
- Unsafe functions are not allowed to be called from safe code. And will cause a compiler error.
- Unsafe functions force memory safety rules.

#### Public functions
Public functions are functions that can be called from anywhere. They visible from calling them in another class using `self.public_function()` that unlike `private` functions. To make a function public you need to attactch `public` keyword before the function definition. Example:
```vix
class Example()
    public func example()
        // Public code here
    end
end

class Example() // Using same function
    self.example() // Works! Function is public!
end
```

- Public functions & private function can still be called from class it self using `self.function_name()`. Or outside the class using `ClassName.function_name()` or `variable_name = ClassName.function_name()` and `variable_name()`.

#### Private functions
Private functions are functions that can only be called from inside the class it self. To make a function private function you need to not attatch any keyword before the function definition. Example:
```vix
class Example()
    func example()
        // Private code here
    end

    self.example() // Works! Function is private!
end

class Example() // Using same function
    self.example() // Error: function is private!
end
```

#### Specific functions & return types
Functions allowed to use `Self` keyword as return type to the class. Only if the function has param or struct/enum attached to it. To store the data on return type you need to use `Self { ... }` and inside the brackets you need to put the data you want to store. Example:
```vix
class Example(a, b)
    func example(x, y): Self
        return Self {
            a = y
            b = x
        }
    end
end

Example(10, 30)
```

- `Self` return type is allowed only in classes and enums.
- `Self` return type can only be used if the function has params or struct/enum attached to it.

Function allowed too to use `self` keyword to accese function stored data in param/Struct/enum etc... Example `self.a = self.b`. Presinting self.feild of function fields. To implement `self` you need to use `self` keyword in function param: `func example(self)`. And it's allowed to be used in any all function implementions outside of orginal one. `self` can be in very other types like mutable/Immutable.

```vix
class Example(a, b)
    func example(self, x, y)
        return self.a = self.b
    end
end

class Example()
    func example_another_class(self)
        return self.a = self.b // Allowed operation
    end
end

Example(10, 30)
```
### Error handling
Error can be caused by class functions are presented in E90 to E99. Every error will show the exact location in the code, the class function that caused the error, and a suggestion on how to fix it.

| Error Code | Error | Description |


## Notes

Function return type must match what is actually returned inside the function body.

Function return type must match the variable type when assigning a function call result to a variable.

A function with no declared return type automatically returns `void`.

A function can contain any number of parameters and each parameter can have a specific type.

Parameter types and return types can be inferred automatically by the typechecker when possible.