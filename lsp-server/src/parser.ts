import { tokenize, Token, TokenType } from './tokenizer';
import { Symbol, Subroutine, Variable, Parameter } from './symbols';

export interface ParseResult {
	symbols: Symbol[];
	includes: string[];
}

export class A2Parser {
	private tokens: Token[] = [];
	private position = 0;

	parse(text: string): ParseResult {
		this.tokens = tokenize(text);
		this.position = 0;
		const symbols: Symbol[] = [];
		const includes: string[] = [];

		// First pass: extract includes from comments
		this.extractIncludes(text, includes);

		// Second pass: parse symbols
		this.position = 0;
		while (this.position < this.tokens.length) {
			const parsedSymbols = this.parseTopLevelStatement();
			symbols.push(...parsedSymbols);
			this.advance();
		}

		return { symbols, includes };
	}

	private extractIncludes(text: string, includes: string[]): void {
		// Look for lines like: ; .include "filename.asm"
		const lines = text.split('\n');
		const includeRegex = /^\s*;\s*\.include\s+"([^"]+)"\s*$/i;

		for (const line of lines) {
			const match = line.match(includeRegex);
			if (match) {
				includes.push(match[1]);
			}
		}
	}

	private parseTopLevelStatement(): Symbol[] {
		const token = this.currentToken();
		if (!token) return [];

		if (token.type === TokenType.KEYWORD) {
			switch (token.text) {
				case 'let':
					return this.parseLetStatement();
				case 'var':
					return this.parseVarStatement();
				case 'use':
					return this.parseUseStatement();
			}
		}

		return [];
	}

	private parseLetStatement(): Symbol[] {
		// Look for documentation comments before 'let'
		const documentation = this.extractPrecedingComments();

		this.advance(); // skip 'let'

		const symbols: Symbol[] = [];

		// Check if it's a block syntax: let (...)
		if (this.currentToken()?.text === '(') {
			this.advance(); // skip '('

			while (this.currentToken() && this.currentToken()?.text !== ')') {
				const symbol = this.parseLetArgument(documentation);
				if (symbol) {
					symbols.push(symbol);
				}

				if (this.currentToken()?.text === ',') {
					this.advance();
				}
			}

			if (this.currentToken()?.text === ')') {
				this.advance(); // skip ')'
			}
		} else {
			// Single argument syntax: let name = value
			const symbol = this.parseLetArgument(documentation);
			if (symbol) {
				symbols.push(symbol);
			}
		}

		return symbols;
	}

	private parseLetArgument(documentation?: string): Symbol | null {
		const nameToken = this.expectToken(TokenType.IDENTIFIER);
		if (!nameToken) return null;

		this.advance(); // skip identifier

		if (!this.expectAndAdvance('=')) return null;

		const valueToken = this.currentToken();
		if (!valueToken) return null;

		if (valueToken.type === TokenType.KEYWORD && valueToken.text === 'sub') {
			// Parse subroutine definition
			const subroutine = this.parseSubroutineDefinition(nameToken.text, nameToken);
			if (subroutine) {
				// Add documentation to subroutine
				subroutine.documentation = documentation;

				return {
					name: nameToken.text,
					kind: 'subroutine',
					data: subroutine,
					scope: 'global',
					definitionRange: {
						start: { line: nameToken.line, character: nameToken.character },
						end: { line: nameToken.line, character: nameToken.character + nameToken.text.length }
					}
				};
			}
		}

		// For non-subroutine values, we could add support for constants/variables later
		return null;
	}

	private parseVarStatement(): Symbol[] {
		this.advance(); // skip 'var'

		const variables: Symbol[] = [];

		// Check if it's a block syntax: var [...]
		if (this.currentToken()?.text === '[') {
			this.advance(); // skip '['

			while (this.currentToken() && this.currentToken()?.text !== ']') {
				const variable = this.parseParameter();
				if (variable) {
					variables.push({
						name: variable.name,
						kind: 'variable',
						data: variable as Variable,
						scope: 'global'
					});
				}

				if (this.currentToken()?.text === ',') {
					this.advance();
				}
			}

			if (this.currentToken()?.text === ']') {
				this.advance(); // skip ']'
			}
		} else {
			// Single variable syntax: var PTR: word @ $06
			const variable = this.parseParameter();
			if (variable) {
				variables.push({
					name: variable.name,
					kind: 'variable',
					data: variable as Variable,
					scope: 'global'
				});
			}
		}

		return variables;
	}

	private parseUseStatement(): Symbol[] {
		this.advance(); // skip 'use'

		const symbols: Symbol[] = [];

		// Check if it's a block syntax: use [...]
		if (this.currentToken()?.text === '[') {
			this.advance(); // skip '['

			while (this.currentToken() && this.currentToken()?.text !== ']') {
				const symbol = this.parseUseDeclaration();
				if (symbol) {
					symbols.push(symbol);
				}

				if (this.currentToken()?.text === ',') {
					this.advance();
				}
			}

			if (this.currentToken()?.text === ']') {
				this.advance(); // skip ']'
			}
		} else {
			// Single parameter syntax: use COUT : sub <- [ch: char @ A] @ $FDED
			const symbol = this.parseUseDeclaration();
			if (symbol) {
				symbols.push(symbol);
			}
		}

		return symbols;
	}

	private parseUseDeclaration(): Symbol | null {
		// Look for comments before this declaration
		const documentation = this.extractPrecedingComments();

		const nameToken = this.expectToken(TokenType.IDENTIFIER);
		if (!nameToken) return null;

		this.advance(); // skip identifier

		if (!this.expectAndAdvance(':')) return null;

		const typeToken = this.currentToken();
		if (typeToken?.type === TokenType.KEYWORD && typeToken.text === 'sub') {
			// Parse external subroutine declaration
			const subroutine = this.parseSubroutineType(nameToken.text, nameToken);
			if (subroutine) {
				// Add documentation to subroutine
				subroutine.documentation = documentation;

				return {
					name: nameToken.text,
					kind: 'subroutine',
					data: subroutine,
					scope: 'global',
					definitionRange: {
						start: { line: nameToken.line, character: nameToken.character },
						end: { line: nameToken.line, character: nameToken.character + nameToken.text.length }
					}
				};
			}
		}

		return null;
	}

	private parseSubroutineDefinition(name: string, nameToken: Token): Subroutine | null {
		this.advance(); // skip 'sub'

		const inputs: Parameter[] = [];
		const outputs: Parameter[] = [];

		// Parse input parameters
		if (this.currentToken()?.text === '<-') {
			this.advance(); // skip '<-'
			this.parseParameterList(inputs);
		}

		// Parse output parameters
		if (this.currentToken()?.text === '->') {
			this.advance(); // skip '->'
			this.parseParameterList(outputs);
		}

		return {
			name,
			inputs,
			outputs,
			definitionRange: {
				start: { line: nameToken.line, character: nameToken.character },
				end: { line: nameToken.line, character: nameToken.character + nameToken.text.length }
			}
		};
	}

	private parseSubroutineType(name: string, nameToken: Token): Subroutine | null {
		this.advance(); // skip 'sub'

		const inputs: Parameter[] = [];
		const outputs: Parameter[] = [];

		// Parse input parameters
		if (this.currentToken()?.text === '<-') {
			this.advance(); // skip '<-'
			this.parseParameterList(inputs);
		}

		// Parse output parameters
		if (this.currentToken()?.text === '->') {
			this.advance(); // skip '->'
			this.parseParameterList(outputs);
		}

		// Parse location if present
		let location: string | undefined;
		if (this.currentToken()?.text === '@') {
			this.advance(); // skip '@'
			const locationToken = this.currentToken();
			if (locationToken) {
				location = locationToken.text;
				this.advance();
			}
		}

		return {
			name,
			inputs,
			outputs,
			location,
			definitionRange: {
				start: { line: nameToken.line, character: nameToken.character },
				end: { line: nameToken.line, character: nameToken.character + nameToken.text.length }
			}
		};
	}

	private parseParameterList(parameters: Parameter[]): void {
		if (!this.expectAndAdvance('[')) return;

		while (this.currentToken() && this.currentToken()?.text !== ']') {
			const param = this.parseParameter();
			if (param) {
				parameters.push(param);
			}

			if (this.currentToken()?.text === ',') {
				this.advance();
			}
		}

		if (this.currentToken()?.text === ']') {
			this.advance();
		}
	}

	private parseParameter(): Parameter | null {
		const nameToken = this.expectToken(TokenType.IDENTIFIER);
		if (!nameToken) return null;

		this.advance(); // skip identifier

		let type = 'unknown';
		let location: string | undefined;
		let register: string | undefined;

		// Parse type
		if (this.currentToken()?.text === ':') {
			this.advance(); // skip ':'
			const typeToken = this.currentToken();
			if (typeToken) {
				type = typeToken.text;

				// Handle builtin alias: text -> char^
				if (type === 'text') {
					type = 'char^';
				}

				this.advance();

				// Handle array types (e.g., char^16)
				if (this.currentToken()?.text === '^') {
					this.advance(); // skip '^'
					const sizeToken = this.currentToken();
					if (sizeToken) {
						type += sizeToken.text;
						this.advance();
					}
				}
			}
		}

		// Parse location/register
		if (this.currentToken()?.text === '@') {
			this.advance(); // skip '@'
			const locationToken = this.currentToken();
			if (locationToken) {
				if (locationToken.text === 'A' || locationToken.text === 'X' || locationToken.text === 'Y') {
					register = locationToken.text;
				} else {
					location = locationToken.text;
				}
				this.advance();
			}
		}

		return {
			name: nameToken.text,
			type,
			location,
			register
		};
	}

	private currentToken(): Token | undefined {
		return this.position < this.tokens.length ? this.tokens[this.position] : undefined;
	}

	private advance(): void {
		if (this.position < this.tokens.length) {
			this.position++;
		}
		// Skip whitespace and comments
		while (this.position < this.tokens.length) {
			const token = this.tokens[this.position];
			if (token.type !== TokenType.WHITESPACE && token.type !== TokenType.COMMENT) {
				break;
			}
			this.position++;
		}
	}

	private expectToken(type: TokenType): Token | null {
		const token = this.currentToken();
		return token && token.type === type ? token : null;
	}

	private expectAndAdvance(text: string): boolean {
		const token = this.currentToken();
		if (token && token.text === text) {
			this.advance();
			return true;
		}
		return false;
	}

	private extractPrecedingComments(): string | undefined {
		// Look backwards from current position to find comment tokens
		const comments: string[] = [];
		let checkPosition = this.position - 1;

		// Go backwards through tokens, collecting comments
		while (checkPosition >= 0) {
			const token = this.tokens[checkPosition];

			if (token.type === TokenType.COMMENT) {
				// Extract comment text without the ';' prefix
				const commentText = token.text.slice(1).trim();
				if (commentText) {
					comments.unshift(commentText);
				}
			} else if (token.type === TokenType.WHITESPACE) {
				// Skip whitespace tokens
			} else {
				// Stop when we hit a non-comment, non-whitespace token
				break;
			}

			checkPosition--;
		}

		return comments.length > 0 ? comments.join('\n') : undefined;
	}
}