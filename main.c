/***
 * +-----------------------------------------------------------------+
 * |     01001001  01000011 01101000 01101001 01101110 01100111      |
 * +-----------------------------------------------------------------+
 *
 * La divinazione dell'I Ching, anche chiamato Yijing, è una pratica
 * antica che viene solitamente praticata con il lancio di tre monete
 * ripetuto sei volte.  Il lancio, tradizione vuole, dovrebbe essere
 * contemporaneo per tutte e tre le monete, e per questa ragione gli
 * adattamenti realizzati in JavaScript non sono mai aderenti alla
 * versione reale.  L'unico modo per poter simulare correttamente il
 * lancio delle monete è attraverso l'utilizzo di thread.
 *
 * (Esiste un altro metodo, l'interrogazione con l'uso di steli di
 * achillea, che nella prassi non richiede la sincronizzazione di certi
 * passaggi e che perciò sarebbe più facilmente realizzabile.  È però
 * meno utilizzabile da terminale.)
 */

#include <fcntl.h>
#include <getopt.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <string.h>

/***
 * Per prima cosa, definiamo cos'è un oggetto Hexagram, ossia l'esagramma,
 * e successivamente creiamo un array di oggetti Hexagram che rappresenta il
 * contenuto dell'I Ching.  Ogni esagramma è rappresentato da un numero che ne
 * denota l'indice e da una sua rappresentazione binaria.
 */

typedef struct Hexagram {
  int index;
  unsigned char binary_representation;
} Hexagram;

const Hexagram iching[] = {
    {1, 0b00111111},  {2, 0b00000000},  {3, 0b00010001},  {4, 0b00100010},
    {5, 0b00010111},  {6, 0b00111010},  {7, 0b00000010},  {8, 0b00010000},
    {9, 0b00110111},  {10, 0b00111011}, {11, 0b00000111}, {12, 0b00111000},
    {13, 0b00111101}, {14, 0b00101111}, {15, 0b00000100}, {16, 0b00001000},
    {17, 0b00011001}, {18, 0b00100110}, {19, 0b00000011}, {20, 0b00110000},
    {21, 0b00101001}, {22, 0b00100101}, {23, 0b00100000}, {24, 0b00000001},
    {25, 0b00111001}, {26, 0b00100111}, {27, 0b00100001}, {28, 0b00011110},
    {29, 0b00010010}, {30, 0b00101101}, {31, 0b00011100}, {32, 0b00001110},
    {33, 0b00111100}, {34, 0b00001111}, {35, 0b00101000}, {36, 0b00000101},
    {37, 0b00110101}, {38, 0b00101011}, {39, 0b00010100}, {40, 0b00001010},
    {41, 0b00100011}, {42, 0b00110001}, {43, 0b00011111}, {44, 0b00111110},
    {45, 0b00011000}, {46, 0b00000110}, {47, 0b00011010}, {48, 0b00010110},
    {49, 0b00011101}, {50, 0b00101110}, {51, 0b00001001}, {52, 0b00100100},
    {53, 0b00110100}, {54, 0b00001011}, {55, 0b00001101}, {56, 0b00101100},
    {57, 0b00110110}, {58, 0b00011011}, {59, 0b00110010}, {60, 0b00010011},
    {61, 0b00110011}, {62, 0b00001100}, {63, 0b00010101}, {64, 0b00101010},
    {35, 0b00101000}, {36, 0b00000101}, {37, 0b00110101}, {38, 0b00101011},
    {39, 0b00010100}, {40, 0b00001010}, {41, 0b00100011}, {42, 0b00110001},
    {43, 0b00011111}, {44, 0b00111110}, {45, 0b00011000}, {46, 0b00000110},
    {47, 0b00011010}, {48, 0b00010110}, {49, 0b00011101}, {50, 0b00101110},
    {51, 0b00001001}, {52, 0b00100100}, {53, 0b00110100}, {54, 0b00001011},
    {55, 0b00001101}, {56, 0b00101100}, {57, 0b00110110}, {58, 0b00011011},
    {59, 0b00110010}, {60, 0b00010011}, {61, 0b00110011}, {62, 0b00001100},
    {63, 0b00010101}, {64, 0b00101010}};

/***
 * Qui definiamo alcune costanti basilari: tre monete, sei lanci.
 */

#define COINS 3
#define THROWS 6

