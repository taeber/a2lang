export interface Token {
	type: TokenType;
	text: string;
	start: number;
	end: number;
	line: number;
	character: number;
}

export enum TokenType {
	KEYWORD = 'keyword',
	IDENTIFIER = 'identifier',
	SUBROUTINE = 'subroutine',
	NUMBER = 'number',
	STRING = 'string',
	CHAR = 'char',
	OPERATOR = 'operator',
	PUNCTUATION = 'punctuation',
	COMMENT = 'comment',
	WHITESPACE = 'whitespace',
	TYPE = 'type',
	UNKNOWN = 'unknown'
}

const KEYWORDS = new Set([
	'use', 'var', 'let', 'sub', 'if', 'loop', 'stop', 'repeat', 'asm'
]);

const TYPES = new Set([
	'byte', 'char', 'word'
]);

const OPERATORS = new Set([
	'=', ':', '+', '-', '&', '|', '^', '!',
	'==', '<>', '<', '>', '<=', '>=',
	':=', '+=', '-=', '&=', '|=', '^=', '!=',
	'->', '<-'
]);

const PUNCTUATION = new Set([
	'{', '}', '(', ')', '[', ']', ',', ';', '.', '_', '@', '$', '%', '`', '"', '\\'
]);

export function tokenize(text: string, knownSubroutines?: Set<string>): Token[] {
	const tokens: Token[] = [];
	let i = 0;
	let line = 0;
	let lineStart = 0;

	while (i < text.length) {
		const ch = text[i];
		const character = i - lineStart;

		if (ch === '\n') {
			line++;
			lineStart = i + 1;
			i++;
			continue;
		}

		if (ch === ' ' || ch === '\t' || ch === '\r') {
			const start = i;
			while (i < text.length && /\s/.test(text[i]) && text[i] !== '\n') {
				i++;
			}
			tokens.push({
				type: TokenType.WHITESPACE,
				text: text.slice(start, i),
				start,
				end: i,
				line,
				character
			});
			continue;
		}

		if (ch === ';') {
			const start = i;
			while (i < text.length && text[i] !== '\n') {
				i++;
			}
			tokens.push({
				type: TokenType.COMMENT,
				text: text.slice(start, i),
				start,
				end: i,
				line,
				character
			});
			continue;
		}

		if (ch === '"') {
			const start = i;
			i++;
			while (i < text.length && text[i] !== '"') {
				if (text[i] === '\\' && i + 1 < text.length) {
					i += 2;
				} else {
					i++;
				}
			}
			if (i < text.length) i++;
			tokens.push({
				type: TokenType.STRING,
				text: text.slice(start, i),
				start,
				end: i,
				line,
				character
			});
			continue;
		}

		if (ch === '`') {
			const start = i;
			i++;
			if (i < text.length) i++;
			tokens.push({
				type: TokenType.CHAR,
				text: text.slice(start, i),
				start,
				end: i,
				line,
				character
			});
			continue;
		}

		if (ch === '$') {
			const start = i;
			i++;
			while (i < text.length && /[0-9a-fA-F]/.test(text[i])) {
				i++;
			}
			tokens.push({
				type: TokenType.NUMBER,
				text: text.slice(start, i),
				start,
				end: i,
				line,
				character
			});
			continue;
		}

		if (ch === '%') {
			const start = i;
			i++;
			while (i < text.length && /[01]/.test(text[i])) {
				i++;
			}
			tokens.push({
				type: TokenType.NUMBER,
				text: text.slice(start, i),
				start,
				end: i,
				line,
				character
			});
			continue;
		}

		if (/[0-9]/.test(ch) || (ch === '-' && i + 1 < text.length && /[0-9]/.test(text[i + 1]))) {
			const start = i;
			if (ch === '-') i++;
			while (i < text.length && /[0-9]/.test(text[i])) {
				i++;
			}
			tokens.push({
				type: TokenType.NUMBER,
				text: text.slice(start, i),
				start,
				end: i,
				line,
				character
			});
			continue;
		}

		if (/[a-zA-Z]/.test(ch)) {
			const start = i;
			while (i < text.length && /[a-zA-Z0-9]/.test(text[i])) {
				i++;
			}
			const word = text.slice(start, i);

			let tokenType: TokenType;
			if (KEYWORDS.has(word)) {
				tokenType = TokenType.KEYWORD;
			} else if (TYPES.has(word)) {
				tokenType = TokenType.TYPE;
			} else if (knownSubroutines && knownSubroutines.has(word)) {
				tokenType = TokenType.SUBROUTINE;
			} else {
				tokenType = TokenType.IDENTIFIER;
			}

			tokens.push({
				type: tokenType,
				text: word,
				start,
				end: i,
				line,
				character
			});
			continue;
		}

		let operatorFound = false;
		for (let len = 3; len >= 1; len--) {
			const substr = text.slice(i, i + len);
			if (OPERATORS.has(substr)) {
				tokens.push({
					type: TokenType.OPERATOR,
					text: substr,
					start: i,
					end: i + len,
					line,
					character
				});
				i += len;
				operatorFound = true;
				break;
			}
		}
		if (operatorFound) continue;

		if (PUNCTUATION.has(ch)) {
			tokens.push({
				type: TokenType.PUNCTUATION,
				text: ch,
				start: i,
				end: i + 1,
				line,
				character
			});
			i++;
			continue;
		}

		tokens.push({
			type: TokenType.UNKNOWN,
			text: ch,
			start: i,
			end: i + 1,
			line,
			character
		});
		i++;
	}

	return tokens;
}