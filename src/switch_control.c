/* Copyright (c) 2016 Nicholas Esterer. All rights reserved. */

#include "switch_control.h" 
#include <stddef.h> 

/* Functions for looking at a GPIO port and calling functions on the value */
#ifdef SWITCH_CONTROL_DEBUG

 #include "leds.h" 

 #define switch_control_set_req_pin()       aux_led2_set()
 #define switch_control_reset_req_pin()     aux_led2_reset()
 #define switch_control_tog_req_pin()       aux_led2_tog()

 #define switch_control_set_check_pin()     aux_led3_set()
 #define switch_control_reset_check_pin()   aux_led3_reset()
 #define switch_control_tog_check_pin()     aux_led3_tog()

 #define switch_control_set_prime_pin()     aux_led1_set()
 #define switch_control_reset_prime_pin()   aux_led1_reset()
 #define switch_control_tog_prime_pin()     aux_led1_tog()
#endif /* SWITCH_CONTROL_DEBUG */ 

static switch_control_t * _switch_controls = NULL;

void switch_control_do_all(void)
{
    switch_control_t *tmp;
    tmp = _switch_controls;
    while (tmp) {
        tmp->func(tmp);
        tmp = tmp->next;
    }
}

void switch_control_add(switch_control_t *sc)
{
    sc->next = _switch_controls;
    _switch_controls = sc;
}

void switch_control_init(switch_control_t *sc,
                         volatile uint32_t *port_addr,
                         uint32_t port_bit,
                         void (*func)(switch_control_t*),
                         void *data)
{
    sc->port_addr = port_addr;
    sc->port_bit = port_bit;
    sc->func = func;
    sc->data = data;
    sc->next = NULL;
}

/* Called when checking pin state (canonically called by switch_control_do_all
 * when switch states are to be checked, usually in some periodic checking
 * function, like the callback that is called when the codec requests samples,
 * for instance).
 *
 * DEBOUNCE METHOD A
 * This method takes into account any transition from low to high on the switch
 * pin. This is problematic if the switch can give spurious low/high transitions
 * of very short duration.
 */
static void switch_control_debounce_func_a(switch_control_t *sc)
{
    switch_debouncer_t *sd = (switch_debouncer_t*)sc->data;
#ifdef SWITCH_CONTROL_DEBUG 
    switch_control_set_check_pin();
    switch_control_reset_check_pin();
#endif  
    if (sd->primed) {
        if (sd->get_pin_state(sd) == 0) {
            /* The pin is low. */
            /* Pin has been brought to its rest (reset) position again */
            /* call function */
            sd->func(sd);
            /* reset primed */
            sd->primed = 0;
#ifdef SWITCH_CONTROL_DEBUG 
            switch_control_reset_prime_pin();
#endif  
            /* start ignore period by setting number of ignores */
            sd->n_ignores = sd->init_n_ignores;
        }
        /* otherwise we're still waiting for the pin to go low */
    } else {
        /* if the ignore period is over and request was made*/
        if ((sd->n_ignores == 0) && (sd->get_req_state(sd))) {
            /* prime */
            sd->primed = 1;
#ifdef SWITCH_CONTROL_DEBUG 
            switch_control_set_prime_pin();
#endif  
        }
    }
    /* reset request in any case*/
    sd->reset_req_state(sd);
#ifdef SWITCH_CONTROL_DEBUG 
    switch_control_reset_req_pin();
#endif  
    /* decrement ignores */
    if (sd->n_ignores) {
        sd->n_ignores--;
    }
}

/* This ignores "sd->init_n_ignores - 1" readings of the switch line being low
 * and then sets a primed variable. When primed, if the reading is high, the
 * function "sd->fun" is called. This means that the switch must be low for at
 * least (sd->init_n_ignores - 1)*time_between_checks (where time_between_checks
 * is determined by how often a routine checks it, say the time of an audio
 * buffer) before it is primed, so must be checked more often than the shortest
 * recognizable switch high-low-high sequence.
 *
 * This is called
 * DEBOUNCE METHOD B
 * in the documentation.
 */
