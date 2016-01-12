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
#include "errno.h"

extern int errno;
int shellpid;
ProzessListe prozesse;

int interpretiere(Kommando k, int forkexec);
void setStatus(int pid, int status);

void setshellpid(int pid){
	shellpid = pid;
}

void do_execvp(int argc, char **args) {
	execvp(*args, args);
	perror("exec-Fehler");
	fprintf(stderr, "bei Aufruf von \"%s\"\n", *args);
	exit(1);
}

int interpretiere_pipeline(Kommando k) {
	Liste l = k->u.sequenz.liste;
	pid_t childpid[listeLaenge(l)];
	int status;
	int numbOfPip = listeLaenge(l) - 1;
	int pipefd[numbOfPip * 2];
	Kommando einfach = (Kommando) listeKopf(l);
	int count = 0;
	int i;
	for (i = 0; i < numbOfPip; i++) {
		if (pipe(pipefd + i * 2) < 0) {
			perror("pipe-Fehler");
		}
	}

	while (!listeIstleer(l)) {
		switch (childpid[count] = fork()) {

		case -1:
			perror("fork-Fehler");
			exit(1);

		case 0:

			setpgid(0, childpid[0]);
				if (k->endeabwarten) {
					tcsetpgrp(STDIN_FILENO, childpid[0]);
				}
			/* if not first command */
			if (count != 0) {
				if (dup2(pipefd[(count*2) - 2], STDIN_FILENO) < 0) {
					perror("dup2-Fehler (not first)");
					exit(1);
				}
			}
			/* if not last command*/
			if (listeLaenge(l) > 1) {
				if (dup2(pipefd[(count*2) + 1], STDOUT_FILENO) < 0) {
					perror("dup2-Fehler (not last)");
					exit(1);
				}
			}
			for (i = 0; i < 2 * numbOfPip; i++) {
				close(pipefd[i]);
			}

			interpretiere(einfach, 0);
			break;
		default:
			if ((setpgid(childpid[count], childpid[0])) == -1) {
				perror("setpgid-Fehler");
			}

			if (k->endeabwarten) { /* Prozess im Vordergrund */
				tcsetpgrp(STDIN_FILENO, childpid[0]);
			}
			prozesse = prozessAnfuegen(childpid[count], getpgid(childpid[count]), -1, einfach->u.einfach.worte[0], prozesse);
			l = listeRest(l);
			if (!listeIstleer(l))
				einfach = (Kommando) listeKopf(l);
			count++;
		}
	}

	for (i = 0; i < 2 * numbOfPip; i++) {
		close(pipefd[i]);
	}

	for (i = 0; i < numbOfPip + 1; i++) {
		waitpid(childpid[i], &status, WUNTRACED);
			setStatus(childpid[i], status);
	}

	tcsetpgrp(STDIN_FILENO, shellpid);

	return 1;
}

int interpretiere_ifthenelse(Kommando k, int forkexec){
	Liste l = k->u.sequenz.liste;

	if (interpretiere((Kommando) listeKopf(l), forkexec) == 0) {
		l = listeRest(l);
		return interpretiere((Kommando) listeKopf(l), forkexec);
	} else {
		l = listeRest(l);
		l = listeRest(l);
		if (l == NULL) /* no if-else */
			return 0;
		return interpretiere((Kommando) listeKopf(l), forkexec);
	}
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
			if ((file = open(umlenkung.pfad, O_RDONLY)) == -1) {
			 	//fputs("No such file or directory!", stderr);
			 	//fprintf(stderr, "\nFehler beim öffnen der Datei: %s", umlenkung.pfad);
			 	//exit(1);

				abbruch2("%s\nFehler beim lesen der Datei: %s", strerror(errno), umlenkung.pfad);
			}

			break;
		case WRITE:
			if ((file = open(umlenkung.pfad, O_RDWR | O_TRUNC | O_CREAT, 0644))
					== -1)
				abbruch2("%s\nFehler beim schreiben der Datei: %s", strerror(errno), umlenkung.pfad);
			break;
		case APPEND:
			if ((file = open(umlenkung.pfad, O_RDWR | O_APPEND | O_CREAT, 0644))
					== -1)
				abbruch2("%s\nFehler beim anfügen an Datei: %s", strerror(errno), umlenkung.pfad);
			break;
		}

		if (dup2(file, umlenkung.filedeskriptor) == -1)
			printf("%s\n", umlenkung.pfad);

		close(file);

		if (i - 1 < anzahl)
			umlenkungen = listeRest(umlenkungen);
	}

	return 0;
}


