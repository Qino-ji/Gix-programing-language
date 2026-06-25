# Function Example

This example demonstrates function usage in Gix.

- **main.gix** - Example public function
- **another_script.gix** - Import example

```ruby
    import example from "src/main.gix"
```

### Usage
- Public functions are visible for all other scripts inside the src
- Private functions cannot be imported from other files

```ruby
    import example from "src/another_main.gix"
    ----------------------------------------- # Function is private.