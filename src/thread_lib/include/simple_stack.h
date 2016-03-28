#ifndef SIMPLE_STACK_H
#define SIMPLE_STACK_H

struct simple_stack{
    void ** entries;
    size_t max_len;
    size_t current_len;
};

static struct simple_stack * simple_stack_init(size_t max_len){
    struct simple_stack * stack = (struct simple_stack *)WRAP(malloc)(sizeof(struct simple_stack));
    stack->entries=(void **)WRAP(malloc)(sizeof(void *)*max_len);
    stack->max_len=max_len;
    stack->current_len=0;
    return stack;
}

static bool simple_stack_push(struct simple_stack * stack, void * entry){
    bool result;
    if (stack->current_len < stack->max_len){
        stack->entries[stack->current_len++]=entry;
        result=true;
    }
    else{
        result=false;
    }
    return result;
}

static void * simple_stack_pop(struct simple_stack * stack){
    if (stack->current_len > 0){
        return stack->entries[--stack->current_len];
    }
    else{
        return NULL;
    }
}

static void simple_stack_clear(struct simple_stack * stack){
    stack->current_len=0;
}

#endif
