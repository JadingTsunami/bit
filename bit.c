/* Bit Manipulator
 * Copyright (C) 2019 Jading Tsunami
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <termios.h> 
#include <unistd.h>
#include <ctype.h>


typedef unsigned long long int uint64_t;

#define MODE_32_BIT (32)
#define MODE_64_BIT (64)

#define DELIMITER_LIST (" ,.;'")

#define HISTORY_SIZE 128

#define FALSE (0)
#define TRUE (1)
#define MAX_LINESIZE (1024)

static uint64_t val = 0;
static unsigned char mode = MODE_32_BIT;
static unsigned char thinprint = 0;
static unsigned char termset = 0;
static struct termios term_settings;
static char history[HISTORY_SIZE][MAX_LINESIZE] = { { 0 } };
static int history_wrptr = 0;
static int history_rdptr = 0;

void getInput(char* in);
void resetTerminal();
void setTerminal();
void binaryPrint();
void printHelp();
void processInput( char cmd, uint64_t arg );
void stripSpaces(char* in);


int main() {
    char command[MAX_LINESIZE];
    // cmds:
    // s - set bit
    // c - clear bit
    // x - set to hex value
    // l - left shift by
    // r - right shift by
    // 3 - 32-bit mode
    // 6 - 64-bit mode
    // 0 - clear value
    // p - print value
    // a - and with value
    // o - or with value
    // v - invert
    // t - thin print
    // b - binary print
    char cmd;
    uint64_t arg;
    char* tokens;

    setTerminal();
    printf("  %04llX %04llX\n", (val&0xffff0000UL)>>16, val&0xffffUL );

    while( 1 ) {
        getInput(command);
        sscanf( command, "%c %*s", &cmd );
        if( cmd == 'x' || cmd == 'a' || cmd =='o' ) {
            stripSpaces(command);
            if(EOF == sscanf( command, "%*c %llx", &arg )) { 
                printf("Bad hex input.\n"); 
                continue; 
            }
        } else if( cmd == '3' || cmd == '6' || cmd == '0' || cmd == 'q' || cmd == 't' || cmd == 'v' ) { 
            goto PROCESS;
        }
        else if( cmd == 'p' ) { 
            goto PRINT; 
        } else if( cmd == 'b' ) {
            // binary print
            binaryPrint();
            continue;
        } else if( cmd == '\n' ) { 
            continue; 
        } else if( cmd == 's' || cmd == 'c' ) { 
            if( EOF == sscanf( command, "%*c %lld", &arg ) ) { 
                printf("Bad decimal input.\n"); continue; 
            } 
            // just process here if multi-arg
            // ignore the first command and arg
            tokens = strtok( command, DELIMITER_LIST );
            tokens = strtok( NULL, DELIMITER_LIST );
            tokens = strtok( NULL, DELIMITER_LIST );
            if( tokens == NULL ) goto PROCESS;
            else processInput( cmd, arg ); // handle first arg
            while( NULL != tokens ) {
                processInput( cmd, atoi( tokens ) );
                tokens = strtok( NULL, DELIMITER_LIST );
            }
            goto PRINT;
        } else if( cmd == 'l' || cmd == 'r' ) {
            if( EOF == sscanf( command, "%*c %lld", &arg ) ) { 
                printf("Bad decimal input.\n"); continue; 
            } 
        } else if(cmd == 'h' || cmd == '?' ) {
            printHelp();
            continue;
        } else {
            printf("Unknown command.\n"); continue; 
        }

        if( feof(stdin) || ferror(stdin) || cmd == '\0' ){
            fprintf(stderr, "Bad input.\n" );
            resetTerminal();
            exit(1);
        }
PROCESS:
        processInput( cmd, arg );
PRINT:
        if( mode == MODE_32_BIT ) {
            val &= val & 0xffffffffUL;
            if( thinprint ) 
                printf("  %08llX\n", val );
            else
                printf("  %04llX %04llX\n", (val&0xffff0000UL)>>16, val&0xffffUL );
        } else if( mode == MODE_64_BIT ) {
            if( thinprint ) 
                printf("  %016llX\n", val);
            else
                printf("  %04llX %04llX %04llX %04llX\n",
                        (val&0xffff000000000000ULL)>>48,
                        (val&0x0000ffff00000000ULL)>>32,
                        (val&0x00000000ffff0000ULL)>>16,
                        (val&0x000000000000ffffULL)
                      );
        } else {
            fprintf(stderr, "Bad mode. Switching to 32-bit.\n" );
            mode = MODE_32_BIT;
        }
    }
    resetTerminal();
    return 0;
}

void processInput( char cmd, uint64_t arg ) {
    switch( cmd ) {
        case 's':
            val |= ((uint64_t)1<<arg);
            break;
        case 'c':
            val &= ~((uint64_t)1<<arg);
            break;
        case 'x':
            val = arg;
            break;
        case 'l':
            val <<= arg;
            break;
        case 'r':
            val >>= arg;
            break;
        case 'a':
            val &= arg;
            break;
        case 'o':
            val |= arg;
            break;
        case '3':
            mode = MODE_32_BIT;
            printf("32-bit mode.\n");
            break;
        case '6':
            printf("64-bit mode.\n");
            mode = MODE_64_BIT;
            break;
        case '0':
            val = 0;
            break;
        case 'v':
            val = ~val;
            break;
        case 't':
            thinprint = ~thinprint;
            printf("Thin print mode %s.\n", thinprint ? "on" : "off" );
            break;
        case 'h':
        case '?':
            printHelp();
            break;
        case 'q':
            resetTerminal();
            exit(0);
            break;
        default:
            printf("Invalid command. Type 'h' for help.\n" );
            break;
    }
}

void binaryPrint() {
    int i = 0;
    int j = 0;
    if( mode == MODE_32_BIT ) {
        if(!thinprint) {
            for( i = 31; i >= 0; i-- ) {
                if( (i%4)==0 ) 
                    printf(" %02d ", i);
                if(((i+1)%4)==0)
                    printf("%02d ", i);
                if( (i%16)==0) printf("   ");
            }
            printf("\n");
        }
        printf(" ");
        for( i = 31; i >= 0; i-- ) {
            if((1<<i)&val) printf("1");
            else printf("0");
            if( (i % 4) == 0 && !thinprint ) printf("   ");
            if( (i % 16) == 0 && !thinprint ) printf("   ");
        }
    } else if( mode == MODE_64_BIT ) {
        if(!thinprint) {
            for( i = 63; i >= 32; i-- ) {
                if( (i%4)==0 ) 
                    printf(" %02d ", i);
                if(((i+1)%4)==0)
                    printf("%02d ", i);
                if( (i%16)==0) printf("   ");
            }
            printf("\n");
        }
        printf(" ");
        for( i = 63; i >= 0; i-- ) {
            if((1ULL<<i)&val) printf("1");
            else printf("0");
            if( (i % 4) == 0 && !thinprint ) printf("   ");
            if( (i % 16) == 0 && !thinprint ) printf("   ");
            if( (i>0) && (i % 32) == 0 && !thinprint ) {
                printf("\n\n");
                for( j = 31; j >= 0; j-- ) {
                    if( (j%4)==0 ) 
                        printf(" %02d ", j);
                    if(((j+1)%4)==0)
                        printf("%02d ", j);
                    if( (j%16)==0) printf("   ");
                }
                printf("\n ");
            }
        }
    }
    printf("\n");
}

/* strip out all spaces and underscores from the non-command part */
void stripSpaces( char* in ) { 
    unsigned len = strlen(in);
    unsigned i,j;
    if( !in ) return;
    for( i = 1; i < len; i++ )
        if( !isspace(in[i]) ) break;
    for( ; i < len; i++ ) {
        if( in[i] == ' ' || in[i] == '_' ) {
            // delete this character
            for( j = i; j < len; j++ ) {
                in[j] = in[j+1];
            }
            in[j] = 0;
        }
    }
}
void printHelp() {
    printf("<cmd> <(hex) value>:\n" );
    printf(" s - set bit\n" );
    printf(" c - clear bit\n" );
    printf(" a - and with hex value\n" );
    printf(" o - or with hex value\n" );
    printf(" x - set to hex value\n" );
    printf(" l - left shift by\n" );
    printf(" r - right shift by\n" );
    printf(" 3 - 32-bit mode\n" );
    printf(" 6 - 64-bit mode\n" );
    printf(" 0 - clear value\n" );
    printf(" v - invert value\n" );
    printf(" p - print value\n" );
    printf(" t - thin mode (no print spacing)\n" );
    printf(" b - binary print\n" );
    printf("\n\n");
}

