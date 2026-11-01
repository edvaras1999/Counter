Scriptname GuardCountersMCM extends SKI_ConfigBase
{ Guard Counters LE - in-game menu (SkyUI MCM). Nothing is stored in this script: every option reads and
  writes straight to the Guard Counters DLL, which saves GuardCounters.ini. The effects (sounds, mark,
  immunity, extra damage) are in GuardCountersFX. }

; ---- table of the options shown on the current page (rebuilt on every page reset)
string[] okey
int[] oid
int[] okind     ; 1 = toggle, 2 = slider, 3 = menu, 5 = button, 6 = status
int[] oisint    ; slider: 1 = integer setting, 0 = float setting
int[] omenu     ; menu: which list of choices (see MenuChoices)
float[] omin
float[] omax
float[] ostep
float[] odef
float[] oscale  ; shown value = real value * scale (e.g. 0..1 shown as percent)
string[] ofmt
string[] oinfo
int ocount
int lang        ; 0 = English, 1 = Portugues

int Function GetVersion()
    return 1
EndFunction

Event OnConfigInit()
    Debug.Trace("GuardCountersMCM: OnConfigInit - the menu script is running")
    ModName = "Guard Counters"
    InitTables()
    SetPageNames()
EndEvent

Event OnVersionUpdate(int a_version)
    InitTables()
    SetPageNames()
EndEvent

Event OnConfigOpen()
    InitTables()
    SetPageNames()
EndEvent

; Page names follow the chosen language (they refresh the next time the menu is opened).
Function SetPageNames()
    lang = GuardCountersNative.GetInt("Language")
    Pages = new string[4]
    Pages[0] = T("General", "Geral")
    Pages[1] = T("Counter", "Contra-ataque")
    Pages[2] = T("Effects", "Efeitos")
    Pages[3] = T("Combat", "Combate")
EndFunction

string Function ChoiceText(int menuId, int index)
    if menuId == 1
        if index == 1
            return "Portugues"
        endIf
        return "English"
    endIf
    return ""
EndFunction

; ---- language helper and rows (same engine as the FreeClimb menu)
string Function T(string en, string pt)
    if lang == 1
        return pt
    endIf
    return en
EndFunction

string[] Function MenuChoices(int menuId)
    string[] c = new string[2]
    c[0] = ChoiceText(menuId, 0)
    c[1] = ChoiceText(menuId, 1)
    return c
EndFunction

Function AddToggleRow(string key, string label, float def, string info)
    if ocount >= 64
        return
    endIf
    int i = ocount
    ocount += 1
    okey[i] = key
    okind[i] = 1
    odef[i] = def
    oinfo[i] = info
    oid[i] = AddToggleOption(label, GuardCountersNative.GetBool(key))
EndFunction

Function AddSliderRow(string key, string label, float minV, float maxV, float stepV, float def, float scale, int isInt, string fmt, string info)
    if ocount >= 64
        return
    endIf
    int i = ocount
    ocount += 1
    okey[i] = key
    okind[i] = 2
    oisint[i] = isInt
    omin[i] = minV
    omax[i] = maxV
    ostep[i] = stepV
    odef[i] = def
    oscale[i] = scale
    ofmt[i] = fmt
    oinfo[i] = info
    oid[i] = AddSliderOption(label, ShownValue(i), fmt)
EndFunction

Function AddMenuRow(string key, string label, int menuId, float def, string info)
    if ocount >= 64
        return
    endIf
    int i = ocount
    ocount += 1
    okey[i] = key
    okind[i] = 3
    omenu[i] = menuId
    odef[i] = def
    oinfo[i] = info
    oid[i] = AddMenuOption(label, ChoiceText(menuId, GuardCountersNative.GetInt(key)))
EndFunction

Function AddButtonRow(string key, string label, string btn, string info)
    if ocount >= 64
        return
    endIf
    int i = ocount
    ocount += 1
    okey[i] = key
    okind[i] = 5
    oinfo[i] = info
    oid[i] = AddTextOption(label, btn)
EndFunction

Function AddStatusRow(string label, string value, string info)
    if ocount >= 64
        return
    endIf
    int i = ocount
    ocount += 1
    okey[i] = ""
    okind[i] = 6
    oinfo[i] = info
    oid[i] = AddTextOption(label, value, OPTION_FLAG_DISABLED)
EndFunction

float Function ShownValue(int i)
    float v
    if oisint[i] == 1
        v = GuardCountersNative.GetInt(okey[i]) as float
    else
        v = GuardCountersNative.GetFloat(okey[i])
    endIf
    return v * oscale[i]
