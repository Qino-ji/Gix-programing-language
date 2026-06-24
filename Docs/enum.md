# Enum Stmt

#### What are Enums?
Enums are custom data types that can have multiple variants. Each variant can have zero or more fields. Enums are declared using the `enum` keyword followed by the enum name and a list of variants. Each variant is separated by a comma. The body is closed with the `end` keyword.

---

#### Naming Conventions
By convention, enum names and variant names should use **PascalCase** (e.g., `MyEnum`, `SomeVariant`). This is not enforced by the compiler but is the recommended style throughout Vix.

---

#### Variant Forms
A variant supports multiple forms, including a default form, a form with fields, and a form with associated functions. Variants are separated by commas (`,`), and each form follows its own syntax and constraints. Here are the different forms of a variant:

- **Default form**: `variant_name`   A variant with no fields.
- **Form with fields**: `variant_name(field1: type1, field2: type2, ...)`   A variant with one or more fields.
- **Form with associated functions**: A variant with a `do...end` block containing functions.
- **Form with fields and associated functions**: A variant with both fields and a `do...end` block containing functions.

The syntax for calling an enum variant varies based on the variant type. Here are the calling conventions for every variant form:

- **Default form**: `EnumName.variant`   Calls the enum variant directly without passing any fields.
- **Form with fields**: `EnumName.variant(10, 50, 30)`   Calls the variant with positional field values, or with named fields like `(field1: 10, field3: 50, field2: 30)`.
- **Form with functions**: `EnumName.variant.function_name()`   Calls a function attached to the variant.
- **Form with fields and functions**: `EnumName.variant(10, 50, 30).function_name()`   Calls the variant with fields, then calls one of its functions.

---

#### Enum-Level Methods
You can attach methods to the enum itself (not to a specific variant) using a `do...end` block inside the enum body. These methods belong to the enum as a whole and can be called using `EnumName.function_name()`.

```vix
enum Direction
    North,
    South,
    East,
    West
end

    func opposite(self): Direction
        match self
            case North do return Direction.South
            case South do return Direction.North
            case East  do return Direction.West
            case West  do return Direction.East
        end
    end
end

let d = Direction.North
print(d.opposite()) // will print South
```

---

#### Generic Usage
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

#### Enum Visibility
Enums have 3 types of visibility: `public`, `private`, and `local`. Enums are `private` by default, meaning they cannot be used outside the file they are defined in. Types of visibility:

- `public`   The enum is visible outside the file it is defined in and can be imported using `import EnumName from file`.
- `private`   The enum is only visible inside the file it is defined in and cannot be imported. This is the default.
- `local`   The enum is only visible inside the library it is defined in. It is public within the library but not accessible from outside it.

**Public enum**   add the `public` keyword before `enum`:

```vix
// file.vix
public enum EnumName
    Variant
end
```

```vix
// main.vix
import EnumName from file

EnumName.Variant          // Using the enum variant
EnumName.Variant()        // Using the enum variant with fields
EnumName.Variant.field    // Using the enum variant field
EnumName.Variant.function() // Using the enum variant function
```

**Private enum**   no keyword required (default behavior):

```vix
// file.vix
enum EnumName
    Variant
end

EnumName.Variant // visible here inside the same file
```

```vix
// main.vix
EnumName.Variant // Error: EnumName is not visible here
```

**Local enum**   add the `local` keyword before `enum`:

```vix
// library/library.vix
local enum EnumName
    Variant
end

EnumName.Variant // visible here inside the same library
```

```vix
// library/test.vix
EnumName.Variant // visible here inside the same library
```

```vix
// root/main.vix
EnumName.Variant // Error: EnumName is not visible here
```

---

#### Enum as Function Parameter
Enums can be passed as parameters into functions just like any other type. The parameter type is the enum name.

```vix
enum Direction
    North,
    South,
    East,
    West
end

func describe(dir: Direction): str
    match dir
        case North do return "Going north"
        case South do return "Going south"
        case East  do return "Going east"
        case West  do return "Going west"
    end
end

print(describe(Direction.North)) // will print Going north
```

Generic enum parameters work the same way:

```vix
enum Result[T, E]
    Ok(T),
    Err(E)
end

func handle(result: Result[int32, str])
    match result
        case Ok(value)  do print(value)
        case Err(error) do print(error)
    end
end

handle(Ok(42))       // will print 42
handle(Err("oops"))  // will print oops
```

---

#### Nested Enums
A variant's field can hold another enum type. This is fully supported and works the same as any other field type.

