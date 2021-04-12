#include <vslc.h>

// Externally visible, for the generator
extern tlhash_t *global_names;
extern char **string_list;
extern size_t n_string_list,stringc;

/* External interface */

void create_symbol_table(void);
void print_symbol_table(void);
void print_symbols(void);
void print_bindings(node_t *root);
void destroy_symbol_table(void);
void find_globals(void);
void bind_names(symbol_t *function, node_t *root);
void destroy_symtab(void);

static size_t scope_depth = 0, number_scopes = 1;
static tlhash_t **scopes = NULL;

void
create_symbol_table ( void )
{
  find_globals();
  size_t n_globals = tlhash_size ( global_names );
  symbol_t *global_list[n_globals];
  tlhash_values ( global_names, (void **)&global_list );
  for ( size_t i=0; i<n_globals; i++ )
      if ( global_list[i]->type == SYM_FUNCTION )
          bind_names ( global_list[i], global_list[i]->node );
}


void
print_symbol_table ( void )
{
	print_symbols();
	print_bindings(root);
}



void
print_symbols ( void )
{
    printf ( "String table:\n" );
    for ( size_t s=0; s<stringc; s++ )
        printf  ( "%zu: %s\n", s, string_list[s] );
    printf ( "-- \n" );

    printf ( "Globals:\n" );
    size_t n_globals = tlhash_size(global_names);
    symbol_t *global_list[n_globals];
    tlhash_values ( global_names, (void **)&global_list );
    for ( size_t g=0; g<n_globals; g++ )
    {
        switch ( global_list[g]->type )
        {
            case SYM_FUNCTION:
                printf (
                    "%s: function %zu:\n",
                    global_list[g]->name, global_list[g]->seq
                );
                if ( global_list[g]->locals != NULL )
                {
                    size_t localsize = tlhash_size( global_list[g]->locals );
                    printf (
                        "\t%zu local variables, %zu are parameters:\n",
                        localsize, global_list[g]->nparms
                    );
                    symbol_t *locals[localsize];
                    tlhash_values(global_list[g]->locals, (void **)locals );
                    for ( size_t i=0; i<localsize; i++ )
                    {
                        printf ( "\t%s: ", locals[i]->name );
                        switch ( locals[i]->type )
                        {
                            case SYM_PARAMETER:
                                printf ( "parameter %zu\n", locals[i]->seq );
                                break;
                            case SYM_LOCAL_VAR:
                                printf ( "local var %zu\n", locals[i]->seq );
                                break;
                        }
                    }
                }
                break;
            case SYM_GLOBAL_VAR:
                printf ( "%s: global variable\n", global_list[g]->name );
                break;
        }
    }
    printf ( "-- \n" );
}


void
print_bindings ( node_t *root )
{
    if ( root == NULL )
        return;
    else if ( root->entry != NULL )
    {
        switch ( root->entry->type )
        {
            case SYM_GLOBAL_VAR: 
                printf ( "Linked global var '%s'\n", root->entry->name );
                break;
            case SYM_FUNCTION:
                printf ( "Linked function %zu ('%s')\n",
                    root->entry->seq, root->entry->name
                );
                break; 
            case SYM_PARAMETER:
                printf ( "Linked parameter %zu ('%s')\n",
                    root->entry->seq, root->entry->name
                );
                break;
            case SYM_LOCAL_VAR:
                printf ( "Linked local var %zu ('%s')\n",
                    root->entry->seq, root->entry->name
                );
                break;
        }
    } else if ( root->type == STRING_DATA ) {
        size_t string_index = *((size_t *)root->data);
        if ( string_index < stringc )
            printf ( "Linked string %zu\n", *((size_t *)root->data) );
        else
            printf ( "(Not an indexed string)\n" );
    }
    for ( size_t c=0; c<root->n_children; c++ )
        print_bindings ( root->children[c] );
}


void
destroy_symbol_table ( void )
{
      destroy_symtab();
}


void
find_globals ( void )
{
	global_names = malloc(sizeof(tlhash_t));
	tlhash_init(global_names, 32);
	string_list = malloc(n_string_list * sizeof(char*));
	size_t n_functions = 0;
	node_t *first_born = root->children[0];

	for(int64_t i=0; i < first_born->n_children; i++) {
		node_t *grand_child = first_born->children[i];
		node_t *name_list;
		symbol_t *symbol;
		switch(grand_child->type) {
			case FUNCTION:
				symbol = malloc(sizeof(symbol_t));
				*symbol = (symbol_t) {
					.type = SYM_FUNCTION,
					.name = grand_child->children[0]->data,
					.node = grand_child->children[2],
					.seq = n_functions,
					.nparms = 0,
					.locals = malloc(sizeof(tlhash_t))
				};
				n_functions++;
				tlhash_init(symbol->locals, 32);
				if(grand_child->children[1] != NULL) {
					symbol->nparms = grand_child->children[1]->n_children;
					for(int64_t j=0; j < symbol->nparms; j++) {
						symbol_t *param =  malloc(sizeof(symbol_t));
						*param = (symbol_t) {
							.type = SYM_PARAMETER,
							.name = grand_child->children[1]->children[j]->data,
							.node = NULL,
							.seq = j,
							.nparms = 0,
							.locals = NULL
						};
						tlhash_insert(symbol->locals, param->name, strlen(param->name), param);
					}
				}
				tlhash_insert(global_names, symbol->name, strlen(symbol->name), symbol);
				break;

			case DECLARATION:
				name_list = grand_child->children[0];
				for(int64_t k=0; k < name_list->n_children; k++) {
					symbol = malloc(sizeof(symbol_t));
					*symbol = (symbol_t) {
						.type = SYM_GLOBAL_VAR,
						.name = name_list->children[k]->data,
						.node = NULL,
						.seq = 0,
						.nparms = 0,
						.locals = NULL
					};
					tlhash_insert(global_names, symbol->name, strlen(symbol->name), symbol);
				}
				break;

		}

	}

}

