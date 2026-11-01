// Counter.h - Guard Counters LE: detetor do deflect do Inpa Sekiro + janela de counter + efeitos.
#pragma once

void CounterInit();				// le o GuardCounters.ini (chamar quando os dados do jogo carregarem)
void CounterRegisterInput();	// regista os receptores de teclas e de hits (chamar em kMessage_InputLoaded)
void CounterReset();			// carregou um save / novo jogo: larga qualquer estado pendente
int  CounterStatus(int what);	// 0 = sinal por atributo funciona (1/0), 1 = estado (0 parado, 1 pronto, 2 em curso)
