#include <stdlib.h>
#include <string.h>

#include "task.h"

#include <stdio.h>

struct read {
   struct node* node;
};

struct operand {
   struct func* func;
   struct dim* dim;
   struct pos pos;
   struct name* name_offset;
   struct ns* ns;
   struct type* type;
   int value;
   bool is_value;
   bool is_space;
   bool folded;
};

static void read_op( struct task*, struct read_expr*, struct read* );
static void read_operand( struct task*, struct read_expr*, struct read* );
static void add_binary( struct read*, int, struct pos*, struct read* );
static void add_unary( struct read*, int );
static void read_primary( struct task*, struct read_expr*, struct read* );
static void read_postfix( struct task*, struct read_expr*, struct read* );
static void read_call( struct task*, struct read_expr*, struct read* );
static void init_operand( struct operand*, struct name* );
static void test_node( struct task*, struct expr_test*, struct operand*,
   struct node* );
static void test_name_usage( struct task*, struct expr_test*, struct operand*,
   struct name_usage* );
static void use_object( struct task*, struct operand*, struct object* );
static void test_call( struct task*, struct expr_test*, struct operand*,
   struct call* );
static void test_access( struct task*, struct expr_test*, struct operand*,
   struct access* );
static void read_string( struct task*, struct read_expr*, struct read* );

void t_init_read_expr( struct read_expr* read ) {
   read->node = NULL;
   read->stmt_read = NULL;
   read->has_str = false;
   read->skip_assign = false;
   read->skip_function_call = false;
}

void t_read_expr( struct task* task, struct read_expr* read ) {
   struct read operand;
   read_op( task, read, &operand );
   struct expr* expr = mem_alloc( sizeof( *expr ) );
   expr->node.type = NODE_EXPR;
   expr->root = operand.node;
   read->node = expr;
}

