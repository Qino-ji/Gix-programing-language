# Gix Programming Language
Including a simple and fast prograaming language. With simple to use and simple to setup and use in the future. No worries about slow compiling time, diffecult usage. With so simple usage and syntax you can type your code easily and fast, it will granty will be fully readable. Unlike C++ gix is fully memory manule and give you the full control exacutly like C with simple usage/syntax and so fast compilion time. Gix is perfect for low level and is good too for high level including our optional ARC. Not only that, gix has smart type system may can even figer out the type of your full codebase easily in compiling time. And you have modules you can use like `Result/Option` or smart pointers too! and mutiblity.

Warning: This language is still on Alpha version. Full pre beta on 6/1/26. Wait for the release same time you can explore our compiler source code or if you have found an issues. Feel free to report it to us for fixing!

For more informations. 
You can ofcource join our discrod community.
Check out our website too.

## > Why Gix?

Gix is fully unsafe language with full control and no worries. Unlike C gix is easier to setup/use and with simple syntax and type system you can easily write anything you want! Recommanded for real and low level projects or even high level with our ARC optional system. And you have modules you can use like `Result/Option` or smart pointers too! and mutiblity Explore our syntax:

```gix
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
| main.gix:10
|
| create input = input("ask me something")
| ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
| > Unknow type cannot be knowen in compilion time
|--------------------------------------------------
|
|-> help:
| create input: Type = input("ask me something")
                ---- Add type here example: "int/str/and so on"
|-> note:
  You cannot define a variable with no way for the compile to define the type.
```
## Commands
Gix have ton of commands you can use and explore. Commands you can start with and use:
```powershell
gix run my_file.gix # You can compile any file with JIT compiler
```
```powershell
gix run --release my_file.gix # You can compile any file with AOT and full optimization
```
```powershell
gix install library_name # Install any library you want or framework or even custom compilers
```

##  Features

- **Fast**: Performance comparable to C/C++ using LLVM ( Clang++ Compiler )
- **Simple Syntax**: Easy to read and write, quick to learn and develop with. No using of "{}" only "end"
- **Flexible**: Write low level system code or high level applications fast and easily
- **Error handling & Help**: Easy to read & know the issues from the error msg

>> **Built for developers who want speed and safety without complexity.**