EndFunction

int Function FindRow(int a_option)
    int i = 0
    while i < ocount
        if oid[i] == a_option
            return i
        endIf
        i += 1
    endWhile
    return -1
EndFunction

Function StoreSlider(int i, float shown)
    float raw = shown / oscale[i]
    if oisint[i] == 1
        GuardCountersNative.SetInt(okey[i], (raw + 0.5) as int)
    else
        GuardCountersNative.SetFloat(okey[i], raw)
    endIf
EndFunction

string Function YesNo(int v)
    if v != 0
        return T("yes", "sim")
    endIf
    return T("no", "nao")
EndFunction

Function InitTables()
    okey = new string[64]
    oid = new int[64]
    okind = new int[64]
    oisint = new int[64]
    omenu = new int[64]
    omin = new float[64]
    omax = new float[64]
    ostep = new float[64]
    odef = new float[64]
    oscale = new float[64]
    ofmt = new string[64]
    oinfo = new string[64]
    ocount = 0
EndFunction

; ---- pages
Event OnPageReset(string a_page)
    InitTables()
    lang = GuardCountersNative.GetInt("Language")

    int p = 0
    if a_page != ""
        p = Pages.Find(a_page)
        if p < 0
            p = 0
        endIf
    endIf

    if p == 0
        PageGeneral()
    elseIf p == 1
        PageCounter()
    elseIf p == 2
        PageEffects()
    else
        PageCombat()
    endIf
EndEvent

Function PageGeneral()
    AddHeaderOption("Guard Counters LE")
    AddEmptyOption()
    AddMenuRow("Language", T("Menu language", "Idioma do menu"), 1, 0.0, T("Language of this menu. Page names update the next time you open the menu.", "Idioma deste menu. Os nomes das paginas mudam na proxima vez que abrir o menu."))
    AddEmptyOption()
    AddHeaderOption(T("Status", "Estado"))
    AddEmptyOption()
    AddStatusRow(T("Counter signal working", "Sinal do counter a funcionar"), YesNo(GuardCountersNative.Status(0)), T("If this says no, the DLL could not write its signal and the counter animations will not play. See GuardCountersLE.log.", "Se disser nao, a DLL nao conseguiu escrever o sinal e as animacoes de counter nao tocam. Veja o GuardCountersLE.log."))
    AddEmptyOption()
    AddHeaderOption(T("Settings file", "Ficheiro de configuracao"))
    AddEmptyOption()
    AddButtonRow("@save", T("Save my profile", "Guardar o meu perfil"), T("[Save]", "[Guardar]"), T("Saves all current options as your own profile, to load again later.", "Guarda todas as opcoes atuais como o seu perfil, para carregar mais tarde."))
    AddButtonRow("@load", T("Load my profile", "Carregar o meu perfil"), T("[Load]", "[Carregar]"), T("Replaces all current options with the ones you saved.", "Substitui todas as opcoes atuais pelas que guardou."))
    AddButtonRow("@reset", T("Reset to defaults", "Repor valores de fabrica"), T("[Reset]", "[Repor]"), T("Restores all options to the factory values.", "Repoe todas as opcoes para os valores de fabrica."))
    AddEmptyOption()
    AddHeaderOption(T("Diagnostics", "Diagnostico"))
    AddEmptyOption()
    AddToggleRow("Verbose", T("Detailed log", "Registo detalhado"), 1.0, T("Writes extra detail to GuardCountersLE.log. Makes the log bigger; keep it on while testing.", "Escreve mais detalhe no GuardCountersLE.log. O log fica maior; deixe ligado enquanto testa."))
EndFunction

