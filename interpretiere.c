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
#include "process.h"
#include "errno.h"

Process listeProzesse;
extern int errno;

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
	int status;
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

			interpretiere(einfach, 0);
			break;
		default:
			if ((setpgid(childpid, 0)) == -1) {
				perror("setpgid-Fehler letzter Prozess");
			}
			waitpid(childpid, &status, WUNTRACED);
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
			if ((setpgid(childpid, 0)) == -1) {
				perror("setpgid-Fehler mittlerer Prozess");
			}
			interpretiere(einfach, 0);
			break;
		default:

			waitpid(childpid, &status, WUNTRACED);
			close(pipefd2[1]);
			return interpretiere_pipelineRek(listeRest(l), pipefd2, cpid);
		}

	}

	return 1;

}

/* Einstieg der Prozesskette *//* ls -l | grep david | grep shell.c > david.txt */

int interpretiere_pipeline(Kommando k) {

	kommandoZeigen(k);
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
		interpretiere(einfach, 0);
		break;
	default:
		close(pipefd[1]);
	}

	return interpretiere_pipelineRek(listeRest(l), pipefd, childpid);
}

/* Pipeline Iterativ, lahm und eckelig */

//int interpretiere_pipeline(Kommando k) {
//	Liste l = k->u.sequenz.liste;
//	pid_t childpid[listeLaenge(l)];
//	int status;
//	int numbOfPip = listeLaenge(l) - 1;
//	int pipefd[numbOfPip * 2];
//	Kommando einfach = (Kommando) listeKopf(l);
//	int count = 0;
//	int i;
//	for (i = 0; i < numbOfPip; i++) {
//		if (pipe(pipefd + i * 2) < 0) {
//			perror("pipe-Fehler");
//		}
//	}
//
//	while (!listeIstleer(l)) {
//		switch (childpid[count / 2] = fork()) {
//
//		case -1:
//			perror("fork-Fehler");
//			exit(1);
//
//		case 0:
//			/* if not first command */
//			if (count != 0) {
//				if (dup2(pipefd[count - 2], STDIN_FILENO) < 0) {
//					perror("dup2-Fehler (not first)");
//					exit(1);
//				}
//			}
//			/* if not last command*/
//			if (listeLaenge(l) > 1) {
//				if (dup2(pipefd[count + 1], STDOUT_FILENO) < 0) {
//					perror("dup2-Fehler (not last)");
//					exit(1);
//				}
//			}
//			for (i = 0; i < 2 * numbOfPip; i++) {
//				close(pipefd[i]);
//			}
//
//			interpretiere(einfach, 0);
//			break;
//		default:
//			if ((setpgid(childpid[count / 2], childpid[0])) == -1) {
//				perror("setpgid-Fehler");
//			}
//			l = listeRest(l);
//			if (!listeIstleer(l))
//				einfach = (Kommando) listeKopf(l);
//			count += 2;
//		}
//	}
//
//	for (i = 0; i < 2 * numbOfPip; i++) {
//		close(pipefd[i]);
//	}
//
//	for (i = 0; i < numbOfPip + 1; i++) {
//		waitpid(childpid[i], &status, WUNTRACED);
//	}
//
//	return 1;
//}
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
			if ((file = open(umlenkung.pfad, O_RDWR | O_TRUNC | O_CREAT, 0644)) == -1)
				abbruch("Fehler WRITE, Status = %d %s\n", errno ,umlenkung.pfad);
			break;
		case APPEND:
			if ((file = open(umlenkung.pfad, O_RDWR | O_APPEND | O_CREAT, 0644))
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
	int processCount = processListeLaenge(listeProzesse);
	Process liste = listeProzesse;

	int i;

	if (processCount == 0) {
		fputs("Keine Prozesse aktiv!\n", stderr);
		return 1;
	}
	printf("processCount = %d\n", processCount);
//	printf("pid = %d\n", (*liste)->next->pid);

	printf("NUM	PID		PGID	STATUS		PROG\n");
	for (i = 1; i <= processCount; i++, liste = liste->next) { /* TODO doesnt work */
		int pid = getPID(liste);
		int status;
		int pid_t = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

		if (pid_t == 0) { /* running */
			printf("%d	%d		%d	%s		%s\n", i, pid, pid, "running", "");
		} else if (WIFEXITED(status)) /* process terminated normally */
			printf("%d	%d		%d	%s%d%s		%s\n", i, pid_t, pid, "exit(",
					WEXITSTATUS(status), ")", "");
		else if (WIFSIGNALED(status)) /* process was terminated by a signal */
			printf("%d	%d		%d	%s%d%s		%s\n", i, pid_t, pid, "signal(",
					WTERMSIG(status), ")", "");
		else if (WIFSTOPPED(status)) /* process was stopped by delivery of a signal */
			printf("%d	%d		%d	%s%d%s		%s\n", i, pid_t, pid, "stopped(",
					WSTOPSIG(status), ")", "");
		else if (WIFCONTINUED(status)) /* process was resumed by delivery of SIGCONT */
			printf("%d	%d		%d	%s		%s\n", i, pid_t, pid, "continued", "");
	}

	/* TODO delete */
	return 1;

	liste = listeProzesse;
	/* Delete stopped processes */
	for (i = 1; i <= processCount; i++) {
		int pid = getPID(listeProzesse);
		int status;
		int pid_t = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

		if (pid_t != 0) { /* running */
			processLoeschen(pid, &listeProzesse);
			processCount = processCount - 1;
		}

		if (i != processCount)
			listeProzesse = listeProzesse->next;
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
			/* no break */
		default:
			if (processListeLaenge(listeProzesse) == 0) { /* Prozessliste besitzt keine Einträge */
				listeProzesse = processNeu(pid, pid, "running", "program name");
			} else {
				/* Prozessliste hat bereits Prozess-IDs enthalten */
				listeProzesse = processAnfuegen(pid, pid, "running",
						"program name", listeProzesse);
			}
			printf("PID: %d\n", pid);
			if (k->endeabwarten) /* Prozess im Vordergrund */
				waitpid(pid, NULL, 0); /* STRG+Z ?? */
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
			break;
		case 2:
			if ((chdir(worte[1])) == -1)
				fputs("cd couldnt find path", stderr);
			break;
		default:
			fputs("Aufruf: cd [ Dateipfad ]", stderr);
		}
	} else
		/* PROGRAM CALL */
		return aufruf(k, forkexec);

	return 1;
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

