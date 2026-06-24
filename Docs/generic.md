# What is Generic
Generics allow you to write reusable code that works across multiple types without sacrificing type safety. Instead of duplicating logic for each type, you define a single function, enum, or class with a type parameter (e.g. [T]) that gets resolved at compile time. Followed by this example:
```vix
func example[T](a: T, b: T)
    print(a)
    print(b)
end

func main()
    example[int, int](1, 4)
            --------- will convert T to int
end
```

> **note**: you can declare generic in any name or define multi generics. Example
```vix
func example[A, B, C, D](a: A, b: B, c: C, d: D)
```

## Function Generic

Vix supports generic functions using type parameters declared with `[T]` after the function name. A generic type can then be used as a parameter type or return type. The typechecker will automatically infer the generic type when possible, or you can declare it explicitly.
```vix
func example[T](a: T): T
    return a
end

let a = assert(example[int32](10))    // returns 10
let b = assert(example[str]("Hello")) // returns "Hello"
let c = assert(example[int]("Hello")) // ERROR: "Hello" is not int

print(a) // 10
print(b) // "Hello"
print(c) // false, error
```

You can also use multiple generic types in a single function:
```vix
func example[A, B](a: A, b: B): A
    return a + b
end

let a = assert(example[int32, int32](10, 20))      // returns 30
let b = assert(example[int32, str](10, "Hello"))   // ERROR: return type is int32
let c = assert(example[int32, int32](10, "Hello")) // ERROR: "Hello" is not int32

print(a) // 30
print(b) // false, error
print(c) // false, error
```

When the typechecker can determine the generic types automatically from the arguments passed, you do not need to declare them explicitly:
```vix
func example[A, B](a: A, b: B): A
    return a + b
end

print(example(10, 50)) // 60, typechecker infers A and B as int automatically
```

Case calling a function with same name but different parameters. A function with normal types and another with genertic. Automatcilly compiler check if there is any type of generic type implemetion like `calling_function[int32, int32]()`, If yes will call generic function if no example `calling_function(10, 50)` will not call generic function. example: 
```vix
func add(a: int32, b: int32): int32
    return a + b
end

func add[T](a: T, b: T): T
    return a + b
end

add(1, 2) // will call add(a: int32, b: int32)
```
---


#### Enum Generic
Enums can be generic by using the `[T]` syntax after the enum name, e.g. `enum Example[T]`. Multiple generic types are supported too, like `enum Example[T, E]`, and used like `Example[int32]` or `Example[int32, str]` and so on. This can be done by directly assigning the enum to a variable `let s = EnumName[int32, int32]`, or in a function return type `func example(): EnumName[int32, int32]`, and so on. Variants can also be called directly as `Ok(10)` or `Err("error")` as an optional approach. Example usage:

```vix
enum Result[T, E]
    Ok(T),
    Err(E)
end

func example(): Result[int32, str]
    return Ok(10)
end

print(example()) // will print Ok(10)
```

> **Note**: Generic enums can be constrained using Philosophy with the syntax `enum Example[T: SomePhilosophy]`. This ensures the generic type `T` must implement `SomePhilosophy`. Example:
> ```vix
> enum Wrapper[T: Printable]
>     Some(T),
>     None
> end
> ```

---


## Generic Classes

Vix supports generic classes  classes that can work with different types by declaring type parameters. Generic type parameters are placed in square brackets after the class name, following this syntax: `class Example[T](a: T, b: T)`. The type parameter `T` can then be used anywhere inside the class for fields, function parameters, and return types.

```vix
class Example[T](a: T, b: T)
    public func get(self): T
        return self.a
    end
end

Example[int](10, 20)
Example.get() // 10
```

Generic type parameters are public to all functions inside the class and can be used freely in any function signature or body without redeclaring them.

---
