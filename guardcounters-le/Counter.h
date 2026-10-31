// Counter.h - Guard Counters LE: detetor do deflect do Inpa Sekiro + janela de counter.
#pragma once

void CounterInit();				// le o GuardCounters.ini (chamar quando os dados do jogo carregarem)
void CounterRegisterInput();	// regista o receptor de teclas (chamar em kMessage_InputLoaded)
void CounterReset();			// carregou um save / novo jogo: larga qualquer estado pendente
