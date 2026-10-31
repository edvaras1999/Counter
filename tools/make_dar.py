#!/usr/bin/env python3
# make_dar.py - converte as pastas OAR do Guard Counters (config.json) para pastas DAR (_conditions.txt)
# que o Themis (OAR/DAR do Skyrim LE) sabe avaliar. A condicao de variavel de grafo
# (GuardCounters_Trigger) e trocada por IsActorValueEqualTo(<atributo>, 1): e o valor de atributo que a
# GuardCountersLE.dll poe a 1 durante a janela de counter.
#
# Uso:  python make_dar.py <pasta OpenAnimationReplacer\GuardCountersMCO> <pasta meshes de saida> [--av 77]
# Ex.:  python make_dar.py "Mod\meshes\actors\character\animations\OpenAnimationReplacer\GuardCountersMCO" "Mod\meshes" --av 77
# O --av tem de ser igual ao SignalActorValue do GuardCounters.ini (77 = Variable10).
import json, os, shutil, sys

KW_WARHAMMER = "0x0006D930"   # WeapTypeWarhammer (Skyrim.esm)
KW_BATTLEAXE = "0x0006D932"   # WeapTypeBattleaxe (Skyrim.esm)

def form(plugin, fid):
    return '"%s"|0x%08X' % (plugin, int(str(fid), 16))

def equipped(c):
    t = int(float(c["Type"]["value"]))
    left = bool(c.get("Left hand"))
    neg = "NOT " if c.get("negated") else ""
    if not left and t == 6:  return neg + 'IsEquippedRightHasKeyword("Skyrim.esm"|%s)' % KW_BATTLEAXE
    if not left and t == 10: return neg + 'IsEquippedRightHasKeyword("Skyrim.esm"|%s)' % KW_WARHAMMER
    return neg + "IsEquipped%sType(%d)" % ("Left" if left else "Right", t)

def terms(conds, av, value):
    """devolve uma lista de grupos; cada grupo e uma lista de termos ligados por OR; os grupos ligam-se por AND"""
    groups = []
    for c in conds:
        n = c["condition"]
        if n == "IsActorBase":
            groups.append(["IsActorBase(%s)" % form(c["Actor base"]["pluginName"], c["Actor base"]["formID"])])
        elif n == "IsEquippedType":
            groups.append([equipped(c)])
        elif n == "CompareValues" and c.get("Value A", {}).get("graphVariable") == "GuardCounters_Trigger":
            groups.insert(0, ["IsActorValueEqualTo(%d, %d)" % (av, value)])
        elif n == "OR":
            groups.append([equipped(x) for x in c["Conditions"]])
        else:
            raise SystemExit("condicao nao suportada: %s" % n)
    return groups

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    av = 77
    if "--av" in sys.argv: av = int(sys.argv[sys.argv.index("--av") + 1]); args = [a for a in args if a != str(av)]
    if len(args) != 2: raise SystemExit(__doc__)
    src, meshes = args
    dest = os.path.join(meshes, "actors", "character", "animations", "DynamicAnimationReplacer", "_CustomConditions")
    for d in sorted(os.listdir(src)):
        cfg = os.path.join(src, d, "config.json")
        if not os.path.isfile(cfg): continue
        c = json.load(open(cfg, encoding="utf-8"))
        out = os.path.join(dest, str(c["priority"]))
        os.makedirs(out, exist_ok=True)
        lines = []
        for g in terms(c["conditions"], av, 1):
            for i, t in enumerate(g):
                last_in_group = i == len(g) - 1
                lines.append(t + ("" if not last_in_group else " AND"))   # dentro do grupo: OR; entre grupos: AND
                if not last_in_group: lines[-1] += " OR"
        lines[-1] = lines[-1][:-4] if lines[-1].endswith(" AND") else lines[-1]
        open(os.path.join(out, "_conditions.txt"), "w", newline="").write("\r\n".join(lines) + "\r\n")
        for f in os.listdir(os.path.join(src, d)):
            if f.lower().endswith(".hkx"): shutil.copy2(os.path.join(src, d, f), os.path.join(out, f))
        print("%-18s -> %s" % (d, os.path.relpath(out, meshes)))

main()
