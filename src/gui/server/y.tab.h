#ifndef YYERRCODE
#define YYERRCODE 256
#endif

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
typedef union {
 int number;
 char *username;
} YYSTYPE;
extern YYSTYPE yylval;