Function PageCounter()
    AddHeaderOption(T("Counter window", "Janela do contra-ataque"))
    AddEmptyOption()
    AddSliderRow("WindowSeconds", T("Time to counter", "Tempo para contra-atacar"), 0.5, 5.0, 0.1, 2.5, 1.0, 0, "{1} s", T("How long after a deflect you can still attack to counter.", "Quanto tempo depois de um deflect ainda pode atacar para contra-atacar."))
    AddSliderRow("CooldownSeconds", T("Cooldown", "Pausa entre counters"), 0.0, 30.0, 0.5, 0.0, 1.0, 0, "{1} s", T("Wait after a counter before the next deflect can arm another one. 0 = none.", "Espera depois de um counter antes de o proximo deflect armar outro. 0 = nenhuma."))
    AddSliderRow("ArmedHoldSeconds", T("Longest counter attack", "Golpe de counter mais longo"), 0.5, 5.0, 0.1, 3.0, 1.0, 0, "{1} s", T("Safety limit: the counter effects end after this long even if the attack seems to keep running.", "Limite de seguranca: os efeitos do counter acabam passado este tempo mesmo que o golpe pareca continuar."))
    AddEmptyOption()
    AddHeaderOption(T("Deflect detection (Inpa Sekiro)", "Deteccao do deflect (Inpa Sekiro)"))
    AddEmptyOption()
    AddToggleRow("RequireBlocking", T("Must have been blocking", "Tinha de estar a bloquear"), 1.0, T("Only counts as a deflect if you were blocking just before. Avoids false triggers from other mods.", "So conta como deflect se estava a bloquear ha pouco. Evita ativacoes falsas de outros mods."))
    AddSliderRow("BlockGraceSeconds", T("Blocking grace", "Tolerancia do bloqueio"), 0.1, 5.0, 0.1, 1.5, 1.0, 0, "{1} s", T("How long ago you may have been blocking for a deflect to count.", "Ha quanto tempo pode ter estado a bloquear para o deflect contar."))
    AddToggleRow("RequireWeaponDrawn", T("Weapon drawn", "Arma desembainhada"), 1.0, T("Only arms the counter with your weapon drawn.", "So arma o counter com a arma desembainhada."))
    AddEmptyOption()
    AddHeaderOption(T("Stamina", "Estamina"))
    AddEmptyOption()
    AddToggleRow("StaminaCostEnabled", T("Counter costs stamina", "O counter gasta estamina"), 1.0, T("Consumes stamina when the counter attack starts.", "Gasta estamina quando o golpe de counter comeca."))
    AddSliderRow("StaminaCost", T("Stamina cost", "Custo de estamina"), 0.0, 100.0, 1.0, 20.0, 1.0, 0, "{0}", T("Flat stamina cost. Your stamina will not fall below 0.", "Custo fixo de estamina. A sua estamina nao desce de 0."))
EndFunction

Function PageEffects()
    AddHeaderOption(T("Counter-ready mark", "Marca de counter pronto"))
    AddEmptyOption()
    AddToggleRow("GlowEnabled", T("Show the mark", "Mostrar a marca"), 1.0, T("Shows the Inpa Sekiro perilous mark on you while a counter is ready.", "Mostra a marca perilous do Inpa Sekiro enquanto um counter esta pronto."))
    AddEmptyOption()
    AddHeaderOption(T("Sounds", "Sons"))
    AddEmptyOption()
    AddToggleRow("ReadySoundEnabled", T("Counter-ready sound", "Som de counter pronto"), 1.0, T("Plays when a deflect arms the counter.", "Toca quando um deflect arma o counter."))
    AddSliderRow("ReadySoundVolume", T("Ready sound volume", "Volume do som de pronto"), 0.0, 200.0, 5.0, 100.0, 1.0, 1, "{0}%", T("Above 100% the sound is layered to be louder.", "Acima de 100% o som e sobreposto para ficar mais alto."))
    AddToggleRow("SwingSoundEnabled", T("Counter-attack sound", "Som do golpe de counter"), 1.0, T("Plays when the counter attack starts.", "Toca quando o golpe de counter comeca."))
    AddSliderRow("SwingSoundVolume", T("Counter-attack volume", "Volume do golpe de counter"), 0.0, 200.0, 5.0, 100.0, 1.0, 1, "{0}%", T("Above 100% the sound is layered to be louder.", "Acima de 100% o som e sobreposto para ficar mais alto."))
EndFunction

