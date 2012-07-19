
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "gc.h"

Object**_roots[MAX_ROOTS];
int _rp;
void* gc_p;
void* gc_end_p;
void* gc_next_p;

ClassDescriptor String_class = {
    "String_class",
    sizeof(struct String),
    0,
    NULL
};

void gc_init(int size)
{
    if(size < 1)
    {
        fprintf(stderr, "*** cannot allocate size less than 1 ***\n");
        exit(0);
    }
    gc_p = malloc(size);
    gc_end_p = gc_p+(size);
    
    gc_next_p = gc_p;
    
#ifdef gcdebug
    printf("===== gc_init ======\n");
    printf("gc_size: %d\n", size);
    printf("gc_addr: %p - %p\n", gc_p, gc_end_p-1);
    printf("====================\n");
#endif
}

void gc()
{
#ifdef gcdebug
    printf("******** gc ********\n");
    print_roots();
#endif
    
    reset_mark();

    // marking
    int i;
    int none = 0;
    for(i = 0; i < _rp; i++)
    {
        Object* o = (*(_roots[i]));
        none += walk_graph(o);
    }
    if(none != 0)
    {
       compact();
    }
    else
    {
        gc_next_p = gc_p;
    }
    
#ifdef gcdebug
    printf("********************\n");
#endif
}

int walk_graph(Object* o)
{
    if(o == NULL)
    {
#ifdef gcdebug
        printf("walk o Null\n");
#endif
        return 0;   
    }
    if(o->marked != 0)
    {
#ifdef gcdebug
        printf("walk mark != 0 %p\n", o);
#endif
        return 0;
    }
    o->marked = 1;
    
#ifdef gcdebug
    char* name = o->class->name;
    printf("marking %p to %p : %s ", o, o->forwarded, name);
    if(strcmp(name, "String_class") == 0)
    {
        char* sss = ((String*)o)->str;
        printf("- %s\n",sss);
    }
    else
    {
        printf("\n");
    }
#endif
    
    int nf = o->class->num_fields;
    int* fo = o->class->field_offsets;

    int i;
    for(i = 0; i < nf; i++)
    {     
        int member_offset = fo[i];
        void** curr_p = ((void*)o)+member_offset;
        
        Object* x = (Object*) *curr_p;
        walk_graph(x);
        if(x != NULL)
        {
            if(x->forwarded != NULL)
                (*curr_p) = x->forwarded;
        }
    }
    return 1;
}

void reset_mark()
{
    void* curr_p = gc_p;
    
    while(curr_p < gc_next_p)
    {
        Object* o = (Object*) curr_p;
        o->marked = 0;
        int size = o->class->size;
        size += get_string_length(o);
        
        curr_p = curr_p+size;
    }
}

void compact()
{
    calculate_forwarded();
    
    int i;
    for(i = 0; i < _rp; i++)
    {
        Object* o = (*(_roots[i]));

        walk_graph(o);
        if(o != NULL)
        {
            if(o->forwarded != NULL)
                (*(_roots[i])) = o->forwarded;  
        }
    }

    moveto_forwarded();
    
#ifdef gcdebug
    printf("next available %p\n", gc_next_p);
    print_roots();
    for(i = 0; i < _rp; i++)
    {
        Object* o = (*(_roots[i]));
        walk_graph(o);
    }
#endif
}

void calculate_forwarded()
{    
    int can_move = 0; // false
    void* moveto_p = NULL;
    void* curr_p = gc_p;
    
    while(curr_p < gc_next_p)
    {
        Object* o = (Object*) curr_p;
        int size = o->class->size;
        size += get_string_length(o);
        
#ifdef gcdebug
        printf("forward: mark %d can_move %d %p move_to %p ", o->marked, can_move, o, moveto_p);
#endif
        if(o->marked == 1)
        {
            if(can_move == 1)
            {
                o->forwarded = moveto_p;
                moveto_p = moveto_p+size;
            }
            else
            {
                o->forwarded = NULL;
            }
        }
        else
        {
            if(can_move == 0)
            {
                can_move = 1;
                moveto_p = curr_p;
            }
        }
        
#ifdef gcdebug
        printf("forwarded to %p : %s\n", o->forwarded, name);
#endif
        o->marked = 0;
        curr_p = curr_p+size;
    }
}

void moveto_forwarded()
{
    int move = 0; // false
    void* moveto_p = gc_next_p;
    void* curr_p = gc_p;
    
    while(curr_p < gc_next_p)
    {
        Object* o = (Object*) curr_p;
        int size = o->class->size;
        size += get_string_length(o);
        
        if(o->marked == 1)
        {
            void* target_p = o->forwarded;
            o->marked = 0;
            o->forwarded = NULL;
            
            if(target_p != NULL)
                memcpy(target_p, o, size);
            
            if(move == 1)
            {
                moveto_p = moveto_p+size;
            }
#ifdef gcdebug
            printf("move: mark 1 %p %p %s -- %p\n", o, o->forwarded, name, moveto_p);
#endif
        }
        else
        {
            if(move == 0)
            {
                move = 1;
                moveto_p = curr_p;   
            }
#ifdef gcdebug
            printf("move: mark 0 %p %s -- %p\n", o, name, moveto_p);
#endif
        }
        curr_p = curr_p+size;
    }
    if(move == 1)
        gc_next_p = moveto_p;
    
}

