#!/usr/bin/env python

"""
setup.py file for SWIG example
"""

from distutils.core import setup, Extension


quad_storage_module = Extension('_quad_storage',
                           sources=['quad_index_wrap.c'],
                           )

setup (name = 'quad_storage',
       version = '0.1',
       author      = "Manuel Salvadores",
       description = """quad storage python bindings""",
       ext_modules = [quad_storage_module],
       py_modules = ["quad_storage"],
       )
