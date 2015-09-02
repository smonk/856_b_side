#include <stdint.h>
#include <stddef.h> 
#include "switch_control_test.h" 
#include "switches.h"
#include "switch_control.h" 
#include "leds.h" 

switch_control_t switch_control[4];
uint32_t         switch_data[] = {1,3,5,7};
uint32_t         footswitch_data[] = {1,2};

static void switch_control_test_func(switch_control_t *sc)
{
    /* Turn light on when switch state goes low. */
    uint32_t data = *((uint32_t*)sc->data);
    switch (data) {
        case 1:
            if (!switch_control_get_state(sc)) {
                led1_set();
            } else {
                led1_reset();
            }
            break;
        case 3:
            if (!switch_control_get_state(sc)) {
                led3_set();
            } else {
                led3_reset();
            }
            break;
        case 5:
            if (!switch_control_get_state(sc)) {
                led5_set();
            } else {
                led5_reset();
            }
            break;
        case 7:
            if (!switch_control_get_state(sc)) {
                led7_set();
            } else {
                led7_reset();
            }
            break;
    }
}

static void switch_control_test_fsw_func(switch_control_t *sc)
{
    /* Turn light on when switch state goes low. */
    static uint32_t left_toggle, right_toggle;
    uint32_t data = *((uint32_t*)sc->data);
    switch (data) {
        case 1:
            if (switch_control_get_state(sc)) {
                left_toggle ^= 0x1;
                switch_control_set_state(sc,0x0);
            }
            break;
        case 2:
            if (switch_control_get_state(sc)) {
                right_toggle ^= 0x1;
                switch_control_set_state(sc,0x0);
            }
            break;
    }
    if (left_toggle) {
        led1_reset();
        led3_set();
    } else {
        led1_set();
        led3_reset();
    }
    if (right_toggle) {
        led5_reset();
        led7_set();
    } else {
        led5_set();
        led7_reset();
    }
}

typedef struct __switch_debouncer_t {
    /* Should return 1 if request has been made */
    uint32_t (*get_req_state)(struct __switch_debouncer_t *);
    /* Should return 1 if pin is high, so for a momentary switch that means the
     * state that it is in when untouched. */
    uint32_t (*get_pin_state)(struct __switch_debouncer_t *);
    /* Resets the request state */
    void     (*reset_req_state)(struct __switch_debouncer_t *);
    /* What to do when a request has been verified and acknowledged */
    void     (*func)(struct __switch_debouncer_t *);
    /* The number of ignores that are specified after an acknowledged request */
    uint32_t init_n_ignores;
    /* The number of request that are to be ignored from now on */
    uint32_t n_ignores;
    void     *data;
} switch_debouncer_t;

typedef struct __fsw_state_t {
    volatile uint32_t *req_state_addr;
    uint32_t           req_state_bit;
    volatile uint32_t *pin_state_addr;
    uint32_t           pin_state_bit;
} fsw_state_t;

static void switch_control_test_debounce_fsw_func(switch_control_t *sc)
{
    switch_debouncer_t *sd = (switch_debouncer_t*)sc->data;
    if (sd->get_req_state(sd)) {
        if (sd->n_ignores) {
            /* Did this request come too soon after a previous acknowledge? If
             * so, the request is bogus. */
            sd->reset_req_state(sd);
        } else if (sd->get_pin_state(sd) == 0) {
            /* The pin is low. */
            /* Pin has been brought to its rest (reset) position again */
            sd->func(sd);
            sd->n_ignores = sd->init_n_ignores;
            sd->reset_req_state(sd);
        } 
        /* Otherwise we are still waiting for the pin to go low. */
    }
    if (sd->n_ignores) {
        sd->n_ignores--;
    }
}

static uint32_t get_fsw_req_state(switch_debouncer_t *sd)
{
    fsw_state_t* data = (fsw_state_t*)sd->data;
    return (*(data->req_state_addr) >> data->req_state_bit) & 0x1;
}

static uint32_t get_fsw_pin_state(switch_debouncer_t *sd)
{
    /* Because the pins are active when low, we reverse polarity here */
    fsw_state_t* data = (fsw_state_t*)sd->data;
    return !((*(data->pin_state_addr) >> data->pin_state_bit) & 0x1);
}

static void reset_fsw_req_state(switch_debouncer_t *sd)
{
    fsw_state_t* data = (fsw_state_t*)sd->data;
    *(data->req_state_addr) &= ~(0x1 << data->req_state_bit);
}

static void fsw1_func(switch_debouncer_t *sd)
{
    led1_tog();
}

