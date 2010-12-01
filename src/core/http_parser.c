/* (c) 2010 Anthony Catel */
/* WIP */
/* gcc http.c -fshort-enums -o http_parser_test */

#include "http_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define __   -1

#define MAX_CL 1024000

/* Todo : check for endieness + aligned */
#define BYTES_GET(b) \
	*(uint32_t *) b == ((' ' << 24) | ('T' << 16) | ('E' << 8) | 'G')
	
#define BYTES_POST(b) \
	*(uint32_t *) b == (('T' << 24) | ('S' << 16) | ('O' << 8) | 'P')


typedef enum classes {
	C_NUL,	/* BAD CHAR */
	C_SPACE,/* space */
	C_WHITE,/* other whitespace */
	C_CR,	/* \r */
	C_LF,	/* \n */
	C_COLON,/* : */
	C_COMMA,/* , */
	C_QUOTE,/* " */
	C_BACKS,/* \ */
	C_SLASH,/* / */
	C_PLUS, /* + */
	C_MINUS,/* - */
	C_POINT,/* . */
	C_MARK, /* ? */
	C_PCT,  /* % */
	C_DIGIT,/* 0-9 */
	C_ETC,	/* other */
	C_STAR,	/* * */
	C_ABDF,	/* ABDF */
	C_E,	/* E */
	C_G,	/* G */
	C_T,	/* T */
	C_P,	/* P */
	C_H,	/* H */
	C_O,	/* O */
	C_S,	/* S */
	C_C,	/* C */
	C_N,	/* N */
	C_L,	/* L */
	NR_CLASSES
} parser_class;


static int ascii_class[128] = {
/*
    This array maps the 128 ASCII characters into character classes.
    The remaining Unicode characters should be mapped to C_ETC.
    Non-whitespace control characters are errors.
*/
	C_NUL,    C_NUL,   C_NUL,    C_NUL,    C_NUL,   C_NUL,   C_NUL,   C_NUL,
	C_NUL,    C_NUL,   C_LF,     C_NUL,    C_NUL,   C_CR,    C_NUL,   C_NUL,
	C_NUL,    C_NUL,   C_NUL,    C_NUL,    C_NUL,   C_NUL,   C_NUL,   C_NUL,
	C_NUL,    C_NUL,   C_NUL,    C_NUL,    C_NUL,   C_NUL,   C_NUL,   C_NUL,
	          
	C_SPACE,  C_ETC,   C_QUOTE,  C_ETC,    C_ETC,   C_PCT,   C_ETC,   C_ETC,
	C_ETC,    C_ETC,   C_STAR,   C_PLUS,   C_COMMA, C_MINUS, C_POINT, C_SLASH,
	C_DIGIT,  C_DIGIT, C_DIGIT,  C_DIGIT,  C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT,
	C_DIGIT,  C_DIGIT, C_COLON,  C_ETC,    C_ETC,   C_ETC,   C_ETC,   C_MARK,
	
	C_ETC,   C_ABDF,  C_ABDF,  C_C,     C_ABDF,  C_E,     C_ABDF,  C_G,
	C_H,     C_ETC,   C_ETC,   C_ETC,   C_L,     C_ETC,   C_N,     C_O,
	C_P,   	 C_ETC,   C_ETC,   C_S,     C_T,     C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_BACKS, C_ETC,   C_ETC,   C_ETC,
	
	C_ETC,   C_ABDF,  C_ABDF,  C_C,     C_ABDF,  C_E,     C_ABDF,  C_G,
	C_H,     C_ETC,   C_ETC,   C_ETC,   C_L,     C_ETC,   C_N,     C_O,
	C_P,   	 C_ETC,   C_ETC,   C_S,     C_T,     C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_NUL
};

