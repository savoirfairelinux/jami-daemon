#ifndef lint
static char const 
yyrcsid[] = "$FreeBSD: src/usr.bin/yacc/skeleton.c,v 1.28 2000/01/17 02:04:06 bde Exp $";
#endif
#include <stdlib.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING() (yyerrflag!=0)
static int yygrowstack();
#define YYPREFIX "yy"
#line 2 "protocol.y"
#include <stdio.h>
#include <string.h>
#include <iostream>
#line 7 "protocol.y"
typedef union {
 int number;
 char *username;
} YYSTYPE;
#line 13 "protocol.y"

#include "TCPStreamLexer.h"
#define YYPARSE_PARAM tsl
#define YYPARSE_PARAM_TYPE TCPStreamLexer*


int 
yyerror(const char *str) {}
extern "C" 
{
	int yyparse(TCPStreamLexer *tsl);
}
int 
yylex() 
{ 
	return lexer->yylex(); 
}
void 
TCPStreamLexer::parse( ) 
{
 yyparse(this); 
}
void 
TCPStreamLexer::sip(YYSTYPE &y)
{
	*this << "(debug) saw url: " << y.username << std::endl;
}
void 
TCPStreamLexer::callsip(YYSTYPE &y) 
{
	*this << "200 Calling " << y.username << std::endl;
}
#line 59 "y.tab.c"
#define YYERRCODE 256
#define AROBASE 257
#define SIP_URL_PREFIX 258
#define CMD_CALL 259
#define CMD_CONFIG 260
#define CMD_HANGUP 261
#define CMD_HOLD 262
#define CMD_TRANSFER 263
#define CMD_QUIT 264
#define TOKEN_VOICEMAIL 265
#define TOKEN_RECEPTION 266
#define TOKEN_ALL 267
#define TOKEN_GET 268
#define TOKEN_SET 269
#define NUMBER 270
#define USERNAME 271
const short yylhs[] = {                                        -1,
    0,    0,    1,    1,    1,    1,    1,    1,    8,    2,
    2,    2,    3,    3,    3,    4,    4,    6,    5,    7,
};
const short yylen[] = {                                         2,
    0,    2,    1,    1,    1,    1,    1,    1,    4,    2,
    2,    2,    3,    3,    4,    2,    2,    2,    3,    1,
};
const short yydefred[] = {                                      1,
    0,    0,    0,    0,    0,    0,   20,    2,    3,    4,
    5,    6,    7,    8,    0,   11,   10,   12,    0,    0,
   16,   17,   18,    0,    0,   13,   14,    0,   19,    0,
   15,    9,
};
const short yydgoto[] = {                                       1,
    8,    9,   10,   11,   12,   13,   14,   18,
};
const short yysindex[] = {                                      0,
 -250, -258, -253, -264, -268, -252,    0,    0,    0,    0,
    0,    0,    0,    0, -267,    0,    0,    0, -266, -254,
    0,    0,    0, -239, -237,    0,    0, -249,    0, -248,
    0,    0,
};
const short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,
};
const short yygindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,   -3,
};
#define YYTABLESIZE 23
const short yytable[] = {                                      15,
   26,   23,   21,   25,   27,   22,   16,   17,    2,    3,
    4,    5,    6,    7,   19,   20,   28,   24,   15,   30,
   29,   31,   32,
};
const short yycheck[] = {                                     258,
  267,  270,  267,  271,  271,  270,  265,  266,  259,  260,
  261,  262,  263,  264,  268,  269,  271,  270,  258,  257,
   24,  271,  271,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 271
#if YYDEBUG
const char * const yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"AROBASE","SIP_URL_PREFIX",
"CMD_CALL","CMD_CONFIG","CMD_HANGUP","CMD_HOLD","CMD_TRANSFER","CMD_QUIT",
"TOKEN_VOICEMAIL","TOKEN_RECEPTION","TOKEN_ALL","TOKEN_GET","TOKEN_SET",
"NUMBER","USERNAME",
};
const char * const yyrule[] = {
"$accept : commands",
"commands :",
"commands : commands command",
"command : command_call",
"command : command_config",
"command : command_hangup",
"command : command_transfer",
"command : command_hold",
"command : command_quit",
"sip_url : SIP_URL_PREFIX USERNAME AROBASE USERNAME",
"command_call : CMD_CALL TOKEN_RECEPTION",
"command_call : CMD_CALL TOKEN_VOICEMAIL",
"command_call : CMD_CALL sip_url",
"command_config : CMD_CONFIG TOKEN_GET TOKEN_ALL",
"command_config : CMD_CONFIG TOKEN_GET USERNAME",
"command_config : CMD_CONFIG TOKEN_SET USERNAME USERNAME",
"command_hangup : CMD_HANGUP TOKEN_ALL",
"command_hangup : CMD_HANGUP NUMBER",
"command_hold : CMD_HOLD NUMBER",
"command_transfer : CMD_TRANSFER NUMBER sip_url",
"command_quit : CMD_QUIT",
};
#endif
#if YYDEBUG
#include <stdio.h>
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
int yystacksize;
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack()
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    i = yyssp - yyss;
    newss = yyss ? (short *)realloc(yyss, newsize * sizeof *newss) :
      (short *)malloc(newsize * sizeof *newss);
    if (newss == NULL)
        return -1;
    yyss = newss;
    yyssp = newss + i;
    newvs = yyvs ? (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs) :
      (YYSTYPE *)malloc(newsize * sizeof *newvs);
    if (newvs == NULL)
        return -1;
    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab

