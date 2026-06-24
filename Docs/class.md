## What is Class

Classes are custom data types used to store data and functions. They hold data that can be changed during program execution. Classes are declared using the `class` keyword followed by the class name and `()`. Parameters go inside the parentheses. The body is closed with the `end` keyword:

```vix
class class_name(/* Optional parameters */)
    // Class body
end
```

Class parameters are used when calling a class directly  `class class_name(param_1, param_2)`  and calling the class stores data in fields accessible using `self.field` like `self.a` or `self.b`. Fields are accessible outside of a class:

```vix
class Example(a, b)
    self.a // accessing field a
    self.b // accessing field b
end

Example(10, 50)

print(Example.a) // 10
print(Example.b) // 50
```

> **Note**: Fields in Vix are immutable by default. To make a field mutable, use the `var` keyword before the field name:
```vix
class class_name(var a, b)
    // var a is mutable, b is immutable
end
```

---

## Declaring a Class

To declare a class use the `class` keyword followed by the class name and optional parameters. Close the body with `end`:

```vix
class Point(x, y)
end
```

To instantiate and access the fields:

```vix
Point(10, 20)

print(Point.x) // 10
print(Point.y) // 20
```

---

## Parameters

Parameters are values passed into a class at instantiation. Each parameter becomes a field accessible through `self.field`. Parameters are optional  a class can have none.

```vix
class Empty()
end

class Named(name)
end

class Positioned(x, y)
end
```

When you call a class with values, those values are bound to the parameter names as fields. The original variable names outside the class do not matter inside it.

```vix
class Box(width, height)
end

let w = 100
let h = 50

Box(w, h) // width = 100, height = 50 inside the class
```

---

## Default Parameter Values

Class parameters can have default values, used when the caller omits that argument. Defaults are written using `param: type = value` syntax:

```vix
class Point(x: int = 0, y: int = 0)
end

Point()        // x = 0, y = 0
Point(10)      // x = 10, y = 0
Point(10, 20)  // x = 10, y = 20
```

Defaults can be combined with `var` for mutable fields:

```vix
class Counter(var count: int = 0, max: int = 100)
end

Counter()       // count = 0, max = 100
Counter(5)      // count = 5, max = 100
Counter(5, 50)  // count = 5, max = 50
```

Defaults work alongside non-defaulted parameters too:

```vix
class Window(title: str, var width: int = 800, var height: int = 600)
end

Window("My App")           // width = 800, height = 600
Window("My App", 1280)     // width = 1280, height = 600
Window("My App", 1280, 720) // width = 1280, height = 720
```

> **Note**: Parameters with defaults must come after parameters without defaults. Placing a defaulted parameter before a required one produces a compiler error.

```vix
class Bad(x: int = 0, y: int)   // ERROR: required param after default
end

class Good(x: int, y: int = 0)  // OK
end
```

---

## Field Mutability

Fields in Vix are immutable by default. A field declared without `var` cannot be modified after the class is instantiated. To make a field mutable, add `var` before its name in the parameter list:

```vix
class Counter(var count, max)
    // count is mutable, max is immutable
end
```

Attempting to modify an immutable field produces an error:

```vix
class Example(a, b)
end

Example(10, 20)
Example.a = 99 // ERROR: field a is immutable
```

Mutable fields can be freely changed:

```vix
class Counter(var count, max)
end

Counter(0, 100)
Counter.count += 1 // OK: count is mutable
Counter.max = 200  // ERROR: max is immutable
```

---

## Functions Inside Classes

Classes can contain functions declared with the `func` keyword. To call a function from within the same class, use the `self` keyword: `self.some_function()`. To access fields from inside a function, the function must declare `self` as a parameter.

```vix
class Example(a, b)
    func example(self)
        return self.b // accessing a field
    end

    func another_example(self)
        return self.example() // calling another function in the class
    end
end
```

---

## Function Visibility

Functions inside a class have three visibility levels: private (default), public, and unsafe.

| Keyword | Same Class | Same Scope | Outside Class |
|---|---|---|---|
| none (private) | ✓ | ✗ | ✗ |
| `public` | ✓ | ✓ | ✓ |
| `unsafe` | ✓ (in unsafe) | ✓ (in unsafe) | ✓ (in unsafe) |

### Private