void gc_done()
{
    free(gc_p);
}

Object *gc_alloc(ClassDescriptor *class)
{
#ifdef gcdebug
    printf("----- gc_alloc -----\n");
#endif    
    
    int add_size = class->size;
    
#ifdef gcdebug
    printf("size to allocate: %d\n", add_size);
#endif    
    
    Object* o = NULL;
    if(valid_gc_space(add_size) < 0)
    {
        fprintf(stderr, "*** not enough space ***\n");
        exit(0);
    }
#ifdef gcdebug
    printf("addr: %p\n", gc_next_p);
#endif  
    
    o = gc_next_p;
    o->class = class;
    o->marked = 0;
    o->forwarded = NULL;
    gc_next_p = gc_next_p+add_size;
    int i;
    for(i = 0; i < o->class->num_fields; i++)
    {
        int member_offset = o->class->field_offsets[i];
        void** curr = ((void*)o)+member_offset;
        *curr = NULL;
    }
    
#ifdef gcdebug
    printf("--------------------\n");
#endif
    return o;
}

String *gc_alloc_string(int size)
{
#ifdef gcdebug
    printf("--- alloc_string ---\n");
#endif
    
    int add_size = sizeof(struct String)+(sizeof(char)*(size+1));
    
#ifdef gcdebug
    printf("size request: %d\n", size);
    printf("size to allocate: %d\n", add_size);
#endif

    String* s = NULL;
    if(valid_gc_space(add_size) < 0)
    {
        fprintf(stderr, "*** not enough space ***\n");
        exit(0);
    }
    
#ifdef gcdebug
    printf("addr: %p\n", gc_next_p);
#endif
        
    s = gc_next_p;
    s->class = &String_class;
    s->marked = 0;
    s->forwarded = NULL;
    s->length = size;
    gc_next_p = gc_next_p+add_size;
    
#ifdef gcdebug
    printf("--------------------\n");
#endif
    
    return s;
}

int valid_gc_space(int size)
{
    if(gc_end_p-gc_next_p >= size)
    {
        return 1;
    }
    
    gc();
    if(gc_end_p-gc_next_p < size)
    {
        return -1;
    }
    return 1;
}

char *gc_get_state()
{
    void* curr = gc_p;
    char* found = malloc(sizeof(char)*1000);
    sprintf(found, "next_free=%ld\n", gc_next_p-gc_p);
    sprintf(found, "%sobjects:\n", found);
    
    int i;
    while(curr < gc_next_p)
    {
        Object* o = (Object*)curr;
        char* str = o->class->name;
        int size = o->class->size;
        if(strcmp(str, "String_class") == 0)
        {
            int str_size = ((String*)o)->length + 1;
            sprintf(found, "%s  %04ld:String[%d+%d]=\"%s\"\n", 
                    found, curr-gc_p, size, str_size, ((String*)o)->str);
            size += str_size;
        }
        else
        {
            sprintf(found, "%s  %04ld:%s[%d]->[", found, curr-gc_p, str, size);
            int* fo = o->class->field_offsets;
            for(i = 0; i < o->class->num_fields; i++)
            {
                int member_offset = fo[i];
                void** icurr = curr+member_offset;
                if(i > 0)
                {
                    sprintf(found, "%s,", found);   
                }
                if((*icurr) == NULL)
                {
                    sprintf(found, "%sNULL", found);
                }
                else
                {
                    sprintf(found, "%s%ld", found, (*icurr)-gc_p);
                }
            }
            sprintf(found, "%s]\n", found);
        }
        curr += size;
    }

#ifdef gcdebug
    printf("#########################\n");
    printf("%s\n", found);
    printf("#########################\n");
#endif
    
    return found;
}

int get_string_length(Object* o)
{
    char* name = o->class->name;
    int size = 0;
    if(strcmp(name, "String_class") == 0)
    {
        size = ((String*)o)->length+1;
    }
    
    return size;
}

void print_roots()
{
    printf("++++++++++++++++++++\n");
    int i;
    for(i = 0; i < _rp; i++)
    {
        if(*(_roots[i]) != NULL)
            printf("root[%d]: %p : %s\n", i, *(_roots[i]), (*(_roots[i]))->class->name);
    }
    printf("++++++++++++++++++++\n");
}

int gc_num_roots()
{
    return _rp;
}


