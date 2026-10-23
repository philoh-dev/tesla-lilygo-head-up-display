#pragma once

// NimBLE is a C library and some of its public headers intentionally use C and
// GNU extensions, including zero-length arrays and named variadic macros.
// Keep Tesla Head-Up Display's strict warning policy while treating those external declarations
// as a compatibility boundary.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wvariadic-macros"

#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_sm.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"

#pragma GCC diagnostic pop
