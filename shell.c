/*
 Shell-Beispielimplementierung

 Die Implementierung verzichtet aus Gründen der Einfachheit
 des Parsers auf eine vernünftige Heap-Speicherverwaltung:

 Falls der Parser die Analyse eines Kommandos wegen eines
 Syntaxfehlers abbricht, werden die ggf. vorhandenen
 Syntaxbäume für erfolgreich analysierte Unterstrukturen des
 fehlerhaften Kommandos nicht gelöscht.

 Beispiel: if test ! -d /tmp; then mkdir /tmp; else echo "/tmp vorhanden" fi

 Die Analyse wird mit Fehlermeldung abgebrochen, weil vor dem "fi" das
 obligatorische Semikolon fehlt. Im Heap stehen zu diesem Zeitpunkt die
 Bäume für das test- und das mkdir-Kommando. Diese verbleiben als Müll
 im Heap, da die Verweise ausschließlich auf dem Parser-Stack stehen,
 der im Fehlerfall nicht mehr ohne weiteres zugänglich ist.

 Um dies zu beheben, müsste man
 a) sich beim Parsen die Zeiger auf die Wurzeln aller
 konstruierten Substruktur-Bäume solange in einer globalen Liste merken,
 bis die Analyse der kompletten Struktur ERFOLGREICH beendet ist
 oder
 b) die Grammatik mit Fehlerregeln anreichern, in denen die Freigabe
 im Fehlerfall explizit vorgenommen wird.

 Da beides die Grammatik aber stark aufbläht, wird darauf verzichtet.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "utils.h"
#include "listen.h"
#include "wortspeicher.h"
#include "kommandos.h"
#include "frontend.h"
#include "parser.h"
#include "variablen.h"

extern int yydebug;
extern int yyparse(void);
extern int interpretiere(Kommando k, int forkexec);
extern void setStatus(int pid, int status);

int shellpid;

void endesubprozess(int sig) {
	int status, pid;

	if (sig == SIGINT) {
		printf("Shell-Hauptprozess kann mit 'exit' beendet werden");
	}

	if (sig == SIGCHLD) { /* process terminated */
		tcsetpgrp(STDIN_FILENO, shellpid);
		do {
			pid = waitpid(-1, &status, WNOHANG);
			if (pid > 0)
				setStatus(pid, status);
		} while (pid > 0);

	}

}

void init_signalbehandlung() {
	struct sigaction sa, sa_ign, sa_stop;

	sa_stop.sa_handler = endesubprozess;
	sa_stop.sa_flags = SA_RESTART;
	sigemptyset(&sa_stop.sa_mask);

	sa.sa_handler = endesubprozess;
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sigemptyset(&sa.sa_mask);

	sa_ign.sa_handler = SIG_IGN;
	sigemptyset(&sa_ign.sa_mask);


	if (sigaction(SIGCHLD, &sa, NULL) < 0)
		exit(1);
	if (sigaction(SIGTSTP, &sa_stop, NULL) < 0)
		exit(1);
	if (sigaction(SIGINT, &sa, NULL) < 0)
		exit(1);
	if (sigaction(SIGTTIN, &sa, NULL) < 0)
		exit(1);
	if (sigaction(SIGTTOU, &sa_ign, NULL) < 0)
		exit(1);
	if (sigaction(SIGQUIT, &sa_ign, NULL) < 0)
		exit(1);

}

int main(int argc, char *argv[]) {

	shellpid = getpid();
	int zeigen = 0, ausfuehren = 1;
	int status, i;

	if ((chdir(getenv("HOME"))) == -1)
		fputs("couldnt find home-directory in main", stderr);

	init_signalbehandlung();

	yydebug = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--zeige"))
			zeigen = 1;
		else if (!strcmp(argv[i], "--noexec"))
			ausfuehren = 0;
		else if (!strcmp(argv[i], "--yydebug"))
			yydebug = 1;
		else {
			fprintf(stderr, "Aufruf: %s [--zeige] [--noexec] [--yydebug]\n",
					argv[0]);
			exit(1);
		}
	}

	wsp = wortspeicherNeu();
	//setshellpid(shellpid);

	while (1) {
		int res;
		fputs(">> ", stdout);
		fflush(stdout);
		res = yyparse();
		if (res == 0) {
			if (zeigen)
				kommandoZeigen(k);
			if (ausfuehren)
				status = interpretiere(k, 1);
			if (zeigen)
				fprintf(stderr, "Status: %d\n", status);
			kommandoLoeschen(k);
		} else
			fputs("Fehlerhaftes Kommando\n", stderr);
		wortspeicherLeeren(wsp);
	}
}

