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
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <termios.h>
#include <fcntl.h>
#include "iching.h"

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

typedef struct Response
{
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
 * La seconda opzione, `--verbose`, aumenta la verbosità dell'output.
 * Nelle mie intenzioni l'unico output dovrebbe essere il responso
 * finale con le eventuali linee mutate (per esempio, 53.2.3.6).  Non
 * sempre questo è il comportamento desiderato, specialmente quando si
 * sta scrivendo il codice.
 *
 * L'ultima opzione, non ancora implementata, è `--lookup` e permette di
 * bypassare l'interrogazione dell'oracolo fornendo come input le sei
 * somme e aspettando come output il responso.
 */

typedef struct Options
{
    int no_wait;
    int verbose;
} Options;

/***
 * Le ultime due strutture sono necessarie all'utilizzo dei thread, e
 * servono essenzialmente al passaggio di informazioni tra funzione
 * principale e singoli thread.
 */

typedef struct ThreadData
{
    int thread_id;
    int result;
} ThreadData;

typedef struct ThreadParams
{
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
int kbhit()
{
    struct termios oldt, newt;
    int ch;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    if (ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
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

void *throw_a_coin(void *arg)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t timestamp = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    ThreadParams *params = (ThreadParams *)arg;
    ThreadData *data = &(params->thread_data);
    Options *options = &(params->options);

    int random_number = rand() % 2 + 2;

    if (options->verbose)
    {
        printf("%lld: %d\n", (long long)timestamp, random_number);
    }

    data->result = random_number;
    pthread_exit(NULL);
}

/***
 * Questa è una semplice funzione che permette la stampa di un esagramma
 * a partire dalla sua rappresentazione binaria.
 */

void print_binary(unsigned char hex)
{
    int number_of_bits = 6;
    unsigned char mask = 1 << (number_of_bits - 1);

    for (int i = 0; i < number_of_bits; i++)
    {
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

Response split_hex(Response response)
{
    response.beginning = 0b000000;
    response.end = 0b000000;
    for (int i = 5; i >= 0; i--)
    {
        switch (response.raw[i])
        {
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

int get_hexagram_number(unsigned char binary_hex)
{
    for (int i = 0; i < 64; i++)
    {
        if (iching[i].binary_representation == binary_hex)
        {
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

char *get_variation(unsigned char beginning, unsigned char end)
{
    char* diff_bits_str = malloc(THROWS * 3 * sizeof(char));
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
 * E poi si inizia...
 */

int main(int argc, char *argv[])
{
    Options options = {0, 0};
    int option;

    static struct option long_options[] = {
        {"no-wait", no_argument, 0, 'w'},
        {"verbose", no_argument, 0, 'v'},
        {0, 0, 0, 0}};

    while ((option = getopt_long(argc, argv, "wv", long_options, NULL)) != -1)
    {
        switch (option)
        {
        case 'w':
            options.no_wait = 1;
            break;
        case 'v':
            options.verbose = 1;
            break;
        case '?':
            fprintf(stderr, "Opzione non riconosciuta\n");
            exit(EXIT_FAILURE);
        default:
            abort();
        }
    }

    pthread_t threads[COINS];
    ThreadParams thread_params[COINS];

    srand(time(NULL));
    Response response;

    for (int i = 0; i < THROWS; i++)
    {
        if (!options.no_wait)
        {
            while (!kbhit())
            {
            }
            getchar();
        }

        int sum = 0;

        for (int j = 0; j < COINS; j++)
        {
            thread_params[j].thread_data.thread_id = j;
            thread_params[j].options = options;
            pthread_create(&threads[j], NULL, throw_a_coin, (void *)&thread_params[j]);
        }

        for (int j = 0; j < COINS; j++)
        {
            pthread_join(threads[j], NULL);
            sum += thread_params[j].thread_data.result;
        }

        response.raw[i] = sum;

        if (options.verbose)
        {
            printf("La somma è: %d\n\n", sum);
        }
    }

    response = split_hex(response);
    int beginning_number = get_hexagram_number(response.beginning);
    if (response.beginning != response.end)
    {
        int end_number = get_hexagram_number(response.end);
        char *diff_bits = get_variation(response.beginning, response.end);
        printf("%d%s -> %d\n", beginning_number, diff_bits, end_number);
        free(diff_bits);
    }
    else
    {
        printf("%d\n", beginning_number);
    }

    return 0;
}
