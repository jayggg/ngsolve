from ngsolve import __platform
if __platform.startswith('linux') or __platform.startswith('darwin'):
    # Linux or Mac OS X
    from libngla.ngla import *

if __platform.startswith('win'):
    # Windows
    from ngslib.la import *

__all__ = ['BaseMatrix', 'BaseVector', 'InnerProduct']