static void
push_scope(void) {
	if (scopes == NULL) {
		scopes = malloc(number_scopes * sizeof(tlhash_t*));
	}
	tlhash_t *new_scope = malloc(sizeof(tlhash_t));
	tlhash_init(new_scope, 32);
	scopes[scope_depth] = new_scope;
	scope_depth += 1;
	if (scope_depth >= number_scopes) {
		number_scopes *2;
		scopes = realloc(scopes, number_scopes*sizeof(tlhash_t **));
	}
}

static void
pop_scope(void) {
	scope_depth -= 1;
	tlhash_finalize(scopes[scope_depth]);
	free(scopes[scope_depth]);
	scopes[scope_depth] = NULL;

}

static void
add_local(symbol_t *local) {
	tlhash_insert(scopes[scope_depth - 1], local->name, strlen(local->name), local);

}

static symbol_t *
lookup_local(char *name) {
	symbol_t *result = NULL;
	size_t depth = scope_depth;
	while (result == NULL && depth > 0) {
		depth -= 1;
		tlhash_lookup(scopes[depth], name, strlen(name), (void**)&result);
	}
	return result;
}

void
bind_names ( symbol_t *function, node_t *root ) {
	if (root == NULL) {
		return;
	} else {
		switch (root->type) {
		node_t *first_born;
		symbol_t *entry;
		
		case BLOCK:
			push_scope();
			for (size_t i=0; i < root->n_children; i++) {
				bind_names(function, root->children[i]);
			}
			pop_scope();
			break;
		
		case DECLARATION:
			first_born = root->children[0];
			for (int64_t j=0; j < first_born->n_children; j++) {
				node_t *varname = first_born->children[j];
				size_t local_num = tlhash_size(function->locals) - function->nparms;
				symbol_t *symbol = malloc(sizeof(symbol_t));
				*symbol = (symbol_t) {
					.type = SYM_LOCAL_VAR,
					.name = varname->data,
					.node = NULL,
					.seq = local_num,
					.nparms = 0,
					.locals = NULL
				};
				tlhash_insert(function->locals, &local_num, sizeof(size_t), symbol);
				add_local(symbol);
			}
			break;

		case IDENTIFIER_DATA:
			entry = lookup_local(root->data);
			if (entry == NULL) {
				tlhash_lookup(function->locals, root->data, strlen(root->data), (void**) &entry); //We have to check in function->locals and global_names before we can crash
			}
			if (entry == NULL) {
				tlhash_lookup(global_names, root->data, strlen(root->data), (void**) &entry);
			}
			if (entry == NULL) {
				fprintf(stderr, "Identifier '%s' not in scope\n", (char *) root->data);
				exit(EXIT_FAILURE);
			}
			root->entry = entry;
			break;

		case STRING_DATA:
			string_list[stringc] = root->data;
			root->data = malloc(sizeof(size_t));
			*((size_t *) root->data) = stringc;
			stringc++;
			if (stringc >= n_string_list) {
				n_string_list *= 2;
				string_list = realloc(string_list, n_string_list * sizeof(char *));
			}
		default:
			for (size_t h=0; h < root->n_children; h++) {
				bind_names(function, root->children[h]);
			}
			break;
		}
	}
}

void
destroy_symtab ( void ){
	for (size_t i=0; i< stringc; i++)
		free(string_list[i]);
	free(string_list);

	size_t size_globals = tlhash_size(global_names);
	symbol_t *global_list[size_globals];
	tlhash_values(global_names, (void**) &global_list);
	for (size_t j=0; j < size_globals; j++) {
		symbol_t *glbl = global_list[j];

		if (glbl->locals != NULL) {
			size_t size_locals = tlhash_size(glbl->locals);
			symbol_t *locals[size_locals];
			tlhash_values(glbl->locals, (void**) &locals);
			for (size_t h=0; h < size_locals; h++) {
				free(locals[h]);
			}
			tlhash_finalize(glbl->locals);
			free(glbl->locals);
		}

		free(glbl);
	}
	tlhash_finalize(global_names);
	free(global_names);
	free(scopes);
}
