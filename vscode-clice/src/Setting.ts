import * as vscode from 'vscode';

interface Setting {
    compile_commands_dir: string;
}

export function readSetting(): Setting {
    const config = vscode.workspace.getConfiguration('clice');
    return {
        compile_commands_dir: config.get('compile_commands_dir') || ''
    };
}