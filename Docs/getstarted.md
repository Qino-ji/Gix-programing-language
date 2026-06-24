# Get Started

> Vix programming language starting guide. If this is your first time trying Vix, this will explain exactly what you need to get started. If you haven't seen our overview yet, it will help you understand the language better check it out [here](https://vixlanguage.github.io/docs/overview).


---


## Installation

**Step 1- Install Vix**  
Download and install Vix from the [installer page](https://vixlanguage.github.io/install). Also, you can check out our playground, try vix without installing it! [playground](https://vixlanguage.github.io/playground).

Once installed, verify it works by running:

```powershell
vix --version
```

**Step 2 - Install the LPS (Language Programming Server)**  
Visit the [LPS installer page](https://vixlanguage.github.io/install/lps), or search for `vix programming language` in your IDE's extension marketplace. A [VS Code extension](https://marketplace.visualstudio.com/VSCode) is also officially available.

-> Installer source code: [GitHub](https://github.com/vix-programing-language/installer)  
-> LPS source code: [GitHub](https://github.com/vix-programing-language/lps)  
-> VS Code installation: [VS Code](https://)

---


## Prerequisites
Make sure you have all of the following requirements. The tools below will be installed automatically if not already present:
- LLVM version 21.x
- Development tools required by VS
- At least 5 GB of free storage

Vix supports all major operating systems:
- [Linux](https://kernel.org)
- [Windows](https://microsoft.com/windows)
- [macOS](https://apple.com/macos)

And the following architectures: x86, ARM, and RISC-V. Vix also supports custom operating systems if you compile the compiler from source: [vix compiler on GitHub](https://github.com/vix-programing-language/vix-programing-language).

---
## Is up to date?
To make sure you're using latest vix compiler, run in terminal:
```powershell
vix update # update if avaiable
vix version # current version/update
```

## Your First Program

Create a file called `main.vix` and add the following code:

```vix
import * from io

func main()
    print("Hello, my first program in Vix!")
end
```

Then open a terminal and run:

```powershell
vix run main.vix
```

You should see:

> Hello, my first program in Vix!

## Learning
Learning vix can be quit simple. The syntax and functionality of the language would make it for everyone to learn easily! if you want to learn check out our [docs](https://vixlanguage.github.io/docs) and if you have any question, don't hastotate and join our discord server and ask whatever you want and we'll be there for you! [click here](https://discord.gg/CAemjRc4ya) to join!