void read_op( struct task* task, struct read_expr* read,
   struct read* operand ) {
   int op;

   struct read add;
   struct read* add_parent = NULL;
   int add_op = 0;
   struct pos add_op_pos;

   struct read shift;
   struct read* shift_parent = NULL;
   int shift_op = 0;
   struct pos shift_op_pos;

   struct read lt;
   struct read* lt_parent = NULL;
   int lt_op = 0;
   struct pos lt_op_pos;

   struct read eq;
   struct read* eq_parent = NULL;
   int eq_op = 0;
   struct pos eq_op_pos;

   struct read bit_and;
   struct read* bit_and_parent = NULL;
   struct pos bit_and_pos;

   struct read bit_xor;
   struct read* bit_xor_parent = NULL;
   struct pos bit_xor_pos;

   struct read bit_or;
   struct read* bit_or_parent = NULL;
   struct pos bit_or_pos;

   struct read log_and;
   struct read* log_and_parent = NULL;
   struct pos log_and_pos;

   struct read log_or;
   struct read* log_or_parent = NULL;
   struct pos log_or_pos;

   top:
   // -----------------------------------------------------------------------
   read_operand( task, read, operand );

   op_mul:
   // -----------------------------------------------------------------------
   if ( task->tk == TK_STAR ) {
      op = BOP_MUL;
   }
   else if ( task->tk == TK_SLASH ) {
      op = BOP_DIV;
   }
   else if ( task->tk == TK_MOD ) {
      op = BOP_MOD;
   }
   else {
      goto op_add;
   }
   struct pos op_pos = task->tk_pos;
   t_read_tk( task );
   struct read rside;
   read_operand( task, read, &rside );
   add_binary( operand, op, &op_pos, &rside );
   op = 0;
   goto op_mul;

   op_add:
   // -----------------------------------------------------------------------
   if ( add_parent ) {
      add_binary( add_parent, add_op, &add_op_pos, operand );
      operand = add_parent;
      add_parent = NULL;
   }
   if ( task->tk == TK_PLUS ) {
      op = BOP_ADD;
   }
   else if ( task->tk == TK_MINUS ) {
      op = BOP_SUB;
   }
   else {
      goto op_shift;
   }
   add_parent = operand;
   operand = &add;
   add_op = op;
   add_op_pos = task->tk_pos;
   t_read_tk( task );
   goto top;

   op_shift:
   // -----------------------------------------------------------------------
   if ( shift_parent ) {
      add_binary( shift_parent, shift_op, &shift_op_pos, operand );
      operand = shift_parent;
      shift_parent = NULL;
   }
   if ( task->tk == TK_SHIFT_L ) {
      op = BOP_SHIFT_L;
   }
   else if ( task->tk == TK_SHIFT_R ) {
      op = BOP_SHIFT_R;
   }
   else {
      goto op_lt;
   }
   shift_parent = operand;
   operand = &shift;
   shift_op = op;
   shift_op_pos = task->tk_pos;
   t_read_tk( task );
   goto top;

   op_lt:
   // -----------------------------------------------------------------------
   if ( lt_parent ) {
      add_binary( lt_parent, lt_op, &lt_op_pos, operand );
      operand = lt_parent;
      lt_parent = NULL;
   }
   if ( task->tk == TK_LT ) {
      op = BOP_LT;
   }
   else if ( task->tk == TK_LTE ) {
      op = BOP_LTE;
   }
   else if ( task->tk == TK_GT ) {
      op = BOP_GT;
   }
   else if ( task->tk == TK_GTE ) {
      op = BOP_GTE;
   }
   else {
      goto op_eq;
   }
   lt_parent = operand;
   operand = &lt;
   lt_op = op;
   lt_op_pos = task->tk_pos;
   t_read_tk( task );
   goto top;

   op_eq:
   // -----------------------------------------------------------------------
   if ( eq_parent ) {
      add_binary( eq_parent, eq_op, &eq_op_pos, operand );
      operand = eq_parent;
      eq_parent = NULL;
   }
   if ( task->tk == TK_EQ ) {
      op = BOP_EQ;
   }
   else if ( task->tk == TK_NEQ ) {
      op = BOP_NEQ;
   }
   else {
      goto op_bit_and;
   }
   eq_parent = operand;
   operand = &eq;
   eq_op = op;
   eq_op_pos = task->tk_pos;
   t_read_tk( task );
   goto top;

   op_bit_and:
   // -----------------------------------------------------------------------
   if ( bit_and_parent ) {
      add_binary( bit_and_parent, BOP_BIT_AND, &bit_and_pos, operand );
      operand = bit_and_parent;
      bit_and_parent = NULL;
   }
   if ( task->tk == TK_BIT_AND ) {
      bit_and_parent = operand;
      operand = &bit_and;
      bit_and_pos = task->tk_pos;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_xor_parent ) {
      add_binary( bit_xor_parent, BOP_BIT_XOR, &bit_xor_pos, operand );
      operand = bit_xor_parent;
      bit_xor_parent = NULL;
   }
   if ( task->tk == TK_BIT_XOR ) {
      bit_xor_parent = operand;
      operand = &bit_xor;
      bit_xor_pos = task->tk_pos;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( bit_or_parent ) {
      add_binary( bit_or_parent, BOP_BIT_OR, &bit_or_pos, operand );
      operand = bit_or_parent;
      bit_or_parent = NULL;
   }
   if ( task->tk == TK_BIT_OR ) {
      bit_or_parent = operand;
      operand = &bit_or;
      bit_or_pos = task->tk_pos;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_and_parent ) {
      add_binary( log_and_parent, BOP_LOG_AND, &log_and_pos, operand );
      operand = log_and_parent;
      log_and_parent = NULL;
   }
   if ( task->tk == TK_LOG_AND ) {
      log_and_parent = operand;
      operand = &log_and;
      log_and_pos = task->tk_pos;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( log_or_parent ) {
      add_binary( log_or_parent, BOP_LOG_OR, &log_or_pos, operand );
      operand = log_or_parent;
      log_or_parent = NULL;
   }
   if ( task->tk == TK_LOG_OR ) {
      log_or_parent = operand;
      operand = &log_or;
      log_or_pos = task->tk_pos;
      t_read_tk( task );
      goto top;
   }

   // -----------------------------------------------------------------------
   if ( task->tk == TK_QUESTION ) {
      t_read_tk( task );
      struct read lside;
      read_op( task, read, &lside );
      t_test_tk( task, TK_COLON );
      t_read_tk( task );
      read_op( task, read, &rside );
      struct ternary* ternary = mem_alloc( sizeof( *ternary ) );
      ternary->node.type = NODE_TERNARY;
      ternary->cond = operand->node;
      ternary->lside = lside.node;
      ternary->rside = rside.node;
      operand->node = ( struct node* ) ternary;
   }

   // -----------------------------------------------------------------------
   if ( ! read->skip_assign ) {
      switch ( task->tk ) {
      case TK_ASSIGN: op = AOP_NONE; break;
      case TK_ASSIGN_ADD: op = AOP_ADD; break;
      case TK_ASSIGN_SUB: op = AOP_SUB; break;
      case TK_ASSIGN_MUL: op = AOP_MUL; break;
      case TK_ASSIGN_DIV: op = AOP_DIV; break;
      case TK_ASSIGN_MOD: op = AOP_MOD; break;
      case TK_ASSIGN_SHIFT_L: op = AOP_SHIFT_L; break;
      case TK_ASSIGN_SHIFT_R: op = AOP_SHIFT_R; break;
      case TK_ASSIGN_BIT_AND: op = AOP_BIT_AND; break;
      case TK_ASSIGN_BIT_XOR: op = AOP_BIT_XOR; break;
      case TK_ASSIGN_BIT_OR: op = AOP_BIT_OR; break;
      // Finish.
      default: return;
      }
      t_read_tk( task );
      struct pos rside_pos = task->tk_pos;
      read_op( task, read, &rside ); 
      struct assign* assign = mem_alloc( sizeof( *assign ) );
      assign->node.type = NODE_ASSIGN;
      assign->op = op;
      assign->lside = operand->node;
      assign->rside = rside.node;
      operand->node = ( struct node* ) assign;
   }
}

void add_binary( struct read* lside, int op, struct pos* pos,
   struct read* rside ) {
   struct binary* binary = mem_alloc( sizeof( *binary ) );
   binary->node.type = NODE_BINARY;
   binary->op = op;
   binary->lside = lside->node;
   binary->rside = rside->node;
   lside->node = ( struct node* ) binary;
}

void read_operand( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   // Prefix operations:
   int op = UOP_NONE;
   switch ( task->tk ) {
   case TK_INC: op = UOP_PRE_INC; break;
   case TK_DEC: op = UOP_PRE_DEC; break;
   case TK_MINUS: op = UOP_MINUS; break;
   case TK_LOG_NOT: op = UOP_LOG_NOT; break;
   case TK_BIT_NOT: op = UOP_BIT_NOT; break;
   default: break;
   }
   if ( op ) {
      t_read_tk( task );
      struct pos pos = task->tk_pos;
      read_operand( task, expr, operand );
      add_unary( operand, op );
   }
   else {
      operand->node = NULL;
      read_primary( task, expr, operand );
      read_postfix( task, expr, operand );
   }
}


void add_unary( struct read* operand, int op ) {
   struct unary* unary = mem_alloc( sizeof( *unary ) );
   unary->node.type = NODE_UNARY;
   unary->op = op;
   unary->operand = operand->node;
   operand->node = ( struct node* ) unary;
}

void read_primary( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   // Access object in the global namespace.
   if ( task->tk == TK_ID ) {
      struct name_usage* usage = mem_alloc( sizeof( *usage ) );
      usage->node.type = NODE_NAME_USAGE;
      usage->text = task->tk_text;
      usage->pos = task->tk_pos;
      usage->object = NULL;
      operand->node = ( struct node* ) usage;
      t_read_tk( task );
   }
   else if ( task->tk == TK_PAREN_L ) {
      struct paren* paren = mem_alloc( sizeof( *paren ) );
      paren->node.type = NODE_PAREN;
      paren->pos = task->tk_pos;
      t_read_tk( task );
      read_op( task, expr, operand );
      t_test_tk( task, TK_PAREN_R );
      t_read_tk( task );
      paren->inside = operand->node;
      operand->node = ( struct node* ) paren;
   }
   else if ( task->tk == TK_LIT_STRING ) {
      read_string( task, expr, operand );
   }
   else {
      struct literal* literal = mem_alloc( sizeof( *literal ) );
      literal->node.type = NODE_LITERAL;
      literal->pos = task->tk_pos;
      literal->value = t_read_literal( task );
      operand->node = ( struct node* ) literal;
   }
}

void read_string( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   t_test_tk( task, TK_LIT_STRING );
   struct indexed_string* prev = NULL;
   struct indexed_string* string = task->str_table.head;
   while ( string ) {
      int cmp = strcmp( string->value, task->tk_text );
      if ( cmp == 0 ) {
         break;
      }
      else if ( cmp > 0 ) {
         string = NULL;
         break;
      }
      prev = string;
      string = string->next;
   }
   if ( ! string ) {
      void* block = mem_alloc(
         sizeof( struct indexed_string ) + task->tk_length + 1 );
      string = block;
      string->value = ( ( char* ) block ) + sizeof( *string );
      memcpy( string->value, task->tk_text, task->tk_length );
      string->value[ task->tk_length ] = 0;
      string->length = task->tk_length;
      string->index = 0;
      string->next = NULL;
      string->usage = 1;
      ++task->str_table.size;
      if ( prev ) {
         string->next = prev->next;
         prev->next = string;
      }
      else {
         string->next = task->str_table.head;
         task->str_table.head = string;
      }
   }
   struct indexed_string_usage* usage = mem_alloc( sizeof( *usage ) );
   usage->node.type = NODE_INDEXED_STRING_USAGE;
   usage->pos = task->tk_pos;
   usage->string = string;
   operand->node = ( struct node* ) usage;
   expr->has_str = true;
   t_read_tk( task );
}

int t_read_literal( struct task* task ) {
   int value = 0;
   if ( task->tk == TK_LIT_DECIMAL ) {
      value = strtol( task->tk_text, NULL, 10 );
   }
   else if ( task->tk == TK_LIT_OCTAL ) {
      value = strtol( task->tk_text, NULL, 8 );
   }
   else if ( task->tk == TK_LIT_HEX ) {
      value = strtol( task->tk_text, NULL, 16 );
   }
   else if ( task->tk == TK_LIT_FIXED ) {
      double num = atof( task->tk_text );
      value =
         // Whole.
         ( ( int ) num << 16 ) +
         // Fraction.
         ( int ) ( ( 1 << 16 ) * ( num - ( int ) num ) );
   }
   else if ( task->tk == TK_LIT_CHAR ) {
      value = task->tk_text[ 0 ];
   }
   else {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
         "missing primary expression" );
      bail();
   }
   t_read_tk( task );
   return value;
}

void read_postfix( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   while ( true ) {
      if ( task->tk == TK_BRACKET_L ) {
         t_read_tk( task );
         struct subscript* subscript = mem_alloc( sizeof( *subscript ) );
         subscript->node.type = NODE_SUBSCRIPT;
         struct read index;
         read_op( task, expr, &index );
         t_test_tk( task, TK_BRACKET_R );
         t_read_tk( task );
         subscript->index = index.node;
         subscript->lside = operand->node;
         operand->node = ( struct node* ) subscript;
      }
      else if ( task->tk == TK_DOT ) {
         t_read_tk( task );
         t_test_tk( task, TK_ID );
         struct access* access = mem_alloc( sizeof( *access ) );
         access->node.type = NODE_ACCESS;
         access->name = task->tk_text;
         access->pos = task->tk_pos;
         t_read_tk( task );
         access->lside = operand->node;
         access->rside = NULL;
         operand->node = ( struct node* ) access;
      }
      else if ( task->tk == TK_PAREN_L && ! expr->skip_function_call ) {
         read_call( task, expr, operand );
      }
      else if ( task->tk == TK_INC ) {
         add_unary( operand, UOP_POST_INC );
         t_read_tk( task );
      }
      else if ( task->tk == TK_DEC ) {
         add_unary( operand, UOP_POST_DEC );
         t_read_tk( task );
      }
      else {
         break;
      }
   }
}

void read_call( struct task* task, struct read_expr* expr,
   struct read* operand ) {
   struct pos pos = task->tk_pos;
   t_test_tk( task, TK_PAREN_L );
   t_read_tk( task );
   struct list args;
   list_init( &args );
   int count = 0;
   // Format list:
   if ( task->tk == TK_ID && t_peek_usable_ch( task ) == ':' ) {
      while ( true ) {
         list_append( &args, t_read_format_item( task, TK_COLON ) );
         if ( task->tk == TK_COMMA ) {
            t_read_tk( task );
         }
         else {
            break;
         }
      }
      // All format items count as a single argument.
      ++count;
      if ( task->tk == TK_SEMICOLON ) {
         t_read_tk( task );
         while ( true ) {
            struct read_expr arg;
            t_init_read_expr( &arg );
            t_read_expr( task, &arg );
            list_append( &args, arg.node );
            ++count;
            if ( task->tk == TK_COMMA ) {
               t_read_tk( task );
            }
            else {
               break;
            }
         }
      }
   }
   // Format block:
   else if ( task->tk == TK_BRACE_L ) {
      // For now, a format block can only appear in a function call that is
      // part of a stand-alone expression.
      if ( expr->stmt_read ) {
         t_read_block( task, expr->stmt_read );
         list_append( &args, expr->stmt_read->block );
         if ( task->tk == TK_SEMICOLON ) {
            t_read_tk( task );
            while ( true ) {
               struct read_expr arg;
               t_init_read_expr( &arg );
               t_read_expr( task, &arg );
               list_append( &args, arg.node );
               ++count;
               if ( task->tk == TK_COMMA ) {
                  t_read_tk( task );
               }
               else {
                  break;
               }
            }
         }
      }
      else {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &task->tk_pos,
            "format block outside an expression statement" );
         bail();
      }
   }
   else {
      // This relic is not necessary in new code. The compiler is smart enough
      // to figure out when to use the constant version of an instruction.
      if ( task->tk == TK_CONST ) {
         t_read_tk( task );
         t_test_tk( task, TK_COLON );
         t_read_tk( task );
      }
      if ( task->tk != TK_PAREN_R ) {
         while ( true ) {
            struct read_expr arg;
            t_init_read_expr( &arg );
            t_read_expr( task, &arg );
            list_append( &args, arg.node );
            count += 1;
            if ( task->tk == TK_COMMA ) {
               t_read_tk( task );
            }
            else {
               break;
            }
         }
      }
   }
   t_test_tk( task, TK_PAREN_R );
   t_read_tk( task );
   struct call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_CALL;
   call->pos = pos;
   call->func = NULL;
   call->func_tree = operand->node;
   call->args = args;
   operand->node = ( struct node* ) call;
}

