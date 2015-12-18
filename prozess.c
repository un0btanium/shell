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


Prozess prozessNeu(int pid, int pgid, int status, char *path) {
	Prozess neu = reserviere(sizeof(Prozess));
	neu->pid 	= pid;
	neu->pgid 	= pgid;
	neu->status = status;
	strcpy(neu->path, path);
	return neu;
}

ProzessListe prozessAnfuegen(int pid, int pgid, int status, char* path, ProzessListe prozesse) {


	if (anzahlProzesse(prozesse) == 0) { /* Prozessliste besitzt keine Einträge */
		prozesse = prozessListeNeu(
				prozessNeu(pid, pgid, status, path));
	} else { /* Prozessliste hat bereits Prozess-IDs enthalten */
		prozesse = prozessListeAnfuegen(prozesse,
				prozessNeu(pid, pgid, status, path));
	}

	return prozesse;
}




int anzahlProzesse(ProzessListe liste) {
	return liste==NULL ? 0 : 1+anzahlProzesse(liste->naechster);
}
