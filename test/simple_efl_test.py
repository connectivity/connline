#!/usr/bin/python

from ctypes import cdll, c_void_p, c_int, c_char_p, POINTER, CFUNCTYPE
from ctypes.util import find_library
from sys import exit


CALLBACK_PROTO = CFUNCTYPE(None, c_void_p, c_int, POINTER(c_char_p), c_void_p)

def error_callback(ctx, event, nothing, user_data):
    print 'Error callback %d' %event
    #connline.connline_close(ctx)
    if connline.connline_open(ctx) != 0:
        connline.connline_cleanup()
        exit(0)

error_func = CALLBACK_PROTO(error_callback)

def connected_callback(ctx, event, nothing, user_data):
    print 'Connected callback %d' % event

connected_func = CALLBACK_PROTO(connected_callback)

def disconnected_callback(ctx, event, nothing, user_data):
    print 'Disconnected callback %d' % event

disconnected_func = CALLBACK_PROTO(disconnected_callback)

def property_callback(ctx, event, properties, user_data):
    print 'Property callback %d' % event
    print properties

property_func = CALLBACK_PROTO(property_callback)

connline_library = '../.libs/libconnline.so.0.0.0'

global connline

try:
    connline = cdll.LoadLibrary(connline_library)
except OSError, e:
    print e
    exit(-1)

print connline

init = connline.connline_init(1)

if init < 0:
    print 'Init failed'
    exit(-1)

ctx = connline.connline_new((1 << 1)|(1 << 2))

if ctx == 0:
    print 'failed to create a context'
    exit(-1)

connline.connline_set_error_callback(ctx, error_func)
connline.connline_set_connected_callback(ctx, connected_func)
connline.connline_set_disconnected_callback(ctx, disconnected_func)
connline.connline_set_property_callback(ctx, property_func)

print ctx

connline.connline_open(ctx, True)

print 'loop...'

#ToDo

print '... end'

