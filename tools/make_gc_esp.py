#!/usr/bin/env python3
"""Gera papyrus/GuardCountersLE.esp para o Guard Counters LE (Skyrim LE, form version 43):
  - uma quest (start game enabled) com dois scripts: GuardCountersMCM (menu) e GuardCountersFX (efeitos);
  - 2 descritores de som (SNDR) para os WAV do pacote + 2 marcadores (SOUN) que os embrulham;
  - um efeito magico, um feitico, uma luz e um objeto de arte que mostram a marca "perilous" (kanji) no jogador.
    O modelo e a textura do kanji vem do Inpa Sekiro mas ficam NO NOSSO PACOTE (meshes\\GuardCounters\\kanji_mark.nif e
    textures\\effectsSekiro\\perilnew.dds): este ESP ja nao depende do Inpa Sekiro Combat.esp.

Os registos SNDR, MGEF, SPEL, LIGH e ARTO copiam o formato dos registos do Inpa Sekiro Combat.esp (que ja funcionam no LE).

Mestre unico: 0 = Skyrim.esm  -> os registos novos usam o indice 1 (0x01......).
Formularios: 0x800 quest, 0x801 som "pronto", 0x802 som "golpe", 0x803/0x804 descritores, 0x805 efeito, 0x806 feitico,
0x807 arte (kanji), 0x808 luz.
"""
import struct, sys

def sub(tag, data): return tag + struct.pack('<H', len(data)) + data
def z(s): return s.encode('ascii') + b'\0'
def record(tag, formid, subs, flags=0):
    body = b''.join(subs)
    return tag + struct.pack('<IIIIHH', len(body), flags, formid, 0, 43, 0) + body
def group(label, children):
    body = b''.join(children)
    return b'GRUP' + struct.pack('<I', 24 + len(body)) + label + struct.pack('<IHHI', 0, 0, 0, 43) + body

OWN = 0x01000000
MARK_LIGHT = OWN | 0x808
MARK_ART = OWN | 0x807

# ---- quest com dois scripts
def script(name):
    n = name.encode('ascii')
    return struct.pack('<H', len(n)) + n + b'\0' + struct.pack('<H', 0)
vmad = struct.pack('<HHH', 5, 2, 2) + script('GuardCountersMCM') + script('GuardCountersFX') + b'\x02' + struct.pack('<HHH', 0, 0, 0)
quest = record(b'QUST', OWN | 0x800, [
    sub(b'EDID', z('GuardCountersQuest')),
    sub(b'VMAD', vmad),
    sub(b'FULL', z('Guard Counters')),
    sub(b'DNAM', bytes.fromhex('110100120000000000000000')),
    sub(b'NEXT', b''),
    sub(b'ANAM', struct.pack('<I', 0)),
])

# ---- sons (SNDR copia o SNDR zzzSekiro_perilous_sfx do Inpa; SOUN copia o zzzSekiro_perilous_mk)
def sndr(formid, edid, wav):
    return record(b'SNDR', formid, [
        sub(b'EDID', z(edid)),
        sub(b'CNAM', bytes.fromhex('0a54ef1e')),
        sub(b'GNAM', bytes.fromhex('a1720100')),
        sub(b'ANAM', z('data\\Sound\\fx\\GuardCounters\\' + wav)),
        sub(b'ONAM', bytes.fromhex('caed0700')),
        sub(b'LNAM', bytes.fromhex('00000000')),
        sub(b'BNAM', bytes.fromhex('000a0100f503')),
    ])
def soun(formid, edid, sndr_id):
    return record(b'SOUN', formid, [
        sub(b'EDID', z(edid)),
        sub(b'OBND', struct.pack('<6h', 0, 0, 0, 0, 0, 0)),
        sub(b'SDSC', struct.pack('<I', sndr_id)),
    ])
sndrs  = [sndr(OWN | 0x803, 'GC_CounterReady_sfx', 'CounterReady.wav'), sndr(OWN | 0x804, 'GC_CounterSwing_sfx', 'CounterSwing.wav')]
sounds = [soun(OWN | 0x801, 'GC_CounterReady', OWN | 0x803), soun(OWN | 0x802, 'GC_CounterSwing', OWN | 0x804)]

