# Vix Programming Language
Including a simple and fast prograaming language. With no run time cost and fast compiling time. With simple to use and simple to setup and use in the future. No worries about diffecutly of memory safety. With no GC! this all will be done with simple and readable syntax! and no worrying about slow compiling time can cause with too!

Warning: This language is still on Alpha version. Full pre beta on 6/1/26. Wait for the release same time you can explore our compiler source code or if you have found an issues. Feel free to report it to us for fixing!

For more informations. 
You can ofcource join our discrod community: https://discord.gg/CAemjRc4ya
Check out our website too: https://vixlanguage.github.io

## > Why Vix?

Vix is fully memory safe with predition if all system programming features under no cost of run time performance. And this all does not effect compiling times too. Vix has so fast and smart compiler that predice programs in fastest as possible. This all under full easy and readable syntax made for anyone to can read or understand. Here an example of syntaxs you can really use in vix:

```vix
func main()
  let name = "Machile"
  var age = 3

  age += 10

  print("Hello, i'm", name, "and i'm", age, "old!")
```

It preide you with so simple and easy to follow error messages when you really hit an error. It will explain to you the problem, fixes and notes about it including the file and line the error happened and caused on. Here an example:
```json
## Example helpful message:

[Warning]: Warning, Unexpected type:
| main.vix:10
|
| create input = input("ask me something")
| ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
| > Unkowen type cannot be knowen in compilion time
|--------------------------------------------------
|
|-> help:
| create input: Type = input("ask me something")
                ---- Add type here example: "int/str/and so on"
|-> note:
  You cannot define a variable with no way for the compile to define the type.
```
## Commands
Vix have ton of commands you can use and explore. Commands you can start with and use:
```powershell
vix run my_file.vix # You can compile any file with JIT compiler
```
```powershell
vix run --release my_file.vix # You can compile any file with AOT and full optimization
```
```powershell
vix install library_name # Install any library you want or framework or even custom compilers
```

##  Features

- **Fast**: Performance comparable to C/C++ using LLVM ( Clang++ Compiler )
- **Memory Safe**: Built in safety without garbage collection overhead and using borrow checker and ownership system
- **Simple Syntax**: Easy to read and write, quick to learn and develop with. No using of "{}" only "end"
- **Flexible**: Write low level system code or high level applications fast and easily
- **Error handling & Help**: Easy to read & know the issues from the error msg

>> **Built for developers who want speed and safety without complexity.**