struct format_item* t_read_format_item( struct task* task, enum tk sep ) {
   t_test_tk( task, TK_ID );
   struct format_item* item = mem_alloc( sizeof( *item ) );
   item->node.type = NODE_FORMAT_ITEM;
   item->pos = task->tk_pos;
   bool unknown = false;
   switch ( task->tk_text[ 0 ] ) {
   case 'a': item->cast = FCAST_ARRAY; break;
   case 'b': item->cast = FCAST_BINARY; break;
   case 'c': item->cast = FCAST_CHAR; break;
   case 'd':
   case 'i': item->cast = FCAST_DECIMAL; break;
   case 'f': item->cast = FCAST_FIXED; break;
   case 'k': item->cast = FCAST_KEY; break;
   case 'l': item->cast = FCAST_LOCAL_STRING; break;
   case 'n': item->cast = FCAST_NAME; break;
   case 's': item->cast = FCAST_STRING; break;
   case 'x': item->cast = FCAST_HEX; break;
   default:
      unknown = true;
      break;
   }
   if ( unknown || task->tk_length != 1 ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &task->tk_pos, "unknown format cast '%s'", task->tk_text );
      bail();
   }
   t_read_tk( task );
   t_test_tk( task, sep );
   t_read_tk( task );
   struct read_expr expr;
   t_init_read_expr( &expr );
   t_read_expr( task, &expr );
   item->expr = expr.node;
   return item;
}

