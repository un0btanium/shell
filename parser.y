%{
  #include <stdio.h>
  #include <string.h>
  #include <stdlib.h>
  #include "utils.h"
  #include "listen.h"
  #include "frontend.h"
  #include "wortspeicher.h"
  #include "kommandos.h"
  #define YYDEBUG 1
  #define FEDEBUG 1

  Wortspeicher wsp; /* Speicher für Strings */
  Kommando k;      /* aktuelles Kommando */
  int exitwert=0;

  int yylex();
  void yyerror(const char *s);
%}

%union {
  Umlenkung*         umlenkungsAttr;
  StringAttr         stringAttr;
  StringlistenAttr   stringlistenAttr;
  Kommando           kommandoAttr;
  Liste              listenAttr;
}

%token ANSONSTEN UNDDANN DATEIANFUEGEN IF THEN ELSE FI
%token <stringAttr>       STRING UNDEF
%type  <stringAttr>       String
%type  <stringlistenAttr> Stringliste
%type  <kommandoAttr>     Kommando Kommando1 Kommando2 Kommando3 Kommando4 Kommando5 Kommando6
%type  <listenAttr>       Umlenkungen
%type  <umlenkungsAttr>   Umlenkung

%left     ';'
%left     '|'
%nonassoc '&'

%%
Zeile:  { exitwert=0; } Kommando '\n' { k=$2; return exitwert; }
       | /* leer */ '\n'  { k=kommandoNeuLeer(); return 0; }
       | /* EOF */        { exit(0); }
;

Kommando:    IF Kommando2 ';' THEN Kommando2 ';' ELSE Kommando2 ';' FI 
                  {
                    /* if k1; then k2; else; k3; fi  =  k1 && k2 || k3 */ 
                    $$=kommandoSequenz(K_IFTHENELSE, $2, kommandoSequenz(K_IFTHENELSE, $5, $8)); 
                  }
             |
             IF Kommando2 ';' THEN Kommando2 ';' FI 
                  {
                    /* if k1; then k2; else; k3; fi  =  k1 && k2 || k3 */ 
                    $$=kommandoSequenz(K_UND, $2, $5); 
                  }           
             | Kommando1 { $$=$1; }
;

Kommando1:   Kommando2 ';' Kommando1 { $$=kommandoSequenz(K_SEQUENZ, $1, $3); } 
           | Kommando2              { $$=$1; }
;

Kommando2:   Kommando3 ANSONSTEN Kommando2 { $$=kommandoSequenz(K_ODER, $1, $3); } 
           | Kommando3                     { $$=$1; }
;

Kommando3:   Kommando4 UNDDANN Kommando3 { $$=kommandoSequenz(K_UND, $1, $3); } 
           | Kommando4                   { $$=$1; }
;

Kommando4:   Kommando5 '|' Kommando4 { $$=kommandoSequenz(K_PIPE, $1, $3); } 
           | Kommando5               { $$=$1; }
;

Kommando5:   Kommando6 '&' { $$=$1; kommandoImHintergrund($$); }
           | Kommando6     { $$=$1; }
;

Kommando6:   Stringliste Umlenkungen { $$ = kommandoNeuEinfach($1.laenge, (wsp->worte)+$1.anfang, $2); } 
           | '(' Kommando ')' { $$=$2; }
;

Stringliste:         String { $$.laenge=1; $$.anfang=wortspeicherEinfuegen(wsp,$1.str); } 
                   | Stringliste String {  
                         $$.anfang=$1.anfang;
                         wortspeicherEinfuegen(wsp,$2.str);
                         $$.laenge=$1.laenge+1; } 
;

Umlenkungen:   /* leer */ { $$=NULL; }
             | Umlenkung Umlenkungen { $$=listeAnfuegen($2, $1); }
;

Umlenkung:   '>'           String { $$=reserviere(sizeof (Umlenkung)); 
                                    $$->filedeskriptor = 1; 
                                    $$->modus=WRITE;  
                                    $$->pfad = $2.str; 
                                  }
           | DATEIANFUEGEN String { $$=reserviere(sizeof (Umlenkung)); 
                                    $$->filedeskriptor = 1; 
                                    $$->modus=APPEND;  
                                    $$->pfad = $2.str; 
	                          }
           | '<'           String { $$=reserviere(sizeof (Umlenkung)); 
                                    $$->filedeskriptor = 0; 
                                    $$->modus=READ;  
                                    $$->pfad = $2.str; 
                                  }
;

String:    STRING { $$=$1; } 
         | UNDEF  { fprintf(stderr, "undefiniertes Zeichen \'%c\' (=0x%0x)\n", $1.str[0], $1.str[0]);
                    exitwert = -1; $$=$1; 
                  }
;
%%

