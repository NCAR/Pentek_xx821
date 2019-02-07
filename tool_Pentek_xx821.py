import os
import sys

import eol_scons

# Any environment setting will be the default, but it can be overridden by
# setting the configuration variable.

requiredTools = [
    'boost_thread',
    'logx',
    'Navigator_xx821', # Pentek's Navigator BSP
    'doxygen',
]

env = Environment(tools = ['default'] + requiredTools)
variables = eol_scons.GlobalVariables()
variables.Update(env)

# This library must be compiled with C++11 enabled
env.AppendUnique(CXXFLAGS=['-std=c++11'])

libsources = Split("""
Pentek_xx821.cpp
""")

headers = Split("""
Pentek_xx821.h
""")

libpentek = env.Library('Pentek_xx821', libsources)
Default(libpentek)

if not 'DOXYFILE_DICT' in env:
    env['DOXYFILE_DICT'] = {}
env['DOXYFILE_DICT'].update({'PROJECT_NAME':'Pentek_xx821'})
html = env.Apidocs(libsources + headers)
Default(html)

thisdir = env.Dir('.').srcnode().abspath
def Pentek_xx821(env):
    env.AppendUnique(CPPPATH = [thisdir])
    env.AppendLibrary('Pentek_xx821')
    env.AppendDoxref('Pentek_xx821')
    env.Require(requiredTools)

Export('Pentek_xx821')

# Build the test programs
SConscript("test/SConscript")
