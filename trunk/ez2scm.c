/**
 * @file ez2scm.c
 * @author Srikumar K. S. (srikumarks *at* mac *dot* com)
 * @date 29 July 2006
 *
 * Copyright 2006 (c) Srikumar K. S.
 * This source code is licensed to the public by the copyright
 * holder stated above under the terms of the Artistic License 
 * as described in -
 * http://www.opensource.org/licenses/artistic-license.php
 *
 * DISCLAIMER:
 * The author is not to be held liable for any loss of property
 * or life or any such damages that may be directly or indirectly
 * caused by this piece of code. Damages that might result 
 * include hair falling off due to lack of documentation, 
 * insomnia after suddenly discovering the elegance of lisp, 
 * and such inexplicable phenomena as well, for which too the author
 * is not to be held liable. You've been warned!
 *
 * Usage: ez2scm infile.ez outfile.scm
 * Converts the "infile.ez" file given in ezscheme syntax to
 * "outfile.scm" which will be in lisp/scheme syntax with some
 * minimal pretty printing.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

typedef enum
{
	ConsCell,
	IntCell,
	FractionalCell,
	SymbolCell,
	StringCell,
	QuoteCell
} CellType;

typedef struct 
{
	int head, tail;
} Cons;

typedef struct
{
	char *name; 
	char *name_end;
} Symbol;

typedef struct
{
	char *str;
	char *str_end;
} String;

typedef union
{
	Cons cons;
	Symbol symbol;
	String string;
	int quoted;
	long i;
	double f;	
} CellData;

typedef struct
{
	CellType type;
	CellData data;
} Cell;


char *input = NULL;
int input_size = 0;
Cell *cells = NULL;
int cells_size = 0, cells_top = 0;

int newcell( CellType type ) {
	if ( cells ) {
		if ( cells_top >= cells_size ) {
			cells_size *= 2;
			cells = (Cell*)realloc( cells, cells_size * sizeof(Cell) );
			assert( cells != NULL );
		}
	} else {
		cells_size = 256;
		cells = (Cell*)calloc( sizeof(Cell), cells_size );
	}
		
	cells[cells_top].type = type;
	return cells_top++;	
}

int head( int c ) {
	return cells[c].data.cons.head;
}

int tail( int c ) {
	return cells[c].data.cons.tail;
}

int *phead( int c ) {
	return &(cells[c].data.cons.head);
}

int *ptail( int c ) {
	return &(cells[c].data.cons.tail);
}

int cons( int head, int tail ) {
	int c = newcell(ConsCell);
	(*phead(c)) = head;
	(*ptail(c)) = tail;
	return c;
}

int symbol( char *s, char *s_end ) {
	int c = newcell(SymbolCell);
	cells[c].data.symbol.name = s;
	cells[c].data.symbol.name_end = s_end;
	return c;
}

int csymbol( const char *s ) {
	int c = newcell(SymbolCell);
	cells[c].data.symbol.name = (char*)s;
	cells[c].data.symbol.name_end = (char*)(s + strlen(s));
	return c;
}

int string( char *s, char *s_end ) {
	int c = newcell(StringCell);
	cells[c].data.string.str = s;
	cells[c].data.string.str_end = s_end;
	return c;
}

int integer( char *p, char *p_end ) {
	int c = newcell(IntCell);
	char temp = *p_end;
	p_end[0] = '\0';
	cells[c].data.i = (long)atol(p);
	p_end[0] = temp;
	return c;
}

int fractional( char *p, char *p_end ) {
	int c = newcell(FractionalCell);
	char temp = *p_end;
	cells[c].data.f = atof(p);
	p_end[0] = temp;
	return c;
}

int quote( int cell ) {
	int c = newcell(QuoteCell);
	cells[c].data.quoted = cell;
	return c;
}

typedef struct
{
	int cell;
	int pos_start, pos_end;
	int line_start, line_end;
	int col_start, col_end;
} result_t;

result_t result( int c, int ps, int pe, int ls, int le, int cs, int ce ) {
	result_t r = {
		 c, ps, pe, ls, le, cs, ce
	};
	
	return r;
}

result_t move( result_t r ) {
	r.pos_start = r.pos_end;
	r.line_start = r.line_end;
	r.col_start = r.col_end;
	r.cell = -1;
	return r;
}

result_t whitespacep( result_t r ) {
	int p = r.pos_end;
	
	r = move(r);
	
	while ( p < input_size ) {
		if ( isspace(input[p]) ) {
			r.pos_end++;
			switch ( input[p] ) {
				case '\n' : 
				case '\r' : r.line_end++; r.col_end = 0; break;
				case ' '  : r.col_end++; break;
				case '\t' : r.col_end += 4; break;
			}
			
			++p;
			continue;
		} else if ( input[p] == ';' ) {
			// Skip to line end.			
			while ( p < input_size && input[p] != '\n' && input[p] != '\r' )
				++p;
			
			r.pos_end = p;
			r.col_end = 0;
			r.line_end++;
			continue;
		} else {
			break;
		}
	}
		
	return r;
}

result_t intp( result_t r ) {
	int i = 0;
	int	n = 0;
	int converted = sscanf( input + r.pos_end, "%d%n", &i, &n );
	
	r = move(r);
	
	if ( converted > 0 ) {
		r.cell = newcell(IntCell);
		cells[r.cell].data.i = i;
		r.pos_end += n;
		r.col_end += n;
	}
	
	return r;
}

result_t fractionalp( result_t r ) {
	double f = 0;
	int	n = 0;
	int converted = sscanf( input + r.pos_end, "%lf%n", &f, &n );
	
	r = move(r);
	
	if ( converted > 0 ) {
		r.cell = newcell(FractionalCell);
		cells[r.cell].data.f = f;
		r.pos_end += n;
		r.col_end += n;
	}
	
	return r;
}

int isbracket( char c ) {
	return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
}

result_t symbolp( result_t r ) {
	int p = r.pos_end;
	while ( p < input_size && !isspace(input[p]) && !isbracket(input[p]) && input[p] != '"' && input[p] != ',' )
		++p;
	
	r = move(r);
	
	if ( p == r.pos_end )
		return r;
	else {
		r.cell = newcell(SymbolCell);
		cells[r.cell].data.symbol.name = input + r.pos_end;
		cells[r.cell].data.symbol.name_end = input + p;
		r.col_end += p - r.pos_end;
		r.pos_end = p;
		return r;
	}
}

result_t stringp( result_t r ) {
	int p = r.pos_end;
	
	r = move(r);
	
	if ( p >= input_size || input[p] != '"' )
		return r;
	
	++p;
	r.col_end++;
	while ( p < input_size && input[p] != '"' ) {
		switch ( input[p] ) {
			case '\n' : 
			case '\r' : r.line_end++; r.col_end = 0; break;
			case '\t' : r.col_end += 4; break;
			case '\\' : r.col_end++; ++p; break; // Skip processing an escaped character.
			default   : r.col_end ++;
		}
		
		++p;
	}
	
	r.pos_end = p < input_size ? p+1 : p;
	r.col_end += p < input_size ? 1 : 0;
	r.cell = newcell(StringCell);
	cells[r.cell].data.string.str = input + r.pos_start + 1;
	cells[r.cell].data.string.str_end = input + p;
	return r;	
}

result_t atomp( result_t r ) {
	result_t temp1, temp2;
	
	r = move(r);
	
	temp1 = intp(r);
	temp2 = fractionalp(r);
	
	if ( temp1.cell > 0 ) {
		assert( temp2.cell > 0 );
		if ( temp2.pos_end > temp1.pos_end ) {
			// Its a fractional number.
			return temp2;
		} else {
			// Its an integral.
			return temp1;
		}
	} else {
		// Its not a number. Try a string.
		temp1 = stringp(r);
		if ( temp1.cell > 0 ) {
			// Yes its a string.
			return temp1;
		} else {
			// It has to be a symbol.
			return symbolp(r);
		}
	}
}

int isoperator( int symbol ) {
	if ( cells[symbol].type == SymbolCell )
	{
		// No character of an operator can be alphanumeric or underscore.
		char *c = cells[symbol].data.symbol.name;
		char *c_end = cells[symbol].data.symbol.name_end;
		if ( c_end > c ) {
			while ( c < c_end ) {
				if ( isalnum(*c) || (*c) == '_' ) {
					// Failed. Not an operator.
					return 0;
				}
				
				++c;
			}
			return 1;
		} else 
			return 0;
	} else {
		return 0;
	}
}

result_t operatorp( result_t r ) {
	// An operator has to be a symbol.
	result_t temp = symbolp(r);
	
	if ( temp.cell > 0 && isoperator(temp.cell) ) {
		// Is an operator.
		return temp;
	} else {
		// Not an operator.
		return move(r);
	}
}

int celltextcmp( int c, const char *lit ) {
	assert( c < cells_size );
	assert( cells[c].type == SymbolCell || cells[c].type == StringCell );
	
	{
		char *cp = cells[c].data.string.str;
		char *cp_end = cells[c].data.string.str_end;
		return strncmp( cp, lit, cp_end - cp );
	}
}

result_t applicativeexprp( result_t r, int ishead );
result_t groupexprp( result_t r );
result_t listexprp( result_t r );

result_t applicativeexprp( result_t r, int ishead ) {
	
	if ( r.pos_end >= input_size )
		return move(r);
	else {
		char c = input[r.pos_end];
		result_t h;
		int q = 0;
		
		r = move(r);
		
		if ( c == '\'' ) {
			q = 1;
			r.pos_end++;
			r.col_end++;
			c = input[r.pos_end];
		}
		
		switch ( c ) {
			case ',' : r.pos_end++; r.col_end++;
			case ')' :
			case ']' : return r;
			case '(' : h = groupexprp(r); break;
			case '[' : h = listexprp(r); break;
			default  : h = atomp(r); break;
		}
		
		if ( q && h.cell >= 0 )
			h.cell = quote(h.cell);
		
		if ( h.cell < 0 || !ishead ) {
			return h;
		} else {
			// Search for argument cells.
			result_t ws = whitespacep(h);
			
			if ( ws.pos_end < input_size && input[ws.pos_end] == ',' ) {
				// Separator operator. Consume it and don't collect any arguments.
				return result( h.cell, h.pos_start, ws.pos_end+1, h.line_start, ws.line_end, h.col_start, ws.col_end+1 );
			}
			
			ishead = 0;
			
			if ( ws.line_end > ws.line_start ) {
				// There's been a line break after a head expression.
				if ( ws.col_end <= h.col_start ) {
					// Indentation is at or below the level of the head
					// expression. Head's value is as is.
					return h;
				} else {
					// The subsequent argument is a head expression.
					ishead = 1;
				}
			}
			
			// Check if the first argument is an operator.
			{
				result_t arg1 = applicativeexprp(ws,ishead);
				if ( arg1.cell >= 0 ) {
					if ( isoperator(arg1.cell) ) {
						// Yes its an operator. Place the operator at the head, make the
						// head the first argument and collect one more argument.
						result_t ws2;

						// Check for the sequencing operator that terminates the argument list of a head.
						if ( celltextcmp( arg1.cell, "," ) == 0 ) {
							return result( h.cell, 
										   h.pos_start, arg1.pos_end,
										   h.line_start, arg1.line_end,
										   h.col_start, arg1.col_end );
						}
						
						ws2 = whitespacep(arg1);

						if ( ws2.col_end > h.col_start ) {
							result_t arg2 = applicativeexprp(ws2,1);
							if ( arg2.cell >= 0 ) {
								
								// Check for special operators.
								if ( celltextcmp( arg1.cell, ":" ) == 0 ) {
									// Its the cons operator.
									return result( cons( h.cell, arg2.cell ),
												   h.pos_start, arg2.pos_end, 
												   h.line_start, arg2.line_end, 
												   h.col_start, arg2.col_end );
								} else if ( celltextcmp( arg1.cell, "::" ) == 0 ) {
									// Its the pair operator.
									return result( cons( h.cell, cons( arg2.cell, 0 ) ),
												   h.pos_start, arg2.pos_end, 
												   h.line_start, arg2.line_end, 
												   h.col_start, arg2.col_end );
								} else if ( celltextcmp( arg1.cell, ":=" ) == 0 ) {
									// Its the define operator.
									return result( cons( csymbol("define"), cons( h.cell, cons( arg2.cell, 0 ) ) ),
												   h.pos_start, arg2.pos_end, 
												   h.line_start, arg2.line_end, 
												   h.col_start, arg2.col_end );
								} else {
									// Prefix the operator.
									return result( cons( arg1.cell, cons( h.cell, cons( arg2.cell, 0 ) ) ), 
												   h.pos_start, arg2.pos_end, 
												   h.line_start, arg2.line_end, 
												   h.col_start, arg2.col_end );								
								}
							}
						}
						
						// No applicative expression for this head after the operator.
						// So treat is as a postfix operator.
						return result( cons( arg1.cell, cons( h.cell, 0 ) ),
									   h.pos_start, arg1.pos_end,
									   h.line_start, arg1.line_end,
									   h.col_start, arg1.col_end );						
					} else {
						// Not an operator, so collect arguments.
						int tailcell = cons( arg1.cell, 0 );
						int headcell = cons( h.cell, tailcell );
						result_t ws2;
						
						ishead = 0;
						
						// Check for more arguments.
						do {
							ws2 = whitespacep(arg1);
							if ( ws2.col_end <= h.col_start )
								break;
							
							arg1 = applicativeexprp(ws2, ishead || (ws2.line_end > ws2.line_start));
							ishead = 0;
						
							if ( arg1.cell >= 0 && arg1.col_start > h.col_start ) {
								// Its still this head's argument. 
								// Continue collecting.
								int t = cons( arg1.cell, 0 );
								cells[tailcell].data.cons.tail = t;
								tailcell = t;
								if ( input[arg1.pos_end-1] == ',' ) {
									// Start a new head expression after separator operator.
									ishead = 1;
								}
							} else {
								break;
							}
						} while ( arg1.cell >= 0 );
						
						// End of argument for this head.
						return result( headcell,
									   h.pos_start, arg1.pos_end,
									   h.line_start, arg1.line_end,
									   h.col_start, arg1.col_end );
					}		
				} else if ( arg1.cell == -2 ) {
					// A head expression has been suffixed with ().
					// Invoke the head as a function.
					h.cell = cons( h.cell, 0 );
					return result( h.cell, 
								   h.pos_start, arg1.pos_end,
								   h.line_start, arg1.line_end,
								   h.col_start, arg1.col_end );
				}else {
					// Some erroror terminal character like ']' or ')'.
					return h;
				}
			}			
		}
	}
}

result_t groupexprp( result_t r ) {
	int p = r.pos_end;
	
	result_t r2 = move(r);
	
	if ( p >= input_size || input[p] != '(' )
		return r;
	
	++p;
	r2.pos_end++;
	r2.col_end++;
	r2 = whitespacep(r2);
	
	{
		result_t h = applicativeexprp(r2,1);
		if ( h.cell >= 0 ) {
			result_t r3 = whitespacep(h);
			if ( r3.pos_end < input_size && input[r3.pos_end] == ')' ) {
				// Group successfully parsed.
				return result( h.cell,
							   r.pos_end, r3.pos_end+1,
							   r.line_end, r3.line_end,
							   r.col_end, r3.col_end+1 );
			} else {
				fprintf( stderr, "Syntax error: Expecting ')'...\n" );
				return r2;
			}
		} else if ( h.pos_end < input_size && input[h.pos_end] == ')' ) {
			// Empty group.
			h.cell = -2;
			h.pos_end++;
			h.col_end++;
			return h;
		} else {
			fprintf( stderr, "Syntax error: Expecting applicative expression after '(' ...\n" );
			return r2;			
		}
	}
}

result_t listexprp( result_t r ) {
	if ( r.pos_end > input_size || input[r.pos_end] != '[' )
		return move(r);
	else {
		r.pos_end++;
		r.col_end++;
		{
			result_t ws = whitespacep(r);
			result_t e = applicativeexprp( ws, 1 );
			if ( e.cell >= 0 ) {
				int h = cons( e.cell, 0 );
				int t = h;

				// Collect more expressions into the list.
				do {
					ws = whitespacep(e);
					e = applicativeexprp( ws, 1 );
					if ( e.cell >= 0 ) {
						int nexttail = cons( e.cell, 0 );
						(*ptail(t)) = nexttail;
						t = nexttail;						
					}					
				} while ( e.cell >= 0 );
				
				ws = whitespacep(e);
				
				if ( ws.pos_end < input_size && input[ws.pos_end] == ']' ) {
					// Correct closing of list.
					return result( h, 
								   r.pos_end, ws.pos_end + 1,
								   r.line_end, ws.line_end,
								   r.col_end, ws.col_end + 1 );
				} else {
					fprintf( stderr, "Syntax error: Expecting ']' ...\n" );
					return move(r);
				}
			} else {
				// Either an error or an end of list meaning empty list.
				ws = whitespacep(e);
				if ( ws.pos_end < input_size && input[ws.pos_end] == ']' ) {
					// Correctly ended empty list.
					return result( 0, 
								   r.pos_end, ws.pos_end + 1,
								   r.line_end, ws.line_end,
								   r.col_end, ws.col_end + 1 );
				} else {
					fprintf( stderr, "Syntax error: Incorrect list syntax!\n" );
					return move(r);
				}
			}
		}
	}
}

void linebreaks( int n, FILE *out ) {
	while ( n-- > 0 )
		fputc( '\n', out );
}

void tab2col( int c, FILE *out ) {
	while ( c > 4 ) {
		fputc( '\t', out );
		c -= 4;
	}
	
	while ( c-- > 0 ) {
		fputc( ' ', out );
	}
}

result_t writelistexpr( result_t e, FILE *out );
result_t writeintexpr( result_t e, FILE *out );
result_t writefractionalexpr( result_t e, FILE *out );
result_t writesymbolexpr( result_t e, FILE *out );
result_t writestringcell( result_t e, FILE *out );

result_t writeexpr( result_t e, FILE *out ) {
	
	linebreaks( e.line_start, out );
	if ( e.line_start > 0 )
		tab2col( e.col_start, out );
	
	e.line_start = 0;

	switch ( cells[e.cell].type ) {
		case ConsCell :		return writelistexpr( e, out );
		case IntCell :		return writeintexpr( e, out );
		case FractionalCell : return writefractionalexpr( e, out );
		case SymbolCell :	return writesymbolexpr( e, out );
		case StringCell :	return writestringcell( e, out );
		case QuoteCell :	fputc( '\'', out ); 
							e.cell = cells[e.cell].data.quoted; 
							return writeexpr( e, out );
	}
	
	e.cell = -1;
	return e;
}


result_t writelistexpr( result_t e0, FILE *out ) {
	result_t e = e0;
	result_t r;
	int list = e.cell;
	
	r = e;
	r.line_start = 0;
	r.line_end = e.line_start;
	r.col_start = e.col_start;
	r.col_end = r.col_start;
	
	fputc( '(', out );
	e.col_start++;
	r.col_end++;
	e.line_start = 0;
	while ( list ) {

		if ( cells[list].type != ConsCell ) {
			fprintf( out, " . " );
			e.col_start += 3;
			e.cell = list;
			e.line_start = 0;
			r = writeexpr( e, out );
			return result( -1, 0, 0, 0, e0.line_start + r.line_end, e0.col_start, r.col_end );
		}
		
		e.cell = head(list);
		
		{
			result_t temp = writeexpr(e, out);
			r.line_end += temp.line_end;
			r.col_end = temp.col_end;
				
			if ( cells[e.cell].type == QuoteCell || cells[e.cell].type == ConsCell || cells[e.cell].type == StringCell ) {
				e.line_start = 1;
				e.col_start = temp.col_start;
			} else {
				e.line_start = 0;
				e.col_start = r.col_end;
			}
		}
		
		list = tail(list);	
		
		if ( list && e.line_start == 0 ) {
			fputc( ' ', out );
			r.col_end++;
			e.col_start++;
		}
	}
	
	fputc( ')', out );
	r.col_end++;
	return r;
}

result_t writeintexpr( result_t e, FILE *out ) {
	int n = fprintf( out, "%d", cells[e.cell].data.i );
	return result( -1, 0, 0, 0, 0, e.col_start, e.col_start + n );
}

result_t writefractionalexpr( result_t e, FILE *out ) {
	int n = fprintf( out, "%g", cells[e.cell].data.f );
	return result( -1, 0, 0, 0, 0, e.col_start, e.col_start + n );
}

result_t writesymbolexpr( result_t e, FILE *out ) {
	int n = fwrite( cells[e.cell].data.symbol.name, 1, 
					cells[e.cell].data.symbol.name_end - cells[e.cell].data.symbol.name,
					out );
	
	return result( -1, 0, 0, 0, 0, e.col_start, e.col_start + n );
}

result_t writestringcell( result_t e, FILE *out ) {
	
	result_t r = result( -1, 0, 0, 0, 0, e.col_start, e.col_start );
	
	char *c = cells[e.cell].data.string.str;
	char *c_end = cells[e.cell].data.string.str_end;
	
	r.col_end++;
	fputc( '"', out );
	while ( c < c_end ) {
		switch (*c) {
			case '\r' :
			case '\n' : r.line_end++; r.col_end = 0; break;
			case '\t' : r.col_end += 4; break;
			default : r.col_end++;
		}
		
		fputc( *c++, out );
	}
	fputc( '"', out );
	r.col_end++;
	return r;
}

int ez2scm( FILE *in, FILE *out ) {
	// Load all input data into memory.
	fseek( in, 0, SEEK_END );
	input_size = ftell(in);
	fseek( in, 0, SEEK_SET );
	
	if ( input_size <= 0 )
		return -1;
	
	input = (char *)malloc( input_size+1 );
	input_size = fread( input, 1, input_size, in );
	
	newcell(ConsCell); // The NIL cell.

	{
		result_t r = result( -1, 0, 0, 0, 0, 0, 0 );
		do
		{
			r = applicativeexprp( whitespacep(r), 1 );
			
			if ( r.cell >= 0 ) {
				writeexpr( result( r.cell, 0, 0, 0, 0, 0, 0 ), out );
				fprintf( out, "\n\n" );
			} else {
				fprintf( stderr, "Result %d\n", r.cell );
			}
			
		} while ( r.pos_end < input_size );
	}
	
	return 0;
}

int main (int argc, const char * argv[]) {

	if ( argc < 3 )	{
		fprintf( stderr, "Usage: ez2scm infile.ez outfile.scm\n" );
		return 0;		
	} else {
		FILE *infile = fopen( argv[1], "rb" );
		if ( !infile ) {
			fprintf( stderr, "Could not open input file '%s'!\n", argv[1] );
			return 0;
		} else {
			FILE *outfile = fopen( argv[2], "wb" );
			if ( !outfile ) {
				fprintf( stderr, "Could not create output file '%s'!\n", argv[2] );
				fclose(infile);
				return 0;
			} else {
				int result = ez2scm( infile, outfile );
				fclose( infile );
				fclose( outfile );
				return result;
			}
		}
	}
	
}