static void fsw2_func(switch_debouncer_t *sd)
{
    led3_tog();
}

void switch_control_test_setup(void)
{
#if defined SWITCH_CONTROL_TEST_1
    switch_control_init(&switch_control[0],
                        SW1_TOP_ADDR,
                        SW1_TOP_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[0]);
    switch_control_init(&switch_control[1],
                        SW1_BTM_ADDR,
                        SW1_BTM_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[1]);
    switch_control_init(&switch_control[2],
                        SW2_TOP_ADDR,
                        SW2_TOP_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[2]);
    switch_control_init(&switch_control[3],
                        SW2_BTM_ADDR,
                        SW2_BTM_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[3]);
#elif defined SWITCH_CONTROL_TEST_2
    switch_control_init(&switch_control[0],
                        SW3_TOP_ADDR,
                        SW3_TOP_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[0]);
    switch_control_init(&switch_control[1],
                        SW3_BTM_ADDR,
                        SW3_BTM_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[1]);
    switch_control_init(&switch_control[2],
                        SW4_TOP_ADDR,
                        SW4_TOP_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[2]);
    switch_control_init(&switch_control[3],
                        SW4_BTM_ADDR,
                        SW4_BTM_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[3]);
#elif defined SWITCH_CONTROL_TEST_3
    switch_control_init(&switch_control[0],
                        SW5_TOP_ADDR,
                        SW5_TOP_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[0]);
    switch_control_init(&switch_control[1],
                        SW5_BTM_ADDR,
                        SW5_BTM_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[1]);
    switch_control_init(&switch_control[2],
                        SW6_TOP_ADDR,
                        SW6_TOP_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[2]);
    switch_control_init(&switch_control[3],
                        SW6_BTM_ADDR,
                        SW6_BTM_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[3]);
#elif defined SWITCH_CONTROL_TEST_4
    switch_control_init(&switch_control[0],
                        SW7_TOP_ADDR,
                        SW7_TOP_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[0]);
    switch_control_init(&switch_control[1],
                        SW7_BTM_ADDR,
                        SW7_BTM_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[1]);
    switch_control_init(&switch_control[2],
                        SW8_TOP_ADDR,
                        SW8_TOP_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[2]);
    switch_control_init(&switch_control[3],
                        SW8_BTM_ADDR,
                        SW8_BTM_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[3]);
#elif defined SWITCH_CONTROL_TEST_5
    static fsw_state_t fsw_states[] = {
        {FSW1_TOG_ADDR, FSW1_TOG_PORT_PIN, FSW1_ADDR, FSW1_PORT_PIN},
        {FSW2_TOG_ADDR, FSW2_TOG_PORT_PIN, FSW2_ADDR, FSW2_PORT_PIN}
    };
    static switch_debouncer_t fsw_debouncers[] = {
        {get_fsw_req_state, get_fsw_pin_state, reset_fsw_req_state, fsw1_func, 1, 0,
            (void*)&fsw_states[0]},
        {get_fsw_req_state, get_fsw_pin_state, reset_fsw_req_state, fsw2_func, 1, 0,
            (void*)&fsw_states[1]},
    };
    /* The addresses and pins passed here are just dummy values. */
    switch_control_init(&switch_control[0],
                        (volatile uint32_t*)NULL,
                        0,
                        switch_control_test_debounce_fsw_func,
                        (void*)&fsw_debouncers[0]);
    switch_control_init(&switch_control[1],
                        (volatile uint32_t*)NULL,
                        0,
                        switch_control_test_debounce_fsw_func,
                        (void*)&fsw_debouncers[1]);
#elif defined SWITCH_CONTROL_TEST_6
    switch_control_init(&switch_control[0],
                        FSW1_ADDR,
                        FSW1_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[0]);
    switch_control_init(&switch_control[1],
                        FSW2_ADDR,
                        FSW2_PORT_PIN,
                        switch_control_test_func,
                        (void*)&switch_data[1]);
#endif /* SWITCH_CONTROL_TEST_{1-4,6} */
#if defined(SWITCH_CONTROL_TEST_1) || defined(SWITCH_CONTROL_TEST_2) \
    || defined(SWITCH_CONTROL_TEST_3) || defined(SWITCH_CONTROL_TEST_4)
    int n;
    for (n = 0; n < 4; n++) {
        switch_control_add(&switch_control[n]);
    }
#endif
#if defined(SWITCH_CONTROL_TEST_5)|| defined(SWITCH_CONTROL_TEST_6)
    int n;
    for (n = 0; n < 2; n++) {
        switch_control_add(&switch_control[n]);
    }
#endif
}