void t_init_expr_test( struct expr_test* test ) {
   test->stmt_test = NULL;
   test->needed = true;
   test->has_string = false;
   test->undef_err = false;
   test->undef_erred = false;
}

void t_test_expr( struct task* task, struct expr_test* test,
   struct expr* expr ) {
   if ( setjmp( test->bail ) == 0 ) {
      struct operand operand;
      init_operand( &operand, task->ns->body );
      test_node( task, test, &operand, expr->root );
      expr->folded = operand.folded;
      expr->value = operand.value;
      test->pos = operand.pos;
      if ( test->needed && ! operand.is_value ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &operand.pos,
            "expression does not produce a value" );
         bail();
      }
   }
}

void init_operand( struct operand* operand, struct name* offset ) {
   operand->func = NULL;
   operand->dim = NULL;
   operand->type = NULL;
   operand->ns = NULL;
   operand->value = 0;
   operand->is_value = false;
   operand->is_space = false;
   operand->folded = false;
   operand->name_offset = offset;
}

void test_node( struct task* task, struct expr_test* test,
   struct operand* operand, struct node* node ) {
   if ( node->type == NODE_LITERAL ) {
      struct literal* literal = ( struct literal* ) node;
      operand->pos = literal->pos;
      operand->folded = true;
      operand->value = literal->value;
      operand->is_value = true;
   }
   else if ( node->type == NODE_INDEXED_STRING_USAGE ) {
      struct indexed_string_usage* usage =
         ( struct indexed_string_usage* ) node;
      operand->pos = usage->pos;
      operand->folded = true;
      operand->value = 0;
      operand->is_value = true;
   }
   else if ( node->type == NODE_NAME_USAGE ) {
      test_name_usage( task, test, operand, ( struct name_usage* ) node );
   }
   else if ( node->type == NODE_UNARY ) {
      struct unary* unary = ( struct unary* ) node;
      struct operand target;
      init_operand( &target, operand->name_offset );
      test_node( task, test, &target, unary->operand );
      if ( unary->op == UOP_PRE_INC || unary->op == UOP_PRE_DEC ||
         unary->op == UOP_POST_INC || unary->op == UOP_POST_DEC ) {
         // Only an l-value can be incremented.
         if ( ! target.is_space ) {
            const char* action = "incremented";
            if ( unary->op == UOP_PRE_DEC || unary->op == UOP_POST_DEC ) {
               action = "decremented";
            }
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
               &target.pos, "operand cannot be %s", action );
            bail();
         }
      }
      // Remaining operations require a value to work on.
      else if ( ! target.is_value ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &target.pos, "operand cannot be used in unary operation" );
         bail();
      }
      if ( target.folded ) {
         switch ( unary->op ) {
         case UOP_MINUS:
            operand->value = ( - target.value );
            break;
         case UOP_LOG_NOT:
            operand->value = ( ! target.value );
            break;
         case UOP_BIT_NOT:
            operand->value = ( ~ target.value );
            break;
         default:
            break;
         }
         operand->folded = true;
      }
      operand->pos = target.pos;
      operand->is_value = true;
   }
   else if ( node->type == NODE_SUBSCRIPT ) {
      struct subscript* subscript = ( struct subscript* ) node;
      struct operand target;
      init_operand( &target, operand->name_offset );
      test_node( task, test, &target, subscript->lside );
      if ( ! target.dim ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &target.pos,
            "accessing something not an array" );
         bail();
      }
      struct operand index;
      init_operand( &index, operand->name_offset );
      test_node( task, test, &index, subscript->index );
      operand->pos = target.pos;
      operand->type = target.type;
      operand->dim = target.dim->next;
      if ( ! operand->dim ) {
         operand->is_space = true;
         operand->is_value = true;
      }
   }
   else if ( node->type == NODE_CALL ) {
      test_call( task, test, operand, ( struct call* ) node );
   }
   else if ( node->type == NODE_BINARY ) {
      struct binary* binary = ( struct binary* ) node;
      struct operand lside;
      init_operand( &lside, operand->name_offset );
      test_node( task, test, &lside, binary->lside );
      if ( ! lside.is_value ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &lside.pos,
            "left side of binary operation is not a value" );
         bail();
      }
      struct operand rside;
      init_operand( &rside, operand->name_offset );
      test_node( task, test, &rside, binary->rside );
      if ( ! rside.is_value ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &rside.pos,
            "right side of binary operation is not a value" );
         bail();
      }
      operand->pos = lside.pos;
      if ( lside.folded && rside.folded ) {
         int l = lside.value;
         int r = rside.value;
         // Division and modulo get special treatment because of the possibility
         // of a division by zero.
         if ( ( binary->op == BOP_DIV || binary->op == BOP_MOD ) && ! r ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &rside.pos,
               "division by zero" );
            bail();
         }
         switch ( binary->op ) {
         case BOP_MOD: l %= r; break;
         case BOP_MUL: l *= r; break;
         case BOP_DIV: l /= r; break;
         case BOP_ADD: l += r; break;
         case BOP_SUB: l -= r; break;
         case BOP_SHIFT_R: l >>= r; break;
         case BOP_SHIFT_L: l <<= r; break;
         case BOP_GTE: l = l >= r; break;
         case BOP_GT: l = l > r; break;
         case BOP_LTE: l = l <= r; break;
         case BOP_LT: l = l < r; break;
         case BOP_NEQ: l = l != r; break;
         case BOP_EQ: l = l == r; break;
         case BOP_BIT_AND: l = l & r; break;
         case BOP_BIT_XOR: l = l ^ r; break;
         case BOP_BIT_OR: l = l | r; break;
         case BOP_LOG_AND: l = l && r; break;
         case BOP_LOG_OR: l = l || r; break;
         default: break;
         }
         operand->value = l;
         operand->folded = true;
      }
      operand->is_value = true;
   }
   else if ( node->type == NODE_ASSIGN ) {
      struct assign* assign = ( struct assign* ) node;
      struct operand lside;
      init_operand( &lside, operand->name_offset );
      test_node( task, test, &lside, assign->lside );
      if ( ! lside.is_space ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &lside.pos,
            "cannot assign to operand" );
         bail();
      }
      struct operand rside;
      init_operand( &rside, operand->name_offset );
      test_node( task, test, &rside, assign->rside );
      if ( ! rside.is_value ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &rside.pos,
            "right side of assignment is not a value" );
         bail();
      }
      operand->is_value = true;
   }
   else if ( node->type == NODE_ACCESS ) {
      test_access( task, test, operand, ( struct access* ) node );
   }
   else if ( node->type == NODE_TERNARY ) {
      /*
      struct ternary* ternary = ( struct ternary* ) node;
      struct operand cond;
      init_operand( &cond, test, ternary->cond );
      test_node( task, &cond );
      struct test lside;
      init_test( &lside, test, ternary->lside );
      test_node( task, &lside );
      struct test rside;
      init_test( &rside, test, ternary->rside );
      test_node( task, &rside ); */
   }
   else if ( node->type == NODE_PAREN ) {
      struct paren* paren = ( struct paren* ) node;
      test_node( task, test, operand, paren->inside );
      operand->pos = paren->pos;
   }
}

