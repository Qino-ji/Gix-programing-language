
### Memory safety
**Vix** is a memory safe language. This mean variables need to follow some rules to make sure the memory is safe and not corrupted. And also make sure no UB/Memory leak or anything can couse problems in the program. This done by multi ways in **vix** outside of GC. **Vix** is fully memory safe no GC and prevind errors/problems in compiling time not in runtime this give pure performance and safety.

#### Ownership System
Ownership system is one of the most important parts in **vix memory safety system**. This system make sure that every variable is owned by another variable. This mean that every variable is responsible for it's own memory. And when the owner is out of scope the owned variable will be freed automaticlly. This process is done by the compiler. Variables cannot be owned by more then one owner at a time. Every time a variable get owned it's being moved and cannot be used again. Example:

```vix
var a = 10 // declaring a variable
let b = a // b owned a

print(b) // will print 10
print(a) // will give an error beacuse variable is owned ( moved )
```

Variables can be owned by more then one owner at a time. This can be done with multi ways like `std.RC` [reference counting](https://vixlanguage.github.io/docs/std/rc) But this ways is not recommanded and can lead to memory leaks or other problems. You can make variable value used by multi other variables without owning it using `ref/&` keyword. This will make the variable value not moved and can be used by multi variables or modfied it if the reference/borrow is mutable aka `var ref`/`&var` Example:

```vix
var a = 10
let b = &a // b is referencing a
let c = &a // c is referencing a too

var d = &var a // d is referencing a and can be used to modify a

print(b) // will print 10
print(c) // will print 10
print(a) // will print 10

d += 10
print(a) // will print 20
print(d) // will print 20
```

Variables can be partially owned referenced/borrowed by another variable. But Cannot modfied using immutable borrows. Only mutable borrows can be used to modfy the variable value `&var`/`ref` keywords. This lead to the variable that immutabliy referenced/borrowed value change for all other variables the reference/owned the variable. Example:

```vix
var a = 10
let b = &a // b is referencing a
let c = &a // c is referencing a too
let d = &var a // d is referencing a and can be used to modify a


d += 50
b += 10 // Error! b is immutable and cannot be used to modify a

print(a) // will print 60
print(b) // will print 60
print(c) // will print 60
print(d) // will print 60
``` 

Variables can be partially referenced/borrowed by another variable. But if got modified by one of the variables that referenced/borrowed it the value change for all other variables that reference/borrowed it. This is beacuse the variable is not owned by the other variable and is only referenced/borrowed. This can be fixed using `copy/clone` to make a full copy/clone of the variable value. Tho this is not recommanded and can lead to bad performance if the variable is holding a large chunck of data.

```vix
var a = 10
let b = &var a
let c = a.copy() // or a.clone() if it's a big chunck of data

b += 10
print(b) // will print 20
print(a) // will print 20
print(c) // will print 10
```

> **Warning**: Cloning/Copying a variable can be fast if the variable is small but will be so slow if the variable is holding a large chunck of data. And also will take more memory. So use it only if needed.

---
By default calling a function with a variable going to take the whole ownership of the variable and cannot be used after the function call. Only if the function is returning the orginal variable back. Example:

```vix
func example(a)
    // do something
end

var a = 10
example(a)
print(a) // Error! a is not defined
```

This can be fixed by making a borrow/reference to it. This is done by function param and value can be borrowed/refenced or the variable it self, if the `&var` is before the variable name in the param example `&var a` means the variable is taking a mutable reference/borrow and if the `&var` before the type example `a: &var int32` means will take a borrow/reference to the value it self. If the param is taking mutable reference or borrow the variable that call the function will be automatcilly converted to mutable reference/borrow for the function and if it's modified inside the function, it's will be modified for orginal variable. Example:

```vix
func example(a: &var int32)
    a += 10
end

var a = 10
example(a) // a is 20 now
```

---
After a variable leave the scope it's automaticlly freed. But if the variable is owned by another variable and the owner get freed will make owned variable freed too. And if the variable is not owned it's being freed automaticlly when go out of the scope and no memory leaks will occur. And the returned variable is not going to be freed and will return the value back. Variables that go out of a function scope will be freed using `drop` but if it's out of a defined scope will be freed using `free`. Example:

```vix
func example()
    var a = 10
    var b = 50

    return a
end

func main()
    example()

    scope do
        var a = 10
        var b = 50
    end
end
```

- This will generate something like this in C IR code:
```c
void example() {
    int32_t a = 10;
    int32_t b = 50;

    drop(b); // will automaticlly drop b
    return a;
}

void main() {
    example();

    {
        int32_t a = 10;
        int32_t b = 50;

        free(b); // will free b
    }
}
```

As long not all variables being freed/dropped the same way. Some variables use a structers or classes or even be a library with allocation cannot be freed simply with `drop/free`. This is where `@drop` and `@free` comes in. They are used to define how to free the variable. They can be used only inside a class and should be on a top of the function will be used for freeing the variable. Example:

```vix
struct MyLibrary
    data: ptr[char]
    len: usize
    cap: usize
end

class MyLibrary():
    @drop
    public func drop(self)
        // free the variable
    end

    @free
    public func free(self)
        // free the variable
    end
end
```

- This will generate different C IR code:
```c
void main() {
    {
        MyLibrary a = MyLibrary_new();
        MyLibrary_drop(&a);
    }
}
```

In this example the variable `a` and `b` in `example()` function will be freed automaticlly when the function ends. But the variables `a` and `b` in `main()` function will not be freed because they are not owned by any other variable and will be freed when the scope ends.