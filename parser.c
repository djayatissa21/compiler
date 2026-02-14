/*
 * parser.c - A Simple Integer Language Interpreter
 * =================================================
 * Reads a source file, tokenises it, parses it according to a
 * Context-Free Grammar, detects syntax/semantic errors, executes
 * arithmetic operations, and displays results via print().
 *
 * Compile : gcc -o parser parser.c
 * Run     : ./parser inputfile
 *
 * ---------------------------------------------------------------
 * CONTEXT-FREE GRAMMAR (CFG)
 * ---------------------------------------------------------------
 *   Program      ->  StmtList EOF
 *   StmtList     ->  Stmt StmtList
 *                 |   epsilon
 *   Stmt         ->  Declaration
 *                 |   PrintStmt
 *   Declaration  ->  "int" IDENTIFIER "=" Expr ";"
 *   PrintStmt    ->  "print" "(" Expr ")" ";"
 *   Expr         ->  Term  (( "+" | "-" ) Term)*
 *   Term         ->  Factor (( "*" | "/" ) Factor)*
 *   Factor       ->  INTEGER
 *                 |   IDENTIFIER
 *                 |   "(" Expr ")"
 * ---------------------------------------------------------------
 *
 * TOKEN CATEGORIES
 * ---------------------------------------------------------------
 *   Keywords    : int, print
 *   Identifiers : [a-zA-Z_][a-zA-Z0-9_]*
 *   Integers    : [0-9]+
 *   Operators   : +  -  *  /  =
 *   Punctuation : (  )  ;
 * ---------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ======================== TOKEN DEFINITIONS ======================== */

typedef enum {
    /* Keywords */
    TOK_INT,        /* "int"   */
    TOK_PRINT,      /* "print" */

    /* Literals & identifiers */
    TOK_IDENTIFIER, /* variable names   */
    TOK_INTEGER,    /* integer literals  */

    /* Operators */
    TOK_ASSIGN,     /* =  */
    TOK_PLUS,       /* +  */
    TOK_MINUS,      /* -  */
    TOK_STAR,       /* *  */
    TOK_SLASH,      /* /  */

    /* Punctuation */
    TOK_LPAREN,     /* (  */
    TOK_RPAREN,     /* )  */
    TOK_SEMICOLON,  /* ;  */

    /* Special */
    TOK_EOF,        /* end of file   */
    TOK_UNKNOWN     /* unrecognised  */
} TokenType;

/* Human-readable names for each token type (used in error messages) */
static const char *token_type_name(TokenType t) {
    switch (t) {
        case TOK_INT:        return "keyword 'int'";
        case TOK_PRINT:      return "keyword 'print'";
        case TOK_IDENTIFIER: return "identifier";
        case TOK_INTEGER:    return "integer literal";
        case TOK_ASSIGN:     return "'='";
        case TOK_PLUS:       return "'+'";
        case TOK_MINUS:      return "'-'";
        case TOK_STAR:       return "'*'";
        case TOK_SLASH:      return "'/'";
        case TOK_LPAREN:     return "'('";
        case TOK_RPAREN:     return "')'";
        case TOK_SEMICOLON:  return "';'";
        case TOK_EOF:        return "end of file";
        case TOK_UNKNOWN:    return "unknown token";
    }
    return "?";
}

/* A single token produced by the tokeniser */
typedef struct {
    TokenType type;
    char      text[256]; /* lexeme text                  */
    int       int_value; /* numeric value (for integers) */
    int       line;      /* source line number            */
    int       col;       /* source column number          */
} Token;

/* ======================== TOKENISER (LEXER) ======================== */

#define MAX_TOKENS 4096

static Token tokens[MAX_TOKENS];
static int   token_count = 0;

/*
 * tokenise() - Scans the entire source string and fills the tokens[] array.
 * Returns 0 on success, -1 on a lexical error.
 */