void test_name_usage( struct task* task, struct expr_test* test,
   struct operand* operand, struct name_usage* usage ) {
   struct name* name = t_make_name( task, usage->text, task->ns->body );
   // Try the linked namespaces.
   if ( ! name->object ) {
      struct ns_link* link = task->ns->ns_link;
      while ( link && ! name->object ) {
         name = t_make_name( task, usage->text, link->ns->body );
         link = link->next;
      }
   }
   if ( name->object && name->object->resolved ) {
      use_object( task, operand, name->object );
      usage->object = ( struct node* ) name->object;
      operand->pos = usage->pos;
   }
   else if ( test->undef_err ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &usage->pos,
         "'%s' is undefined", usage->text );
      bail();
   }
   else {
      test->undef_erred = true;
      longjmp( test->bail, 1 );
   }
}

void use_object( struct task* task, struct operand* operand,
   struct object* object ) {
   if ( object->node.type == NODE_NAMESPACE ) {
      operand->ns = ( struct ns* ) object;
   }
   else if ( object->node.type == NODE_CONSTANT ) {
      struct constant* constant = ( struct constant* ) object;
      operand->type = task->type_int;
      operand->value = constant->value;
      operand->folded = true;
      operand->is_value = true;
   }
   else if ( object->node.type == NODE_VAR ) {
      struct var* var = ( struct var* ) object;
      ++var->usage;
      operand->type = var->type;
      if ( var->dim ) {
         operand->dim = var->dim;
      }
      else if ( var->type->primitive ) {
         operand->is_value = true;
         operand->is_space = true;
      }
   }
   else if ( object->node.type == NODE_TYPE_MEMBER ) {
      struct type_member* member = ( struct type_member* ) object;
      operand->type = member->type;
      if ( member->dim ) {
         operand->dim = member->dim;
      }
      else if ( member->type->primitive ) {
         operand->is_value = true;
         operand->is_space = true;
      }
   }
   else if ( object->node.type == NODE_FUNC ) {
      operand->func = ( struct func* ) object;
   }
   else if ( object->node.type == NODE_PARAM ) {
      struct param* param = ( struct param* ) object;
      operand->type = param->type;
      operand->is_value = true;
      operand->is_space = true;
   }
   else if ( object->node.type == NODE_SHORTCUT ) {
      struct shortcut* shortcut = ( struct shortcut* ) object;
      use_object( task, operand, shortcut->target->object );
   }
}