# ---- marca perilous: efeito magico (copia o zzzSekiro_perilous_kanji_mgef do Inpa, sem script nem keyword) e feitico
data = bytearray(bytes.fromhex(
    '029802040000000000000000ffffffffffffffff00000000917d080500000000000000000000000000000000000000000000000000000000000000000000000001000000ffffffff00000000000000000100000001000000ffffffff0000000030b623050000000000000000000000000000803f000000000000000000000000000000000000000000000000010000000000000000000000'))
assert len(data) == 152, len(data)
struct.pack_into('<I', data, 24, MARK_LIGHT)
struct.pack_into('<I', data, 96, MARK_ART)
mgef = record(b'MGEF', OWN | 0x805, [
    sub(b'EDID', z('GC_CounterMark_mgef')),
    sub(b'FULL', z('Guard Counter mark')),
    sub(b'MDOB', bytes.fromhex('a5350400')),
    sub(b'DATA', bytes(data)),
    sub(b'SNDD', b''),
    sub(b'DNAM', b'\0'),
])
spell = record(b'SPEL', OWN | 0x806, [
    sub(b'EDID', z('GC_CounterMark_spell')),
    sub(b'OBND', struct.pack('<6h', 0, 0, 0, 0, 0, 0)),
    sub(b'FULL', z('Guard Counter mark')),
    sub(b'MDOB', bytes.fromhex('a5350400')),
    sub(b'ETYP', bytes.fromhex('443f0100')),
    sub(b'DESC', b'\0'),
    sub(b'SPIT', bytes.fromhex('000000000000380000000000000000000100000001000000000000000000000000000000')),
    sub(b'EFID', struct.pack('<I', OWN | 0x805)),
    sub(b'EFIT', struct.pack('<fII', 0.0, 0, 10)),      # magnitude 0, area 0, 10 s (o script dissipa-o antes)
])

# ---- arte (o kanji) e luz: copias dos registos zzzSekiro_peril_ARTO / zzzSekiro_perilous_light do Inpa, sem scripts
arto = record(b'ARTO', OWN | 0x807, [
    sub(b'EDID', z('GC_CounterMark_ARTO')),
    sub(b'OBND', struct.pack('<6h', 0, 0, 0, 0, 0, 0)),
    sub(b'MODL', z('GuardCounters\\kanji_mark.nif')),
    sub(b'DNAM', struct.pack('<I', 1)),
])
ligh = record(b'LIGH', OWN | 0x808, [
    sub(b'EDID', z('GC_CounterMark_light')),
    sub(b'OBND', struct.pack('<6h', 0, 0, 0, 0, 0, 0)),
    sub(b'DATA', bytes.fromhex('ffffffff80000000d2000000010000000000803f0000b4420000803f0000803f00000000000000000000000000000000')),
    sub(b'FNAM', struct.pack('<f', 1.0)),
])

groups = [group(b'QUST', [quest]), group(b'SOUN', sounds), group(b'SNDR', sndrs), group(b'MGEF', [mgef]), group(b'SPEL', [spell]), group(b'ARTO', [arto]), group(b'LIGH', [ligh])]
n_records = len(groups) + 1 + len(sounds) + len(sndrs) + 4

tes4 = record(b'TES4', 0, [
    sub(b'HEDR', struct.pack('<fII', 0.94, n_records, 0x807)),
    sub(b'CNAM', z('GuardCountersLE')),
    sub(b'SNAM', z('Guard Counters LE - in-game menu, sounds and counter-ready mark (no Inpa master)')),
    sub(b'MAST', z('Skyrim.esm')), sub(b'DATA', b'\0' * 8),
    sub(b'INTV', struct.pack('<I', 1)),
])
out = sys.argv[1] if len(sys.argv) > 1 else 'GuardCountersLE.esp'
open(out, 'wb').write(tes4 + b''.join(groups))
print('escrito', out)
