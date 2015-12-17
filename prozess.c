#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "prozess.h"


ProzessListe prozessListeNeu(Prozess prozess) {
	ProzessListe neu = reserviere(sizeof(struct prozessListe));
	neu->prozess 	= prozess;
	neu->naechster 	= NULL;
	return neu;
}

ProzessListe prozessListeAnfuegen(ProzessListe liste, Prozess prozess) {
	ProzessListe neu = reserviere(sizeof(struct prozessListe));
	neu->prozess 	= prozess;
	neu->naechster 	= liste;
	return neu;
}


Prozess prozessNeu(int pid, int pgid, int status, char* path) {
	Prozess neu = reserviere(sizeof(struct prozess));
	neu->pid 	= pid;
	neu->pgid 	= pgid;
	neu->status = status;
	neu->path	= path;
	return neu;
}


void prozessLoeschen(ProzessListe liste, int pid) {
	ProzessListe aktuellerListenEintrag;
	ProzessListe vorherigerListenEintrag = NULL;
	Prozess aktuellerProzess;
	Prozess vorherigerProzess = NULL;

	if (liste == NULL)
		return;

	for (aktuellerListenEintrag = liste; aktuellerListenEintrag != NULL; vorherigerListenEintrag = aktuellerListenEintrag, aktuellerListenEintrag = aktuellerListenEintrag->naechster) {
		aktuellerProzess = aktuellerListenEintrag->prozess;
		if (vorherigerListenEintrag != NULL)
			vorherigerProzess = vorherigerListenEintrag->prozess;

		if (aktuellerProzess->pid == pid) {												/* wenn gesuchter Prozess gefunden wurde */
			if (vorherigerProzess == NULL)												/* wenn gefundener Prozess das erste Element in der Liste ist */
				liste = aktuellerListenEintrag->naechster;								/* dann überspringe in der liste das erste Element und fängt beim zweiten Eintrag an */
			else																		/* wenn gefundener Prozess nicht das erste Element ist */
				vorherigerListenEintrag->naechster = aktuellerListenEintrag->naechster;	/* dann überspringe den aktuellen Eintrag */
		/* freigabe(aktuellerListenEintrag->prozess); */
		/* freigabe(aktuellerListenEintrag); */
		}
	}
}


int anzahlProzesse(ProzessListe liste) {
	return liste==NULL ? 0 : 1+anzahlProzesse(liste->naechster);
}



