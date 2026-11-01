Scriptname GuardCountersFX extends Quest
{ Guard Counters LE - counter effects. The DLL decides when a counter is ready, used, over, or has hit someone,
  and tells this script through the mod event "GuardCounters_Event" (what = ready | swing | expire | end | hit).
  This script plays the sounds, shows the Inpa Sekiro perilous mark, grants total damage immunity (SetGhost,
  the method Inpa Sekiro uses during a parry) and adds the extra counter damage. All options come from the DLL. }

Sound sndReady
Sound sndSwing
Spell markSpell
bool ghostOn
bool markShown
int evtSeen

Event OnInit()
    RegisterForModEvent("GuardCounters_Event", "OnGuardCountersEvent")
    Debug.Trace("GuardCountersFX: registered for the mod event GuardCounters_Event")
EndEvent

Event OnGuardCountersEvent(string a_eventName, string a_what, float a_value, Form a_sender)
    if evtSeen < 4
        evtSeen += 1
        Debug.Trace("GuardCountersFX: event '" + a_what + "' received")
    endIf
    Actor pl = Game.GetPlayer()

    if a_what == "ready"
        if GuardCountersNative.GetBool("GlowEnabled")
            ShowMark(pl)
        endIf
        if GuardCountersNative.GetBool("ReadySoundEnabled")
            PlayCounterSound(0, GuardCountersNative.GetInt("ReadySoundVolume"))
        endIf
        RegisterForSingleUpdate(GuardCountersNative.GetFloat("WindowSeconds") + 1.0)
    elseIf a_what == "swing"
        HideMark(pl)
        if GuardCountersNative.GetBool("SwingSoundEnabled")
            PlayCounterSound(1, GuardCountersNative.GetInt("SwingSoundVolume"))
        endIf
        if GuardCountersNative.GetBool("Invulnerability")
            pl.SetGhost(true)
            ghostOn = true
        endIf
        RegisterForSingleUpdate(GuardCountersNative.GetFloat("ArmedHoldSeconds") + 1.5)
    elseIf a_what == "expire"
        HideMark(pl)
    elseIf a_what == "end"
        HideMark(pl)
        ReleaseGhost(pl)
    elseIf a_what == "hit"
        ApplyExtraDamage(pl, a_sender as Actor, a_value)
    endIf
EndEvent

; Safety net: if an "end" event is ever missed, nothing stays on (no stuck immunity, no stuck mark).
Event OnUpdate()
    Actor pl = Game.GetPlayer()
    HideMark(pl)
    ReleaseGhost(pl)
EndEvent

Function ReleaseGhost(Actor pl)
    if ghostOn
        pl.SetGhost(false)
        ghostOn = false
    endIf
EndFunction

; The mark is the perilous kanji of Inpa Sekiro, in our own magic effect (no Inpa script runs).
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
EndFunction

Function HideMark(Actor pl)
    if markShown && markSpell != None
        pl.DispelSpell(markSpell)
    endIf
    markShown = false
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
