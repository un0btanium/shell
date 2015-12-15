#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "process.h"

Process processNeu(int pid, int pgid, char *status, char *path) {
	Process neu = reserviere(sizeof (struct process));
	neu -> pid = pid;
	neu -> pgid = pgid;
	neu -> status = status;
	neu -> path = path;
	neu -> next = NULL;
	return neu;
}

Process processAnfuegen(int pid, int pgid, char *status, char *path, Process next) {
	Process neu = reserviere(sizeof (struct process));
	neu -> pid = pid;
	neu -> pgid = pgid;
	neu -> status = status;
	neu -> path = path;
	neu -> next = next;
	return neu;
}

void processLoeschen(int pid, Process *l) {
	Process *currentP;
	Process *previousP = NULL;

	if (l == NULL) /* list is empty */
		return;

	for (currentP = *l; currentP != NULL; previousP = currentP, currentP = getNext(*currentP)) { /* iterate over elements */
		if (getPID(*currentP) == pid) {				/* found process with pid */
			if (previousP == NULL) { 				/* is first element in list */
				*l = getNext(*currentP);				/* set pointer of list to the second element */
			} else {								/* is not first element in list */
				(*previousP)->next = getNext(*currentP);	/* skip element by switching pointers */
			}
			printf("Delete %d", pid);
			/* delete element */
			/* free(currentP); */
			return;
		}
	}
}

Process getProcess(int pid, Process l) {
	while (l != NULL && getPID(l) != pid) {
		l = l->next;
	}
	return l;
}

Process getNext(Process p) {
	return p -> next;
}

void stopProcess() {
	/* TODO */
}

void resumeProcess() {
	/* TODO */
}

int processListeIstleer(Process l){
  return l==NULL;
}

int processListeLaenge(Process l){
  return l==NULL ? 0 : 1+processListeLaenge(l->next);
}

/* GETTER */

int getPID(Process p) {
	return p -> pid;
}

int getPGID(Process p) {
	return p -> pgid;
}

char *getStatusType(Process p) {
	return p -> status;
}

char *getPath(Process p) {
	return p -> path;
}



/* SETTER */

void setPID(Process p, int pid) {
	p -> pid = pid;
}

void setPGID(Process p, int pgid) {
	p -> pgid = pgid;
}

void setStatusType(Process p, char *status) {
	p -> status = status;
}

void setPath(Process p, char *path) {
	p -> path = path;
}







