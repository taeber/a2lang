import {
	createConnection,
	TextDocuments,
	Diagnostic,
	DiagnosticSeverity,
	ProposedFeatures,
	InitializeParams,
	DidChangeConfigurationNotification,
	CompletionItem,
	CompletionItemKind,
	TextDocumentPositionParams,
	TextDocumentSyncKind,
	InitializeResult,
	Hover,
	Position,
	Range,
	Location,
	Definition,
	SignatureHelp,
	SignatureInformation,
	ParameterInformation
} from 'vscode-languageserver/node';

import { TextDocument } from 'vscode-languageserver-textdocument';
import { spawn } from 'child_process';
import * as path from 'path';
import { SymbolTable, Subroutine, Variable, Parameter } from './symbols';
import { A2Parser } from './parser';
import { tokenize, TokenType } from './tokenizer';

const connection = createConnection(ProposedFeatures.all);
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);
const symbolTable = new SymbolTable();
const parser = new A2Parser();

let hasConfigurationCapability = false;
let hasWorkspaceFolderCapability = false;
let hasDiagnosticRelatedInformationCapability = false;

connection.onInitialize((params: InitializeParams) => {
	const capabilities = params.capabilities;

	hasConfigurationCapability = !!(
		capabilities.workspace && !!capabilities.workspace.configuration
	);
	hasWorkspaceFolderCapability = !!(
		capabilities.workspace && !!capabilities.workspace.workspaceFolders
	);
	hasDiagnosticRelatedInformationCapability = !!(
		capabilities.textDocument &&
		capabilities.textDocument.publishDiagnostics &&
		capabilities.textDocument.publishDiagnostics.relatedInformation
	);

	const result: InitializeResult = {
		capabilities: {
			textDocumentSync: TextDocumentSyncKind.Incremental,
			completionProvider: {
				resolveProvider: true
			},
			hoverProvider: true,
			definitionProvider: true,
			signatureHelpProvider: {
				triggerCharacters: ['(', ',']
			}
		}
	};

	if (hasWorkspaceFolderCapability) {
		result.capabilities.workspace = {
			workspaceFolders: {
				supported: true
			}
		};
	}

	return result;
});

connection.onInitialized(() => {
	if (hasConfigurationCapability) {
		connection.client.register(DidChangeConfigurationNotification.type, undefined);
	}
	if (hasWorkspaceFolderCapability) {
		connection.workspace.onDidChangeWorkspaceFolders(_event => {
			connection.console.log('Workspace folder change event received.');
		});
	}
});

interface A2Settings {
	compilerPath: string;
}

const defaultSettings: A2Settings = { compilerPath: '/Users/taeber/code/a2lang/compile' };
let globalSettings: A2Settings = defaultSettings;

const documentSettings: Map<string, Thenable<A2Settings>> = new Map();

connection.onDidChangeConfiguration(change => {
	if (hasConfigurationCapability) {
		documentSettings.clear();
	} else {
		globalSettings = <A2Settings>(
			(change.settings.a2LanguageServer || defaultSettings)
		);
	}

	documents.all().forEach(validateTextDocument);
});

function getDocumentSettings(resource: string): Thenable<A2Settings> {
	if (!hasConfigurationCapability) {
		return Promise.resolve(globalSettings);
	}
	let result = documentSettings.get(resource);
	if (!result) {
		result = connection.workspace.getConfiguration({
			scopeUri: resource,
			section: 'a2LanguageServer'
		});
		documentSettings.set(resource, result);
	}
	return result;
}

documents.onDidClose(e => {
	documentSettings.delete(e.document.uri);
});

documents.onDidChangeContent(change => {
	updateSymbolTable(change.document);
	// Temporarily disable compiler validation - using LSP parsing only
	// validateTextDocument(change.document);
});

