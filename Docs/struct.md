# What is struct?
A struct is a custom data type that groups together variables of different types under a single name. It is similar to a class, but it does not support inheritance or methods. Structs are declared using the `struct` keyword followed by the struct name and a list of fields. Each field is separated by a comma. The body is closed with the `end` keyword.

A struct's Field is defined using the `field_name: field_type` syntax. The field type can be any valid type in **vix**. Feilds are immutuable by default and private by default. To make a field mutable you need to use the `var` keyword before the field name. Example:

```vix
struct Point
    x: int // immutable field
    var y: int // mutable field
end
```

# Fields
## Fields Declaring
### Fields Usage
Fields in the structers are declared automaticlly as **immutable** and **private** form. To declare a field you need to add the visiblity type e.g `mutable( var )` `immutable ( nothing )` then select the field name and shouldn't there is another field with the same name otherwise that will be declared as an error. And Finally select the Type via `: Type` Final form: `visiblity field_name: Type`. Example blueprint:

```vix
// Declaring a structer
struct Example
    a: int
// |- |---
// |  |> Type
// |> Field name
    var b: int
//  |--
//  |> Visiblity
end
```

> Fields usage after declaring can be done via multi ways. Here all different ways to use a field:
- `variables` — fields can be declared via a variable. And modifable only if the field is mutable or the variable is mutable.
- `classes` — fields can be declared via `self` in Classes. And modifable only if the field is mutable
- `Struct literal` — fields can be defaulted directly via `My_Struct { field_1 = Value }`

#### Declaring via Variables
Fields can be declared directly via a variable `let field_example = p.field` and if the variable/field is mutable that allow you to change it. To use the field you need to be declared the structer in another variable already `let p = Point`. You cannot use it directly otherwise that will be declared as a error. Example BluePrint usage:

```vix
sturct Point
    y: int
    x: int
end

let p = Point
let x_field = p.x // now you have the x_field. But it declared in immutabble variable, means it cannot be change it
var y_field = p.x // now you have the y_field. It declared in mutable variable, means it can be change it

print(x_field) // No problms will print 0
x_field = 3 // Error: Immutable field/variable
y_field = 5 // no problem

print(y_field) // will print 5
```

#### Declaring via classes
Fields can be declared via classes `self.field`. Only when the structer is connected to the class it self. Fields are declared are not modifiable only if the field is already mutable. Followed by this blue print:

```vix
struct Point
    var y: int
    x: int
end

class Point()
//----------- Declaring the class and connecting it to the struct by using the same name
    func new(): Self
        return Self {
            y = 0,
            x = 0
        }
    end

    func example(self)
        print(self.y) // will print 0
        print(self.x) // will print 0
    end

    func modify(var self)
        self.y = 1 // will modify y with no problems
        self.x = 4 // error x is immutable
    end
end
```

#### Declaring via Struct literal 
Fields can be declared and being in default state `Struct { field = 0 }`. This make the field defaulted and modifiable only when it's mutable. Cannot default the fields more then once. Example blue print
```vix
struct Point
    x: int
    y: int
end

func example()
    let p = Point

    p { // Struct literal 
        x = 0,
        y = 0
    }

    // Directly Struct literaling when it's declared
    let p = Point {
        x = 0,
        y = 0
    }
```

> **Note**: Defaulting can be done once and bypass the visiblity bc always will change the value matter what the visiblity is of the field
### Nested Fields
#### Declaring Nested Fields
Structers can be declared inside another structers fields `field = AnotherStructer` and this called **Nested Fields**. This allow you to declare any structer inside another structers fields and it's allow you to use all fields in the other structers from the field it self `field.other_structer_field`. This can be stacked to infinity amout of structers e.g you can do `field.another_structer.another_structer` etc... infinity.

```vix
struct Example
    p: Point // Another structer.
struct Point
    y: int
    x: int
    i: Add // Point to another structer this can be done infinitly
end

struct Add
    a: int
    b: int
end

struct Math
    add: Add // Declare a structer inside a filed
    p: Point
    example: Example
end
```

Visiblity Applies here. For private fields you cannot use them as `Nested Fields`. Like declaring a structer inside a field and trying to use that struct's fields but it's private. This will declare as an error beacuse it's private `field.another_structer.private_field`. Followed by this blueprint example:

```vix
// Main.vix
public struct Math // public structer
    public a: int // Public field
    b: int // private field
end

public struct Point // public structer
    y: int // private field y
    public x: Math // public field x pointing to Math
end

// Example.vix
import Point

let p = Point {
    y = 0 // Error filed is private
    x.a // no problems. Field is already public
    x.b // Error. Field is private in math
}
```

> **Note**: If you declared structer and used inside the fields another structers but private. This operation is allowed and u can still use the private structer. Example

```vix
// Main.vix

struct Math // Private structer
    // fields
end

struct Point
    m = Math // This allowed
end

// Example.vix
import Point

let p = Point {
    m.any_field // no problems
}
```

#### Usage Of Nested Fields
Nasted Fields usage is by getting the structer fields throught the structer field it self `field.another_structer.field`. And this can be done throught **Struct literal** By using the field needed and after adding `=` with the structer Name then opening `{}`, `.field = StructerName {}`.  Here blue print of the usage

```vix
struct Math
    a: int
    b: int
end

struct Point
    y: Math // Declared another structer via the field
    x: int
end

// Usage via Struct literal:
Point {
    y = Math {
        a = 0,
        b = 0
    }
}

// Usage via the field
let p = Point {
    y.a = 0, // Directly using the math
    y.b = 0,
}
```

> **Warning** This only can be done with public fields in the included structer in the used field `field.another_structer.only_public_field` Otherwise, this will be delcared as an error.

### Default field value
Structer fields can have the type but default value in the structer it self too `field: Type = Value`. This will make the value is always defaulted and can be changed only if the field is `mutable` Example followed by this blue print:

```vix
struct Point
    y: int = 3
    var x: int = 5
end

print(Point.y) // will print 3
print(Point.x) // will print 5

Point {
    y = 5, // Error field is private
    x = 3 // Change the value to 3
}
```

## Delcaring Structer
> Structers can be declared by multi ways and usage is the same from example variables/directly/classes/import and much more. Here different between all the types:

- `variable` — Struct can be declared via a variable like `let my_structer = MyStructer`. And structer is modifable to the variable. Only if the field is mutable
- `classes` — Struct can be used in vix [`classes`](https://vixlanguage.github.io/docs/class) and use the fields via `self.field` keyword
- `importing` — Struct can be declared/included via [`import`](https://vixlanguage.github.io/docs/import#structers) only if structer is public. And use all public fields.

#### Declaring Via variable
Structers can be declared via a `vaiarable` and the structer will be used the same and all changes will be declared only inside the variable it self. Example changing the value is only effect the variable that use the structer. Structers can be declared as a variable Type `let ex: Example_Sturct = Example_Struct {}` or via the value directly `let ex = Example_Struct`. Followed by this example:

```vix
struct Example
    a: int
    b: int
end

// declaring a structer via the type and value
let ex: Example_Struct = Example_Struct {
    a = 0,
    b = 0
}

// declaring a struct via the type
let ex: Example_Struct = {
    a = 0,
    b = 0
}

// declaring a struct via the value
let ex = Example_Struct {
    a = 0,
    b = 0
}
```

Variables can declare a Structer inside their `Type`/`Value` and be used later `let my_structer = My_Struct`. And u can default the value too directly via `{}` or accesing the field using `my_structer_variable.field`. Followed by this example

```vix
struct Point
    var a: int
    var b: int
end

let p = Point // declare the struct via the variable. In the value
let p2: Point = {} // declare the struct via the variable. In the type

// Defaulting:
p {
    a = 0,
    b = 0
}

// Accesing a field
print(p2.a) // will print 0
print(p2.b) // will print 0

// Modifiying a field
p2.a = 3
p2.b = 4
```
> **Warning**: You cannot modify a structer field if it's not `mutable` otherwise that will be declared as a error. Example
```vix
struct Point
    var a: int
    b: int
end

let p = Point

p.a = 10 // No problem
p.b = 30 // Error: field is not mutable
```

#### Declaring Via Class
Structers can be used inside a class too. Vix [classes](https://vixlanguage.github.io/class) are allowed to be inculded with a structer `Class MyStructer()`. In the class all fields are accesable via `self.field` but modifiable only if the field is `mutable`. Followed by this Example:

```vix
struct Example // include the structer
    var a: int // mutable field
    b: int
end

Class Example() // Include the class with the structer
    func new(): Self // Defaulting
        return Self {
            a = 0,
            b = 0
        }

        // Info: Defaulting can be used using the struct name example:

        return My_Struct {
            a = 0,
            b = 0
        }
    end

    func test(self)
        // Print
        print(self.a)
        print(self.b)
    end
    
    func modifying(var self)
        self.a = 30 // Modifying
        self.b = 4 // Modifying
    end
end
```

#### Declaring Via import
Structers can be imported only if they **public**. By importing a structer, you have full accese to the public fields only. Class can be used with public structers if they not connected to another one already and use all fields even the onces are not public. `import MyStruct`. After importing you can use the struct normally like included in the file the structer included from. And you can import a struct via libraries too. Followed by this example:

```vix
// >> Main.vix
public struct Example
--------------------- Import a public structer
    public a: int // Public field 
    b: int // Private filed
end

Class Example()
    func new(): Example {
        return Example {
            a = 50,
            b = 30
        }
    }

    func test(self)
        print(self.a) // will print 30
        print(self.b) // will print 50
    end
end

let ex = Example

print(ex.a) // will print 0
print(ex.b) // will print 0

// >> Example.vix
import Example

class Example()
    func new(): Example {
        return Example {
            a = 50,
            b = 30
        }
    }

    func test(self)
        print(self.a) // will print 30
        print(self.b) // will print 50
    end
end

print(ex.a) // will print 0. no problems 
print(ex.b) // Error. Field are private
// 
```

### Structer Visibility

> Structers can be visible in multi ways like `public/local/private` like functions/enums. Here different between all types:
- `public` — The struct is visible outside the file it is defined in and can be imported using `import StructName from file`.
- `private` — The struct is only visible inside the file it is defined in and cannot be imported. This is the default.
- `local` — The struct is only visible inside the library it is defined in. It is public within the library but not accessible from outside it.
- `static` — Structer will be globle and accesable from all other scopes.

#### Public Visibility
Structers are privated by default means the structer cannot be imported from another file and visible for only current file. To make a struct public you need to use the `public` keyword before the struct definition. And by default all fields are private and cannot be defined even if the structer it self is public. To make a field public you need to use the `public` keyword before the field name. Example:

```vix
public struct Point
------------------- // public struct
    public x: int // public field
    y: int // private field
end

print(Point.x) // OK: x is in same file
print(Point.y) // OK: y is in same file
```

- You cannot accese private struct/fields from another file even if the struct is public. Followed by this example:

```vix
import Point // will be important beacuse 'Point' is public

print(Point.y) // OK: y is public
print(Point.x) // Error: x is private
```

#### Private Visiblity
Structers by default are always consern as **private** visiblity. This means the struct cannot be accesed outside the current file. Fields in the struct are too private by default and cannot be used even if the struct is already defined as **public**. To define a private struct you need to use the keyword `struct` only no keyword before it. However structers are allowed to be used with "class" even if they are private and accese the fields using `self` keyword and outside the struct you can use the field. Example usage:

```vix
struct Point 
------------ // Private struct
    x: int // private field
    y: int
end

class Point() // vailed
    func new(): Point
        retrun Point {
            x = 0,
            y = 0
        }
    end

    func example(self)
        return self.x + self.y
    end
end

let p = Point

print(Point.x)
print(Point.y)
```
> **Note**: Private structers make the class automaticlly private. Cannot define a private struct with public function example:

```vix
struct Point
    x: int
    y: int
end

class Point() // defined the class but cannot use "public"
// ---------- Allowed no problems
public class Point()
// ----------------- Errors. Cannot make public class on a private structer
```

Fields are **private** by default. Means they cannot be accesed even if the struct is not **private too**. However, private fields are accesable only when using the struct in same file. Otherwise that not allowed operation and they are allowed to be use inside the class using **self** keyword.

```
struct Point
    x: int // private field
    y: int
end

class Point()
    func example(self)
        return self.y + self.x
    end
end

let p = Point
print(p.y)
print(p.x)

// In other file ---------------------->
import Point // Error point is private

print(Point.y) // Error struct is private
```
#### Globle Visiblity
Structers are allowed to be globle too. It's not same way as including a variable or a function using `local` keyword. Structers are doing that with different way. By implementing the structer with any visiblity can be private/public and connecting it to the globle variable example:

```vix
struct Point
    x: int
    y: int
end

local p = Point

func example()
    print(p.x)
    print(p.y)
end

example()
```

> **Warning**: Fields are public can be accesed if the globle variable is public too example:
```vix
// Main.vix:
struct Example
    a: int
    b: str
end

public local ex = Example
// Example.vix:

func example()
    print(ex.a) // will print 0
    print(ex.b) // will print " " no problems
end

example()
```