/***
 * Di seguito definiamo le strutture che ci serviranno nel codice.
 * Abbiamo come primo struct quello che ho chiamato Response, il quale
 * include tre variabili: una, `raw`, memorizzerà il risultato dei lanci
 * ed è perciò un array che verrà in un secondo momento scisso in due
 * variabili distinte definite in maniera binaria.  Qui l'integer
 * offerto da C contiene 8 bit.  A noi ne servirebbero solamente 6.
 */

typedef struct Response {
  char raw[THROWS];
  unsigned char beginning;
  unsigned char end;
} Response;

/***
 * Questa struttura serve invece a memorizzare le opzioni che possiamo
 * passare al programma.  Desidero fare un programma il più minimale
 * possibile, preferendo piuttosto la creazione di un secondo o terzo
 * programma per aggiungere certe funzionalità, ma ci sono comunque
 * alcune variazioni che possono essere utili.
 *
 * La prima riguarda `--no-wait`.  Tradizione vuole che per interrogare
 * l'oracolo si debba lanciare le tre monete per sei volte, e questo
 * richiede una intenzionalità che qui è data dalla pressione di un
 * qualsiasi tasto (qualsiasi a eccezione dei modificatori, come si
 * vedrà più avanti.  In alcuni casi, come durante la programmazione, è
 * invece più utile avere far sì che tutti e sei i lanci avvengano senza
 * interazione con l'utente.
 *
 * La seconda opzione, `--unicode`, permette di visualizzare nel responso
 * l'esagramma in forma unicode.  Per certi versi è un semplice vezzo in
 * quanto il carattere è minuscolo e difficilmente leggibile.
 *
 * La terza opzione è `--lookup` e permette di bypassare l'interrogazione
 * dell'oracolo fornendo come input le sei somme e aspettando come output
 * il responso.  Ovviamente, è utilizzabile assieme a `--unicode`.
 *
 * La quarta opzione, `--verbose`, aumenta la verbosità dell'output.
 * Nelle mie intenzioni l'unico output dovrebbe essere il responso
 * finale con le eventuali linee mutate (per esempio, 53.2.3.6).  Non
 * sempre questo è il comportamento desiderato, specialmente quando si
 * sta scrivendo il codice.
 */

typedef struct Options {
  int no_wait;
  int unicode;
  int lookup;
  int verbose;
  int csv;
} Options;

/***
 * Le ultime due strutture sono necessarie all'utilizzo dei thread, e
 * servono essenzialmente al passaggio di informazioni tra funzione
 * principale e singoli thread.
 */

typedef struct ThreadData {
  int thread_id;
  int result;
} ThreadData;

typedef struct ThreadParams {
  ThreadData thread_data;
  Options options;
} ThreadParams;

/***
 * La funzione `kbhit` serve a gestire l'input dell'utente che avvia il
 * lancio delle monete.  La funzione `getch`, infatti, pur intercettando
 * il tasto non impediva la stampa del carattere digitato.  Questa
 * funzione, pur facendo il suo mestiere, non è perfetta perché non
 * permette l'utilizzo di tasti come SHIFT, CTRL, ALT, ecc.
 */
int kbhit() {
  struct termios oldt, newt;
  int ch;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

  if (ch != EOF) {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}

/***
 * get_timestamp() è una semplice funzione che ritorna il timestamp.
 * Viene chiamata tanto da throw_a_coin quanto nel caso di stampa del
 * risultato come righe CSV, e perciò ha senso che sia scorporata.
 */

time_t get_timestamp() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/***
 * Qui abbiamo la funzione che viene lanciata dai singoli thread.  Ogni
 * utilizzo corrisponde al lancio di una moneta.
 *
 * Per tradizione, colui che interroga l'oracolo assegna alle due facce
 * di ogni moneta o il valore 2 o il valore 3.  Di base, perciò, sarebbe
 * sufficiente recuperare un numero intero a caso tra 2 e 3.
 *
 * Nella realtà, invece, dobbiamo svolgere alcune operazioni in più, e
 * nello specifico mi riferisco alla variabile `time_t timestamp`.
 * Proprio perché la sincronizzazione dei lanci è un elemento importante
 * nell'interrogazione dell'oracolo, è possibile constatare la sincronia
 * dei thread.  È una informazione di per sé poco utile nella pratica di
 * tutti i giorni, e per questa ragione è ottenibile solo dopo aver
 * impostato l'opzione `--verbose` al lancio del programma.
 */

void *throw_a_coin(void *arg) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  time_t timestamp = get_timestamp();

  ThreadParams *params = (ThreadParams *)arg;
  ThreadData *data = &(params->thread_data);
  Options *options = &(params->options);

  int random_number = rand() % 2 + 2;

  if (options->verbose) {
    printf("%lld: %d\n", (long long)timestamp, random_number);
  }

  data->result = random_number;
  pthread_exit(NULL);
}

