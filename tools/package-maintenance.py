from pathlib import Path
root=Path(__file__).resolve().parents[1];output=root/'.private/robetix-usb-maintenance.html';output.parent.mkdir(exist_ok=True)
html=(root/'remote/public/recovery.html').read_text(encoding='utf-8').replace('<link rel="stylesheet" href="/recovery.css">','<style>'+(root/'remote/public/recovery.css').read_text(encoding='utf-8')+'</style>').replace('<script src="/recovery.js"></script>','<script>'+(root/'remote/public/recovery.js').read_text(encoding='utf-8')+'</script>')
output.write_text(html,encoding='utf-8');print('Private offline USB maintenance page packaged; contains code only, no device credentials.')
