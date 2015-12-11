#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "utils.h"
#include "listen.h"
#include "wortspeicher.h"
#include "kommandos.h"
#include "frontend.h"
#include "parser.h"
#include "variablen.h"

int interpretiere(Kommando k, int forkexec);

void do_execvp(int argc, char **args) {
	execvp(*args, args);
	perror("exec-Fehler");
	fprintf(stderr, "bei Aufruf von \"%s\"\n", *args);
	exit(1);
}

int interpretiere_pipeline(Kommando k) {
	/* NOT IMPLEMENTED */
	fputs("NOT IMPLEMENTED", stderr);
	return 0;
}

int umlenkungen(Kommando k) {
	int file, i;
	Liste umlenkungen = k->u.einfach.umlenkungen;
	int anzahl = listeLaenge(umlenkungen);
	Umlenkung umlenkung;

	if (anzahl == 0) {
		return 0;
	}

	for (i = 0; i < anzahl; i++) {
		umlenkung = *(Umlenkung*) listeKopf(umlenkungen);

		switch (umlenkung.modus) {
		case READ:
			if ((file = open(umlenkung.pfad, O_RDONLY)) == -1)
				abbruch("Fehler READ %s\n", umlenkung.pfad);
			break;
		case WRITE:
			if ((file = open(umlenkung.pfad, O_RDWR | O_TRUNC | O_CREAT)) == -1)
				abbruch("Fehler WRITE %s\n", umlenkung.pfad);
			break;
		case APPEND:
			if ((file = open(umlenkung.pfad, O_RDWR | O_APPEND | O_CREAT)) == -1)
				abbruch("Fehler APPEND %s\n", umlenkung.pfad);
			break;
		}

		if (dup2(file, umlenkung.filedeskriptor) == -1)
			printf("Fehler dup in %s\n", umlenkung.pfad);

		close(file);

		/* Exit(1) falls Liste leer (mögl. Fehlerquelle) */
		if(i-1<anzahl) umlenkungen = listeRest(umlenkungen);
	}

	return 1;
}

int aufruf(Kommando k, int forkexec) {

	/* Programmaufruf im aktuellen Prozess (forkexec==0)
	 oder Subprozess (forkexec==1)
	 */

	if (forkexec) {
		int pid = fork();

		switch (pid) {
		case -1:
			perror("Fehler bei fork");
			return (-1);
		case 0:
			if (umlenkungen(k))
				exit(1);
			do_execvp(k->u.einfach.wortanzahl, k->u.einfach.worte);
			abbruch("interner Fehler 001"); /* sollte nie ausgeführt werden */
		default:
			if (k->endeabwarten)

				/* pipelining implementieren */
				/* So einfach geht das hier nicht! */
				waitpid(pid, NULL, 0);
			return 0;
		}
	}

	/* nur exec, kein fork */
	if (umlenkungen(k))
		exit(1);
	do_execvp(k->u.einfach.wortanzahl, k->u.einfach.worte);
	abbruch("interner Fehler 001"); /* sollte nie ausgeführt werden */
	exit(1);
}

int interpretiere_einfach(Kommando k, int forkexec) {

	char **worte = k->u.einfach.worte;
	int anzahl = k->u.einfach.wortanzahl;

	if (strcmp(worte[0], "exit") == 0) {
		switch (anzahl) {
		case 1:
			exit(0);
		case 2:
			exit(atoi(worte[1]));
		default:
			fputs("Aufruf: exit [ ZAHL ]\n", stderr);
			return -1;
		}
	}

	if (strcmp(worte[0], "cd") == 0) {

		switch (anzahl) {
		case 1:
			fputs(" NOT IMPLEMENTED. Should lead to homedir", stderr);
			break;
		case 2:
			fputs(" NOT IMPLEMENTED. Should lead to path", stderr);
			break;
		default:
			fputs("Aufruf: cd [ Dateipfad ]", stderr);
		}
		return 0;
	}

	return aufruf(k, forkexec);
}

int interpretiere(Kommando k, int forkexec) {
	int status;

	switch (k->typ) {
	case K_LEER:
		return 0;
	case K_EINFACH:
		return interpretiere_einfach(k, forkexec);
	case K_SEQUENZ: {
		Liste l = k->u.sequenz.liste;
		while (!listeIstleer(l)) {
			status = interpretiere((Kommando) listeKopf(l), forkexec);
			l = listeRest(l);
		}
	}
		return status;
	default:
		fputs("unbekannter Kommandotyp, Bearbeitung nicht implementiert\n",
				stderr);
		break;
	}
	return 0;
}