typedef enum actions {
	MG = -2, /* Method get */
	MP = -3, /* Method post */
	PE = -4, /* Path end */
	HA = -5, /* HTTP MAJOR */
	HB = -6, /* HTTP MINOR  */
	KH = -7, /* header key */
	VH = -8, /* header value */
	KC = -9, /* Content length */
	VC = -10, /* Content-length digit */
	VE = -11, /* content length finish */
	EA = -12, /* first hex in % path */
	EB = -13, /* second digit in % path */
	PC = -14, /* path char */
	EH = -15, /* end of headers */
	QS = -16, /* query string */
	BC = -17, /* body char */
} parser_actions;


static int state_transition_table[NR_STATES][NR_CLASSES] = {
/*  
                       nul   white                                      etc   ABDF       
                       | space |  \r\n  :  ,  "  \  /  +  -  .  ?  % 09  |  *  | E  G  T  P  H  O  S  C  N  L*/
/*start           GO*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,G1,__,P1,__,__,__,__,__,__},
/*GE              G1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,G2,__,__,__,__,__,__,__,__,__},
/*GET             G2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,G3,__,__,__,__,__,__,__},
/*GET             G3*/ {__,MG,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*PO              P1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,P2,__,__,__,__},
/*POS             P2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,P3,__,__,__},
/*POST            P3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,P4,__,__,__,__,__,__,__},
/*POST            P4*/ {__,MP,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/* path	          PT*/ {__,PE,__,__,__,__,__,__,__,PC,PC,PC,PC,QS,E1,PC,PC,__,PC,PC,PC,PC,PC,PC,PC,PC,PC,PC,PC},
/*H 	          H1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,H2,__,__,__,__,__},
/*HT	          H2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,H3,__,__,__,__,__,__,__},
/*HTT	          H3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,H4,__,__,__,__,__,__,__},
/*HTTP	          H4*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,H5,__,__,__,__,__,__},
/*HTTP/	          H5*/ {__,__,__,__,__,__,__,__,__,H6,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*HTTP/[0-9]      H6*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,HA,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*HTTP/[0-9]/     H7*/ {__,__,__,__,__,__,__,__,__,__,__,__,H8,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*HTTP/[0-9]/[0-9]H8*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,HB,__,__,__,__,__,__,__,__,__,__,__,__,__},
/* new line 	  EL*/ {__,__,__,ER,C1,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/* \r expect \n   ER*/ {__,__,__,__,C1,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/* header key 	  HK*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,HK,HK,HK,HK,HK,HK,HK,HK},
/* header value   HV*/ {__,HV,__,VH,VH,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV},
/*C               C1*/ {__,__,__,FI,EH,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,HK,HK,HK,HK,HK,C2,HK,HK},
/*Co              C2*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,HK,HK,HK,C3,HK,HK,HK,HK},
/*Con             C3*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,HK,HK,HK,HK,HK,HK,C4,HK},
/*Cont            C4*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,C5,HK,HK,HK,HK,HK,HK,HK},
/*Conte           C5*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,C6,HK,HK,HK,HK,HK,HK,HK,HK,HK},
/*Conten          C6*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,HK,HK,HK,HK,HK,HK,C7,HK},
/*Content         C7*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,C8,HK,HK,HK,HK,HK,HK,HK},
/*Content-        C8*/ {__,__,__,__,__,KH,__,__,__,__,__,C9,__,__,__,__,HK,__,HK,HK,HK,HK,HK,HK,HK,HK,HK,HK,HK},
/*Content-l       C9*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,HK,HK,HK,HK,HK,HK,HK,CA},
/*Content-le      CA*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,CB,HK,HK,HK,HK,HK,HK,HK,HK,HK},
/*Content-len     CB*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,HK,HK,HK,HK,HK,HK,CC,HK},
/*Content-leng    CC*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,CD,HK,HK,HK,HK,HK,HK,HK,HK},
/*Content-lengt   CD*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,CE,HK,HK,HK,HK,HK,HK,HK},
/*Content-length  CE*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,HK,HK,CF,HK,HK,HK,HK,HK},
/*Content-length: CF*/ {__,__,__,__,__,CG,__,__,__,__,__,HK,__,__,__,__,HK,__,HK,HK,HK,HK,HK,HK,HK,HK,HK,HK,HK},
/*Content-length: CG*/ {__,KC,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/* CL value       CV*/ {__,__,__,VE,VE,__,__,__,__,__,__,__,__,__,__,VC,__,__,__,__,__,__,__,__,__,__,__,__,__},
/* 		  E1*/ {__,H1,__,__,__,__,__,__,__,PC,PC,PC,PC,__,PC,EA,PC,__,EA,EA,PC,PC,PC,PC,PC,PC,EA,PC,PC},
/* 		  E2*/ {__,H1,__,__,__,__,__,__,__,PC,PC,PC,PC,__,PC,EB,PC,__,EB,EB,PC,PC,PC,PC,PC,PC,EB,PC,PC},
/* 		  FI*/ {__,__,__,__,EH,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__}
};

/* compiled as jump table by gcc */
inline int parse_http_char(struct _http_parser *parser, const unsigned char c)
{
#define HTTP_FLG_POST (1 << 31)
#define HTTP_FLG_QS (1 << 30)
#define HTTP_FLG_BODYCONTENT (1 << 29)


#define HTTP_ISQS() (parser->rx & HTTP_FLG_QS)
#define HTTP_ISPOST() (parser->rx & HTTP_FLG_POST)
#define HTTP_ISBODYCONTENT() (parser->rx & HTTP_FLG_BODYCONTENT)

#define HTTP_PATHORQS (HTTP_ISQS() ? (HTTP_ISBODYCONTENT() ? HTTP_BODY_CHAR : HTTP_QS_CHAR) : HTTP_PATH_CHAR)
	
	parser_class c_classe = ascii_class[c];
	int8_t state;
	unsigned char ch;
	
	parser->step++;
	
	if (c_classe == C_NUL) return 0;
	
	state = state_transition_table[parser->state][c_classe]; /* state > 0, action < 0 */

	if (state >= 0) {
		parser->state = state;
	} else {
		
		switch(state) {
			case MG: /* GET detected */
				parser->callback(parser->ctx, HTTP_METHOD, HTTP_GET, parser->step);
				parser->state = PT;
				break;
			case MP:
				parser->callback(parser->ctx, HTTP_METHOD, HTTP_POST, parser->step);
				parser->rx |= HTTP_FLG_POST;
				parser->state = PT;
				break;
			case HA: /* HTTP Major */
				parser->callback(parser->ctx, HTTP_VERSION_MAJOR, c-'0', parser->step);
				parser->state = H7;
				break;
			case HB: /* HTTP Minor */
				parser->callback(parser->ctx, HTTP_VERSION_MINOR, c-'0', parser->step);
				parser->state = EL;
				break;
			case KH: /* Header key */
				parser->callback(parser->ctx, HTTP_HEADER_KEY, 0, parser->step);
				parser->state = HV;
				break;
			case VH: /* Header value */
				parser->callback(parser->ctx, HTTP_HEADER_VAL, 0, parser->step);
				parser->state = (c_classe == C_CR ? ER : C1); /* \r\n or \n */
				break;
			case KC: /* Content length */
				parser->callback(parser->ctx, HTTP_HEADER_KEY, 0, parser->step);
				parser->state = CV;
				break;
			case VC: /* Content length digit */
				if ((parser->cl = (parser->cl*10) + (c - '0')) > MAX_CL) {
					return 0;
				}
				parser->state = CV;
				break;
			case VE:
				parser->callback(parser->ctx, HTTP_CL_VAL, parser->cl, parser->step);
				parser->state = (c_classe == C_CR ? ER : C1); /* \r\n or \n */
				break;
			case EA: /* first char from %x */
				ch = (unsigned char) (c | 0x20); /* tolower */
				
				if (ch >= '0' && ch <= '9') {
					parser->rx |= (unsigned char)(ch - '0') | (c << 8); /* convert to int and store the original char to 0xXXFF */
				} else if (ch >= 'a' && ch <= 'f') {
					parser->rx |= (unsigned char)(ch - 'a' + 10) | (c << 8);
				}
				parser->state = E2;
				break;
			case EB: /* second char from %xx */
				ch = (unsigned char) (c | 0x20); /* tolower */
				
				if (ch >= '0' && ch <= '9') {
					ch = (unsigned char) (((parser->rx & 0x000000ff /* mask the original char */) << 4) + ch - '0');
				} else if (ch >= 'a' && ch <= 'f') {
					ch = (unsigned char) (((parser->rx & 0x000000ff) << 4) + ch - 'a' + 10);
				}
				
				parser->callback(parser->ctx, HTTP_PATHORQS, ch, parser->step); /* return the decoded char */
				parser->state = PT;
				break;
			case PC: PC:
			case QS:
			case BC:
				switch(parser->state) {
				case E1:
					parser->callback(parser->ctx, HTTP_PATHORQS, '%', parser->step);
					parser->rx &= 0xF0000000;
					break;
				case E2:
					parser->callback(parser->ctx, HTTP_PATHORQS, '%', parser->step);
					parser->callback(parser->ctx, HTTP_PATHORQS, parser->rx >> 8 /* restor original char */, parser->step);
					break;
				default:
					break;
				}
				parser->state = PT;
				if (state == QS && !HTTP_ISQS()) {
					parser->rx |= HTTP_FLG_QS;
					parser->callback(parser->ctx, HTTP_PATH_END, 0, parser->step);
					break;
				}
				parser->callback(parser->ctx, HTTP_PATHORQS, c, parser->step);
				break;
			case PE:
				if (!HTTP_ISQS()) {
					parser->callback(parser->ctx, HTTP_PATH_END, 0, parser->step);
				} else if (HTTP_ISBODYCONTENT()) {
					parser->state = PC;
					goto PC;
				}
				parser->state = H1;
				
				break;
			case EH:
				parser->callback(parser->ctx, HTTP_HEADER_END, 0, parser->step);
				if (HTTP_ISPOST()) {
					parser->state = PT;
					parser->rx = HTTP_FLG_POST | HTTP_FLG_QS | HTTP_FLG_BODYCONTENT;
					break;
				}
				parser->callback(parser->ctx, HTTP_READY, 0, parser->step);
				break;
			default:
				return 0;
		}
	}
	
	return 1;
}



/* also compiled as jump table */
/*
static int parse_callback(void *ctx, callback_type type, int value, uint32_t step)
{
	switch(type) {
		case HTTP_METHOD:
			switch(value) {
				case HTTP_GET:
					printf("GET method detected\n");
					break;
				case HTTP_POST:
					printf("POST method detected\n");
					break;
			}
			break;
		case HTTP_PATH_END:
			printf("\n", step);
			break;
		case HTTP_PATH_CHAR: 
			printf("%c", (unsigned char)value);
			break;
		case HTTP_VERSION_MAJOR:
		case HTTP_VERSION_MINOR:
			printf("Version detected %i\n", value);
			break;
		case HTTP_HEADER_KEY:
			printf("Header key\n");
			break;
		case HTTP_HEADER_VAL:
			printf("Header value\n");
			break;
		case HTTP_CL_VAL:
			printf("CL value : %i\n", value);
			break;
		case HTTP_HEADER_END:
			printf("--------- HEADERS END ---------\n");
			break;
		default:
			break;
	}
	return 1;
}

*/

#if 0
/* TEST */
int main()
{
	int length = 0, i;
	struct _http_parser p;
	
	/* Process BYTE_GET/POST opti check before running the parser */

	PARSER_RESET(&p);
	
	p.ctx = &p;
	p.callback = parse_callback;
	
	char chaine[] = "POST /foo/bar/beer HTTP/1.1\nCONTENT-LENGTH: 320900\r\nfoo: bar\n";
	
	/* TODO implement a "duff device" here */
	for (i = 0, length = strlen(chaine); i < length; i++) {
		if (parse_http_char(&p, chaine[i]) == 0) {
			printf("fail\n");
			break;
		}
	}
}
#endif