Private is the default when no keyword is used. A private function can only be called from within the class it is defined in. Calling a private function from another class or scope produces an error.

```vix
class Example()
    func private_fn()
        // only accessible inside this class
    end

    func caller(self)
        self.private_fn() // OK: same class
    end
end

class Other()
    func caller(self)
        self.private_fn() // ERROR: private_fn is private
    end
end
```

### Public

`public` functions can be called from anywhere  the same class, other classes, and outside of any class scope entirely. Use `public` when a function should be part of the class's external interface.

```vix
class Math()
    public func add(a, b)
        return a + b
    end
end

class Usage()
    func use(self)
        self.add(1, 2) // OK: add is public
    end
end

print(Math.add(10, 50)) // OK: callable from outside too
```

### Unsafe

`unsafe` functions can only be called from other `unsafe` functions or `unsafe` blocks. They are designed for low-level operations like raw pointer dereferencing or out-of-bounds memory access that Vix cannot safety-check at compile time.

```vix
class LowLevel()
    unsafe func raw_op()
        // unsafe operations here
    end

    unsafe func caller(self)
        self.raw_op() // OK: calling unsafe from unsafe
    end

    func safe_caller(self)
        self.raw_op() // ERROR: cannot call unsafe from safe context
    end
end
```

Unsafe functions cannot be called from safe code and will produce a compiler error if attempted.

---

## Calling a Class

Vix supports multiple ways to call a class and invoke its functions:

```vix
class Math()
    public func add(a, b)
        return a + b
    end
end

class Example(a, b)
end

// Direct call using class name
print(Math.add(10, 50))

// Using a variable
let math = Math()
print(math.add(10, 50))

// Storing a function reference in a variable
let add_fn = Math.add
print(add_fn(10, 50))

// Instantiating a parameterized class
let example = Example()
example(10, 50)  // using a variable
Example(10, 50)  // direct call
```

---

## The `self` Keyword

`self` gives a function access to the class's fields and other functions. To use `self` inside a function, declare it as the first parameter of that function:

```vix
class Counter(var count)
    public func increment(self)
        self.count += 1
    end

    public func get(self)
        return self.count
    end
end

Counter(0)
Counter.increment()
print(Counter.get()) // 1
```

