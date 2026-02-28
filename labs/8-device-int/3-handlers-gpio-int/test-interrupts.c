/*
 * interrupt handling test code. 
 *
 * All your code goes here.  Search for the three <todo>'s.
 */

// libpi/include --- is just the code from lab <4-interrupts>
#include "timer-interrupt.h"
// you write this header in part 1.
#include "vector-base.h"

// our prototypes. 
#include "test-interrupts.h"

/*************** You don't need to modify this first part ********** 
 *
 */
// enum { 
//     ARM_Timer_Base      = 0x2000B400,
//     ARM_Timer_Load      = ARM_Timer_Base + 0x00, // p196
//     ARM_Timer_Value     = ARM_Timer_Base + 0x04, // read-only
//     ARM_Timer_Control   = ARM_Timer_Base + 0x08,

//     ARM_Timer_IRQ_Clear = ARM_Timer_Base + 0x0c,

//     // Errata fo p198: 
//     // neither are register 0x40C raw is 0x410, masked is 0x414
//     ARM_Timer_IRQ_Raw   = ARM_Timer_Base + 0x10,
//     ARM_Timer_IRQ_Masked   = ARM_Timer_Base + 0x14,

//     ARM_Timer_Reload    = ARM_Timer_Base + 0x18,
//     ARM_Timer_PreDiv    = ARM_Timer_Base + 0x1c,
//     ARM_Timer_Counter   = ARM_Timer_Base + 0x20,
// };

// initialization routine
typedef void (*init_fn_t)(void);
// client interrupt handler: returns 0 if didn't handle anything.
typedef int (*interrupt_fn_t)(uint32_t pc);

// count of all interrupts that occured.
volatile int n_interrupt;

// function pointer called for interrupt dispatch (below).
static interrupt_fn_t interrupt_fn;

// default vector: just forwards it to the registered
// handler 
void interrupt_vector(unsigned pc) {
    dev_barrier();
    n_interrupt++;

    if(!interrupt_fn(pc))
        panic("got interrupt: but not handled by registered handlers\n");

    dev_barrier();
}


// simple check to make sure <in_pin> and <out_pin>
// are (1) configured and (2) have a jumper between them.
static int loopback_check(int in_pin, int out_pin) {
    gpio_write(out_pin,1);
    if(gpio_read(in_pin) != 1)
        panic("connect pin<%d> and pin<%d> with loopback\n",
            in_pin, out_pin);
    gpio_write(out_pin,0);
    if(gpio_read(in_pin) != 0)
        panic("connect pin<%d> and pin<%d> with loopback\n",
            in_pin, out_pin);
    return 1;
}

// initialize all the interrupt stuff.  client passes in the 
// gpio int routine <fn>
//
// make sure you understand how this works.
void test_init(init_fn_t init_fn, interrupt_fn_t int_fn) {
    // check loopback.
    gpio_set_output(out_pin);
    gpio_set_input(in_pin);
    if(!loopback_check(in_pin, out_pin))
        panic("connect <%d> and <%d>\n", in_pin,out_pin);

    // initialize.

    // make sure system interrupts are off.
    cpsr_int_disable();

    // just like lab 4-interrupts: force clear 
    // interrupt state
    //  BCM2835 manual, section 7.5 , 112
    dev_barrier();
    PUT32(IRQ_Disable_1, 0xffffffff);
    PUT32(IRQ_Disable_2, 0xffffffff);
    dev_barrier();

    // setup the vector.
    extern uint32_t interrupt_vec[];
    vector_base_set(interrupt_vec);

    init_fn();
    interrupt_fn = int_fn;

    // in case there was an event queued up.
    gpio_event_clear(in_pin);

    // start global interrupts.
    cpsr_int_enable();
}

/********************************************************************
 * falling edge code.
 */

// count of all falling edges.
volatile int n_falling;

/*
 * implement this handler: 
 *  1. check if there is a GPIO event.
 *  2. check if it was a falling edge: return 1 if so, 0 otherwise
 */
int falling_handler(uint32_t pc) {
    // todo("implement this: return 0 if no rising int\n");
    int result = 0;
    if (gpio_event_detected(in_pin)) {
        dev_barrier();
        if (!gpio_read(in_pin)) {
            result = 1;
            n_falling += 1;
            gpio_event_clear(in_pin);
        }
        dev_barrier();
    }
    
    return result;
}

// initialize for a falling edge
void falling_init_fn(void) {
    // falling = 1->0 transition, so set to 1.
    gpio_write(out_pin, 1);
    gpio_int_falling_edge(in_pin);
}

// called by test to setup.
void falling_init(void) {
    test_init(falling_init_fn, falling_handler);
}


/********************************************************************
 * rising edge.
 */

volatile int n_rising;


/*
 * implement this handler: 
 *  1. check if there is a GPIO event.
 *  2. check if it was a rising edge: return 1 if so, 0 otherwise
 */
int rising_handler(uint32_t pc) {
    // todo("implement this: return 0 if no rising int\n");
    int result = 0;
    if (gpio_event_detected(in_pin)) {
        dev_barrier();
        if (gpio_read(in_pin)) {
            result = 1;
            n_rising += 1;
            gpio_event_clear(in_pin);
        }
        dev_barrier();
    }
    
    return result;
}

static void rising_init_fn(void) {
    gpio_write(out_pin, 0);
    gpio_int_rising_edge(in_pin);
}

void rising_init(void) {
    test_init(rising_init_fn, rising_handler);
}

/********************************************************************
 * rise-falle edges: make sure you can handle composition.  you don't
 * have to modify anything.
 */

int rise_fall_handler(uint32_t pc) {
    // NOTE: C short circuits: use "|" not "||"!
    return rising_handler(pc)
        | falling_handler(pc);
}

// compose rise and fall.
void rise_fall_init_fn(void) {
    // standarize: call rise first, then fall.
    rising_init_fn();
    falling_init_fn();
    // make sure in known state.
    assert(gpio_read(in_pin) == 1);
}

void rise_fall_init(void) {
    test_init(rise_fall_init_fn, rise_fall_handler);
}


/********************************************************************
 * timer interrupt: pull the relevant timer interrupt code  from
 * lab 4.
 */

/*
 * implement this handler: 
 *  1. check if it was a timer event.
 *  2. return 1 if so.
 */
int timer_test_handler(uint32_t pc) {
    dev_barrier();
    int result = 0;

    // should look very similar to the timer interrupt handler.
    // todo("implement thi s by stealing pieces from 4-interrupts/0-timer-int");
    unsigned pending = GET32(IRQ_basic_pending);
    dev_barrier();
    if ((pending & ARM_Timer_IRQ) == 1) {
        n_interrupt += 1;
        result = 1;
        dev_barrier();
        PUT32(ARM_Timer_IRQ_Clear, 1);
        dev_barrier();
    }
    return result;
}

void timer_init_fn(void) {
    // turn on timer interrupts.
    timer_init(1, 0x4);
}

void timer_test_init(void) {
    test_init(timer_init_fn, timer_test_handler);
}

/**********************************************************************
 * rise+fall+timer: composes all the above.  you don't have to modify.
 */

int rise_fall_timer_handler(uint32_t pc) {
    // NOTE: C short circuits: use "|" not "||"!
    return rise_fall_handler(pc)
        | timer_test_handler(pc);
}

// compose rise and fall and timer.
void rise_fall_timer_fn(void) {
    rise_fall_init_fn();
    timer_init_fn();
}

void rise_fall_timer_init(void) {
    test_init(rise_fall_timer_fn, rise_fall_timer_handler);
}
