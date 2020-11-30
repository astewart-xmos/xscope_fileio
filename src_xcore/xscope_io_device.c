// Copyright (c) 2020, XMOS Ltd, All rights reserved
#include "xscope_io_device.h"
#include <xcore/chanend.h>
#include <xcore/hwtimer.h>
#include <xcore/select.h>
#include <xcore/assert.h>
#include <xcore/lock.h>

#include <string.h>
#include <xscope.h>
#include <stdio.h>

#define VERBOSE                 0


//Global chanend so we don't need to keep passing it in for read operations
chanend_t c_xscope = 0;
unsigned file_idx = 0;
lock_t file_access_lock;

void xscope_io_init(chanend_t xscope_end){
    file_access_lock = lock_alloc();
    xscope_mode_lossless();
    c_xscope = xscope_end;
    xscope_connect_data_from_host(c_xscope);
}

xscope_file_t xscope_open_file(char* filename, char* attributes){
    lock_acquire(file_access_lock);
    xscope_file_t xscope_file;
    strcpy(xscope_file.filename, filename);
    char packet[1 + MAX_FILENAME_LEN + 1];
    unsigned length = 1 + 1 + strlen(xscope_file.filename) + 1;
    xassert(length <= 1 + 1 + MAX_FILENAME_LEN + 1);
    packet[0] = '0' + file_idx;
    if(!strcmp(attributes, "rb")){
        xscope_file.mode = XSCOPE_IO_READ_BINARY;
    }
    else if(!strcmp(attributes, "rt")){
                xscope_file.mode = XSCOPE_IO_READ_TEXT;
    }
    else if(!strcmp(attributes, "wb")){
                xscope_file.mode = XSCOPE_IO_WRITE_BINARY;
    }
    else if(!strcmp(attributes, "wt")){
                xscope_file.mode = XSCOPE_IO_WRITE_TEXT;
    }
    else{
        printf("Unknown file attribytes: %s. Please specify from: rb, rt, wb, wt\n", attributes);
    }
    packet[1] = '0' + xscope_file.mode;
    strcpy(&packet[2], xscope_file.filename);
    xscope_file.index = file_idx;
    xscope_bytes(XSCOPE_ID_OPEN_FILE, length, (const unsigned char *)packet);
    file_idx++;
    lock_release(file_access_lock);
    if(file_idx == MAX_FILES_OPEN){
        printf("Maximum number of files open exceeded (%u)", MAX_FILES_OPEN);
    }

    //Pass a copy of the struct back to the caller
    return xscope_file;
}

size_t xscope_fread(uint8_t *buffer, size_t n_bytes_to_read, xscope_file_t *xscope_file){
    lock_acquire(file_access_lock);
    xassert(xscope_file->mode == XSCOPE_IO_READ_BINARY || xscope_file->mode == XSCOPE_IO_READ_TEXT);

    unsigned end_marker_found = 0;
    unsigned n_bytes_read = 0;

    uint8_t *buffer_ptr = buffer;
    unsigned chunk_complete = 0;

    unsigned char packet[1 + sizeof(size_t)];
    packet[0] = xscope_file->index + '0';
    memcpy(&packet[1], &n_bytes_to_read, sizeof(n_bytes_to_read));

    xscope_bytes(XSCOPE_ID_READ_BYTES, sizeof(packet), packet);

    do
    {
        int bytes_read = 0;
        SELECT_RES(CASE_THEN(c_xscope, read_host_data))
        {
        read_host_data:
            {
                xscope_data_from_host(c_xscope, (char *)buffer_ptr, &bytes_read);
                // printf("rx %u bytes\n", bytes_read);
                end_marker_found = ((bytes_read == END_MARKER_LEN) && !memcmp(buffer_ptr, END_MARKER_STRING, END_MARKER_LEN)) ? 1 : 0;
                if(end_marker_found){
                    // printf("end_marker_found\n");
                    break;
                }
                buffer_ptr += bytes_read;
                n_bytes_read += bytes_read;
                break;
            }
        }
        if((n_bytes_read == n_bytes_to_read) || end_marker_found){
            chunk_complete = 1;
        }
    } while(!chunk_complete);
    if(VERBOSE) printf("Received: %u bytes\n", n_bytes_read);

    lock_release(file_access_lock);
    return n_bytes_read;
}

void xscope_fwrite(uint8_t *buffer, size_t n_bytes_to_write, xscope_file_t *xscope_file){
    lock_acquire(file_access_lock);
    xassert(xscope_file->mode == XSCOPE_IO_WRITE_BINARY || xscope_file->mode == XSCOPE_IO_WRITE_TEXT);

    unsigned char packet[1 + sizeof(unsigned)];
    packet[0] = xscope_file->index + '0';
    memcpy(&packet[1], &n_bytes_to_write, sizeof(n_bytes_to_write));

    xscope_bytes(XSCOPE_ID_WRITE_SETUP, sizeof(packet), packet);

    // Chunk it up as seems more reliable although should be OK with tools 15.0.1
    // Tx is around 10x faster anyhow so a little extra overhead not an issue
    unsigned sent_so_far = 0;
    do{
        if(n_bytes_to_write - sent_so_far >=  MAX_XSCOPE_SIZE_BYTES){
            xscope_bytes(XSCOPE_ID_WRITE_BYTES, MAX_XSCOPE_SIZE_BYTES, (const unsigned char*)&buffer[sent_so_far]);
            sent_so_far += MAX_XSCOPE_SIZE_BYTES;
        }
        else{
            xscope_bytes(XSCOPE_ID_WRITE_BYTES, n_bytes_to_write - sent_so_far, (const unsigned char*)&buffer[sent_so_far]);
            sent_so_far = n_bytes_to_write;
        }
        // delay_ticks(10000); /// Magic number found to make xscope stable on MAC, else you get WRITE ERROR ON UPLOAD ....
        // Not needed with tools 15.0.1
    }
    while (sent_so_far < n_bytes_to_write);
    
    if(VERBOSE) printf("Sent %u bytes\n", n_bytes_to_write);
    lock_release(file_access_lock);
}

void xscope_close_files(void){
    xscope_bytes(XSCOPE_ID_HOST_QUIT, 1, (unsigned char*)"!");
    if(VERBOSE) printf("Sent close files\n");
    hwtimer_t t = hwtimer_alloc(); hwtimer_delay(t, 5000000); //50ms to allow messages to make it before xgdb quit
}