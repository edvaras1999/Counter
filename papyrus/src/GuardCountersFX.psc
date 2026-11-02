Scriptname GuardCountersFX extends Quest
{ Guard Counters LE - counter effects. The DLL decides when a counter is ready, used, over, or has hit someone,
  and tells this script through the mod event "GuardCounters_Event"
  (what = ready | swing | expire | end | hit | slow | slowend).
  This script plays the sounds, shows the perilous mark, grants total damage immunity (SetGhost, the method Inpa
  Sekiro uses during a parry), slows time (optional) and adds the extra counter damage. All options come from the DLL. }

Sound sndReady
Sound sndSwing
Spell markSpell
Spell slowSpell
bool ghostOn
bool markShown
bool slowOn
bool active
bool swinging
float activeUntil
int evtSeen

Event OnInit()
    RegisterForModEvent("GuardCounters_Event", "OnGuardCountersEvent")
    Debug.Trace("GuardCountersFX: registered for the mod event GuardCounters_Event")
EndEvent

Event OnGuardCountersEvent(string a_eventName, string a_what, float a_value, Form a_sender)
    Debug.Trace("GuardCountersFX: event '" + a_what + "' received")
    Actor pl = Game.GetPlayer()

    if a_what == "ready"
        active = true
        swinging = false
        activeUntil = Utility.GetCurrentRealTime() + GuardCountersNative.GetFloat("WindowSeconds") + GuardCountersNative.GetFloat("ArmedHoldSeconds") + 2.0
        if GuardCountersNative.GetBool("GlowEnabled")
            ShowMark(pl)
        endIf
        if GuardCountersNative.GetBool("ReadySoundEnabled")
            PlayCounterSound(0, GuardCountersNative.GetInt("ReadySoundVolume"))
        endIf
        ; immunity from the deflect: the counter cannot be interrupted before it starts
        if GuardCountersNative.GetBool("Invulnerability") && GuardCountersNative.GetBool("ImmunityFromDeflect")
            GrantGhost(pl)
        endIf
        RegisterForSingleUpdate(0.15)
    elseIf a_what == "swing"
        active = true
        swinging = true
        activeUntil = Utility.GetCurrentRealTime() + GuardCountersNative.GetFloat("ArmedHoldSeconds") + 2.0
        HideMark(pl)
        if GuardCountersNative.GetBool("SwingSoundEnabled")
            PlayCounterSound(1, GuardCountersNative.GetInt("SwingSoundVolume"))
        endIf
        if GuardCountersNative.GetBool("Invulnerability")
            GrantGhost(pl)
        endIf
        RegisterForSingleUpdate(0.15)
    elseIf a_what == "slow"
        StartSlow(pl)
    elseIf a_what == "slowend"
        StopSlow(pl)
    elseIf a_what == "expire"
        active = false
        HideMark(pl)
        if !swinging
            ReleaseGhost(pl)
        endIf
    elseIf a_what == "end"
        active = false
        swinging = false
        HideMark(pl)
        ReleaseGhost(pl)
        StopSlow(pl)
    elseIf a_what == "hit"
        ApplyExtraDamage(pl, a_sender as Actor, a_value)
    endIf
EndEvent

; While a counter is active this keeps the immunity on (Inpa Sekiro clears SetGhost itself when its parry ends, which
; would cut ours) and is also the safety net: if an "end" event is ever missed, nothing stays on.
Event OnUpdate()
    Actor pl = Game.GetPlayer()
    if active && Utility.GetCurrentRealTime() < activeUntil
        if ghostOn && !pl.IsGhost()
            pl.SetGhost(true)
        endIf
        RegisterForSingleUpdate(0.15)
    else
        active = false
        swinging = false
        HideMark(pl)
        ReleaseGhost(pl)
        StopSlow(pl)
    endIf
EndEvent

