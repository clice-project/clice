import * as path from 'path';
import { readSetting } from './Setting';
import { workspace, window, ExtensionContext, OutputChannel } from 'vscode';
import { LanguageClient, LanguageClientOptions, ServerOptions, Trace, TransportKind } from 'vscode-languageclient/node';

let client: LanguageClient;

export async function activate(context: ExtensionContext) {
	console.log('Congratulations, your extension "clice" is now active!');

	let channel = window.createOutputChannel('clice');

	let serverPath = "/home/ykiko/Project/C++/clice/build/clice";

	let settings = JSON.stringify(readSetting());

	const serverOptions: ServerOptions = {
		run: { command: serverPath, args: [settings] },
		debug: { command: serverPath, args: [settings] }
	};

	const clientOptions: LanguageClientOptions = {
		documentSelector: [{ scheme: 'file', language: 'cpp' }],
		traceOutputChannel: channel,
		// outputChannel: channel,
		synchronize: {
			fileEvents: workspace.createFileSystemWatcher('**/.clientrc')
		},


	};

	client = new LanguageClient(
		'clice-client',
		'clice-client',
		serverOptions,
		clientOptions
	);

	client.start();
}

export function deactivate(): Thenable<void> | undefined {
	if (!client) {
		return undefined;
	}
	return client.stop();
}