void setStatus(int pid, int status) {
	ProzessListe aktuellerListenEintrag = prozesse;

	while(aktuellerListenEintrag != NULL) {
		if (aktuellerListenEintrag->prozess->pid == pid) {
			aktuellerListenEintrag->prozess->status = status;
			break;
		}

		aktuellerListenEintrag = aktuellerListenEintrag->naechster;
	}
}

int status() {
	ProzessListe aktuellerListenEintrag = prozesse;
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

//	for (aktuellerListenEintrag = prozesse; aktuellerListenEintrag != NULL;
//			aktuellerListenEintrag = aktuellerListenEintrag->naechster) {
//
//		int pid = aktuellerListenEintrag->prozess->pid;
//		int pid_t = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
//		if (pid_t == 0) // running
//			aktuellerListenEintrag->prozess->status = -1;
//		else if (pid_t > 0)
//			aktuellerListenEintrag->prozess->status = status;
//	}

	/* Status Output */
	printf("NUM	PID		PGID	STATUS			PROG\n---------------------------------------------------------------------\n");
	for (i = 1, aktuellerListenEintrag = prozesse;
			aktuellerListenEintrag != NULL; i++, aktuellerListenEintrag =
					aktuellerListenEintrag->naechster) {

		char status_string[50] = "";
		status = aktuellerListenEintrag->prozess->status;
		if (status == -1) {
			strcat(status_string, "running");
		} else if (WIFEXITED(status)) { /* process terminated normally */
			sprintf(status_string, "exit(%d)", WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) { /* process was terminated by a signal */
			sprintf(status_string, "signal(%d)", WTERMSIG(status));
		} else if (WIFSTOPPED(status)) { /* process was stopped by delivery of a signal */
			sprintf(status_string, "stopped(%d)", WSTOPSIG(status));
		} else if (WIFCONTINUED(status)) /* process was resumed by delivery of SIGCONT */
			strcat(status_string, "continued");
		else {
			strcat(status_string, "unknown status");
		}

		printf("%d	%d		%d	%s			%s\n", i, aktuellerListenEintrag->prozess->pid,
				aktuellerListenEintrag->prozess->pgid, status_string,
				aktuellerListenEintrag->prozess->path);
	}


	/* ls -l | grep david | grep shell.c > david.txt */

	/* Delete stopped processes */
	vorherigerListenEintrag = NULL;
	vorherigerProzess = NULL;
	aktuellerListenEintrag = prozesse;
	while (aktuellerListenEintrag != NULL) {

		status = aktuellerListenEintrag->prozess->status;
		if (status != -1 && !WIFSTOPPED(status) && !WIFCONTINUED(status)) { /* not running anymore */
			aktuellerProzess = aktuellerListenEintrag->prozess;
			if (vorherigerListenEintrag != NULL)
				vorherigerProzess = vorherigerListenEintrag->prozess;

			if (vorherigerProzess == NULL) { /* wenn gefundener Prozess das erste Element in der Liste ist */
				prozesse = aktuellerListenEintrag->naechster; /* dann überspringe in der liste das erste Element und fängt beim zweiten Eintrag an */
				aktuellerListenEintrag = prozesse;
			} else { /* wenn gefundener Prozess nicht das erste Element ist */
				vorherigerListenEintrag->naechster = aktuellerListenEintrag->naechster; /* dann überspringe den aktuellen Eintrag */
				aktuellerListenEintrag = aktuellerListenEintrag->naechster;
			}
		} else {
			vorherigerListenEintrag = aktuellerListenEintrag;
			aktuellerListenEintrag = aktuellerListenEintrag->naechster;
		}
	}

	return 0;
}

int aufruf(Kommando k, int forkexec) {

	/* Programmaufruf im aktuellen Prozess (forkexec==0)
	 oder Subprozess (forkexec==1)
	 */
	char* prog = k->u.einfach.worte[0];
	int status = 0;

	if (forkexec) {
		int pid = fork();
		switch (pid) {
		case -1:
			perror("Fehler bei fork");
			return (-1);
		case 0:
			setpgid(0, 0);
			if (k->endeabwarten) {
				tcsetpgrp(STDIN_FILENO, getpgid(pid));
			}
			if (umlenkungen(k))
				exit(1);
			do_execvp(k->u.einfach.wortanzahl, k->u.einfach.worte);
			abbruch("interner Fehler 001"); /* sollte nie ausgeführt werden */
			/* no break */
		default:
			setpgid(pid, pid);
			prozesse = prozessAnfuegen(pid, getpgid(pid), status, prog,
					prozesse);
			if (k->endeabwarten) { /* Prozess im Vordergrund */
				tcsetpgrp(STDIN_FILENO, getpgid(pid));

				waitpid(pid, &status, WUNTRACED);
				tcsetpgrp(STDIN_FILENO, shellpid);
				setStatus(pid, status);

			} else {
				waitpid(pid, &status, WUNTRACED | WNOHANG | WCONTINUED);
				setStatus(pid, status);
			}
			return status;
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

	int statuss;
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
			return 0;
			break;
		case 2:
			if ((chdir(worte[1])) == -1)
				fprintf(stderr,"%s : Datei oder Verzeichnis nicht gefunden",worte[1]);
			return 0;
			break;
		default:
			fputs("Aufruf: cd [ Dateipfad ]", stderr);
		}
		return -1;
	}

	// FG
	if (strcmp(worte[0], "fg") == 0) {
		ProzessListe aktuellerListenEintrag;
		if (anzahl != 2) {
			fputs("Aufruf: fg [ pgid ]", stderr);
			return -1;
		}

		/* send SIGNCONT singal to all process with pgid */
		for (aktuellerListenEintrag = prozesse; aktuellerListenEintrag != NULL; aktuellerListenEintrag = aktuellerListenEintrag->naechster) {
			if (aktuellerListenEintrag->prozess->pgid == atoi(worte[1])) {
				kill(aktuellerListenEintrag->prozess->pid, SIGCONT);
			}
		}
		tcsetpgrp(STDIN_FILENO, atoi(worte[1]));
		/* wait for all process to be terminated */
		for (aktuellerListenEintrag = prozesse; aktuellerListenEintrag != NULL; aktuellerListenEintrag = aktuellerListenEintrag->naechster) {
			if (aktuellerListenEintrag->prozess->pgid == atoi(worte[1])) {
				waitpid(aktuellerListenEintrag->prozess->pid, &statuss, WUNTRACED);
				setStatus(aktuellerListenEintrag->prozess->pid, statuss);
			}
		}
		tcsetpgrp(STDIN_FILENO, shellpid);

		return 0;
	}

	// BG
	if (strcmp(worte[0], "bg") == 0) {
		ProzessListe aktuellerListenEintrag;
		if (anzahl != 2) {
			fputs("Aufruf: bg [ pgid ]", stderr);
			return -1;
		}

		for (aktuellerListenEintrag = prozesse; aktuellerListenEintrag != NULL; aktuellerListenEintrag = aktuellerListenEintrag->naechster) {
			if (aktuellerListenEintrag->prozess->pgid == atoi(worte[1])) {
				kill(aktuellerListenEintrag->prozess->pid, SIGCONT);
				waitpid(aktuellerListenEintrag->prozess->pid, &statuss, WNOHANG | WUNTRACED | WCONTINUED);
				setStatus(aktuellerListenEintrag->prozess->pid, statuss);
			}
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
		Liste l = k->u.sequenz.liste;
		while (!listeIstleer(l)) {
			status = interpretiere((Kommando) listeKopf(l), forkexec);
			l = listeRest(l);
			if(status != 0) break;
		}
		return status;
	}
	case K_ODER: {
		Liste l = k->u.sequenz.liste;
		while (!listeIstleer(l)) {
			status = interpretiere((Kommando) listeKopf(l), forkexec);
			l = listeRest(l);
			if(status == 0) break;
		}
		return status;
	}
	case K_IFTHENELSE: {
		return interpretiere_ifthenelse(k, forkexec);
	}
	default:
		fputs("unbekannter Kommandotyp, Bearbeitung nicht implementiert\n",
		stderr);
		break;
	}
	return 0;
}

