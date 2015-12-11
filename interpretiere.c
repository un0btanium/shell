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

Liste processList;

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
	/* Umlenkungen bearbeiten */

	return 0;
}

int status() {
	int processCount = listeLaenge(processList);
	if (processCount == 0) {
		fputs("Keine Prozesse aktiv!\n", stderr);
		return 0;
	}
	Liste newProcessList;
	int i;

	printf("PID		PGID	STATUS		PROG\n");
	for (i = 1; i <= processCount; i++) {
		int pid = processList->kopf;
		int status;
		int pid_t = waitpid(pid, status, WNOHANG | WUNTRACED);

		if (pid_t == -1) { // ERROR
			printf("%d		%d	", pid_t, pid);
			fputs("Error!	", stderr);
			printf("%s\n", "(path no yet implemented)");
		} else if (pid_t == 0)		// RUNNING
			printf("%d		%d	%d		%s\n",pid_t, pid, "running", "(path not yet implemented)");
		else				// STOPPED OR TERMINATED
			printf("%d		%d	%d		%s\n",pid_t, pid, status, "(path not yet implemented)");

		if (pid_t == 0) {
			if (listeLaenge(newProcessList) == 0)
				newProcessList = listeNeu(pid);
			else
				newProcessList = listeAnfuegen(newProcessList, pid);
		}
		processList = processList->rest; // check required? use methods?
	}
	processList = newProcessList; // aktualisiere Liste mit aktiven Prozessen
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
			/* no break */
		default:
			if (k->endeabwarten) /* Prozess im Vordergrund ?? */
				waitpid(pid, NULL, 0);
			else { /* Prozess im Hintergrund */
				if (listeLaenge(processList) == 0) /* Prozessliste noch nicht inizialisiert */
					processList = listeNeu(pid);
				else
					/* Prozessliste hat bereits Prozess-IDs enthalten */
					processList = listeAnfuegen(processList, pid);
			}
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

	// EXIT
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
	// STATUS
	if (strcmp(worte[0], "status") == 0) {
		if (anzahl > 1)
			fputs("Aufruf: status\n", stderr);
		return status();
	}
	// CD
	if (strcmp(worte[0], "cd") == 0) {
		fputs("NOT IMPLEMENTED", stderr);
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