void test_call( struct task* task, struct expr_test* test,
   struct operand* operand, struct call* call ) {
   struct operand callee;
   init_operand( &callee, operand->name_offset );
   test_node( task, test, &callee, call->func_tree );
   if ( ! callee.func ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &callee.pos,
         "calling something not a function" );
      bail();
   }
   int count = 0;
   list_iter_t i;
   list_iter_init( &i, &call->args );
   if ( callee.func->type == FUNC_FORMAT ) {
      struct node* node = NULL;
      if ( ! list_end( &i ) ) {
         node = list_data( &i );
      }
      if ( ! node || ( node->type != NODE_FORMAT_ITEM &&
         node->type != NODE_BLOCK ) ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &call->pos,
            "function call missing format argument" );
         bail();
      }
      // Format-block:
      if ( node->type == NODE_BLOCK ) {
         struct stmt_test stmt_test;
         t_init_stmt_test( &stmt_test, test->stmt_test );
         stmt_test.format_block = ( struct block* ) node;
         t_test_block( task, &stmt_test, stmt_test.format_block );
         list_next( &i );
      }
      // Format-list:
      else {
         while ( ! list_end( &i ) ) {
            struct node* node = list_data( &i );
            if ( node->type == NODE_FORMAT_ITEM ) {
               t_test_format_item( task, ( struct format_item* ) node,
                  test->stmt_test, operand->name_offset );
               list_next( &i );
            }
            else {
               break;
            }
         }
      }
      ++count;
   }
   else if ( list_size( &call->args ) ) {
      struct node* node = list_data( &i );
      if ( node->type == NODE_FORMAT_ITEM ) {
         struct format_item* item = ( struct format_item* ) node;
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &item->pos,
            "passing format-list argument to non-format function" );
         bail();
      }
   }
   // Arguments:
   while ( ! list_end( &i ) ) {
      struct expr_test arg;
      t_init_expr_test( &arg );
      arg.undef_err = true;
      arg.needed = true;
      t_test_expr( task, &arg, list_data( &i ) );
      list_next( &i );
      ++count;
   }
   if ( count < callee.func->min_param ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &call->pos,
         "not enough arguments in function call" );
      const char* at_least = "";
      if ( callee.func->min_param != callee.func->max_param ) {
         at_least = "at least ";
      }
      const char* s = "";
      if ( callee.func->min_param != 1 ) {
         s = "s";
      }
      t_copy_name( callee.func->name, &task->str, '.' );
      diag( DIAG_FILE, &call->pos, "function '%s' needs %s%d argument%s",
         task->str.value, at_least, callee.func->min_param, s );
      bail();
   }
   else if ( count > callee.func->max_param ) {
      diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &call->pos,
         "too many arguments in function call" );
      const char* up_to = "";
      if ( callee.func->min_param != callee.func->max_param ) {
         up_to = "up to ";
      }
      const char* s = "";
      if ( callee.func->max_param != 1 ) {
         s = "s";
      }
      t_copy_name( callee.func->name, &task->str, '.' );
      diag( DIAG_FILE, &call->pos,
         "function '%s' takes %s%d argument%s",
         task->str.value, up_to, callee.func->max_param, s );
      bail();
   }
   // Call to a latent function cannot appear in a function or a format block.
   if ( callee.func->type == FUNC_DED ) {
      struct func_ded* impl = callee.func->impl;
      if ( impl->latent && task->in_func ) {
         t_copy_name( callee.func->name, &task->str, '.' );
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &callee.pos,
            "calling a latent function inside a function" );
         diag( DIAG_FILE, &callee.pos,
         "waiting functions like '%s' can only be called inside a script",
         task->str.value );
         bail();
      }
   }
   call->func = callee.func;
   operand->pos = callee.pos;
   operand->type = callee.func->value;
   if ( operand->type ) {
      operand->is_value = true;
   }
}

