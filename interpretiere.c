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
#include "prozess.h"

ProzessListe prozesse;

int interpretiere(Kommando k, int forkexec);

void do_execvp(int argc, char **args) {
	execvp(*args, args);
	perror("exec-Fehler");
	fprintf(stderr, "bei Aufruf von \"%s\"\n", *args);
	exit(1);
}

/* pipelining beliebig vieler childs */

int interpretiere_pipelineRek(Liste l, int pipefd[2], pid_t cpid) {
	pid_t childpid;
	int pipefd2[2];
	Kommando einfach = (Kommando) listeKopf(l);

	/* letztes child bekommt nur den output des vorherigen childs,
	 * sein eigener output bleibt auf standart
	 * Abbruchbedingung der Rekursion
	 * */
	if (listeLaenge(l) == 1) {
		switch (childpid = fork()) {
		case -1:
			perror("fork-Fehler");
			exit(1);
		case 0:
			close(pipefd[1]);
			dup2(pipefd[0], STDIN_FILENO);
			do_execvp(einfach->u.einfach.wortanzahl, einfach->u.einfach.worte);
			break;
		default:
			close(pipefd[0]);
			return 0;
		}

		/* Alle mittleren Childs bekommen input vom vorherigen child ihr output geht ins nächste child */
	} else {
		pipe(pipefd2);
		switch (childpid = fork()) {
		case -1:
			perror("fork-Fehler");
			exit(1);
		case 0:
			close(pipefd[1]);
			close(pipefd2[0]);
			dup2(pipefd[0], STDIN_FILENO);
			dup2(pipefd2[1], STDOUT_FILENO);
			do_execvp(einfach->u.einfach.wortanzahl, einfach->u.einfach.worte);
			break;
		default:
			close(pipefd2[1]);
			return interpretiere_pipelineRek(listeRest(l), pipefd2, childpid);
		}

	}

	return 1;

}

/* Einstieg der Prozesskette */

int interpretiere_pipeline(Kommando k) {
	pid_t childpid;
	int pipefd[2];
	Liste l = k->u.sequenz.liste;
	Kommando einfach = (Kommando) listeKopf(l);

	/* ersters child behält sein standartinput, standartouput geht ins nächste child */

	pipe(pipefd);
	switch (childpid = fork()) {
	case -1:
		perror("fork-Fehler");
		exit(1);
	case 0:
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		do_execvp(einfach->u.einfach.wortanzahl, einfach->u.einfach.worte);
		break;
	default:
		close(pipefd[1]);
	}

	return interpretiere_pipelineRek(listeRest(l), pipefd, childpid);
}

int interpretiere_und(Kommando k) {
	Liste l = k->u.sequenz.liste;
	pid_t childpid[listeLaenge(l)];
	int status[listeLaenge(l)];

	Kommando einfach = (Kommando) listeKopf(l);
	int i;
	for (i = 0; i < listeLaenge(l); i++) {

		switch (childpid[i] = fork()) {
		case -1:
			perror("fork-Fehler");
			exit(1);
		case 0:
			do_execvp(einfach->u.einfach.wortanzahl, einfach->u.einfach.worte);
			break;
		default:
			l = listeRest(l);
			einfach = (Kommando) listeKopf(l);
			waitpid(childpid[i], status + i, WNOHANG | WUNTRACED | WCONTINUED);
			/* hier ein if exit(1) und abbruch aller prozesse falls true */
		}
	}

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
			if ((file = open(umlenkung.pfad, O_RDWR | O_APPEND | O_CREAT))
					== -1)
				abbruch("Fehler APPEND %s\n", umlenkung.pfad);
			break;
		}

		if (dup2(file, umlenkung.filedeskriptor) == -1)
			printf("Fehler dup in %s\n", umlenkung.pfad);

		close(file);

		if (i - 1 < anzahl)
			umlenkungen = listeRest(umlenkungen);
	}

	return 0;
}