static int tokenise(const char *src) {
    int i = 0;
    int line = 1;
    int col  = 1;

    token_count = 0;

    while (src[i] != '\0') {

        /* ---------- skip whitespace ---------- */
        if (src[i] == ' ' || src[i] == '\t' || src[i] == '\r') {
            col++;
            i++;
            continue;
        }
        if (src[i] == '\n') {
            line++;
            col = 1;
            i++;
            continue;
        }

        /* ---------- skip single-line comments // ---------- */
        if (src[i] == '/' && src[i + 1] == '/') {
            while (src[i] != '\0' && src[i] != '\n') { i++; }
            continue;
        }

        /* ---------- identifiers & keywords ---------- */
        if (isalpha((unsigned char)src[i]) || src[i] == '_') {
            int start = i;
            int start_col = col;
            while (isalnum((unsigned char)src[i]) || src[i] == '_') {
                i++;
                col++;
            }
            int len = i - start;
            if (len >= 255) len = 255;

            Token tok;
            memset(&tok, 0, sizeof(tok));
            strncpy(tok.text, &src[start], len);
            tok.text[len] = '\0';
            tok.line = line;
            tok.col  = start_col;

            /* Check for keywords */
            if (strcmp(tok.text, "int") == 0) {
                tok.type = TOK_INT;
            } else if (strcmp(tok.text, "print") == 0) {
                tok.type = TOK_PRINT;
            } else {
                tok.type = TOK_IDENTIFIER;
            }

            if (token_count >= MAX_TOKENS) {
                fprintf(stderr, "Error: too many tokens (max %d)\n", MAX_TOKENS);
                return -1;
            }
            tokens[token_count++] = tok;
            continue;
        }

        /* ---------- integer literals ---------- */
        if (isdigit((unsigned char)src[i])) {
            int start = i;
            int start_col = col;
            while (isdigit((unsigned char)src[i])) {
                i++;
                col++;
            }
            /* Check that the integer is not immediately followed by a letter
               (e.g., "123abc" is not valid) */
            if (isalpha((unsigned char)src[i]) || src[i] == '_') {
                fprintf(stderr,
                    "Lexical Error [line %d, col %d]: invalid token '%c' after integer literal\n",
                    line, start_col, src[i]);
                return -1;
            }

            int len = i - start;
            if (len >= 255) len = 255;

            Token tok;
            memset(&tok, 0, sizeof(tok));
            strncpy(tok.text, &src[start], len);
            tok.text[len] = '\0';
            tok.type      = TOK_INTEGER;
            tok.int_value = atoi(tok.text);
            tok.line      = line;
            tok.col       = start_col;

            if (token_count >= MAX_TOKENS) {
                fprintf(stderr, "Error: too many tokens (max %d)\n", MAX_TOKENS);
                return -1;
            }
            tokens[token_count++] = tok;
            continue;
        }

        /* ---------- single-character tokens ---------- */
        {
            Token tok;
            memset(&tok, 0, sizeof(tok));
            tok.text[0] = src[i];
            tok.text[1] = '\0';
            tok.line     = line;
            tok.col      = col;

            switch (src[i]) {
                case '=': tok.type = TOK_ASSIGN;    break;
                case '+': tok.type = TOK_PLUS;      break;
                case '-': tok.type = TOK_MINUS;     break;
                case '*': tok.type = TOK_STAR;      break;
                case '/': tok.type = TOK_SLASH;     break;
                case '(': tok.type = TOK_LPAREN;    break;
                case ')': tok.type = TOK_RPAREN;    break;
                case ';': tok.type = TOK_SEMICOLON; break;
                default:
                    fprintf(stderr,
                        "Lexical Error [line %d, col %d]: unexpected character '%c'\n",
                        line, col, src[i]);
                    return -1;
            }

            if (token_count >= MAX_TOKENS) {
                fprintf(stderr, "Error: too many tokens (max %d)\n", MAX_TOKENS);
                return -1;
            }
            tokens[token_count++] = tok;
            i++;
            col++;
        }
    }

    /* Append EOF token */
    {
        Token eof;
        memset(&eof, 0, sizeof(eof));
        eof.type = TOK_EOF;
        strcpy(eof.text, "EOF");
        eof.line = line;
        eof.col  = col;
        tokens[token_count++] = eof;
    }

    return 0;
}