/***
 * Questa è una semplice funzione che permette la stampa di un esagramma
 * a partire dalla sua rappresentazione binaria.
 */

void print_binary(unsigned char hex) {
  int number_of_bits = 6;
  unsigned char mask = 1 << (number_of_bits - 1);

  for (int i = 0; i < number_of_bits; i++) {
    if (hex & mask)
      printf("1");
    else
      printf("0");
    mask >>= 1;
  }
  printf("\n");
}

/***
 * `split_hex` è una funzione fondamentale, perché è quella che in un
 * certo senso si occupa della lettura della risposta.  Partendo da ciò
 * che ha risposto l'oracolo, questa funzione crea l'esagramma primario
 * e quello relativo, o secondario.
 */

Response split_hex(Response response) {
  response.beginning = 0b000000;
  response.end = 0b000000;
  for (int i = 5; i >= 0; i--) {
    switch (response.raw[i]) {
    case 6:
      response.beginning |= (0b0 << (5 - i));
      response.end |= (0b1 << (5 - i));
      break;
    case 7:
      response.beginning |= (0b1 << (5 - i));
      response.end |= (0b1 << (5 - i));
      break;
    case 8:
      response.beginning &= ~(0b1 << (5 - i));
      response.end &= ~(0b1 << (5 - i));
      break;
    case 9:
      response.beginning |= (0b1 << (5 - i));
      response.end &= ~(0b1 << (5 - i));
      break;
    default:
      fprintf(stderr, "Error: invalid character.\n");
      break;
    }
  }

  return response;
}

/***
 * Una volta ottenuto l'esagramma in forma binaria, possiamo recuperare
 * il numero dell'esagramma.
 */

int get_hexagram_number(unsigned char binary_hex) {
  for (int i = 0; i < 64; i++) {
    if (iching[i].binary_representation == binary_hex) {
      return iching[i].index;
    }
  }
  return -1;
}

/***
 * Questa funzione ha lo scopo di capire quali sono le differenze tra
 * due esagrammi.  Per comodità costruisce una stringa in cui sono
 * indicate le singole linee mutanti.
 */

char *get_variation(unsigned char beginning, unsigned char end) {
  char *diff_bits_str = malloc(THROWS * 3 * sizeof(char));
  int index = 0;
  for (int i = 0; i < THROWS; i++) {
    unsigned char mask = 1 << i;
    if ((beginning & mask) != (end & mask)) {
      diff_bits_str[index++] = '.';
      diff_bits_str[index++] = (i + 1) + '0';
    }
  }
  diff_bits_str[index] = '\0';
  return diff_bits_str;
}

/**
 * Poi, una funzione che controlla se la stringa passata come argomento a
 * `--lookup` è valida, ossia se è composta da sei cifre tra 6 e 9.
 */

int is_valid_lookup_string(const char *str) {
  if (strlen(str) != 6) {
    printf("La stringa deve essere lunga 6 caratteri.\n");
    return 0;
  }
  for (int i = 0; i < 6; i++) {
    if (str[i] < '6' || str[i] > '9') {
      printf("La stringa deve essere composta da cifre tra 6 e 9.\n");
      return 0;
    }
  }
  return 1;
}

// Robert Jenkins' 96 bit Mix Function
unsigned long mix(unsigned long a, unsigned long b, unsigned long c)
{
  a=a-b;  a=a-c;  a=a^(c >> 13);
  b=b-c;  b=b-a;  b=b^(a << 8);
  c=c-a;  c=c-b;  c=c^(b >> 13);
  a=a-b;  a=a-c;  a=a^(c >> 12);
  b=b-c;  b=b-a;  b=b^(a << 16);
  c=c-a;  c=c-b;  c=c^(b >> 5);
  a=a-b;  a=a-c;  a=a^(c >> 3);
  b=b-c;  b=b-a;  b=b^(a << 10);
  c=c-a;  c=c-b;  c=c^(b >> 15);
  return c;
}