```vix
enum Color
    Red,
    Green,
    Blue
end

enum Shape
    Circle(radius: int32, color: Color),
    Rectangle(width: int32, height: int32, color: Color)
end

let s = Shape.Circle(10, Color.Red)
print(s) // will print Circle(10, Red)
```

Nested enums can also be matched against in `match` or `if` statements:

```vix
match s
    case Circle(radius, color) do
        print(radius)
        print(color)
    end
    case Rectangle(width, height, color) do
        print(width)
    end
end
```

---

#### Recursive Enums
Enums can reference themselves in their variant fields. No special keyword or pointer type is needed   recursive enums work directly.

```vix
enum List
    Node(value: int32, next: List),
    Empty
end

let list = List.Node(1, List.Node(2, List.Node(3, List.Empty)))
```

You can traverse a recursive enum using `match`:

```vix
func sum(list: List): int32
    match list
        case Node(value, next) do return value + sum(next)
        case Empty             do return 0
    end
end

print(sum(list)) // will print 6
```

---

#### Class Usage
Classes can use enums and access their variants using `ClassName.EnumName.variant`, or inside the class using `self`. Example usage:

```vix
enum Example
    Yes(int32),
    No(int32)
end

class Example()
    func example(self): Self
        return Example.Yes(10)
    end
end

print(Example().example()) // will print Yes(10)
```

> **Note**: Classes can use generic enums too, using the same syntax: `EnumName[int32].variant`. Example:

```vix
enum Result[T, E]
    Ok(T),
    Err(E)
end

class Example()
    func example(self): Self
        return Result[int32, str].Ok(10)
    end
end

print(Example().example()) // will print Ok(10)
```

---

#### Declaring an Enum
Enums can be declared through a variable `let a = SomeEnum` or by assigning a specific variant to a variable `let a = SomeEnum.Variant`. This allows you to use the enum or its fields like `a.Variant` or just `Variant`. Example syntax:

```vix
enum EnumName
    Variant1,
    Variant2,
    Variant3
end

let a = EnumName.Variant1
let b = EnumName.Variant2
let c = EnumName.Variant3

print(a) // will print Variant1
print(b) // will print Variant2
print(c) // will print Variant3
```

Enums also allow you to change the declared variant of a mutable variable. Using `var` makes the variable mutable, so the variant can be reassigned. Using `let` makes it immutable.

```vix
enum EnumName
    Variant1,
    Variant2,
    Variant3
end

var a = EnumName.Variant1
a = EnumName.Variant2 // allowed   a is mutable
a = EnumName.Variant3 // allowed too

let b = EnumName.Variant1
b = EnumName.Variant2 // Error: cannot change immutable enum variant
b = EnumName.Variant3 // Error too
```

> **Note**: Enum variants are immutable by default. To make a variant mutable, use the `var` keyword: `var s = SomeEnum.Variant`.

Enums work as return types in classes or functions by placing the enum name after the `:` in the return type position:

```vix
enum EnumName
    Variant1,
    Variant2,
    Variant3
end

func example(): EnumName
    return EnumName.Variant1
end

print(example()) // will print Variant1
```

This also works with `Self` inside a class:

```vix
class Example()
    func example(self): Self
        return EnumName.Variant1
    end
end

print(Example().example()) // will print Variant1
```

---

#### Using Enums in Match and If Statements
Enums can be used in `match` statements with `case Variant(value) do`, and in `if` statements with `if Variant(value) = example() then`. Values can be ignored using `_`.

> **Note**: The compiler does **not** return an error or warning when a `match` statement does not cover all variants. It is your responsibility to handle all cases you need.

```vix
enum Example
    Yes(int32),
    No(int32)
end

func example(): Example
    return Yes(10)
end

// Using match
match example()
    case Yes(value) do print(value)
    case No(value)  do print(value)
end

// Using if statement   triggers if the variant is Yes and assigns value
if Yes(value) = example() then
    print(value)
end

if No(_) = example() then // ignore the value and just check the variant
    print("No")
end
```

> **Note**: Declaring only one case in a `match` statement does not cause an error. However, declaring more than one case with the same variant will cause an error.

---

#### Using Enums in For and While Statements
Enums can be used with `for` and `while` statements.

```vix
enum Example
    Yes(int32),
    No(int32)
end

func example(): Example
    return Yes(10)
end

// Using for statement
for i in example() do
    print(i)
end

// Using while statement
while example() = Yes(_) do
    print("Yes")
end
```

---

#### Enum Operations
Enums can be used with operators like `==`, `!=`, `!`, `&&`, `||`, and more. Comparisons can also check field values.

```vix
enum Example
    Yes(int32),
    No(int32)
end

let example = Example.Yes(10)

if example == Example.Yes(10) then
    print("Yes")
end

if example != Example.Yes(10) then
    print("No")
end
```