/* ======================== SYMBOL TABLE ======================== */
/*
 * A simple variable store: maps identifier names to integer values.
 * Used for semantic checking (undeclared / redeclared variables)
 * and for execution (storing and retrieving values).
 */

#define MAX_VARS 256

typedef struct {
    char name[256];
    int  value;
    int  declared; /* 1 = yes */
} Variable;

static Variable sym_table[MAX_VARS];
static int      sym_count = 0;

/* Look up a variable; returns pointer or NULL */
static Variable *sym_lookup(const char *name) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0)
            return &sym_table[i];
    }
    return NULL;
}

/* Declare a new variable; returns pointer or NULL on redeclaration */
static Variable *sym_declare(const char *name, int line) {
    if (sym_lookup(name)) {
        fprintf(stderr,
            "Semantic Error [line %d]: variable '%s' is already declared\n",
            line, name);
        return NULL;
    }
    if (sym_count >= MAX_VARS) {
        fprintf(stderr, "Error: too many variables (max %d)\n", MAX_VARS);
        return NULL;
    }
    Variable *v = &sym_table[sym_count++];
    strncpy(v->name, name, 255);
    v->name[255] = '\0';
    v->declared = 1;
    v->value    = 0;
    return v;
}

/* ============= RECURSIVE-DESCENT PARSER + INTERPRETER ============= */
/*
 * The parser is a recursive-descent parser that directly evaluates
 * expressions as it parses (interpreter mode).  It follows the CFG
 * exactly.
 *
 * A global `pos` index walks through the tokens[] array.
 * A global `had_error` flag is set on the first error; once set,
 * parsing continues to report as many errors as practical, but
 * execution results are suppressed.
 */

static int pos       = 0;   /* current position in tokens[] */
static int had_error = 0;   /* set to 1 on first error      */

/* Return the current token without consuming it */
static Token *current_token(void) {
    return &tokens[pos];
}

/* Consume the current token and advance */
static Token *advance(void) {
    Token *t = &tokens[pos];
    if (t->type != TOK_EOF) pos++;
    return t;
}

/* Check if the current token matches a given type */
static int check(TokenType type) {
    return current_token()->type == type;
}

/*
 * expect() - Consume the current token if it matches the expected type.
 * On mismatch, report a syntax error and set had_error.
 * Returns a pointer to the consumed token (or the mismatched one).
 */
static Token *expect(TokenType type) {
    Token *t = current_token();
    if (t->type == type) {
        return advance();
    }
    fprintf(stderr,
        "Syntax Error [line %d, col %d]: expected %s but found %s ('%s')\n",
        t->line, t->col,
        token_type_name(type),
        token_type_name(t->type),
        t->text);
    had_error = 1;
    return t;
}

/* Forward declarations for recursive descent */
static int parse_expr(void);
static int parse_term(void);
static int parse_factor(void);

/*
 * Factor -> INTEGER | IDENTIFIER | "(" Expr ")"
 */
static int parse_factor(void) {
    Token *t = current_token();

    /* INTEGER literal */
    if (t->type == TOK_INTEGER) {
        advance();
        return t->int_value;
    }

    /* IDENTIFIER - look up variable value */
    if (t->type == TOK_IDENTIFIER) {
        advance();
        Variable *v = sym_lookup(t->text);
        if (!v) {
            fprintf(stderr,
                "Semantic Error [line %d, col %d]: undeclared variable '%s'\n",
                t->line, t->col, t->text);
            had_error = 1;
            return 0;
        }
        return v->value;
    }

    /* Parenthesised expression */
    if (t->type == TOK_LPAREN) {
        advance(); /* consume '(' */
        int val = parse_expr();
        expect(TOK_RPAREN);
        return val;
    }

    /* Error recovery: unexpected token */
    fprintf(stderr,
        "Syntax Error [line %d, col %d]: expected expression but found %s ('%s')\n",
        t->line, t->col,
        token_type_name(t->type),
        t->text);
    had_error = 1;
    /* Skip the bad token to avoid infinite loops */
    if (t->type != TOK_EOF) advance();
    return 0;
}

/*
 * Term -> Factor (( "*" | "/" ) Factor)*
 */
