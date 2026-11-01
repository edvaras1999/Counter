Scriptname Form Hidden
; Arquivo de apoio SO PARA COMPILAR. Nao e distribuido. O jogo usa o script real.

Function RegisterForModEvent(string a_eventName, string a_callbackName) native
Function UnregisterForModEvent(string a_eventName) native
Function RegisterForSingleUpdate(float afInterval) native
Function UnregisterForUpdate() native
Event OnInit()
EndEvent
Event OnUpdate()
EndEvent
; Evento do Guard Counters LE (ModEvent); declarado aqui so para o compilador.
Event OnGuardCountersEvent(string a_eventName, string a_what, float a_value, Form a_sender)
EndEvent