function updateSymbolTable(document: TextDocument): void {
	try {
		const text = document.getText();
		const parseResult = parser.parse(text);
		symbolTable.updateDocument(document.uri, parseResult.symbols, parseResult.includes);
	} catch (error) {
		// If parsing fails, continue with empty symbols
		symbolTable.updateDocument(document.uri, [], []);
	}
}

async function validateTextDocument(textDocument: TextDocument): Promise<void> {
	const settings = await getDocumentSettings(textDocument.uri);
	const text = textDocument.getText();

	const diagnostics: Diagnostic[] = [];

	try {
		const compilerPath = path.resolve(settings.compilerPath);

		// Check if compiler exists
		const fs = require('fs');
		if (!fs.existsSync(compilerPath)) {
			const diagnostic: Diagnostic = {
				severity: DiagnosticSeverity.Error,
				range: {
					start: { line: 0, character: 0 },
					end: { line: 0, character: 0 }
				},
				message: `A2 compiler not found at: ${compilerPath}. Please build the compiler with 'make compile' or configure the path in settings.`,
				source: 'a2'
			};
			diagnostics.push(diagnostic);
			connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
			return;
		}

		const child = spawn(compilerPath, ['-'], {
			stdio: ['pipe', 'pipe', 'pipe']
		});

		let stdout = '';
		let stderr = '';

		child.stdout.on('data', (data) => {
			stdout += data.toString();
		});

		child.stderr.on('data', (data) => {
			stderr += data.toString();
		});

		child.on('close', (code) => {
			if (code !== 0 && stderr) {
				// Parse error messages to extract line numbers if possible
				const lines = stderr.split('\n');
				for (const line of lines) {
					if (line.trim()) {
						const diagnostic: Diagnostic = {
							severity: DiagnosticSeverity.Error,
							range: {
								start: { line: 0, character: 0 },
								end: { line: 0, character: 0 }
							},
							message: line.trim(),
							source: 'a2'
						};
						diagnostics.push(diagnostic);
					}
				}
			}
			connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
		});

		child.on('error', (error) => {
			const diagnostic: Diagnostic = {
				severity: DiagnosticSeverity.Error,
				range: {
					start: { line: 0, character: 0 },
					end: { line: 0, character: 0 }
				},
				message: `Failed to start compiler: ${error.message}`,
				source: 'a2'
			};
			diagnostics.push(diagnostic);
			connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
		});

		child.stdin.write(text);
		child.stdin.end();

	} catch (error) {
		const diagnostic: Diagnostic = {
			severity: DiagnosticSeverity.Error,
			range: {
				start: { line: 0, character: 0 },
				end: { line: 0, character: 0 }
			},
			message: `Failed to run compiler: ${error}`,
			source: 'a2'
		};
		diagnostics.push(diagnostic);
		connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
	}
}

connection.onCompletion((params: TextDocumentPositionParams): CompletionItem[] => {
	const document = documents.get(params.textDocument.uri);
	if (!document) {
		return getBasicCompletions();
	}

	const text = document.getText();
	const position = params.position;
	const offset = document.offsetAt(position);

	// Check if we're in a function call context
	const context = getCompletionContext(text, offset);

	if (context.type === 'function-call') {
		return getFunctionCallCompletions(context.subroutineName, document.uri);
	}

	// Default completion: keywords + types + subroutines + variables
	const completions = getBasicCompletions();

	// Add subroutines from CURRENT FILE AND INCLUDED FILES
	const allSymbols = symbolTable.getSymbolsWithIncludes(document.uri);
	const subroutines = allSymbols
		.filter(symbol => symbol.kind === 'subroutine')
		.map(symbol => symbol.data as Subroutine);

	for (const sub of subroutines) {
		const paramText = sub.inputs.map(p => `${p.name}: ${p.type}`).join(', ');
		const outputText = sub.outputs.length > 0 ?
			' -> ' + sub.outputs.map(p => `${p.name}: ${p.type}`).join(', ') : '';

		completions.push({
			label: sub.name,
			kind: CompletionItemKind.Function,
			documentation: {
				kind: 'markdown',
				value: `**${sub.name}**\n\nParameters: (${paramText})${outputText}`
			},
			insertText: sub.inputs.length > 0 ? `${sub.name}($1)` : sub.name,
			insertTextFormat: 2 // snippet format
		});
	}

	// Add variables from CURRENT FILE AND INCLUDED FILES
	const variables = allSymbols
		.filter(symbol => symbol.kind === 'variable')
		.map(symbol => symbol.data as Variable);

	for (const variable of variables) {
		completions.push({
			label: variable.name,
			kind: CompletionItemKind.Variable,
			documentation: `Variable: ${variable.type}` +
				(variable.location ? ` @ ${variable.location}` : '') +
				(variable.register ? ` @ ${variable.register}` : '')
		});
	}

	return completions;
});