`self` can also appear in functions from a different class scope when the classes share a name with a matching struct or enum. See the [Hybrid Classes](#hybrid-classes) section below.

---

## Hybrid Classes

Classes in Vix are hybrid  they can be used alongside a `struct` or `enum` of the same name. This gives the class access to the struct or enum's fields through `self.field`.

```vix
struct Example
    var a: int
    b: int
end

class Example()
    func compare(self): bool
        if self.a > self.b then
            return true
        elif self.a < self.b then
            return false
        else
            self.a += 10
        end
    end
end

Example.a = 10
Example.b = 30

if var out = Example.compare() then
    print(out)        // true or false
else
    print(Example.a)  // 20
end
```

For more details see [Struct](https://vixlanguage.github.io/docs/structers) and [Enum](https://vixlanguage.github.io/docs/enums).

---

## The `Self` Return Type

A function inside a class can return a new instance of the class using `Self` as the return type. To construct the return value, use `Self { ... }` with the fields to initialize:

```vix
struct Example
    a: int
    b: int
end

class Example()
    func new(): Self
        return Self {
            a = 0,
            b = 0
        }
    end
end

Example.new() // creates an instance with a = 0, b = 0
```

`Self` as a return type is only valid inside classes and enums. It can only be used when the function has parameters or when a struct or enum is attached to the class.

A function that takes parameters and returns `Self`:

```vix
class Point(a, b)
    func swap(self, x, y): Self
        return Self {
            a = y,
            b = x
        }
    end
end

Point(10, 30)
```

---

## Function as a Type in Classes

Since class functions can be stored in variables and passed around, they behave as first-class values just like standalone functions. Storing a function reference from a class:

```vix
class Math()
    public func multiply(a, b)
        return a * b
    end
end

let multi_fn = Math.multiply
multi_fn(3, 4) // 12
```

Passing a class function as an argument:

```vix
class Ops()
    public func double(x)
        return x * 2
    end
end

func apply(value: int, operation: func(int): int): int
    return operation(value)
end

apply(10, Ops.double) // 20
```

---

## Special Functions

Vix classes support a set of special functions that are triggered automatically by the compiler or runtime under specific conditions. These are declared like normal functions but annotated with a decorator that tells the compiler their role. A class can implement any combination of these.

---

### `@operation`

`@operation` lets a class define the behavior of an operator like `==`, `<`, `+`, or any other operator. The decorator takes the operator as a string argument. When the compiler sees that operator applied to two instances of the class, it calls this function automatically.

```vix
class Point(x, y)
    @operation("==")
    public func equals(self, other: Self): bool
        return self.x == other.x and self.y == other.y
    end

    @operation("<")
    public func less_than(self, other: Self): bool
        return self.x < other.x
    end

    @operation("+")
    public func add(self, other: Self): Self
        return Self {
            x = self.x + other.x,
            y = self.y + other.y
        }
    end
end

Point(1, 2)
let a = Point(1, 2)
let b = Point(3, 4)

print(a == b) // calls equals()     → false
print(a < b)  // calls less_than()  → true
print(a + b)  // calls add()        → Point(4, 6)
```

All operators are supported. Each operator requires its own `@operation` decorated function. Using an operator on a class that has no matching `@operation` function produces a compiler error.

---

### `@iter`

`@iter` marks a function as the class's iterator provider. The compiler calls this function automatically when the class is used in a `for` loop. The annotated function must return something the compiler can iterate over  the loop variable receives each element in sequence.

```vix
class Range(var current: int, stop: int)
    @iter
    public func each(self)
        return self.current to self.stop
    end
end

Range(0, 5)

for n in Range.each() do
    print(n) // 0, 1, 2, 3, 4
end
```

`@iter` is compiler-only  you never call the annotated function directly in a `for` loop. Writing `Range.each()` outside of a loop calls it as a normal function. The `@iter` annotation is what signals the compiler to invoke it when it sees `for a in class`.

---

### `@drop`

`@drop` marks a function that the compiler calls automatically when the class instance goes out of scope or its lifetime ends. It is used for cleanup that needs to happen deterministically  closing handles, releasing resources, resetting state.

```vix
class File(var handle: int, path: str)
    @drop
    public func close(self)
        close_handle(self.handle)
    end
end

func read_file()
    File(open("data.txt"), "data.txt")
    // ... use file ...
end // File goes out of scope here  close() is called automatically
```

`@drop` runs exactly once per instance, at the point the lifetime ends. You cannot call a `@drop` function manually  doing so produces a compiler error. If cleanup needs to be triggered early, use `@free` instead.

---

### `@free`

`@free` marks a function for manual memory release. Unlike `@drop`, which the compiler calls automatically at lifetime end, `@free` gives you explicit control  you call it yourself when you are done with the instance and want to release it immediately.

```vix
class Buffer(var data: int[], var size: int)
    @free
    public func release(self)
        self.data = []
        self.size = 0
    end
end

let buf = Buffer([1, 2, 3], 3)
buf.release() // manually release the buffer
```

`@free` does not run automatically. If you never call it, the instance is cleaned up normally when its lifetime ends. Use `@free` when you need to release resources earlier than the lifetime system would otherwise allow.

---

### `@call`

`@call` marks a function that the compiler invokes when an already-instantiated class is called like a function  `instance()`. This lets a class behave as a callable, similar to how a function variable works.

```vix
class Multiplier(factor: int)
    @call
    public func invoke(self, value: int): int
        return self.factor * value
    end
end

Multiplier(3)

let triple = Multiplier
print(triple(10)) // calls invoke() → 30
print(triple(5))  // calls invoke() → 15
```

`@call` is triggered only on an instance that already exists  `triple(10)` where `triple` holds an instantiated class. Calling the class by name for the first time, `Multiplier(3)`, is instantiation and does not go through `@call`.

A class can only have one `@call` function. Declaring more than one produces a compiler error.

---

## Type Constraints

A generic type parameter can be constrained to a trait or type using `: Trait` after the parameter name. This ensures the type passed to the class satisfies the required interface, and lets functions inside the class call methods defined by that trait.

```vix
class Example[T: Clone](a: T, b: T)
    public func get(self): T
        return self.a
    end
end
```

Multiple constraints can be combined using `+`:

```vix
class Example[T: Clone + Compare](a: T, b: T)
    public func greater(self): bool
        return self.a > self.b
    end
end
```

The typechecker will produce an error if a type is passed that does not satisfy every listed constraint.

---

## Trait Implementation with `for`

A class can declare that it implements a trait using the `for Trait` syntax after the parameter list. This commits the class to providing every function required by that trait. The compiler will verify that all required functions are present and have matching signatures.

```vix
trait Clone
    func clone(self): Self
end

class Example[T](a: T, b: T) for Clone
    func clone(self): Self
        return Self {
            a = self.a,
            b = self.b
        }
    end
end
```

A class can implement multiple traits by chaining them with `+`:

```vix
trait Clone
    func clone(self): Self
end

trait Display
    func display(self): str
end

class Example[T](a: T, b: T) for Clone + Display
    func clone(self): Self
        return Self {
            a = self.a,
            b = self.b
        }
    end

    func display(self): str
        return "Example"
    end
end
```

Omitting a required trait function produces a compiler error listing which functions are missing.

---

## Multiple Generic Parameters

A class can declare more than one type parameter by separating them with commas inside the square brackets:

```vix
class Pair[A, B](first: A, second: B)
    public func get_first(self): A
        return self.first
    end

    public func get_second(self): B
        return self.second
    end
end

Pair[int, str](10, "hello")

print(Pair.get_first())  // 10
print(Pair.get_second()) // "hello"
```

Each parameter can carry its own constraints independently:

```vix
class Map[K: Hash, V: Clone](key: K, value: V)
    public func get(self): V
        return self.value
    end
end
```

---

## Type Inference

The typechecker can infer generic type parameters automatically from the values passed at instantiation. When the type can be determined from the arguments, you do not need to declare it explicitly:

```vix
class Box[T](value: T)
    public func get(self): T
        return self.value
    end
end

Box(42)        // typechecker infers T as int
Box("hello")   // typechecker infers T as str

print(Box.get()) // "hello"
```

Explicit annotation is still required when the type cannot be inferred from the arguments alone, or when you want to enforce a specific type at the call site:

```vix
Box[int](42)   // explicit: T is int
Box[str]("hi") // explicit: T is str
```

---

## Common Mistakes

### Forgetting `self` in a function that accesses fields

Functions inside a class do not automatically have access to the class's fields. You must declare `self` as the first parameter explicitly.

```vix
//   Wrong
class Counter(var count: int)
    public func increment()
        count += 1  // ERROR: count is not in scope
    end
end

//  Correct
class Counter(var count: int)
    public func increment(self)
        self.count += 1
    end
end
```

---

### Forgetting `var` on a field you want to mutate

Fields are immutable by default. Assigning to one without `var` is a compile error.

```vix
//   Wrong
class Timer(elapsed: int)
    public func tick(self)
        self.elapsed += 1  // ERROR: elapsed is immutable
    end
end

//  Correct
class Timer(var elapsed: int)
    public func tick(self)
        self.elapsed += 1
    end
end
```

---

### Wrong visibility  calling a private function from outside

Functions are private by default. Forgetting `public` is the most common reason a function is unreachable from outside the class.

```vix
//   Wrong
class Math()
    func add(a: int, b: int): int  // private by default
        return a + b
    end
end

print(Math.add(1, 2))  // ERROR: add is private

//  Correct
class Math()
    public func add(a: int, b: int): int
        return a + b
    end
end

print(Math.add(1, 2))  // OK
```

---

### Calling a `@drop` function manually

`@drop` is reserved for the compiler. Calling it yourself is a hard error. Use `@free` instead when you need explicit early cleanup.

```vix
//   Wrong
class File(var handle: int)
    @drop
    public func close(self)
        close_handle(self.handle)
    end
end

File(1)
File.close()  // ERROR: cannot call @drop manually

//  Correct  use @free for manual cleanup
class File(var handle: int)
    @free
    public func close(self)
        close_handle(self.handle)
    end
end

File(1)
File.close()  // OK
```

---

### Using `Self` return type without a struct or parameters

`Self` can only be returned when the class has parameters or an attached struct/enum. Without those, there is nothing to construct.

```vix
//   Wrong
class Empty()
    func new(): Self   // ERROR: nothing to construct Self from
        return Self {}
    end
end

//  Correct
struct Point
    x: int
    y: int
end

class Point()
    func new(): Self
        return Self { x = 0, y = 0 }
    end
end
```

---

### Putting a defaulted parameter before a required one

Required parameters must always come before defaulted ones.

```vix
//   Wrong
class Rect(var w: int = 100, h: int)  // ERROR: required param after default
end

//  Correct
class Rect(h: int, var w: int = 100)
end
```

---

## Real-World Examples

### Stack

A generic last-in, first-out stack with push, pop, and peek.

```vix
class Stack[T](var items: T[], var size: int = 0)
    public func push(self, value: T)
        self.items[self.size] = value
        self.size += 1
    end

    public func pop(self): T
        self.size -= 1
        return self.items[self.size]
    end

    public func peek(self): T
        return self.items[self.size - 1]
    end

    public func is_empty(self): bool
        return self.size == 0
    end
end

Stack[int]([], 0)
Stack.push(1)
Stack.push(2)
Stack.push(3)
print(Stack.pop())   // 3
print(Stack.peek())  // 2
```

---

### Linked List

A singly linked list with append and iteration support via `@iter`.

```vix
struct Node[T]
    value: T
    var next: Node[T]?
end

class LinkedList[T](var head: Node[T]? = none, var length: int = 0)
    public func append(self, value: T)
        let node = Node { value = value, next = none }
        if self.head == none then
            self.head = node
        else
            let current = self.head
            while current.next != none do
                current = current.next
            end
            current.next = node
        end
        self.length += 1
    end

    @iter
    public func each(self)
        let current = self.head
        while current != none do
            yield current.value
            current = current.next
        end
    end
end

LinkedList[int]()
LinkedList.append(10)
LinkedList.append(20)
LinkedList.append(30)

for val in LinkedList.each() do
    print(val)  // 10, 20, 30
end
```

---

### Simple State Machine

A traffic light that cycles through states. Uses `@call` to advance the state when the instance is invoked like a function.

```vix
class TrafficLight(var state: str = "red")
    @call
    public func next(self)
        if self.state == "red" then
            self.state = "green"
        elif self.state == "green" then
            self.state = "yellow"
        else
            self.state = "red"
        end
    end

    public func current(self): str
        return self.state
    end
end

TrafficLight()
print(TrafficLight.current())  // "red"
TrafficLight()                 // advance via @call
print(TrafficLight.current())  // "green"
TrafficLight()
print(TrafficLight.current())  // "yellow"
TrafficLight()
print(TrafficLight.current())  // "red"
```

---

## Error Handling

Errors caused by class function usage range from E90 to E100. Every error shows the exact location in the code, the class function that caused the error, and a suggestion for how to fix it.

| Error Code | Error | Description |
|------------|-------|-------------|
| E90 | Wrong return type | Returning a value with a type that does not match the declared return type |
| E91 | Calling a function with wrong param type | Passing a value of the wrong type to a function parameter |
| E92 | Wrong parameter count | Calling a function with a different number of parameters than it declares |
| E93 | Using an unimplemented function | Calling a function that has no implementation |
| E94 | Missing parameter | Calling a function while omitting a required parameter |
| E95 | Extra parameter | Calling a function with more arguments than it accepts |
| E96 | Calling a class function without `self` | Calling a function inside a class that uses fields or other functions without declaring `self` in its parameter list |
| E97 | Disallowed function usage in class | Using a function that is not permitted inside a class context |
| E98 | Calling a private function | Calling a private function from outside the class it is defined in |
| E99 | Infinite loop in function | A function that calls itself with no base case, causing infinite recursion |
| E100 | Using a local function outside its library | Calling a `local` function from outside the library it belongs to |

---

## Notes

A class function's return type must match what is actually returned inside the function body.

A class with no parameters can still declare functions and be used as a namespace for related operations.

Fields are immutable by default. Always use `var` when a field needs to change after instantiation.

Parameters with default values must come after parameters without defaults.

`Self` as a return type requires the function to construct a value using `Self { field = value, ... }`.

Unsafe functions force memory safety rules off for their scope and must only be called from other unsafe contexts.

Generic type parameters are visible to all functions inside the class and do not need to be redeclared per function.

When implementing a trait with `for`, the compiler verifies that every required function is present with a matching signature. Missing functions are a hard error.

A class can only have one `@call` function. A `@drop` function cannot be called manually  use `@free` for explicit early release. Each operator requires its own `@operation` decorated function.