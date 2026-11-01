#!/usr/bin/env python3
"""Gera papyrus/GuardCountersLE.esp para o Guard Counters LE (Skyrim LE, form version 43):
  - uma quest (start game enabled) com dois scripts: GuardCountersMCM (menu) e GuardCountersFX (efeitos);
  - 2 descritores de som (SNDR) para os WAV do pacote + 2 marcadores (SOUN) que os embrulham;
  - um efeito magico e um feitico que mostram a marca "perilous" (kanji) do Inpa Sekiro no jogador.

Os registos SNDR, MGEF e SPEL copiam o formato dos registos do Inpa Sekiro Combat.esp (que ja funcionam no LE).
O efeito magico NAO leva o script do Inpa: usa so a luz e o modelo do kanji dele (por isso o Inpa e mestre).

Mestres:  0 = Skyrim.esm   1 = Inpa Sekiro Combat.esp   -> os registos novos usam o indice 2 (0x02......).
Formularios: 0x800 quest, 0x801 som "pronto", 0x802 som "golpe", 0x803/0x804 descritores, 0x805 efeito, 0x806 feitico.
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

INPA_LIGHT = 0x01087D91      # zzzSekiro_perilous_light (Inpa, mestre 1)
INPA_KANJI = 0x0123B630      # zzzSekiro_peril_ARTO (modelo inpa_sekiro\kanji_peril.nif)

# ---- quest com dois scripts
def script(name):
    n = name.encode('ascii')
    return struct.pack('<H', len(n)) + n + b'\0' + struct.pack('<H', 0)
vmad = struct.pack('<HHH', 5, 2, 2) + script('GuardCountersMCM') + script('GuardCountersFX') + b'\x02' + struct.pack('<HHH', 0, 0, 0)
quest = record(b'QUST', 0x02000800, [
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
sndrs  = [sndr(0x02000803, 'GC_CounterReady_sfx', 'CounterReady.wav'), sndr(0x02000804, 'GC_CounterSwing_sfx', 'CounterSwing.wav')]
sounds = [soun(0x02000801, 'GC_CounterReady', 0x02000803), soun(0x02000802, 'GC_CounterSwing', 0x02000804)]

# ---- marca perilous: efeito magico (copia o zzzSekiro_perilous_kanji_mgef do Inpa, sem script nem keyword) e feitico
data = bytearray(bytes.fromhex(
    '029802040000000000000000ffffffffffffffff00000000917d080500000000000000000000000000000000000000000000000000000000000000000000000001000000ffffffff00000000000000000100000001000000ffffffff0000000030b623050000000000000000000000000000803f000000000000000000000000000000000000000000000000010000000000000000000000'))
assert len(data) == 152, len(data)
struct.pack_into('<I', data, 24, INPA_LIGHT)
struct.pack_into('<I', data, 96, INPA_KANJI)
mgef = record(b'MGEF', 0x02000805, [
    sub(b'EDID', z('GC_CounterMark_mgef')),
    sub(b'FULL', z('Guard Counter mark')),
    sub(b'MDOB', bytes.fromhex('a5350400')),
    sub(b'DATA', bytes(data)),
    sub(b'SNDD', b''),
    sub(b'DNAM', b'\0'),
])
spell = record(b'SPEL', 0x02000806, [
    sub(b'EDID', z('GC_CounterMark_spell')),
    sub(b'OBND', struct.pack('<6h', 0, 0, 0, 0, 0, 0)),
    sub(b'FULL', z('Guard Counter mark')),
    sub(b'MDOB', bytes.fromhex('a5350400')),
    sub(b'ETYP', bytes.fromhex('443f0100')),
    sub(b'DESC', b'\0'),
    sub(b'SPIT', bytes.fromhex('000000000000380000000000000000000100000001000000000000000000000000000000')),
    sub(b'EFID', struct.pack('<I', 0x02000805)),
    sub(b'EFIT', struct.pack('<fII', 0.0, 0, 10)),      # magnitude 0, area 0, 10 s (o script dissipa-o antes)
])

groups = [group(b'QUST', [quest]), group(b'SOUN', sounds), group(b'SNDR', sndrs), group(b'MGEF', [mgef]), group(b'SPEL', [spell])]
n_records = len(groups) + 1 + len(sounds) + len(sndrs) + 2

tes4 = record(b'TES4', 0, [
    sub(b'HEDR', struct.pack('<fII', 0.94, n_records, 0x807)),
    sub(b'CNAM', z('GuardCountersLE')),
    sub(b'SNAM', z('Guard Counters LE - in-game menu, sounds and counter-ready mark')),
    sub(b'MAST', z('Skyrim.esm')), sub(b'DATA', b'\0' * 8),
    sub(b'MAST', z('Inpa Sekiro Combat.esp')), sub(b'DATA', b'\0' * 8),
    sub(b'INTV', struct.pack('<I', 1)),
])
out = sys.argv[1] if len(sys.argv) > 1 else 'GuardCountersLE.esp'
open(out, 'wb').write(tes4 + b''.join(groups))
print('escrito', out)