static void switch_control_debounce_func_b(switch_control_t *sc)
{
    switch_debouncer_t *sd = (switch_debouncer_t*)sc->data;
    if (sd->get_pin_state(sd)) {
        if (sd->get_req_state(sd)) {
            sd->reset_req_state(sd);
            if (!sd->primed) {
                sd->n_ignores = sd->init_n_ignores;
            }
        }
        /* If < 0, sits at -1 until reset to init_n_ignores. */
        if (sd->n_ignores >= 0) {
            sd->n_ignores--;
        }
        if (sd->n_ignores == 0) {
            sd->primed = 1;
        }
    } else {
        if (sd->primed) {
            sd->func(sd);
            sd->primed = 0;
        }
        sd->reset_req_state(sd);
    }
}

/* This ignores sd->init_n_ignores - 1 readings like DEBOUNCE METHOD B but
 * instead of setting a primed variable, just calls the function. This is called
 * DEBOUNCE METHOD C.
 */
static void switch_control_debounce_func_c(switch_control_t *sc)
{
    switch_debouncer_t *sd = (switch_debouncer_t*)sc->data;
    // This method doesn't use interrupts and so sd->get_req_state(sd) is not
    // called
    if (sd->get_pin_state(sd) == 1) {
        // switch is down
        switch (sd->state) {
            case switch_debouncer_state_WAIT_1ST_TRIG:
                sd->n_ignores = sd->init_n_ignores;
                sd->state = switch_debouncer_state_WAIT_NTH_TRIG;
            case switch_debouncer_state_WAIT_NTH_TRIG:
                if (sd->n_ignores == 0) {
                    sd->func(sd);
                    sd->state = switch_debouncer_state_WAIT_RESET;
                } else {
                    sd->n_ignores--;
                }
            case switch_debouncer_state_WAIT_RESET:
            case switch_debouncer_state_RESET_TIMEOUT:
                return;
        }
    } else {
        // switch is up
        switch (sd->state) {
            case switch_debouncer_state_WAIT_RESET:
                // Switch has now been reset
                sd->n_ignores = sd->init_n_ignores;
                sd->state = switch_debouncer_state_RESET_TIMEOUT;
            case switch_debouncer_state_RESET_TIMEOUT:
                // Must be down for n_ignore checks before active again
                if (sd->n_ignores == 0) {
                    sd->state = switch_debouncer_state_WAIT_1ST_TRIG;
                } else {
                    sd->n_ignores--;
                }
                return;
            case switch_debouncer_state_WAIT_NTH_TRIG:
                // Switch hasn't been down long enough
                sd->state = switch_debouncer_state_WAIT_1ST_TRIG;
            case switch_debouncer_state_WAIT_1ST_TRIG:
                return;
        }
    }
}

static uint32_t get_mom_req_state(switch_debouncer_t *sd)
{
    mom_state_t* data = (mom_state_t*)sd->data;
    return (*(data->req_state_addr) >> data->req_state_bit) & 0x1;
}

static uint32_t get_mom_pin_state(switch_debouncer_t *sd)
{
    /* Because the pins are active when low, we reverse polarity here */
    mom_state_t* data = (mom_state_t*)sd->data;
    return !((*(data->pin_state_addr) >> data->pin_state_bit) & 0x1);
}

static void reset_mom_req_state(switch_debouncer_t *sd)
{
    mom_state_t* data = (mom_state_t*)sd->data;
    *(data->req_state_addr) &= ~(0x1 << data->req_state_bit);
}

void switch_debouncer_init(switch_debouncer_t *sd,
                           void (*func)(switch_debouncer_t *),
                           uint32_t init_n_ignores,
                           mom_state_t *state)
{
    sd->get_req_state = get_mom_req_state;
    sd->get_pin_state = get_mom_pin_state;
    sd->reset_req_state = reset_mom_req_state;
    sd->func = func;
    sd->init_n_ignores = init_n_ignores;
    sd->n_ignores = 0;
    sd->primed = 0;
    sd->data = (void*)state;
}

void switch_control_debounce_init(switch_control_t *sc,
                                  switch_debouncer_t *sd,
                                  switch_debouncer_style_t sty)
{
    void (*func) (switch_control_t *);
    switch (sty) {
        case switch_debouncer_style_A:
            func = switch_control_debounce_func_a;
            break;
        case switch_debouncer_style_B:
            func = switch_control_debounce_func_b;
            break;
        case switch_debouncer_style_C:
            func = switch_control_debounce_func_c;
            break;
    }
    switch_control_init(sc,
                        (volatile uint32_t*)NULL,
                        0,
                        func,
                        (void*)sd);
}