Function PageCombat()
    AddHeaderOption(T("Protection during the counter attack", "Protecao durante o golpe de counter"))
    AddEmptyOption()
    AddToggleRow("Invulnerability", T("Total damage immunity", "Imunidade total ao dano"), 1.0, T("You cannot be damaged while the counter attack runs (the same method Inpa Sekiro uses during a parry).", "Nao leva dano enquanto o golpe de counter decorre (o mesmo metodo que o Inpa Sekiro usa no parry)."))
    AddToggleRow("SuperArmor", T("Super armor", "Super armadura"), 1.0, T("You are not staggered while the counter attack runs.", "Nao leva stagger enquanto o golpe de counter decorre."))
    AddEmptyOption()
    AddHeaderOption(T("Effect on the enemy", "Efeito no inimigo"))
    AddEmptyOption()
    AddToggleRow("ForcedStagger", T("Forced stagger", "Stagger forcado"), 1.0, T("Staggers each enemy the counter attack hits.", "Faz cambalear cada inimigo atingido pelo golpe de counter."))
    AddSliderRow("StaggerMagnitude", T("Stagger strength", "Forca do stagger"), 0.2, 1.0, 0.05, 1.0, 1.0, 0, "{2}", T("0.20 small, 0.50 medium, 0.75 large, 1.00 maximum.", "0.20 pequeno, 0.50 medio, 0.75 grande, 1.00 maximo."))
    AddSliderRow("CounterDamagePercent", T("Counter damage", "Dano do counter"), 0.0, 300.0, 5.0, 100.0, 1.0, 1, "{0}%", T("100% = normal damage. Above: extra damage is added to the enemy. Below: part of the damage is given back (approximate, based on the weapon's base damage).", "100% = dano normal. Acima: soma dano extra ao inimigo. Abaixo: devolve parte do dano (aproximado, a partir do dano base da arma)."))
EndFunction

; ---- events
Event OnOptionSelect(int a_option)
    int i = FindRow(a_option)
    if i < 0
        return
    endIf
    if okind[i] == 1
        bool v = !GuardCountersNative.GetBool(okey[i])
        GuardCountersNative.SetBool(okey[i], v)
        SetToggleOptionValue(a_option, v)
    elseIf okind[i] == 5
        if okey[i] == "@save"
            GuardCountersNative.SaveProfile()
            SetTextOptionValue(a_option, T("[Saved]", "[Guardado]"))
        elseIf okey[i] == "@load"
            if GuardCountersNative.LoadProfile() == 1
                ForcePageReset()
            else
                ShowMessage(T("No saved profile found. Use 'Save my profile' first.", "Nenhum perfil guardado. Use primeiro 'Guardar o meu perfil'."), false)
            endIf
        elseIf okey[i] == "@reset"
            if ShowMessage(T("Restore all Guard Counters options to the factory values?", "Repor todas as opcoes do Guard Counters para os valores de fabrica?"), true)
                GuardCountersNative.ResetDefaults()
                ForcePageReset()
            endIf
        endIf
    endIf
EndEvent

Event OnOptionDefault(int a_option)
    int i = FindRow(a_option)
    if i < 0
        return
    endIf
    if okind[i] == 1
        bool v = odef[i] != 0.0
        GuardCountersNative.SetBool(okey[i], v)
        SetToggleOptionValue(a_option, v)
    elseIf okind[i] == 2
        StoreSlider(i, odef[i] * oscale[i])
        SetSliderOptionValue(a_option, ShownValue(i), ofmt[i])
    elseIf okind[i] == 3
        GuardCountersNative.SetInt(okey[i], odef[i] as int)
        SetMenuOptionValue(a_option, ChoiceText(omenu[i], odef[i] as int))
    endIf
EndEvent

Event OnOptionSliderOpen(int a_option)
    int i = FindRow(a_option)
    if i < 0
        return
    endIf
    SetSliderDialogStartValue(ShownValue(i))
    SetSliderDialogDefaultValue(odef[i] * oscale[i])
    SetSliderDialogRange(omin[i] * oscale[i], omax[i] * oscale[i])
    SetSliderDialogInterval(ostep[i] * oscale[i])
EndEvent

Event OnOptionSliderAccept(int a_option, float a_value)
    int i = FindRow(a_option)
    if i < 0
        return
    endIf
    StoreSlider(i, a_value)
    SetSliderOptionValue(a_option, ShownValue(i), ofmt[i])
EndEvent

Event OnOptionMenuOpen(int a_option)
    int i = FindRow(a_option)
    if i < 0
        return
    endIf
    SetMenuDialogOptions(MenuChoices(omenu[i]))
    SetMenuDialogStartIndex(GuardCountersNative.GetInt(okey[i]))
    SetMenuDialogDefaultIndex(odef[i] as int)
EndEvent

Event OnOptionMenuAccept(int a_option, int a_index)
    int i = FindRow(a_option)
    if i < 0
        return
    endIf
    GuardCountersNative.SetInt(okey[i], a_index)
    if okey[i] == "Language"
        lang = a_index
        ForcePageReset()
    else
        SetMenuOptionValue(a_option, ChoiceText(omenu[i], a_index))
    endIf
EndEvent

Event OnOptionHighlight(int a_option)
    int i = FindRow(a_option)
    if i >= 0
        SetInfoText(oinfo[i])
    endIf
EndEvent