void t_test_format_item( struct task* task, struct format_item* item,
   struct stmt_test* stmt_test, struct name* name_offset ) {
   struct expr_test expr_test;
   t_init_expr_test( &expr_test );
   expr_test.stmt_test = stmt_test;
   expr_test.undef_err = true;
   struct operand arg;
   init_operand( &arg, name_offset );
   test_node( task, &expr_test, &arg, item->expr->root );
   if ( item->cast == FCAST_ARRAY ) {
      if ( ! arg.dim ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &arg.pos, "argument not an array" );
         bail();
      }
      else if ( arg.dim->next ) {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &arg.pos, "array argument not of a single dimension" );
         bail();
      }
   }
}

void test_access( struct task* task, struct expr_test* test,
   struct operand* operand, struct access* access ) {
   struct type* type = NULL;
   struct ns* ns = NULL;
   if ( access->lside ) {
      struct operand object;
      init_operand( &object, operand->name_offset );
      test_node( task, test, &object, access->lside );
      if ( object.ns ) {
         ns = object.ns;
      }
      else if ( object.type && ! object.dim ) {
         type = object.type;
      }
      else {
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &object.pos,
            "using . on something not a struct or a namespace" );
         bail();
      }
   }
   else {
      ns = task->ns_global;
   }
   struct name* name_offset = NULL;
   if ( type ) {
      name_offset = type->body;
   }
   else {
      name_offset = ns->body;
   }
   struct name* name = t_make_name( task, access->name, name_offset );
   struct object* object = name->object;
   if ( ns ) {
      while ( object && object->link ) {
         object = object->link;
      }
   }
   if ( ! object ) {
      if ( ! test->undef_err ) {
         test->undef_erred = true;
         longjmp( test->bail, 0 );
      }
      if ( ns ) {
         if ( ns == task->ns_global ) {
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &access->pos,
               "'%s' not found in global namespace", access->name );
         }
         else {
            t_copy_name( ns->name, &task->str, '.' );
            diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &access->pos,
               "'%s' not found in namespace '%s'", access->name,
               task->str.value );
         }
      }
      else if ( type ) {
         // TODO: Remember to implement for anonynous structs!
         const char* subject = "type";
         if ( ! type->primitive ) {
            subject = "struct";
         }
         t_copy_name( type->name, &task->str, '.' );
         diag( DIAG_ERR | DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &access->pos,
            "%s `%s` has no member named `%s`", subject, task->str.value,
            access->name );
      }
      bail();
   }
   use_object( task, operand, object );
   access->rside = ( struct node* ) object;
   operand->pos = access->pos;
}