/**
 * E poi si inizia...
 */

int main(int argc, char *argv[]) {
  setlocale(LC_ALL, "");
  Options options = {0, 0, 0, 0, 0};
  int option;
  char *lookup_string = NULL;

  static struct option long_options[] = {{"no-wait", no_argument, 0, 'w'},
                                         {"unicode", no_argument, 0, 'u'},
                                         {"lookup", optional_argument, 0, 'l'},
                                         {"verbose", no_argument, 0, 'v'},
                                         {"csv", no_argument, 0, 'c'},
                                         {0, 0, 0, 0}};

  while ((option = getopt_long(argc, argv, "wuvlc::", long_options, NULL)) !=
         -1) {
    switch (option) {
    case 'w':
      options.no_wait = 1;
      break;
    case 'v':
      options.verbose = 1;
      break;
    case 'l':
      options.lookup = 1;
      if (optarg) {
        lookup_string = optarg;
      }
      break;
    case 'c':
      options.csv = 1;
      break;
    case '?':
      fprintf(stderr, "Opzione non riconosciuta\n");
      exit(EXIT_FAILURE);
    default:
      abort();
    }
  }

  Response response;

  if (options.lookup) {
    if (lookup_string && is_valid_lookup_string(lookup_string)) {
      for (int i = 0; i < THROWS; i++) {
        response.raw[i] = lookup_string[i] - '0';
      }
    } else {
    int counter = 0;
    char input;

    printf("Inserisci le sei somme: ");

    while (counter < THROWS) {
      input = getchar();
      if (input >= '6' && input <= '9') {
        printf("%c", input);
        response.raw[counter] = input - '0';
        counter++;
      } else {
        continue;
        }
      }
    }
  } else {

    pthread_t threads[COINS];
    ThreadParams thread_params[COINS];

    /* Prima srand() veniva inizializzata con time(NULL), ma ciò dava
     * chiari problemi quando veniva chiamato molte volte.  Ora il risultato
     * sembra più accurato.
     */
    unsigned long seed = mix(clock(), time(NULL), getpid());
    srand(seed);

    if (!options.no_wait) {
      printf("Premi un tasto per lanciare le monete...");
      fflush(stdout);
    }

    for (int i = 0; i < THROWS; i++) {
      if (!options.no_wait) {
        while (!kbhit()) {
        }
        if (!options.verbose) {
          if (i == 0) {
            printf("\033[2K\r");
          }
          printf("·");
        }
        getchar();
      }

      int sum = 0;

      for (int j = 0; j < COINS; j++) {
        thread_params[j].thread_data.thread_id = j;
        thread_params[j].options = options;
        pthread_create(&threads[j], NULL, throw_a_coin,
                       (void *)&thread_params[j]);
      }

      for (int j = 0; j < COINS; j++) {
        pthread_join(threads[j], NULL);
        sum += thread_params[j].thread_data.result;
      }

      response.raw[i] = sum;

      if (options.verbose) {
        printf("La somma è: %d\n\n", sum);
      }
    }
  }

  if (!options.csv) {
  printf("\033[2K\r");
  }
  response = split_hex(response);
  int beginning_number = get_hexagram_number(response.beginning);
  int beginning_hex = 0x4DBF + beginning_number;
  if (response.beginning != response.end) {
    int end_number = get_hexagram_number(response.end);
    int end_hex = 0x4DBF + end_number;
    char *diff_bits = get_variation(response.beginning, response.end);
    if (options.unicode) {
      wprintf(L"%lc %d%s -> %lc %d\n", (wchar_t)(beginning_hex),
              beginning_number, diff_bits, (wchar_t)(end_hex), end_number);
    } else if (options.csv) {
      time_t now = get_timestamp();
      printf("%ld,%d,primary\r\n%ld,%d,secondary\r\n", now, beginning_number, now, end_number);
    } else {
      printf("%d%s -> %d\n", beginning_number, diff_bits, end_number);
    }
    free(diff_bits);
  } else {
    if (options.unicode) {
      wprintf(L"%lc %d\n", (wchar_t)(beginning_hex), beginning_number);
    } else if (options.csv) {
      time_t now = get_timestamp();
      printf("%ld,%d,static\r\n", now, beginning_number);
    }else {
      printf("%d\n", beginning_number);
    }
  }

  return 0;
}
