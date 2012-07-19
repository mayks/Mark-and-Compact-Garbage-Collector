typedef unsigned char byte;

typedef struct ClassDescriptor {
    char *name;
    int size;        /* size in bytes of struct */
    int num_fields;
    /* offset from ptr to object of only fields that are managed ptrs */
    int *field_offsets;
} ClassDescriptor;

typedef struct Object {
	ClassDescriptor *class;
	byte marked;
	struct Object *forwarded; // where we've moved this object
} Object;

typedef struct String /* extends Object */ {
	ClassDescriptor *class;
	byte marked;
	Object *forwarded; // where we've moved this object
    
	int length;
	char str[];        
    /* the string starts at the end of fixed fields; this field
     * does not take any room in the structure */
} String;

extern ClassDescriptor String_class;

#define MAX_ROOTS 100
extern Object **_roots[MAX_ROOTS];
extern int _rp;

// GC interface
extern void gc_init(int size);
extern void gc();
extern void gc_done();
extern Object *gc_alloc(ClassDescriptor *class);
extern String *gc_alloc_string(int size);
extern char *gc_get_state();
extern int valid_gc_space(int size);

extern int walk_graph(Object* o);
extern int valid_gc_space(int size);
extern void print_roots();
extern void print_debug(char* msg);
extern int gc_num_roots();
extern void compact();
extern void calculate_forwarded();
extern void moveto_forwarded();
extern void reset_mark();
extern int get_string_length(Object* o);

#define gc_save_rp			int __rp = _rp;
#define gc_add_root(p)      _roots[_rp++] = (Object **)(&(p));
#define gc_restore_roots	_rp = __rp;



