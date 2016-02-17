from distutils.core import setup, Extension

extension = Extension("rrrr",
                      sources = ["rrrr.c"],
                      libraries = ['rrrr', 'protobuf-c', 'm'],
                      library_dirs = ['../build'],
                      include_dirs = ['..'])

setup(name="rrrr", version="1.0", ext_modules = [extension])

