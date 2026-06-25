# Struct Example

This example demonstrates struct usage in Gix.

- **main.gix** - Example public struct
- **example.gix** - Import example

```ruby
    import Example from "src/main.gix"
```

### Usage
- Public/Private structers have different visibility rules
- Public fields can be accessed from other scripts inside the src
- Private fields can only be used within the same file