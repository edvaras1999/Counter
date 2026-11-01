Scriptname Actor extends ObjectReference Hidden
; Arquivo de apoio SO PARA COMPILAR. Nao e distribuido. O jogo usa o script real.

Function SetGhost(bool abIsGhost) native
Weapon Function GetEquippedWeapon(bool abLeftHand = false) native
Function DamageActorValue(string asValueName, float afDamage) native
Function RestoreActorValue(string asValueName, float afAmount) native
bool Function IsDead() native
bool Function DispelSpell(Spell akSpell) native
bool Function IsGhost() native