function getBasicCompletions(): CompletionItem[] {
	return [
		{
			label: 'use',
			kind: CompletionItemKind.Keyword,
			documentation: 'Declare external dependencies'
		},
		{
			label: 'var',
			kind: CompletionItemKind.Keyword,
			documentation: 'Declare variables'
		},
		{
			label: 'let',
			kind: CompletionItemKind.Keyword,
			documentation: 'Define values'
		},
		{
			label: 'sub',
			kind: CompletionItemKind.Keyword,
			documentation: 'Define subroutine type'
		},
		{
			label: 'if',
			kind: CompletionItemKind.Keyword,
			documentation: 'Conditional statement'
		},
		{
			label: 'loop',
			kind: CompletionItemKind.Keyword,
			documentation: 'Loop construct'
		},
		{
			label: 'stop',
			kind: CompletionItemKind.Keyword,
			documentation: 'Stop execution'
		},
		{
			label: 'repeat',
			kind: CompletionItemKind.Keyword,
			documentation: 'Repeat loop iteration'
		},
		{
			label: 'asm',
			kind: CompletionItemKind.Keyword,
			documentation: 'Inline assembly block'
		},
		{
			label: 'byte',
			kind: CompletionItemKind.TypeParameter,
			documentation: 'Single byte type'
		},
		{
			label: 'char',
			kind: CompletionItemKind.TypeParameter,
			documentation: 'Character type'
		},
		{
			label: 'word',
			kind: CompletionItemKind.TypeParameter,
			documentation: '16-bit word type'
		},
		{
			label: 'text',
			kind: CompletionItemKind.TypeParameter,
			documentation: 'Text type (alias for char^)'
		}
	];
}

interface CompletionContext {
	type: 'normal' | 'function-call';
	subroutineName?: string;
}

function getCompletionContext(text: string, offset: number): CompletionContext {
	// Look backwards from the cursor to see if we're in a function call
	let i = offset - 1;
	let parenCount = 0;

	// Skip whitespace
	while (i >= 0 && /\s/.test(text[i])) {
		i--;
	}

	// Look for function call pattern: identifier(
	while (i >= 0) {
		const ch = text[i];

		if (ch === ')') {
			parenCount++;
		} else if (ch === '(') {
			if (parenCount === 0) {
				// Found opening paren, now look for identifier before it
				i--;
				while (i >= 0 && /\s/.test(text[i])) {
					i--;
				}

				// Extract identifier
				const identEnd = i + 1;
				while (i >= 0 && /[a-zA-Z0-9_]/.test(text[i])) {
					i--;
				}

				if (i + 1 < identEnd) {
					const subroutineName = text.slice(i + 1, identEnd);
					return { type: 'function-call', subroutineName };
				}
				break;
			} else {
				parenCount--;
			}
		}

		i--;
	}

	return { type: 'normal' };
}

