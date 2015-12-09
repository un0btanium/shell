/* 
  kommandos.h - definiert den Datentyp "Kommando" zur internen 
                Repräsentation eines Kommandos
*/

typedef enum  { K_EINFACH, K_LEER, K_SEQUENZ, K_PIPE, K_UND, K_ODER, K_IFTHENELSE } Kommandotyp;

/* Umlenkungsarten:                       */
typedef enum { READ, WRITE, APPEND } Modus;

typedef     struct {
  int         filedeskriptor; /* beliebige Deskriptoren können umgelenkt werden */
  Modus       modus;          /* Umlenkungsart */
  char *      pfad;           /* Datei von der / in die umgelenkt wird */
} Umlenkung;


typedef     struct {
      Liste       umlenkungen;
      int         wortanzahl;
      char        **worte;
    } EinfachKommando;

typedef     struct {
      Liste liste;
      int   laenge;
    } SequenzKommando;

typedef struct kommando {
  Kommandotyp typ;
  int         endeabwarten;
  union { 
    EinfachKommando einfach; 
    SequenzKommando sequenz;
  } u;
} * Kommando;

extern Kommando kommandoNeuLeer(void);
extern Kommando kommandoNeuEinfach(int laenge, char**wortliste, Liste umlenkungen);
extern Kommando kommandoSequenz(Kommandotyp t, Kommando neu, Kommando sequenz);
extern void kommandoZeigen ( Kommando k );
extern void kommandoLoeschen (Kommando k );
extern void kommandoImHintergrund(Kommando k);
