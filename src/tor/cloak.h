/* Copyright (c) 2014, Cloak Developers */
/* See LICENSE for licensing information */

/**
 * \file cloak.h
 * \brief Headers for cloak.cpp
 **/

#ifndef TOR_CLOAK_H
#define TOR_CLOAK_H

#ifdef __cplusplus
extern "C" {
#endif

    char const* cloak_tor_data_directory(
    );

    char const* cloak_service_directory(
    );

    int check_interrupted(
    );

    void set_initialized(
    );

    void wait_initialized(
    );

#ifdef __cplusplus
}
#endif

#endif