#ifndef YYPARSE_PARAM
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG void
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif	/* ANSI-C/C++ */
#else	/* YYPARSE_PARAM */
#ifndef YYPARSE_PARAM_TYPE
#define YYPARSE_PARAM_TYPE void *
#endif
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG YYPARSE_PARAM_TYPE YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL YYPARSE_PARAM_TYPE YYPARSE_PARAM;
#endif	/* ANSI-C/C++ */
#endif	/* ! YYPARSE_PARAM */

int
yyparse (YYPARSE_PARAM_ARG)
    YYPARSE_PARAM_DECL
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register const char *yys;

    if ((yys = getenv("YYDEBUG")))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate])) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#if defined(lint) || defined(__GNUC__)
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#if defined(lint) || defined(__GNUC__)
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 9:
#line 69 "protocol.y"
{
		yyval=yyvsp[-2];
		tsl->sip(yyvsp[-2]);
		printf ("(debug) saw url: %s\n", yyval);
	}
break;
case 10:
#line 78 "protocol.y"
{
		printf ("200 Calling Receptionist\n");
	}
break;
case 11:
#line 83 "protocol.y"
{
		printf ("200 Calling VoiceMail\n");
	}
break;
case 12:
#line 88 "protocol.y"
{
		tsl->callsip(yyvsp[0]);
		printf ("200 Calling \"%s\"\n", yyvsp[0]);
	}
break;
case 13:
#line 96 "protocol.y"
{
		printf ("200 Sending ALL config variables\n");
	}
break;
case 14:
#line 100 "protocol.y"
{
		printf ("200 Sending config variable \"%s\"\n", yyvsp[0]);
	}
break;
case 15:
#line 104 "protocol.y"
{
		printf ("200 Setting config variable \"%s\"=\"%s\"\n", yyvsp[-1],yyvsp[0]);
	}
break;
case 16:
#line 111 "protocol.y"
{
		printf ("200 Hangup all calls  !!!\n");
	}
break;
case 17:
#line 115 "protocol.y"
{
		printf ("200 Hangup call %d\n", yyvsp[0]);
	}
break;
case 18:
#line 122 "protocol.y"
{
		printf ("200 Call %d on hold.\n", yyvsp[0]);
	}
break;
case 19:
#line 129 "protocol.y"
{
		printf ("200 Transferring call %d to \"%s\"\n", yyvsp[-1], yyvsp[0]);
	}
break;
case 20:
#line 136 "protocol.y"
{
		printf ("200 Quitting...Bye Bye\n");
	}
break;
#line 457 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
