# Overview

## Features

Gix is a typed, compiled programming language built on [**LLVM**](https://llvm.org), designed for simplicity and control from high-level work like compilers and applications, to low-level work like operating systems, kernels, and firmware with performance comparable to C/C++.

Gix gives you full control with no runtime costs, and optionally supports automatic memory management via ARC. Unlike C++, Gix lets choose between manual memory management or optional ARC at runtime. You have complete control over your code, inline assembly, compiler output, and program behavior making it ideal for systems programming, especially [**OS development**](https://en.wikipedia.org/wiki/Operating_system), as well as high-level application development with optional ARC.

```gix
func main()
    print("Hello, world!")
end
```

---

## Installation & LPS

Gix is easy to install. Head to the installer page and follow the steps via the installation guide.

The following tools are required and will be installed automatically if not already present:
- LLVM version 21.x
- Development tools required by VS
- At least 5 GB of free storage

To install the Gix LPS (Language Programming Server), visit the LPS installer page, or search for `gix programming language` in your IDE's extension marketplace.

---

## C Library Compatibility

Gix has full support for C libraries, giving you access to the entire C ecosystem with no extra setup. Example:

```gix
import c_lib

c_lib.some_function()
```

Installing a C library is straightforward using the Gix CLI:

```powershell
gix install c_lib_name -c
```

---

## Supported Platforms

Gix supports all major operating systems:
- [Linux](https://kernel.org)
- [Windows](https://microsoft.com/windows)
- [macOS](https://apple.com/macos)

And the following architectures: x86, ARM, and RISC-V. Gix also supports custom operating systems if you compile the compiler from source.

---

## Language Features

### GTS - Gix Type System

Gix includes the **GTS** (Gix Type System), which automatically infers types at compile time in most cases, across your entire codebase. This works in both default mode and ARC mode.

```gix
func example(a, b) // a, b inferred as int, return type inferred as int
    return a + b
end

func main()
    let a = 3 // inferred as int
    let b = 5 // inferred as int

    print(example(a, b))
end
```

### Memory Safety

Gix provides memory safety features through the **core library** and **standard library**, including `Result`, `Option`, smart pointers, and more all with minimal runtime cost.

```gix
func example(a, b): Result[int, str]
    if a > b then
        return Ok(10)
    else
        return Err("Hello, world!")
    end
end

func main()
    var a: Option[int] = None // mutable
    let b = 3                 // immutable

    a += Some(14)

    match example(a, b)
        case Ok(a)
            print(a)  // prints 10
        case Err(b)
            print(b)  // prints "Hello, world!"
    end
end
```

In Gix, everything is immutable by default. Use `let` for immutable bindings and `var` for mutable ones:

```gix
let a = 30 // immutable
var b = 3  // mutable

a += 3 // Error! cannot modify an immutable binding
b += 6 // OK — b is now 9

print(a) // 30
print(b) // 9
```

---

-> [Memory Safety docs](https://vixlanguage.github.io/docs/safety)  
-> [VTS docs](https://vixlanguage.github.io/docs/vts)  
-> [ARC docs](https://vixlanguage.github.io/docs/arc)  
-> [Benchmarks](https://vixlanguage.github.io/benchmarks)