void setTerminal() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    term_settings = t;
    t.c_lflag &= ~ICANON;
    t.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, 0, &t);
    termset = 1;
}

void resetTerminal() {
    if( termset )
        tcsetattr(STDIN_FILENO, 0, &term_settings);
}

void getInput(char* input) {
    int c;
    int pos = 0;
    char in[MAX_LINESIZE] = { 0 };
    while((c=getc(stdin)) != '\n') {
        if( c == 0x41 ) {
            // up arrow
            printf("\33[2K\r");
            int oldptr = history_rdptr;
            history_rdptr--;
            // wrap around
            if( history_rdptr < 0 )
                history_rdptr = HISTORY_SIZE-1;
            if( (history[history_rdptr][0] == '\0') ||
                    (history_rdptr == history_wrptr)) {
                history_rdptr = oldptr;
            } {
                strcpy(in, history[history_rdptr]);
                printf("%s",in);
                pos = strlen(in);
                fflush(stdin);
            }
        } else if ( c == 0x42 ) {
            // down arrow
            printf("\33[2K\r");
            int oldptr = history_rdptr;
            history_rdptr++;
            if( history_rdptr >= HISTORY_SIZE )
                history_rdptr = 0;
            if( (history[history_rdptr][0] == '\0') ||
                    (history_rdptr == history_wrptr)) {
                history_rdptr = oldptr;
            } {
                strcpy(in, history[history_rdptr]);
                printf("%s",in);
                pos = strlen(in);
                fflush(stdin);
            }
        } else if ( (c==0x1b)||(c==0x5b)) {
            continue;
        } else if ( c==0x15 ) {
            printf("\33[2K\r");
            pos = 0;
            in[pos] = '\0';
        } else if ( c==0x7f ) {
            printf("\b \b");
            pos--;
            if( pos < 0 ) pos = 0;
            in[pos] = '\0';
        } else {
            in[pos++] = c;
            in[pos] = '\0';
            printf("%c",c);
            fflush(stdin);
        }
    }
    // add to history
    // set rdptr == wrptr
    strcpy(history[history_wrptr],in);
    history_wrptr++;
    if( history_wrptr >= HISTORY_SIZE )
        history_wrptr = 0;
    history_rdptr = history_wrptr;
    printf("\n");
    strcpy(input,in);
    return;
}