static int parse_term(void) {
    int left = parse_factor();

    while (check(TOK_STAR) || check(TOK_SLASH)) {
        Token *op = advance();
        int right = parse_factor();
        if (op->type == TOK_STAR) {
            left = left * right;
        } else {
            if (right == 0) {
                fprintf(stderr,
                    "Runtime Error [line %d, col %d]: division by zero\n",
                    op->line, op->col);
                had_error = 1;
                left = 0;
            } else {
                left = left / right;
            }
        }
    }
    return left;
}

/*
 * Expr -> Term (( "+" | "-" ) Term)*
 */
static int parse_expr(void) {
    int left = parse_term();

    while (check(TOK_PLUS) || check(TOK_MINUS)) {
        Token *op = advance();
        int right = parse_term();
        if (op->type == TOK_PLUS) {
            left = left + right;
        } else {
            left = left - right;
        }
    }
    return left;
}

/*
 * Declaration -> "int" IDENTIFIER "=" Expr ";"
 */
static void parse_declaration(void) {
    expect(TOK_INT);   /* consume "int" */

    Token *id = expect(TOK_IDENTIFIER);

    expect(TOK_ASSIGN); /* consume "=" */

    int value = parse_expr();

    expect(TOK_SEMICOLON); /* consume ";" */

    /* Semantic action: declare the variable and store the value */
    if (!had_error) {
        Variable *v = sym_declare(id->text, id->line);
        if (v) {
            v->value = value;
        } else {
            had_error = 1; /* redeclaration error already printed */
        }
    }
}

/*
 * PrintStmt -> "print" "(" Expr ")" ";"
 */
static void parse_print(void) {
    expect(TOK_PRINT);  /* consume "print" */
    expect(TOK_LPAREN); /* consume "("     */

    int value = parse_expr();

    expect(TOK_RPAREN);    /* consume ")" */
    expect(TOK_SEMICOLON); /* consume ";" */

    /* Execute: print the value only if no errors so far */
    if (!had_error) {
        printf("%d\n", value);
    }
}

/*
 * Stmt -> Declaration | PrintStmt
 */
static void parse_stmt(void) {
    Token *t = current_token();

    if (t->type == TOK_INT) {
        parse_declaration();
    } else if (t->type == TOK_PRINT) {
        parse_print();
    } else {
        fprintf(stderr,
            "Syntax Error [line %d, col %d]: expected 'int' or 'print' "
            "at start of statement but found %s ('%s')\n",
            t->line, t->col,
            token_type_name(t->type),
            t->text);
        had_error = 1;
        /* Skip token for error recovery */
        if (t->type != TOK_EOF) advance();
    }
}

/*
 * Program -> StmtList EOF
 * StmtList -> Stmt StmtList | epsilon
 */
static void parse_program(void) {
    while (!check(TOK_EOF)) {
        parse_stmt();
    }
    expect(TOK_EOF);
}

/* ======================== FILE READING ======================== */

/*
 * read_file() - Reads the entire contents of a file into a
 * dynamically allocated string.  Returns NULL on failure.
 */
static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file '%s'\n", path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)malloc(length + 1);
    if (!buf) {
        fprintf(stderr, "Error: out of memory\n");
        fclose(fp);
        return NULL;
    }

    fread(buf, 1, length, fp);
    buf[length] = '\0';
    fclose(fp);
    return buf;
}

/* ======================== MAIN ======================== */

int main(int argc, char *argv[]) {
    /* --- Check command-line arguments --- */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source_file>\n", argv[0]);
        return 1;
    }

    /* --- Read source file --- */
    char *source = read_file(argv[1]);
    if (!source) return 1;

    /* --- Phase 1: Tokenisation --- */
    if (tokenise(source) != 0) {
        free(source);
        return 1;
    }

    /* --- Phase 2 & 3: Parsing + Execution --- */
    pos       = 0;
    had_error = 0;
    sym_count = 0;

    parse_program();

    free(source);

    if (had_error) {
        fprintf(stderr, "\nParsing/execution failed due to errors above.\n");
        return 1;
    }

    return 0;
}
