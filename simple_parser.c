/*
 * simple_parser.c - Minimal Integer Language Parser & Interpreter
 *
 * Compile: gcc -o simple_parser simple_parser.c
 * Run:     ./simple_parser <source_file>
 *
 * Grammar:
 *   Program -> Stmt* EOF
 *   Stmt    -> "int" ID "=" Expr ";"
 *            | "print" "(" Expr ")" ";"
 *   Expr    -> Term (("+" | "-") Term)*
 *   Term    -> Factor (("*" | "/") Factor)*
 *   Factor  -> NUMBER | ID | "(" Expr ")"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- Token types ---- */
enum {
    T_INT, T_PRINT, T_ID, T_NUM,
    T_ASSIGN, T_PLUS, T_MINUS, T_STAR, T_SLASH,
    T_LPAREN, T_RPAREN, T_SEMI, T_EOF, T_ERR
};

/* ---- Global state ---- */
static const char *src;       /* source pointer   */
static int   tok;             /* current token     */
static char  tok_text[256];   /* token text        */
static int   tok_num;         /* numeric value     */

/* ---- Symbol table (simple array) ---- */
#define MAX_VARS 100
static char var_names[MAX_VARS][256];
static int  var_vals[MAX_VARS];
static int  var_count = 0;

static int var_get(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(var_names[i], name) == 0) return var_vals[i];
    fprintf(stderr, "Error: undefined variable '%s'\n", name);
    exit(1);
}

static void var_set(const char *name, int val) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_names[i], name) == 0) {
            fprintf(stderr, "Error: variable '%s' already declared\n", name);
            exit(1);
        }
    }
    strcpy(var_names[var_count], name);
    var_vals[var_count] = val;
    var_count++;
}

/* ---- Lexer: read next token ---- */
static void next(void) {
    /* skip whitespace and comments */
    while (*src) {
        if (isspace((unsigned char)*src)) { src++; continue; }
        if (src[0] == '/' && src[1] == '/') {
            while (*src && *src != '\n') src++;
            continue;
        }
        break;
    }

    if (*src == '\0') { tok = T_EOF; return; }

    /* identifiers and keywords */
    if (isalpha((unsigned char)*src) || *src == '_') {
        int i = 0;
        while (isalnum((unsigned char)*src) || *src == '_')
            tok_text[i++] = *src++;
        tok_text[i] = '\0';

        if (strcmp(tok_text, "int") == 0)        tok = T_INT;
        else if (strcmp(tok_text, "print") == 0)  tok = T_PRINT;
        else                                      tok = T_ID;
        return;
    }

    /* numbers */
    if (isdigit((unsigned char)*src)) {
        int i = 0;
        while (isdigit((unsigned char)*src))
            tok_text[i++] = *src++;
        tok_text[i] = '\0';
        tok_num = atoi(tok_text);
        tok = T_NUM;
        return;
    }

    /* single-char tokens */
    char c = *src++;
    switch (c) {
        case '=': tok = T_ASSIGN; return;
        case '+': tok = T_PLUS;   return;
        case '-': tok = T_MINUS;  return;
        case '*': tok = T_STAR;   return;
        case '/': tok = T_SLASH;  return;
        case '(': tok = T_LPAREN; return;
        case ')': tok = T_RPAREN; return;
        case ';': tok = T_SEMI;   return;
        default:
            fprintf(stderr, "Error: unexpected character '%c'\n", c);
            exit(1);
    }
}

/* ---- Expect a specific token or error ---- */
static void expect(int expected) {
    if (tok != expected) {
        fprintf(stderr, "Error: unexpected token '%s'\n", tok_text);
        exit(1);
    }
    next();
}

/* ---- Recursive-descent parser ---- */

static int parse_expr(void);

static int parse_factor(void) {
    if (tok == T_NUM) {
        int val = tok_num;
        next();
        return val;
    }
    if (tok == T_ID) {
        char name[256];
        strcpy(name, tok_text);
        next();
        return var_get(name);
    }
    if (tok == T_LPAREN) {
        next();
        int val = parse_expr();
        expect(T_RPAREN);
        return val;
    }
    fprintf(stderr, "Error: expected expression\n");
    exit(1);
}

static int parse_term(void) {
    int left = parse_factor();
    while (tok == T_STAR || tok == T_SLASH) {
        int op = tok;
        next();
        int right = parse_factor();
        if (op == T_STAR) left *= right;
        else {
            if (right == 0) { fprintf(stderr, "Error: division by zero\n"); exit(1); }
            left /= right;
        }
    }
    return left;
}

static int parse_expr(void) {
    int left = parse_term();
    while (tok == T_PLUS || tok == T_MINUS) {
        int op = tok;
        next();
        int right = parse_term();
        if (op == T_PLUS) left += right;
        else              left -= right;
    }
    return left;
}

/* Stmt -> "int" ID "=" Expr ";" | "print" "(" Expr ")" ";" */
static void parse_stmt(void) {
    if (tok == T_INT) {
        next();
        if (tok != T_ID) { fprintf(stderr, "Error: expected identifier\n"); exit(1); }
        char name[256];
        strcpy(name, tok_text);
        next();
        expect(T_ASSIGN);
        int val = parse_expr();
        expect(T_SEMI);
        var_set(name, val);
    }
    else if (tok == T_PRINT) {
        next();
        expect(T_LPAREN);
        int val = parse_expr();
        expect(T_RPAREN);
        expect(T_SEMI);
        printf("%d\n", val);
    }
    else {
        fprintf(stderr, "Error: expected 'int' or 'print'\n");
        exit(1);
    }
}

/* Program -> Stmt* EOF */
static void parse_program(void) {
    while (tok != T_EOF)
        parse_stmt();
}

/* ---- Main ---- */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source_file>\n", argv[0]);
        return 1;
    }

    /* read entire file */
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) { fprintf(stderr, "Error: cannot open '%s'\n", argv[1]); return 1; }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, fp);
    buf[len] = '\0';
    fclose(fp);

    /* parse and execute */
    src = buf;
    next();
    parse_program();

    free(buf);
    return 0;
}
