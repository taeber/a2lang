# A2 Language Support for Visual Studio Code

Rich language support for the A2 programming language - a high-level language for Apple II that targets the MOS 6502 architecture.

## Features

### 🎨 Syntax Highlighting
- Keywords: `use`, `var`, `let`, `sub`, `if`, `loop`, `stop`, `repeat`, `asm`
- Data types: `byte`, `char`, `word`, `int`, `text`
- Numbers: Decimal (`123`), hexadecimal (`$FF`), binary (`%1010`)
- Character literals: `` `A ``, `` `H ``, `` `0 ``
- String literals: `"Hello, world!"`
- Comments: `; This is a comment`

### 🚀 IntelliSense & Autocomplete
- **Subroutine completion**: Suggests both built-in and user-defined subroutines
- **Variable completion**: Autocompletes declared variables with type information
- **Context-aware suggestions**: Different completions based on cursor context
- **Parameter completion**: Suggests parameter names inside function calls

### 📋 Signature Help
- Shows parameter information when typing function calls
- Displays parameter types, registers, and memory locations
- Triggered by `(` and `,` characters
- Example: `COUT(ch: char @ A)` shows parameter `ch` of type `char` in register `A`

### 💡 Hover Information
- **Subroutines**: Shows full signature with input/output parameters
- **Variables**: Displays type and memory location information
- **Keywords**: Contextual documentation for language constructs
- **Memory locations**: Shows register assignments (`@ A`, `@ X`, `@ Y`) and addresses (`@ $FDED`)

### 🔧 Language Configuration
- Automatic bracket matching: `{}`, `[]`, `()`
- Smart indentation for code blocks
- Proper comment handling with `;`
- No auto-closing for backticks (single character literals)

## A2 Language Features Supported

### Subroutine Declarations
```a2
; External subroutines
use [
    COUT : sub <- [ch: char @ A] @ $FDED,
    CROUT: sub @ $FD8E
]

; Custom subroutines
let increment = sub <- [n :byte] -> [result :byte] {
    let result = n + 1
    ->
}
```

### Variable Declarations
```a2
var [
    counter :byte @$80,
    message :char^16 @$90,
    pointer :word @$06
]
```

### Register and Memory Binding
- **Register binding**: `@ A`, `@ X`, `@ Y` for 6502 registers
- **Memory locations**: `@ $80`, `@ $FDED` for specific addresses
- **Zero Page**: Automatic recognition of zero page variables

## Getting Started

1. **Install the extension** from the VSCode marketplace
2. **Open or create** an `.a2` file
3. **Start coding** with full IntelliSense support!

### Example A2 Code
```a2
; Hello World in A2
use [
    COUT : sub <- [ch: char @ A] @ $FDED,
    CROUT: sub @ $FD8E
]

let main = sub {
    COUT(`H`)
    COUT(`e`)
    COUT(`l`)
    COUT(`l`)
    COUT(`o`)
    CROUT()
}
```

## Configuration

The extension can be configured through VSCode settings:

- **`a2LanguageServer.compilerPath`**: Path to the A2 compiler executable (default: `./compile`)

## Requirements

- Visual Studio Code 1.74.0 or higher
- A2 compiler (optional, for syntax validation)

## About A2

A2 is a programming language designed specifically for the Apple II computer, providing:
- **Higher-level syntax** than assembly while maintaining low-level control
- **Direct 6502 register access** for optimal performance
- **Memory location binding** for precise hardware control
- **Apple II ROM integration** for system calls

## Links

- **Official Website**: [a2lang.com](http://a2lang.com)
- **GitHub Repository**: [github.com/a2lang/a2lang](https://github.com/a2lang/a2lang)
- **Issues & Support**: [GitHub Issues](https://github.com/a2lang/a2lang/issues)

## Version History

### 0.1.0 (Beta)
- Initial release with full language support
- Syntax highlighting for all A2 constructs
- IntelliSense with subroutine and variable completion
- Signature help for function calls
- Hover information with detailed documentation
- Language configuration and smart indentation

---

**Happy coding on your Apple II!** 🍎✨