from pathlib import Path

source = (Path(__file__).resolve().parents[1] / 'src' / 'Cloud.cpp').read_text(encoding='utf-8')
for needle in [
    'if(!Network::connected())',
    'strlcpy(blockedBy,"network"',
    'if(!Engine::epoch())',
    'strlcpy(blockedBy,"clock"',
    'strlcpy(blockedBy,"memory"',
    'o["blocked_by"]',
    'o["request_free_heap"]',
    'lastHTTP=-1;report("connection_lost","HTTPS client could not start")',
]:
    assert needle in source, needle

network = (Path(__file__).resolve().parents[1] / 'src' / 'Network.cpp').read_text(encoding='utf-8')
for needle in [
    'disconnectAt=millis()',
    'int32_t(lost-started)>=500',
    'retry.disconnected(now)',
    'o["retry_in_ms"]',
]:
    assert needle in network, needle
print('PASS: cloud pairing distinguishes Wi-Fi, clock, memory and HTTPS-client gates; Wi-Fi disconnects end active attempts without a 45-second false wait.')