Function GrantGhost(Actor pl)
    if !ghostOn || !pl.IsGhost()
        pl.SetGhost(true)
    endIf
    ghostOn = true
    Debug.Trace("GuardCountersFX: immunity on (ghost = " + (pl.IsGhost() as string) + ")")
EndFunction

Function ReleaseGhost(Actor pl)
    if ghostOn
        pl.SetGhost(false)
        ghostOn = false
    endIf
EndFunction

; The mark is the perilous kanji, in our own magic effect (own model and light; nothing from Inpa Sekiro runs).
Function ShowMark(Actor pl)
    if markSpell == None
        markSpell = Game.GetFormFromFile(0x806, "GuardCountersLE.esp") as Spell
    endIf
    if markSpell == None
        Debug.Trace("GuardCountersFX: the mark spell was not found in GuardCountersLE.esp")
        return
    endIf
    markSpell.Cast(pl, pl)
    markShown = true
    Debug.Trace("GuardCountersFX: mark cast on the player")
EndFunction

Function HideMark(Actor pl)
    if markShown && markSpell != None
        pl.DispelSpell(markSpell)
    endIf
    markShown = false
EndFunction

; Optional slow time: the vanilla Slow Time shout spell (Skyrim.esm 00048AD2), cast on the player and dispelled when
; the counter ends or after SlowTimeSeconds, whichever comes first.
Function StartSlow(Actor pl)
    if slowOn
        return
    endIf
    if slowSpell == None
        slowSpell = Game.GetFormFromFile(0x048AD2, "Skyrim.esm") as Spell
    endIf
    if slowSpell == None
        Debug.Trace("GuardCountersFX: the Slow Time spell (Skyrim.esm 00048AD2) was not found")
        return
    endIf
    slowSpell.Cast(pl, pl)
    slowOn = true
EndFunction

Function StopSlow(Actor pl)
    if slowOn && slowSpell != None
        pl.DispelSpell(slowSpell)
    endIf
    slowOn = false
EndFunction

; which: 0 = counter ready, 1 = counter attack. Volume in percent (0-200); above 100 the sound is layered.
Function PlayCounterSound(int which, int volumePercent)
    if volumePercent <= 0
        return
    endIf
    Sound s = None
    if which == 0
        if sndReady == None
            sndReady = Game.GetFormFromFile(0x801, "GuardCountersLE.esp") as Sound
        endIf
        s = sndReady
    else
        if sndSwing == None
            sndSwing = Game.GetFormFromFile(0x802, "GuardCountersLE.esp") as Sound
        endIf
        s = sndSwing
    endIf
    if s == None
        Debug.Trace("GuardCountersFX: sound " + (which as string) + " was not found in GuardCountersLE.esp")
        return
    endIf
    Actor pl = Game.GetPlayer()
    float v = volumePercent as float / 100.0
    if v > 1.0
        v = 1.0
    endIf
    int h = s.Play(pl)
    Debug.Trace("GuardCountersFX: sound " + (which as string) + " played, instance " + (h as string))
    if h != 0
        Sound.SetInstanceVolume(h, v)
    endIf
    if volumePercent > 100
        int h2 = s.Play(pl)
        if h2 != 0
            Sound.SetInstanceVolume(h2, (volumePercent - 100) as float / 100.0)
        endIf
    endIf
EndFunction

; percent = CounterDamagePercent. 100 = normal. Above: extra damage; below: gives part of it back.
; The extra is a share of the weapon's base damage (perks and enchantments are not counted).
Function ApplyExtraDamage(Actor pl, Actor target, float percent)
    if target == None || target.IsDead()
        return
    endIf
    Weapon w = pl.GetEquippedWeapon(false)
    if w == None
        return
    endIf
    float baseDamage = w.GetBaseDamage() as float
    float delta = baseDamage * (percent - 100.0) / 100.0
    if delta > 0.5
        target.DamageActorValue("Health", delta)
    elseIf delta < -0.5
        target.RestoreActorValue("Health", 0.0 - delta)
    endIf
EndFunction
