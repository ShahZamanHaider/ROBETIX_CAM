// Remote-only generation; does not touch ESP32 sources or require a firmware upload.
import fs from 'node:fs';import {fileURLToPath} from 'node:url';import path from 'node:path';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const dictionary={...JSON.parse(fs.readFileSync(path.join(root,'data/ur.json'),'utf8')),...JSON.parse(fs.readFileSync(path.join(root,'remote/public/auth-ur.json'),'utf8'))};
const source=fs.readFileSync(path.join(root,'data/i18n.js'),'utf8').replace('/* @translations */',JSON.stringify(dictionary));fs.writeFileSync(path.join(root,'remote/public/i18n.js'),source);
