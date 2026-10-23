# Contributing

CentUI uses C++20, public headers in `include/centui`, and implementations in `src`.
Keep templates and `constexpr` definitions in headers; put other implementation
details in `.cpp` files.

## Code style

- Use `snake_case` for types and functions, and `m_` for private data members.
- Use four spaces, Allman braces, and the repository's `.clang-format` settings.
- Separate functions with a blank line. Inside a function, use blank lines to
  distinguish validation, preparation, state changes, and the result.
- Break long parameter and argument lists onto separate lines. Review constructor
  initializers, lambdas, and nested widget trees after formatting.
- Group public operations before private helpers and state. Use short type aliases
  when a container or resource type obscures the declaration.
- Add brief Doxygen comments to public types and non-obvious contracts. Explain
  ownership, lifetime, units, invalidation, and callback restrictions where relevant.
  Avoid comments that merely repeat a function's name.

## Ownership

Use values and `std::unique_ptr` for exclusive ownership. Use `std::shared_ptr` only
for shared lifetime, and `std::weak_ptr` for observers that may outlive an object.
Borrow objects through references or `std::reference_wrapper`; use `std::optional`
when the reference may be absent. Use `std::span` for borrowed buffers.

Native C APIs require pointer-shaped handles and callback signatures. Keep those at
the integration boundary, pair acquired resources with RAII immediately, and never
use a raw pointer to represent ownership. Declare dependent resources so destruction
releases them before their native context.

Format every changed C++ file, then review the diff manually. Include examples and
tests in style changes, and keep the documentation consistent with the public API.
