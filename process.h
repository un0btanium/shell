typedef struct process{
	int pid;
	int pgid;
	char *status;
	char *path;
	struct process *next;
} *Process;


Process processNeu(int pid, int pgid, char *status, char *path);
Process processAnfuegen(int pid, int pgid, char *status, char *path, Process next);
void processLoeschen(int pid, Process *p);
Process getProcess(int pid, Process l);
Process getNext(Process p);

void stopProcess();
void resumeProcess();
int processListeIstleer(Process l);
int processListeLaenge(Process l);

int getPID(Process p);
int getPGID(Process p);
char *getStatus(Process p);
char *getPath(Process p);

void setPID(Process p, int pid);
void setPGID(Process p, int pgid);
void setStatus(Process p, char *status);
void setPath(Process p, char *path);

