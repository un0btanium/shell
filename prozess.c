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


int anzahlProzesse(ProzessListe liste) {
	return liste==NULL ? 0 : 1+anzahlProzesse(liste->naechster);
}



