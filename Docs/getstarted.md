# Get Started

> Gix programming language starting guide. If this is your first time trying Gix, this will explain exactly what you need to get started.

---

## Installation

**Step 1- Install Gix**  
Download and install Gix from the installer page.

Once installed, verify it works by running:

```powershell
gix --version
```

**Step 2 - Install the LPS (Language Programming Server)**  
Visit the LPS installer page, or search for `gix programming language` in your IDE's extension marketplace.

---

## Prerequisites
Make sure you have all of the following requirements. The tools below will be installed automatically if not already present:
- LLVM version 21.x
- Development tools required by VS
- At least 5 GB of free storage

Gix supports all major operating systems:
- [Linux](https://kernel.org)
- [Windows](https://microsoft.com/windows)
- [macOS](https://apple.com/macos)

And the following architectures: x86, ARM, and RISC-V.

---
## Is up to date?
To make sure you're using latest gix compiler, run in terminal:
```powershell
gix update # update if avaiable
gix version # current version/update
```

## Your First Program

Create a file called `main.gix` and add the following code:

```gix
import * from io

func main()
    print("Hello, my first program in Gix!")
end
```

Then open a terminal and run:

```powershell
gix run main.gix
```

You should see:

> Hello, my first program in Gix!

## Learning
Learning gix can be quit simple. The syntax and functionality of the language would make it for everyone to learn easily! if you want to learn check out the docs and if you have any question, join our discord server and ask whatever you want!