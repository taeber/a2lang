export interface Parameter {
	name: string;
	type: string;
	location?: string;
	register?: string;
}

export interface Subroutine {
	name: string;
	inputs: Parameter[];
	outputs: Parameter[];
	location?: string;
	documentation?: string;
	definitionRange?: {
		start: { line: number; character: number };
		end: { line: number; character: number };
	};
}

export interface Variable {
	name: string;
	type: string;
	location?: string;
	register?: string;
	definitionRange?: {
		start: { line: number; character: number };
		end: { line: number; character: number };
	};
}

export interface Symbol {
	name: string;
	kind: 'subroutine' | 'variable' | 'parameter';
	data: Subroutine | Variable | Parameter;
	scope: 'global' | 'local';
	definitionRange?: {
		start: { line: number; character: number };
		end: { line: number; character: number };
	};
}

export class SymbolTable {
	private symbols: Map<string, Symbol[]> = new Map(); // URI -> symbols
	private includes: Map<string, string[]> = new Map(); // URI -> included files
	private globalSymbols: Map<string, Symbol> = new Map(); // name -> symbol

	updateDocument(uri: string, symbols: Symbol[], includes: string[] = []): void {
		this.symbols.set(uri, symbols);
		this.includes.set(uri, includes);
		this.rebuildGlobalSymbols();
	}

	private rebuildGlobalSymbols(): void {
		this.globalSymbols.clear();

		for (const [uri, documentSymbols] of this.symbols) {
			for (const symbol of documentSymbols) {
				if (symbol.scope === 'global') {
					this.globalSymbols.set(symbol.name, symbol);
				}
			}
		}
	}

	getSymbolsForDocument(uri: string): Symbol[] {
		return this.symbols.get(uri) || [];
	}

	getGlobalSymbols(): Symbol[] {
		return Array.from(this.globalSymbols.values());
	}

	getSubroutines(): Subroutine[] {
		return Array.from(this.globalSymbols.values())
			.filter(symbol => symbol.kind === 'subroutine')
			.map(symbol => symbol.data as Subroutine);
	}

	getVariables(): Variable[] {
		return Array.from(this.globalSymbols.values())
			.filter(symbol => symbol.kind === 'variable')
			.map(symbol => symbol.data as Variable);
	}

	findSymbol(name: string): Symbol | undefined {
		return this.globalSymbols.get(name);
	}

	findSubroutine(name: string): Subroutine | undefined {
		const symbol = this.globalSymbols.get(name);
		return symbol?.kind === 'subroutine' ? symbol.data as Subroutine : undefined;
	}

	findVariable(name: string): Variable | undefined {
		const symbol = this.globalSymbols.get(name);
		return symbol?.kind === 'variable' ? symbol.data as Variable : undefined;
	}

	clear(): void {
		this.symbols.clear();
		this.globalSymbols.clear();
	}

	removeDocument(uri: string): void {
		this.symbols.delete(uri);
		this.includes.delete(uri);
		this.rebuildGlobalSymbols();
	}

	getSymbolsWithIncludes(uri: string): Symbol[] {
		const allSymbols: Symbol[] = [];

		// Add symbols from the current file
		const fileSymbols = this.symbols.get(uri) || [];
		allSymbols.push(...fileSymbols);

		// Add symbols from included files
		const includedFiles = this.includes.get(uri) || [];
		for (const includedFile of includedFiles) {
			// Handle file:// URIs properly
			let includedUri: string;
			if (uri.startsWith('file://')) {
				// For file:// URIs, parse and reconstruct
				const baseUrl = new URL(uri);
				const baseDir = baseUrl.pathname.substring(0, baseUrl.pathname.lastIndexOf('/') + 1);
				includedUri = `file://${baseDir}${includedFile}`;
			} else {
				// For regular paths
				const baseUri = uri.substring(0, uri.lastIndexOf('/') + 1);
				includedUri = baseUri + includedFile;
			}

			// Debug logging
			console.log(`[DEBUG] Looking for included file: ${includedFile}`);
			console.log(`[DEBUG] Original URI: ${uri}`);
			console.log(`[DEBUG] Constructed included URI: ${includedUri}`);
			console.log(`[DEBUG] Available URIs:`, Array.from(this.symbols.keys()));

			const includedSymbols = this.symbols.get(includedUri) || [];
			console.log(`[DEBUG] Found ${includedSymbols.length} symbols in included file`);
			allSymbols.push(...includedSymbols);
		}

		return allSymbols;
	}

	findSymbolWithIncludes(uri: string, name: string): Symbol | undefined {
		const allSymbols = this.getSymbolsWithIncludes(uri);
		return allSymbols.find(symbol => symbol.name === name);
	}
}