import sys
from os import sys, path

# import Base/params_utils.py
sys.path.append(path.join(path.dirname(path.dirname(path.abspath(__file__))), 'Base'))
import params_utils

from params_utils import ParamBool, ParamInt, ParamString, ParamUInt, ParamFloat

NameSpace = 'App'
ClassName = 'LinkParams'
ParamPath = 'User parameter:BaseApp/Preferences/Link'
ClassDoc = 'Convenient class to obtain App::Link related parameters'

Params = [
    ParamBool('CopyOnChangeApplyToAll', True),
]

def declare():
    params_utils.declare_begin(sys.modules[__name__], header=False)
    params_utils.declare_end(sys.modules[__name__])

def define():
    params_utils.define(sys.modules[__name__], header=False)