function getFunctionCallCompletions(subroutineName?: string, documentUri?: string): CompletionItem[] {
	if (!subroutineName || !documentUri) {
		return [];
	}

	// Find subroutine in CURRENT FILE AND INCLUDED FILES
	const subroutineSymbol = symbolTable.findSymbolWithIncludes(documentUri, subroutineName);
	if (!subroutineSymbol || subroutineSymbol.kind !== 'subroutine') {
		return [];
	}

	const subroutine = subroutineSymbol.data as Subroutine;

	// Return parameter completions
	return subroutine.inputs.map((param, index) => ({
		label: param.name,
		kind: CompletionItemKind.Property,
		documentation: `Parameter ${index + 1}: ${param.type}`,
		sortText: `${index.toString().padStart(2, '0')}_${param.name}`
	}));
}

connection.onCompletionResolve((item: CompletionItem): CompletionItem => {
	return item;
});

connection.onHover((_params: TextDocumentPositionParams): Hover | undefined => {
	const document = documents.get(_params.textDocument.uri);
	if (!document) {
		return undefined;
	}

	const wordRange = getWordRangeAtPosition(document, _params.position);
	if (!wordRange) {
		return undefined;
	}

	const word = document.getText(wordRange);

	// Check for keywords first
	const keywordInfo: Record<string, string> = {
		'use': 'Declare external dependencies',
		'var': 'Declare variables with optional type and location binding',
		'let': 'Define values (constants, subroutines, etc.)',
		'sub': 'Subroutine type with optional input/output parameters',
		'if': 'Conditional execution based on comparison',
		'loop': 'Loop construct (infinite or conditional)',
		'stop': 'Stop program execution',
		'repeat': 'Continue to next loop iteration',
		'asm': 'Inline 6502 assembly code block',
		'byte': 'Single byte (8-bit) data type',
		'char': 'Character data type (High-ASCII)',
		'word': '16-bit word data type',
		'text': 'Text type (builtin alias for char^)'
	};

	if (keywordInfo[word]) {
		return {
			contents: {
				kind: 'markdown',
				value: `**${word}** - ${keywordInfo[word]}`
			},
			range: wordRange
		};
	}

	// Check for subroutines in CURRENT FILE AND INCLUDED FILES
	const subroutineSymbol = symbolTable.findSymbolWithIncludes(_params.textDocument.uri, word);
	if (subroutineSymbol && subroutineSymbol.kind === 'subroutine') {
		const subroutine = subroutineSymbol.data as Subroutine;
		const inputParams = subroutine.inputs.map(p =>
			`${p.name}: ${p.type}` +
			(p.register ? ` @ ${p.register}` : '') +
			(p.location ? ` @ ${p.location}` : '')
		).join(', ');

		const outputParams = subroutine.outputs.map(p =>
			`${p.name}: ${p.type}` +
			(p.register ? ` @ ${p.register}` : '') +
			(p.location ? ` @ ${p.location}` : '')
		).join(', ');

		let signature = `${subroutine.name}(${inputParams})`;
		if (outputParams) {
			signature += ` -> (${outputParams})`;
		}

		let content = `**Subroutine**: \`${signature}\`\n\n`;

		if (subroutine.location) {
			content += `**Location**: ${subroutine.location}\n\n`;
		}

		if (subroutine.documentation) {
			content += `**Description**: ${subroutine.documentation}\n\n`;
		}

		if (subroutine.inputs.length > 0) {
			content += `**Parameters**:\n`;
			for (const param of subroutine.inputs) {
				content += `- \`${param.name}\`: ${param.type}`;
				if (param.register) content += ` (register ${param.register})`;
				if (param.location) content += ` (at ${param.location})`;
				content += '\n';
			}
			content += '\n';
		}

		if (subroutine.outputs.length > 0) {
			content += `**Returns**:\n`;
			for (const param of subroutine.outputs) {
				content += `- \`${param.name}\`: ${param.type}`;
				if (param.register) content += ` (register ${param.register})`;
				if (param.location) content += ` (at ${param.location})`;
				content += '\n';
			}
		}

		return {
			contents: {
				kind: 'markdown',
				value: content.trim()
			},
			range: wordRange
		};
	}

	// Check for variables in CURRENT FILE AND INCLUDED FILES
	const variableSymbol = symbolTable.findSymbolWithIncludes(_params.textDocument.uri, word);
	if (variableSymbol && variableSymbol.kind === 'variable') {
		const variable = variableSymbol.data as Variable;
		let content = `**Variable**: \`${variable.name}: ${variable.type}\`\n\n`;

		if (variable.location) {
			content += `**Location**: ${variable.location}\n`;
		}

		if (variable.register) {
			content += `**Register**: ${variable.register}\n`;
		}

		return {
			contents: {
				kind: 'markdown',
				value: content.trim()
			},
			range: wordRange
		};
	}

	return undefined;
});

