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

int interpretiere_pipeline(Kommando k) {
	int pipefd[2];
	pid_t childpid;
	pid_t childpid2;
	/*int leange = listeLaenge(k->u.sequenz.liste); */
	Kommando einfach;
	Liste restKommandos;
	Liste l = k->u.sequenz.liste;

//	while (!listeIstleer(l)) {
	einfach = (Kommando) listeKopf(l);
	restKommandos = listeRest(l);
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
		switch (childpid2 = fork()) {
		case -1:
			perror("fork-Fehler");
			exit(1);
		case 0:
			close(pipefd[1]);
			dup2(pipefd[0], STDIN_FILENO);
			einfach = (Kommando) listeKopf(restKommandos);
			do_execvp(einfach->u.einfach.wortanzahl, einfach->u.einfach.worte);
			break;
			//default:

		}
	}
//		l = listeRest(l);
//	}
//
//		waitpid(childpid2, NULL, WNOHANG | WUNTRACED | WCONTINUED);
//		waitpid(childpid, NULL, WNOHANG | WUNTRACED | WCONTINUED);

	return 0;
}

int interpretiere_pipeline2(Kommando k) {
	pid_t childpid;
	int pipefd[2];
	Liste restKommandos;
	Liste l = k->u.sequenz.liste;
	Kommando einfach = (Kommando) listeKopf(l);

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
		if (listeLaenge(l) > 1) {
			restKommandos = listeRest(l);
			interpretiere_pipeline3(restKommandos, &pipefd);
		}

	}

	return 0;
}

