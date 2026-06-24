# Overview

## Features

Vix is a typed, compiled programming language built on [**LLVM**](https://llvm.org), designed for simplicity and control from high-level work like compilers and applications, to low-level work like operating systems, kernels, and firmware with performance comparable to C/C++.

Vix gives you full control with no runtime costs, and optionally supports automatic memory management via [ARC](https://vixlanguage.github.io/docs/arc). Unlike C++, Vix lets choose between manual memory management or optional ARC at runtime. You have complete control over your code, inline assembly, compiler output, and program behavior making it ideal for systems programming, especially [**OS development**](https://en.wikipedia.org/wiki/Operating_system), as well as high-level application development with optional ARC.

```vix
func main()
    print("Hello, world!")
end
```

---

## Installation & LPS

Vix is easy to install. Head to the [installer page](https://vixlanguage.github.io/install)
and follow the steps via the [installation guide](https://vixlanguage.github.io/install/help).

The following tools are required and will be installed automatically if not already present:
- LLVM version 21.x
- Development tools required by VS
- At least 5 GB of free storage

To install the Vix LPS (Language Programming Server), visit the [LPS installer page](https://vixlanguage.github.io/install/lps), or search for `vix programming language` in your IDE's extension marketplace. A [VS Code extension](https://marketplace.visualstudio.com/VSCode) is also officially available.

-> Installer source code: [GitHub](https://github.com/vix-programing-language/installer)  
-> LPS source code: [GitHub](https://github.com/vix-programing-language/lps)

---

## C Library Compatibility

Vix has full support for [C libraries](https://vixlanguage.github.io/docs/library/c),
giving you access to the entire C ecosystem with no extra setup. Example:

```vix
import c_lib

c_lib.some_function()
```

Installing a C library is straightforward using the Vix CLI:

```powershell
vix install c_lib_name -c
```

---

## Supported Platforms

Vix supports all major operating systems:
- [Linux](https://kernel.org)
- [Windows](https://microsoft.com/windows)
- [macOS](https://apple.com/macos)

And the following architectures: x86, ARM, and RISC-V. Vix also supports custom operating
systems if you compile the compiler from source:
[vix compiler on GitHub](https://github.com/vix-programing-language/vix-programing-language).

-> Full platform list and installation: [install](https://vixlanguage.github.io/install)

---

## Language Features

### VTS - Vix Type System

Vix includes the [**VTS**](https://vixlanguage.github.io/docs/typesystem) (Vix Type System),
which automatically infers types at compile time in most cases, across your entire codebase. This works in both default mode and ARC mode.

```vix
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

Vix provides memory safety features through the
[**core library**](https://vixlanguage.github.io/docs/core) and
[**standard library**](https://vixlanguage.github.io/docs/std), including `Result`,
`Option`, smart pointers, and more all with minimal runtime cost.

```vix
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

In Vix, everything is immutable by default. Use `let` for immutable bindings and `var`
for mutable ones:

```vix
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