Equality checks compare both the variant and its field values, so `Example.Yes(10) == Example.Yes(20)` is `false`.

---

#### Sum Types (Algebraic Data Types)
Enums in Vix can be used as Sum Types (Algebraic Data Types). Here is a summary of all the declaring and calling forms:

**Default variant (no fields):**
```vix
enum EnumName
    Variant1,
    Variant2,
    Variant3
end

EnumName.Variant1 // Calling the variant
```

**Variant with fields:**
```vix
enum EnumName
    Variant1(field1: type1, field2: type2, ...),
    Variant2
end

EnumName.Variant1(10, 50, 30) // Calling the variant with fields
EnumName.Variant2              // Calling the variant without fields
```

**Variant with associated functions:**
```vix
enum EnumName
    Variant1 do
        func function_name(self)
        func function_name2(self)
    end,
    Variant2
end

EnumName.Variant1.function_name() // Calling the variant function
```

**Variant with fields and associated functions:**
```vix
enum EnumName
    Variant1(field1: type1, field2: type2, ...) do
        func function_name(self)
        func function_name2(self)
    end,
    Variant2
end

EnumName.Variant1(10, 50, 30).function_name() // Calling the variant with fields and functions
```

**Tuple variants with direct shorthand calling:**
```vix
enum Example
    Yes(int32),
    No(int32)
end

func example(): Example
    return Yes(10) // shorthand   no need for Example.Yes(10)
end

print(example()) // will print Yes(10)
```

---

#### Enums with Arrays and Collections
Enums can be stored in arrays and collections like any other type. A fixed array of an enum uses the `EnumName[]` syntax, and a generic collection type uses `Something[EnumName]`.

**Fixed array:**
```vix
enum Status
    Active,
    Inactive,
    Pending
end

let items: Status[] = [Status.Active, Status.Inactive, Status.Pending]

for item in items do
    print(item)
end
// will print Active, Inactive, Pending
```

**With fields:**
```vix
enum Example
    Yes(int32),
    No(int32)
end

let items: Example[] = [Example.Yes(1), Example.No(2), Example.Yes(3)]

for item in items do
    match item
        case Yes(value) do print(value)
        case No(value)  do print(value)
    end
end
```

**Generic collection type**   using `Something[EnumName]` for types that wrap or hold enums:
```vix
enum Example
    Yes(int32),
    No(int32)
end

let items: List[Example] = List[Example].new()
```

---

#### Shadowing Imported Enums
Declaring a local enum or variant with the same name as an imported one is a **compiler error**. There is no shadowing   both names must be unique within the file.

```vix
import Result from result

enum Result   // Error: enum Result already declared from import "Result"
    Ok(int32),
    Err(str)
end
```

This also applies if the imported enum has variants whose names clash with a local enum name:

```vix
import Result from result // Result has Ok and Err variants

enum Ok      // Error: Ok already declared from import "Result"
    Value(int32)
end
```

To avoid conflicts, either rename your local enum or use a different import alias if the language supports it.

---

#### Empty Enums
Declaring an enum with no variants is allowed but will produce a **compiler warning**, similar to declaring an unused variable or enum. An empty enum cannot be instantiated since there are no variants to use.

```vix
enum Empty   // Warning: enum Empty has no variants
end

let e = Empty.SomeVariant // Error: enum variant SomeVariant not declared
```

Empty enums are valid as placeholder types during development, but should be given variants before use to avoid warnings.

---

#### Type Safety Rules
Enums have strict type safety rules that must be followed:

**Cannot redeclare an enum that already exists or was imported:**
```vix
import AnotherExample

enum Example
    Yes(int32),
    No(int32)
end

enum Example      // Error: enum already declared
    Yes(int32),
    No(int32)
end

enum AnotherExample // Error: enum already declared from import "AnotherExample"
end

Example = 10 // Error: cannot assign over enum
```

**Cannot change an immutable enum variant:**
```vix
enum Example
    Yes(int32),
    No(int32)
end

let example = Example.Yes(10)
example = Example.No(10) // Error: cannot change immutable enum variant
```

**Cannot use a variant that has not been declared:**
```vix
enum Example
    Yes(int32),
    No(int32)
end

let example2 = Example.Maybe(10) // Error: enum variant Maybe not declared
```

**Cannot declare more than one variant with the same name in the same enum:**
```vix
enum Example
    Yes(int32),
    Yes(int32) // Error: enum variant Yes already declared
end
```

**Cannot declare more than one field with the same name in the same variant:**
```vix
enum Example
    Yes(a: int32, a: int32) // Error: field a already declared
end
```