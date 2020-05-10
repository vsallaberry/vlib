/*
 * Copyright (C) 2020 Vincent Sallaberry
 * vlib <https://github.com/vsallaberry/vlib>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/* ------------------------------------------------------------------------
 * Simple math utilities.
 */
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#include "vlib/util.h"

#include "vlib_private.h"

/* ************************************************************************ */
unsigned long pgcd(long a, long b) {
    unsigned long r;
    while (b != 0) {
        r = a % b;
        a = b;
        b = r;
    }
    if (a >= 0)
        return a;
    return -(a);
}

/* ************************************************************************ */
unsigned long pgcd_rounded(long value1, long value2,
                            double * p_precision, double min_precision) {

    if (p_precision == NULL || *p_precision <= 0.0L || min_precision <= 0.0L) {
        return 0;
    }

    if (value1 == 0 || value2 == 0 || value1 % value2 == 0 || value2 % value1 == 0) {
        return pgcd(value1, value2);
    }

    if (value1 < 0L)
        value1 = -(value1);
    if (value2 < 0L)
        value2 = -(value2);

    long min = value2 < value1 ? value2 : value1;

    while (min < *p_precision && *p_precision > min_precision) {
        *p_precision /= 2.0L;
    }
    if (*p_precision < min_precision)
        *p_precision = min_precision;

    return pgcd(round(value1 / *p_precision), round(value2 / *p_precision)) * (*p_precision);
}

