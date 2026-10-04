#!/usr/bin/env python3
"""Render local SVG via system librsvg/Cairo (documentation regeneration only)."""
import ctypes as C
from ctypes.util import find_library

def render_svg(source, target, width, height):
    r=C.CDLL(find_library('rsvg-2')); c=C.CDLL(find_library('cairo')); g=C.CDLL(find_library('gobject-2.0'))
    r.rsvg_handle_new_from_file.argtypes=[C.c_char_p,C.c_void_p];r.rsvg_handle_new_from_file.restype=C.c_void_p
    r.rsvg_handle_render_cairo.argtypes=[C.c_void_p,C.c_void_p];r.rsvg_handle_render_cairo.restype=C.c_int
    c.cairo_image_surface_create.argtypes=[C.c_int,C.c_int,C.c_int];c.cairo_image_surface_create.restype=C.c_void_p
    c.cairo_create.argtypes=[C.c_void_p];c.cairo_create.restype=C.c_void_p
    c.cairo_surface_write_to_png.argtypes=[C.c_void_p,C.c_char_p];c.cairo_surface_write_to_png.restype=C.c_int
    c.cairo_destroy.argtypes=[C.c_void_p];c.cairo_surface_destroy.argtypes=[C.c_void_p];g.g_object_unref.argtypes=[C.c_void_p]
    handle=r.rsvg_handle_new_from_file(str(source).encode(),None)
    if not handle:raise RuntimeError('Unable to read SVG')
    surface=c.cairo_image_surface_create(0,width,height);ctx=c.cairo_create(surface)
    if not r.rsvg_handle_render_cairo(handle,ctx):raise RuntimeError('Unable to render SVG')
    if c.cairo_surface_write_to_png(surface,str(target).encode()):raise RuntimeError('Unable to write PNG')
    c.cairo_destroy(ctx);c.cairo_surface_destroy(surface);g.g_object_unref(handle)