function getWordRangeAtPosition(document: TextDocument, position: Position): Range | undefined {
	const text = document.getText();
	const offset = document.offsetAt(position);

	let start = offset;
	let end = offset;

	while (start > 0 && /[a-zA-Z0-9_]/.test(text[start - 1])) {
		start--;
	}

	while (end < text.length && /[a-zA-Z0-9_]/.test(text[end])) {
		end++;
	}

	if (start === end) {
		return undefined;
	}

	return {
		start: document.positionAt(start),
		end: document.positionAt(end)
	};
}

connection.onSignatureHelp((params: TextDocumentPositionParams): SignatureHelp | undefined => {
	const document = documents.get(params.textDocument.uri);
	if (!document) {
		return undefined;
	}

	const text = document.getText();
	const offset = document.offsetAt(params.position);

	// Find the function call we're in
	const context = getSignatureContext(text, offset);
	if (!context.subroutineName) {
		return undefined;
	}

	// Find subroutine in CURRENT FILE AND INCLUDED FILES
	const subroutineSymbol = symbolTable.findSymbolWithIncludes(params.textDocument.uri, context.subroutineName);
	if (!subroutineSymbol || subroutineSymbol.kind !== 'subroutine') {
		return undefined;
	}

	const subroutine = subroutineSymbol.data as Subroutine;

	// Build signature information
	const parameters: ParameterInformation[] = subroutine.inputs.map(param => ({
		label: `${param.name}: ${param.type}`,
		documentation: param.register ? `Passed in register ${param.register}` :
			param.location ? `Located at ${param.location}` : undefined
	}));

	const signature: SignatureInformation = {
		label: `${subroutine.name}(${parameters.map(p => p.label).join(', ')})`,
		documentation: subroutine.documentation || `Subroutine ${subroutine.name}`,
		parameters
	};

	return {
		signatures: [signature],
		activeSignature: 0,
		activeParameter: Math.min(context.parameterIndex, parameters.length - 1)
	};
});

interface SignatureContext {
	subroutineName?: string;
	parameterIndex: number;
}

function getSignatureContext(text: string, offset: number): SignatureContext {
	let i = offset - 1;
	let parenCount = 0;
	let commaCount = 0;

	// Count backwards to find the opening paren and function name
	while (i >= 0) {
		const ch = text[i];

		if (ch === ')') {
			parenCount++;
		} else if (ch === '(') {
			if (parenCount === 0) {
				// Found the opening paren, now find the function name
				i--;
				while (i >= 0 && /\s/.test(text[i])) {
					i--;
				}

				// Extract identifier
				const identEnd = i + 1;
				while (i >= 0 && /[a-zA-Z0-9_]/.test(text[i])) {
					i--;
				}

				if (i + 1 < identEnd) {
					const subroutineName = text.slice(i + 1, identEnd);
					return { subroutineName, parameterIndex: commaCount };
				}
				break;
			} else {
				parenCount--;
			}
		} else if (ch === ',' && parenCount === 0) {
			commaCount++;
		}

		i--;
	}

	return { parameterIndex: 0 };
}

connection.onDefinition((_params: TextDocumentPositionParams): Definition | undefined => {
	return undefined;
});

documents.listen(connection);
connection.listen();