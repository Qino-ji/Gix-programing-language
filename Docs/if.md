# What is if stmt?
The `if` statement is used to execute a block of code if a condition is true. It can also have an optional `else` block to execute if the condition is false. And optional `elif/else if` block to execute if the condition is false and another condition is true. But if all conditions are false it will not execute any block and execute `else` block if exists. 

- Definding `if` statement requires the `if` keyword followed by the condition and `then` keyword. The condition can be any expression that returns a boolean value. The `then` keyword is required to start the block of code to execute if the condition is true. Example:

```vix
if condition then
    // code to execute if condition is true
end
```

- The `else` block is optional and is executed if the condition is false. It requires the `else` keyword to start the block of code to execute if the condition is false or all other conditions are false. Example:

```vix
if condition then
    // code to execute if condition is true
elif condition then
    // code to execute if condition is true
else
    // code to execute if all conditions are false
end
```

- The `elif` block is optional and is executed if the condition is false and another condition is true. It requires the `elif` keyword followed by the condition and `then` keyword. This can be done using `else if` too. And single if statement are allowed to contain multiple `elif` blocks. Example:

```vix
if condition then
    // code to execute if condition is true
elif condition then
    // code to execute if condition is true
else
    // code to execute if all conditions are false
end
```
