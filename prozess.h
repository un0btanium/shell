typedef struct prozess{
	int pid;
	int pgid;
	int status;
	char *path;
} *Prozess;

typedef struct prozessListe {
	Prozess prozess;
	struct prozessListe *naechster;
} *ProzessListe;


ProzessListe prozessListeNeu(Prozess prozess);
ProzessListe prozessListeAnfuegen(ProzessListe liste, Prozess prozess);

Prozess prozessNeu(int pid, int pgid, int status, char* path);

void prozessLoeschen(ProzessListe liste, int pid);
int anzahlProzesse(ProzessListe liste);
