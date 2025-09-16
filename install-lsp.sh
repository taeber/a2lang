#!/bin/bash

set -e

echo "Installing A2 Language Server..."

# Build the A2 compiler first
echo "Building A2 compiler..."
make debug

# Install LSP server dependencies
echo "Installing LSP server dependencies..."
cd lsp-server
npm install
npm run compile
cd ..

# Install VSCode extension dependencies
echo "Installing VSCode extension dependencies..."
cd vscode-extension
npm install
npm run compile
cd ..

echo "Installation complete!"
echo ""
echo "To install the VSCode extension:"
echo "1. Open VSCode"
echo "2. Go to Extensions view (Ctrl+Shift+X)"
echo "3. Click the '...' menu and select 'Install from VSIX...'"
echo "4. Navigate to the vscode-extension directory and select the generated .vsix file"
echo ""
echo "Or use the command line:"
echo "  code --install-extension vscode-extension/"
echo ""
echo "The LSP server will automatically start when you open .a2 files in VSCode."