int status() {
	ProzessListe aktuellerListenEintrag;
	ProzessListe vorherigerListenEintrag;
	Prozess aktuellerProzess;
	Prozess vorherigerProzess;

	int i, status;
	int processCount = anzahlProzesse(prozesse);

	if (processCount == 0) {
		fputs("Keine Prozesse aktiv!\n", stderr);
		return 1;
	}

	/* Status Update */
	for (aktuellerListenEintrag = prozesse; aktuellerListenEintrag != NULL;
			aktuellerListenEintrag = aktuellerListenEintrag->naechster) {

		int pid = aktuellerListenEintrag->prozess->pid;
		int pid_t = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
		if (pid_t == 0)
			status = -1; /* running */
		aktuellerListenEintrag->prozess->status = status;
	}

	/* Status Output */
	printf("NUM	PID		PGID	STATUS		PROG\n");
	for (i = 1, aktuellerListenEintrag = prozesse;
			aktuellerListenEintrag != NULL; i++, aktuellerListenEintrag =
					aktuellerListenEintrag->naechster) {

		char status_string[50] = "";
		status = aktuellerListenEintrag->prozess->status;
		if (status == -1) /* running */
			strcat(status_string, "running");
		else if (WIFEXITED(status)) { /* process terminated normally */
			sprintf(status_string, "exit(%d)", WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) { /* process was terminated by a signal */
			sprintf(status_string, "signal(%d)", WTERMSIG(status));
		} else if (WIFSTOPPED(status)) { /* process was stopped by delivery of a signal */
			sprintf(status_string, "stopped(%d)", WSTOPSIG(status));
		} else if (WIFCONTINUED(status)) /* process was resumed by delivery of SIGCONT */
			strcat(status_string, "continued");

		printf("%d	%d		%d	%s		%s\n", i, aktuellerListenEintrag->prozess->pid,
				aktuellerListenEintrag->prozess->pgid, status_string,
				aktuellerListenEintrag->prozess->path);
	}

	/* Delete stopped processes */
	vorherigerListenEintrag = NULL;
	vorherigerProzess = NULL;
	for (aktuellerListenEintrag = prozesse; aktuellerListenEintrag != NULL;
			vorherigerListenEintrag = aktuellerListenEintrag, aktuellerListenEintrag =
					aktuellerListenEintrag->naechster) {

		status = aktuellerListenEintrag->prozess->status;
		if (status != -1 && !WIFSTOPPED(status) && !WIFCONTINUED(status)) { /* not running anymore */
			aktuellerProzess = aktuellerListenEintrag->prozess;
			if (vorherigerListenEintrag != NULL)
				vorherigerProzess = vorherigerListenEintrag->prozess;

			if (vorherigerProzess == NULL) { 										/* wenn gefundener Prozess das erste Element in der Liste ist */
				prozesse = aktuellerListenEintrag->naechster; 						/* dann überspringe in der liste das erste Element und fängt beim zweiten Eintrag an */
			} else { 																/* wenn gefundener Prozess nicht das erste Element ist */
				vorherigerListenEintrag->naechster = aktuellerListenEintrag->naechster; /* dann überspringe den aktuellen Eintrag */
			}
			/* freigabe(aktuellerListenEintrag->prozess); */
			/* freigabe(aktuellerListenEintrag); */
		}

	}

	return 0;
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

		if (anzahlProzesse(prozesse) == 0) { /* Prozessliste besitzt keine Einträge */
			prozesse = prozessListeNeu(prozessNeu(pid, pid, 0, "program name"));
		} else { /* Prozessliste hat bereits Prozess-IDs enthalten */
			prozesse = prozessListeAnfuegen(prozesse,
					prozessNeu(pid, pid, 0, "program name"));
		}
		printf("PID: %d\n", pid);
		if (k->endeabwarten) /* Prozess im Vordergrund */
			waitpid(pid, NULL, 0); /* STRG+Z ?? signalbehandlung?? */
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

/* EXIT */
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
/* STATUS */
if (strcmp(worte[0], "status") == 0) {
	if (anzahl > 1)
		fputs("Aufruf: status\n", stderr);
	return status();
}

/* CD */
if (strcmp(worte[0], "cd") == 0) {
	switch (anzahl) {
	case 1:
		if ((chdir(getenv("HOME"))) == -1)
			fputs("cd couldnt find home-directory", stderr);
		return 1;
		break;
	case 2:
		if ((chdir(worte[1])) == -1)
			fputs("cd couldnt find path", stderr);
		return 1;
		break;
	default:
		fputs("Aufruf: cd [ Dateipfad ]", stderr);
	}
	return 0;
}

/* PROGRAM CALL */
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
case K_PIPE: {
	/*kommandoZeigen(k);*/
	return interpretiere_pipeline(k);
}
case K_UND: {
	fputs("Bearbeitung von K_UND noch nicht implementiert", stderr);
	return interpretiere_und(k);
}
case K_ODER: {
	fputs("Bearbeitung von K_ODER noch nicht implementiert", stderr);
	break;
}
case K_IFTHENELSE: {
	fputs("Bearbeitung von K_IFTHENELSE noch nicht implementiert", stderr);
	break;
}
default:
	fputs("unbekannter Kommandotyp, Bearbeitung nicht implementiert\n",
	stderr);
	break;
}
return 0;
}