int interpretiere_pipeline3(Kommando k, int* pipefd) {
	pid_t childpid;
	int pipefd2[2];
	Liste restKommandos;
	Liste l = k->u.sequenz.liste;
	Kommando einfach = (Kommando) listeKopf(l);

	pipe(pipefd);

	switch (childpid = fork()) {
	case -1:
		perror("fork-Fehler");
		exit(1);
	case 0:
		close(pipefd[1]);
		dup2(pipefd[0], STDIN_FILENO);
		close(pipefd2[0]);
		dup2(pipefd2[1], STDOUT_FILENO);
		do_execvp(einfach->u.einfach.wortanzahl, einfach->u.einfach.worte);
		break;
		if (listeLaenge(l) > 1) {
			restKommandos = listeRest(l);
			interpretiere_pipeline3(restKommandos, *pipefd2);
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

//int status() {
//	int processCount = listeLaenge(processList);
//	Liste newProcessList;
//	int i;
//
//	if (processCount == 0) {
//		fputs("Keine Prozesse aktiv!\n", stderr);
//		return 1;
//	}
//
//	printf("NUM	PID		PGID	STATUS		PROG\n");
//	for (i = 1; i <= processCount; i++) {
//		printf("test\n");
//		int pid = *(int*) listeKopf(processList);
//		int status;
//		printf("test1\n");
//		int pid_t = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
//		printf("test2\n");
//
//		if (pid_t == 0) { /* running */
//			printf("%d	%d		%d	%s		%s\n", i, &pid, &pid, "running",
//					"(path not yet implemented)");
////			if (listeLaenge(newProcessList) == 0) /* Prozessliste besitzt keine Einträge */
////				newProcessList = listeNeu(pid);
////			else
////				/* Prozessliste hat bereits Prozess-IDs enthalten */
////				newProcessList = listeAnfuegen(newProcessList, pid);
//		} else if (WIFEXITED(status)) /* process terminated normally */
//			printf("%d	%d		%d	%s%d%s		%s\n", i, pid_t, pid, "exit(",
//					WEXITSTATUS(status), ")", "(path not yet implemented)");
//		else if (WIFSIGNALED(status)) /* process was terminated by a signal */
//			printf("%d	%d		%d	%s%d%s		%s\n", i, pid_t, pid, "signal(",
//					WTERMSIG(status), ")", "(path not yet implemented)");
//		else if (WIFSTOPPED(status)) /* process was stopped by delivery of a signal */
//			printf("%d	%d		%d	%s%d%s		%s\n", i, pid_t, pid, "stopped(",
//					WSTOPSIG(status), ")", "(path not yet implemented)");
//		else if (WIFCONTINUED(status)) /* process was resumed by delivery of SIGCONT */
//			printf("%d	%d		%d	%s		%s\n", i, pid_t, pid, "continued",
//					"(path not yet implemented)");
//
//		if (i != processCount)
//			processList = listeRest(processList);
//	}
//	processList = newProcessList;
//	return 1;
//}
//
//int aufruf(Kommando k, int forkexec) {
//
//	/* Programmaufruf im aktuellen Prozess (forkexec==0)
//	 oder Subprozess (forkexec==1)
//	 */
//
//	if (forkexec) {
//		int pid = fork();
//		switch (pid) {
//		case -1:
//			perror("Fehler bei fork");
//			return (-1);
//		case 0:
//			if (umlenkungen(k))
//				exit(1);
//			do_execvp(k->u.einfach.wortanzahl, k->u.einfach.worte);
//			abbruch("interner Fehler 001"); /* sollte nie ausgeführt werden */
//			/* no break */
//		default:
//			if (listeLaenge(processList) == 0) /* Prozessliste besitzt keine Einträge */
//				processList = listeNeu((int *) pid);
//			else
//				/* Prozessliste hat bereits Prozess-IDs enthalten */
//				processList = listeAnfuegen(processList, (int *) pid);
//			//printf("PID: %d\n", pid);
//			if (k->endeabwarten) /* Prozess im Vordergrund */
//				waitpid(pid, NULL, 0);
//			return 0;
//		}
//	}
//
//	/* nur exec, kein fork */
//	if (umlenkungen(k))
//		exit(1);
//	do_execvp(k->u.einfach.wortanzahl, k->u.einfach.worte);
//	abbruch("interner Fehler 001"); /* sollte nie ausgeführt werden */
//	exit(1);
//}

int status() {
	ProzessListe aktuellerListenEintrag;
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
			status = -1; 										/* running */
		aktuellerListenEintrag->prozess->status = status;
	}

	/* Status Output */
	printf("NUM	PID		PGID	STATUS		PROG\n");
	for (i = 1, aktuellerListenEintrag = prozesse;
			aktuellerListenEintrag != NULL; i++, aktuellerListenEintrag =
					aktuellerListenEintrag->naechster) {

		status = aktuellerListenEintrag->prozess->status;
		char status_string[50] = "";
		if (status == -1)										/* running */
			strcat(status_string, "running");
		else if (WIFEXITED(status)) { 							/* process terminated normally */
			sprintf(status_string, "exit(%d)", WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) { 						/* process was terminated by a signal */
			sprintf(status_string, "signal(%d)", WTERMSIG(status));
		} else if (WIFSTOPPED(status)) { 						/* process was stopped by delivery of a signal */
			sprintf(status_string, "stopped(%d)", WSTOPSIG(status));
		} else if (WIFCONTINUED(status)) 						/* process was resumed by delivery of SIGCONT */
			strcat(status_string, "continued");

		printf("%d	%d		%d	%s		%s\n", i, aktuellerListenEintrag->prozess->pid,
				aktuellerListenEintrag->prozess->pgid,
				status_string,
				aktuellerListenEintrag->prozess->path);
	}

	/* Delete stopped processes */
	for (aktuellerListenEintrag = prozesse; aktuellerListenEintrag != NULL;
			aktuellerListenEintrag = aktuellerListenEintrag->naechster) {

		status = aktuellerListenEintrag->prozess->status;
		printf("status: %d - %d %d %d\n", status, (status != -1), (!WIFSTOPPED(status)), (!WIFCONTINUED(status)));
		if (status != -1 && !WIFSTOPPED(status) && !WIFCONTINUED(status)) { /* not running anymore */
			printf("prozess löschen; %d\n", aktuellerListenEintrag->prozess->pid);
			prozessLoeschen(prozesse, aktuellerListenEintrag->prozess->pid);
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
				prozesse = prozessListeNeu(
						prozessNeu(pid, pid, 0, "program name"));
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
		return interpretiere_pipeline2(k);
	}
	case K_UND: {
		fputs("Bearbeitung von K_UND noch nicht implementiert", stderr);
		break;
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

