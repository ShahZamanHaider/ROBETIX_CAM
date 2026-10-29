"""Both embedded firmware and the development proxy must identify the same UI."""
from pathlib import Path
import json, subprocess
from firmware_build_info import metadata

root = Path(__file__).resolve().parents[1]
actual = metadata(root)
script = "import {sourceMetadata} from './tools/firmware-source.mjs'; console.log(JSON.stringify(sourceMetadata(process.cwd())))"
proxy = json.loads(subprocess.check_output(['node', '--input-type=module', '-e', script], cwd=root, text=True))
assert proxy['source'] == actual['source']
assert proxy['version'] == actual['version']
assert proxy['local_protocol'] == actual['local_protocol'] == 2
assert len(actual['source']) == 64
print('PASS: firmware and localhost proxy share deterministic version/source identity.')
